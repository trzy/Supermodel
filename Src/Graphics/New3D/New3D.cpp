#include "New3D.h"
#include "Texture.h"
#include "Vec.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include "R3DFloat.h"

#define MAX_RAM_POLYS 100000	
#define MAX_ROM_POLYS 500000

#ifndef M_PI
#define M_PI 3.14159265359
#endif

namespace New3D {

CNew3D::CNew3D()
{
	m_cullingRAMLo	= nullptr;
	m_cullingRAMHi	= nullptr;
	m_polyRAM		= nullptr;
	m_vrom			= nullptr;
	m_textureRAM	= nullptr;
}

CNew3D::~CNew3D()
{
	m_vbo.Destroy();
}

void CNew3D::AttachMemory(const UINT32 *cullingRAMLoPtr, const UINT32 *cullingRAMHiPtr, const UINT32 *polyRAMPtr, const UINT32 *vromPtr, const UINT16 *textureRAMPtr)
{
	m_cullingRAMLo	= cullingRAMLoPtr;
	m_cullingRAMHi	= cullingRAMHiPtr;
	m_polyRAM		= polyRAMPtr;
	m_vrom			= vromPtr;
	m_textureRAM	= textureRAMPtr;
}

void CNew3D::SetStep(int stepID)
{
	m_step = stepID;

	if ((m_step != 0x10) && (m_step != 0x15) && (m_step != 0x20) && (m_step != 0x21)) {
		m_step = 0x10;
	}

	if (m_step > 0x10) {
		m_offset = 0;							// culling nodes are 10 words
		m_vertexFactor = (1.0f / 2048.0f);		// vertices are in 13.11 format
	}
	else {
		m_offset = 2;							// 8 words
		m_vertexFactor = (1.0f / 128.0f);		// 17.7
	}

	m_vbo.Create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(Poly) * (MAX_RAM_POLYS + MAX_ROM_POLYS));
}

bool CNew3D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXResParam, unsigned totalYResParam)
{
	// Resolution and offset within physical display area
	m_xRatio	= xRes / 496.0f;
	m_yRatio	= yRes / 384.0f;
	m_xOffs		= xOffset;
	m_yOffs		= yOffset;
	m_totalXRes	= totalXResParam;
	m_totalYRes = totalYResParam;

	m_r3dShader.LoadShader();

	glUseProgram(0);

	return OKAY;	// OKAY ? wtf ..
}

void CNew3D::UploadTextures(unsigned x, unsigned y, unsigned width, unsigned height)
{
	if (x >= 1024) {
		if (y >= 512 && y < 1024 || y >= 1536 && y < 2048) {
			return;
		}
	}

	m_texSheet.Invalidate(x, y, width, height);
}

void CNew3D::DrawScrollFog()
{
	for (int i = 0; i < 4; i++) {

		for (auto &n : m_nodes) {

			if (n.viewport.scrollFog > 0 && n.viewport.priority == i) {	

				if (n.viewport.fogParams[3] || n.viewport.fogParams[4]) {	// check we some fog values set density or start

					float *rgb = n.viewport.fogParams;
					m_r3dScrollFog.DrawScrollFog(rgb[0], rgb[1], rgb[2], n.viewport.scrollFog);

					return;	// only allowed once per frame?
				}
			}
		}
	}
}

void CNew3D::RenderScene(int priority, bool alpha)
{
	if (alpha) {
		glEnable(GL_BLEND);
	}

	for (auto &n : m_nodes) {

		if (n.viewport.priority != priority || n.models.empty()) {
			continue;
		}

		std::shared_ptr<Texture> tex1;

		glViewport		(n.viewport.x, n.viewport.y, n.viewport.width, n.viewport.height);
		glMatrixMode	(GL_PROJECTION);
		glLoadMatrixf	(n.viewport.projectionMatrix);
		glMatrixMode	(GL_MODELVIEW);

		m_r3dShader.SetViewportUniforms(&n.viewport);

		for (auto &m : n.models) {

			bool matrixLoaded = false;

			if (m.meshes->empty()) {
				continue;
			}

			m_r3dShader.SetModelStates(&m);

			for (auto &mesh : *m.meshes) {

				if (alpha) {
					if (!mesh.textureAlpha && !mesh.polyAlpha) {
						continue;
					}
				}
				else {
					if (mesh.textureAlpha || mesh.polyAlpha) {
						continue;
					}
				}

				if (!matrixLoaded) {
					glLoadMatrixf(m.modelMat);
					matrixLoaded = true;		// do this here to stop loading matrices we don't need. Ie when rendering non transparent etc
				}
				
				if (mesh.textured) {

					int x, y;
					CalcTexOffset(m.textureOffsetX, m.textureOffsetY, m.page, mesh.x, mesh.y, x, y);

					if (tex1 && tex1->Compare(x, y, mesh.width, mesh.height, mesh.format)) {
						tex1->SetWrapMode(mesh.mirrorU, mesh.mirrorV);	
					}
					else {
						tex1 = m_texSheet.BindTexture(m_textureRAM, mesh.format, mesh.mirrorU, mesh.mirrorV, x, y, mesh.width, mesh.height);
						if (tex1) {
							tex1->BindTexture();
							tex1->SetWrapMode(mesh.mirrorU, mesh.mirrorV);
						}
					}

					if (mesh.microTexture) {

						int mX, mY;
						glActiveTexture(GL_TEXTURE1);
						m_texSheet.GetMicrotexPos(y / 1024, mesh.microTextureID, mX, mY);
						auto tex2 = m_texSheet.BindTexture(m_textureRAM, 0, false, false, mX, mY, 128, 128);
						if (tex2) {
							tex2->BindTexture();
						}
						glActiveTexture(GL_TEXTURE0);
					}
				}
				
				m_r3dShader.SetMeshUniforms(&mesh);
				glDrawArrays(GL_TRIANGLES, mesh.vboOffset*3, mesh.triangleCount*3);			// times 3 to convert triangles to vertices
			}
		}
	}

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDisable(GL_STENCIL_TEST);
}

void CNew3D::RenderFrame(void)
{
	// release any resources from last frame
	m_polyBufferRam.clear();	// clear dyanmic model memory buffer
	m_nodes.clear();		// memory will grow during the object life time, that's fine, no need to shrink to fit
	m_modelMat.Release();	// would hope we wouldn't need this but no harm in checking
	m_nodeAttribs.Reset();

	RenderViewport(0x800000);					// build model structure

	DrawScrollFog();							// fog layer if applicable must be drawn here

	glDepthFunc		(GL_LEQUAL);
	glEnable		(GL_DEPTH_TEST);
	glActiveTexture	(GL_TEXTURE0);
	glEnable		(GL_CULL_FACE);
	glFrontFace		(GL_CW);

	glStencilFunc	(GL_EQUAL, 0, 0xFF);			// basically stencil test passes if the value is zero
	glStencilOp		(GL_KEEP, GL_INCR, GL_INCR);		// if the stencil test passes, we incriment the value
	glStencilMask	(0xFF);
	
	m_vbo.Bind(true);
	m_vbo.BufferSubData(MAX_ROM_POLYS*sizeof(Poly), m_polyBufferRam.size()*sizeof(Poly), m_polyBufferRam.data());	// upload all the dynamic data to GPU in one go

	if (m_polyBufferRom.size()) {

		// sync rom memory with vbo
		int romBytes	= (int)m_polyBufferRom.size() * sizeof(Poly);
		int vboBytes	= m_vbo.GetSize();
		int size		= romBytes - vboBytes;

		if (size) {
			//check we haven't blown up the memory buffers
			//we will lose rom models for 1 frame is this happens, not the end of the world, as probably won't ever happen anyway
			if (m_polyBufferRom.size() >= MAX_ROM_POLYS) {
				m_polyBufferRom.clear();
				m_romMap.clear();
				m_vbo.Reset();
			}
			else {
				m_vbo.AppendData(size, &m_polyBufferRom[vboBytes / sizeof(Poly)]);
			}
		}
	}
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	// before draw, specify vertex and index arrays with their offsets, offsetof is maybe evil ..
	glVertexPointer		(3, GL_FLOAT, sizeof(Vertex), 0);
	glNormalPointer		(GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glTexCoordPointer	(2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));
	glColorPointer		(4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, color));

	m_r3dShader.SetShader(true);

	for (int pri = 0; pri <= 3; pri++) {
		glClear		(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		RenderScene	(pri, false);
		RenderScene	(pri, true);
	}

	m_r3dShader.SetShader(false);		// unbind shader
	m_vbo.Bind(false);

	glDisable(GL_CULL_FACE);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

void CNew3D::BeginFrame(void)
{
}

void CNew3D::EndFrame(void)
{
}

/******************************************************************************
Real3D Address Translation

Functions that interpret word-granular Real3D addresses and return pointers.
******************************************************************************/

// Translates 24-bit culling RAM addresses
const UINT32* CNew3D::TranslateCullingAddress(UINT32 addr)
{
	addr &= 0x00FFFFFF;	// caller should have done this already

	if ((addr >= 0x800000) && (addr < 0x840000)) {
		return &m_cullingRAMHi[addr & 0x3FFFF];
	}
	else if (addr < 0x100000) {
		return &m_cullingRAMLo[addr];
	}

	return NULL;
}

// Translates model references
const UINT32* CNew3D::TranslateModelAddress(UINT32 modelAddr)
{
	modelAddr &= 0x00FFFFFF;	// caller should have done this already

	if (modelAddr < 0x100000) {
		return &m_polyRAM[modelAddr];
	}
	else {
		return &m_vrom[modelAddr];
	}
}

bool CNew3D::DrawModel(UINT32 modelAddr)
{
	const UINT32*	modelAddress;
	bool			cached = false;
	Model*			m;

	modelAddress = TranslateModelAddress(modelAddr);

	// create a new model to push onto the vector
	m_nodes.back().models.emplace_back(Model());

	// get the last model in the array
	m = &m_nodes.back().models.back();

	if (IsVROMModel(modelAddr) && !IsDynamicModel((UINT32*)modelAddress)) {

		// try to find meshes in the rom cache

		m->meshes = m_romMap[modelAddr];	// will create an entry with a null pointer if empty

		if (m->meshes) {
			cached = true;
		}
		else {
			m->meshes = std::make_shared<std::vector<Mesh>>();
			m_romMap[modelAddr] = m->meshes;		// store meshes in our rom map here
		}

		m->dynamic = false;
	}
	else {
		m->meshes = std::make_shared<std::vector<Mesh>>();
	}

	// copy current model matrix
	for (int i = 0; i < 16; i++) {
		m->modelMat[i] = m_modelMat.currentMatrix[i];
	}

	//calculate determinant
	m->determinant = Determinant3x3(m_modelMat);

	// update texture offsets
	m->textureOffsetX = m_nodeAttribs.currentTexOffsetX;
	m->textureOffsetY = m_nodeAttribs.currentTexOffsetY;
	m->page = m_nodeAttribs.currentPage;

	if (!cached) {
		CacheModel(m, modelAddress);
	}

	return true;
}

// Descends into a 10-word culling node
void CNew3D::DescendCullingNode(UINT32 addr)
{
	const UINT32	*node, *lodTable;
	UINT32			matrixOffset, child1Ptr, sibling2Ptr;
	float			x, y, z;
	int				tx, ty;
	BBox			bbox;

	if (m_nodeAttribs.StackLimit()) {
		return;
	}

	node = TranslateCullingAddress(addr);

	if (NULL == node) {
		return;
	}

	// Extract known fields
	child1Ptr		= node[0x07 - m_offset] & 0x7FFFFFF;	// mask colour table bits
	sibling2Ptr		= node[0x08 - m_offset] & 0x1FFFFFF;	// mask colour table bits
	matrixOffset	= node[0x03 - m_offset] & 0xFFF;

	if ((node[0x00] & 0x07) != 0x06) {						// colour table seems to indicate no siblings
		if (!(sibling2Ptr & 0x1000000) && sibling2Ptr) {
			DescendCullingNode(sibling2Ptr);				// no need to mask bit, would already be zero
		}
	}

	if ((node[0x00] & 0x04)) {
		m_colorTableAddr = ((node[0x03 - m_offset] >> 19) << 0) | ((node[0x07 - m_offset] >> 28) << 13) | ((node[0x08 - m_offset] >> 25) << 17);
		m_colorTableAddr &= 0x000FFFFF; // clamp to 4MB (in words) range
	}

	x = *(float *)&node[0x04 - m_offset];
	y = *(float *)&node[0x05 - m_offset];
	z = *(float *)&node[0x06 - m_offset];

	m_nodeAttribs.Push();	// save current attribs

	if (!m_offset)	// Step 1.5+
	{
		tx = 32 * ((node[0x02] >> 7) & 0x3F);
		ty = 32 * (node[0x02] & 0x1F);

		// apply texture offsets, else retain current ones
		if ((node[0x02] & 0x8000))	{
			m_nodeAttribs.currentTexOffsetX = tx;
			m_nodeAttribs.currentTexOffsetY = ty;
			m_nodeAttribs.currentPage = (node[0x02] & 0x4000) >> 14;
		}
	}

	// Apply matrix and translation
	m_modelMat.PushMatrix();

	// apply translation vector
	if ((node[0x00] & 0x10)) {
		m_modelMat.Translate(x, y, z);
	}
	// multiply matrix, if specified
	else if (matrixOffset) {
		MultMatrix(matrixOffset,m_modelMat);
	}

	if (m_nodeAttribs.currentClipStatus != Clip::INSIDE) {

		//================
		UINT16	hDistance;
		float	fDistance;
		//================

		hDistance = node[9 - m_offset] & 0xFFFF;
		fDistance = R3DFloat::GetFloat16(hDistance);

		if (hDistance != R3DFloat::Pro16BitMax) {

			CalcBox(fDistance, bbox);
			TransformBox(m_modelMat, bbox);

			m_nodeAttribs.currentClipStatus = ClipBox(bbox, m_planes);
		}
	}

	if (m_nodeAttribs.currentClipStatus != Clip::OUTSIDE) {

		// Descend down first link
		if ((node[0x00] & 0x08))	// 4-element LOD table
		{
			lodTable = TranslateCullingAddress(child1Ptr);

			if (NULL != lodTable) {
				if ((node[0x03 - m_offset] & 0x20000000)) {
					DescendCullingNode(lodTable[0] & 0xFFFFFF);
				}
				else {
					DrawModel(lodTable[0] & 0xFFFFFF);	//TODO
				}
			}
		}
		else {
			DescendNodePtr(child1Ptr);
		}

	}

	m_modelMat.PopMatrix();

	// Restore old texture offsets
	m_nodeAttribs.Pop();
}

void CNew3D::DescendNodePtr(UINT32 nodeAddr)
{
	// Ignore null links
	if ((nodeAddr & 0x00FFFFFF) == 0) {
		return;
	}

	switch ((nodeAddr >> 24) & 0xFF)	// pointer type encoded in upper 8 bits
	{
	case 0x00:	// culling node
		DescendCullingNode(nodeAddr & 0xFFFFFF);
		break;
	case 0x01:	// model (perhaps bit 2 is a flag in this case?)
	case 0x03:
		DrawModel(nodeAddr & 0xFFFFFF);
		break;
	case 0x04:	// pointer list
		DescendPointerList(nodeAddr & 0xFFFFFF);
		break;
	default:
		break;
	}
}

void CNew3D::DescendPointerList(UINT32 addr)
{
	const UINT32*	list;
	UINT32			nodeAddr;
	int				index;

	list = TranslateCullingAddress(addr);

	if (NULL == list) {
		return;
	}

	index = 0;

	while (true) {

		if (list[index] & 0x01000000) {
			break;	// empty list
		}

		nodeAddr = list[index] & 0x00FFFFFF;	// clear upper 8 bits to ensure this is processed as a culling node

		DescendCullingNode(nodeAddr);

		if (list[index] & 0x02000000) {
			break;	// list end
		}

		index++;
	}
}


/******************************************************************************
Matrix Stack
******************************************************************************/

// Macro to generate column-major (OpenGL) index from y,x subscripts
#define CMINDEX(y,x)	(x*4+y)

/*
* MultMatrix():
*
* Multiplies the matrix stack by the specified Real3D matrix. The matrix
* index is a 12-bit number specifying a matrix number relative to the base.
* The base matrix MUST be set up before calling this function.
*/
void CNew3D::MultMatrix(UINT32 matrixOffset, Mat4& mat)
{
	GLfloat		m[4*4];
	const float	*src = &m_matrixBasePtr[matrixOffset * 12];

	if (m_matrixBasePtr == NULL)	// LA Machineguns
		return;

	m[CMINDEX(0, 0)] = src[3];
	m[CMINDEX(0, 1)] = src[4];
	m[CMINDEX(0, 2)] = src[5];
	m[CMINDEX(0, 3)] = src[0];
	m[CMINDEX(1, 0)] = src[6];
	m[CMINDEX(1, 1)] = src[7];
	m[CMINDEX(1, 2)] = src[8];
	m[CMINDEX(1, 3)] = src[1];
	m[CMINDEX(2, 0)] = src[9];
	m[CMINDEX(2, 1)] = src[10];
	m[CMINDEX(2, 2)] = src[11];
	m[CMINDEX(2, 3)] = src[2];
	m[CMINDEX(3, 0)] = 0.0;
	m[CMINDEX(3, 1)] = 0.0;
	m[CMINDEX(3, 2)] = 0.0;
	m[CMINDEX(3, 3)] = 1.0;

	mat.MultMatrix(m);
}

/*
* InitMatrixStack():
*
* Initializes the modelview (model space -> view space) matrix stack and
* Real3D coordinate system. These are the last transforms to be applied (and
* the first to be defined on the stack) before projection.
*
* Model 3 games tend to define the following unusual base matrix:
*
*		0	0	-1	0
*		1	0	0	0
*		0	-1	0	0
*		0	0	0	1
*
* When this is multiplied by a column vector, the output is:
*
*		-Z
*		X
*		-Y
*		1
*
* My theory is that the Real3D GPU accepts vectors in Z,X,Y order. The games
* store everything as X,Y,Z and perform the translation at the end. The Real3D
* also has Y and Z coordinates opposite of the OpenGL convention. This
* function inserts a compensating matrix to undo these things.
*
* NOTE: This function assumes we are in GL_MODELVIEW matrix mode.
*/

void CNew3D::InitMatrixStack(UINT32 matrixBaseAddr, Mat4& mat)
{
	GLfloat m[4 * 4];

	// This matrix converts vectors back from the weird Model 3 Z,X,Y ordering
	// and also into OpenGL viewspace (-Y,-Z)
	m[CMINDEX(0, 0)] = 0.0;	m[CMINDEX(0, 1)] = 1.0;	m[CMINDEX(0, 2)] = 0.0;	m[CMINDEX(0, 3)] = 0.0;
	m[CMINDEX(1, 0)] = 0.0;	m[CMINDEX(1, 1)] = 0.0;	m[CMINDEX(1, 2)] =-1.0; m[CMINDEX(1, 3)] = 0.0;
	m[CMINDEX(2, 0)] =-1.0; m[CMINDEX(2, 1)] = 0.0;	m[CMINDEX(2, 2)] = 0.0;	m[CMINDEX(2, 3)] = 0.0;
	m[CMINDEX(3, 0)] = 0.0;	m[CMINDEX(3, 1)] = 0.0;	m[CMINDEX(3, 2)] = 0.0;	m[CMINDEX(3, 3)] = 1.0;

	mat.LoadMatrix(m);

	// Set matrix base address and apply matrix #0 (coordinate system matrix)
	m_matrixBasePtr = (float *)TranslateCullingAddress(matrixBaseAddr);
	MultMatrix(0, mat);
}

// Draws viewports of the given priority
void CNew3D::RenderViewport(UINT32 addr)
{
	static const GLfloat	color[8][3] =
	{											// RGB1 color translation
		{ 0.0, 0.0, 0.0 },	// off
		{ 0.0, 0.0, 1.0 },	// blue
		{ 0.0, 1.0, 0.0 },	// green
		{ 0.0, 1.0, 1.0 },	// cyan
		{ 1.0, 0.0, 0.0 }, 	// red
		{ 1.0, 0.0, 1.0 },	// purple
		{ 1.0, 1.0, 0.0 },	// yellow
		{ 1.0, 1.0, 1.0 }	// white
	};

	// Translate address and obtain pointer
	const uint32_t *vpnode = TranslateCullingAddress(addr);

	if (NULL == vpnode) {
		return;
	}

	if (vpnode[0x01] == 0) {		// memory probably hasn't been set up yet, abort
		return;
	}

	if (!(vpnode[0] & 0x20)) {	// only if viewport enabled
		uint32_t curPri	= (vpnode[0x00] >> 3) & 3;		// viewport priority
		uint32_t nodeAddr	= vpnode[0x02];							// scene database node pointer

		// create node object 
		m_nodes.emplace_back(Node());
		m_nodes.back().models.reserve(2048);			// create space for models

		// get pointer to its viewport
		Viewport *vp = &m_nodes.back().viewport;

		vp->priority = curPri;

		// Fetch viewport parameters (TO-DO: would rounding make a difference?)
		int vpX			= (int)(((vpnode[0x1A] & 0xFFFF) / 16.0f) + 0.5f);		// viewport X (12.4 fixed point)
		int vpY			= (int)(((vpnode[0x1A] >> 16) / 16.0f) + 0.5f);			// viewport Y (12.4)
		int vpWidth		= (int)(((vpnode[0x14] & 0xFFFF) / 4.0f) + 0.5f);		// width (14.2)
		int vpHeight	= (int)(((vpnode[0x14] >> 16) / 4.0f) + 0.5f);			// height (14.2)
		uint32_t matrixBase	= vpnode[0x16] & 0xFFFFFF;							// matrix base address

		if (vpX) {
			vpX += 2;
		}

		if (vpY) {
			vpY += 2;
		}

		LODBlendTable* tableTest = (LODBlendTable*)TranslateCullingAddress(vpnode[0x17]);

		float angle_left	= -atan2(*(float *)&vpnode[12],  *(float *)&vpnode[13]);
		float angle_right	=  atan2(*(float *)&vpnode[16], -*(float *)&vpnode[17]);
		float angle_top		=  atan2(*(float *)&vpnode[14],  *(float *)&vpnode[15]);
		float angle_bottom	= -atan2(*(float *)&vpnode[18], -*(float *)&vpnode[19]);

		float near = 0.25f;
		float far = 2e6;

		if (m_step == 0x10) {
			near = 8;
		}

		float l = near * tanf(angle_left);
		float r = near * tanf(angle_right);
		float t = near * tanf(angle_top);
		float b = near * tanf(angle_bottom);

		// TO-DO: investigate clipping near/far planes

		if ((vpX == 0) && (vpWidth >= 495) && (vpY == 0) && (vpHeight >= 383))
		{
			float windowAR		= (float)m_totalXRes / (float)m_totalYRes;
			float originalAR	= 496 / 384.f;
			float correction	= windowAR / originalAR;	// expand horizontal frustum planes

			vp->x		= 0;
			vp->y		= m_yOffs + (GLint)((float)(384 - (vpY + vpHeight))*m_yRatio);
			vp->width	= m_totalXRes;
			vp->height	= (GLint)((float)vpHeight*m_yRatio);

			vp->projectionMatrix.Frustum(l*correction, r*correction, b, t, near, far);
		}
		else
		{
			vp->x		= m_xOffs + (GLint)((float)vpX*m_xRatio);
			vp->y		= m_yOffs + (GLint)((float)(384 - (vpY + vpHeight))*m_yRatio);
			vp->width	= (GLint)((float)vpWidth*m_xRatio);
			vp->height	= (GLint)((float)vpHeight*m_yRatio);

			vp->projectionMatrix.Frustum(l, r, b, t, near, far);
		}

		// calculate frustum planes
		CalcFrustumPlanes(m_planes, vp->projectionMatrix);

		// Lighting (note that sun vector points toward sun -- away from vertex)
		vp->lightingParams[0] = *(float *)&vpnode[0x05];								// sun X
		vp->lightingParams[1] = *(float *)&vpnode[0x06];								// sun Y
		vp->lightingParams[2] = *(float *)&vpnode[0x04];								// sun Z
		vp->lightingParams[3] = *(float *)&vpnode[0x07];								// sun intensity
		vp->lightingParams[4] = (float)((vpnode[0x24] >> 8) & 0xFF) * (1.0f / 255.0f);	// ambient intensity
		vp->lightingParams[5] = 0.0;	// reserved

		// Spotlight
		int spotColorIdx = (vpnode[0x20] >> 11) & 7;									// spotlight color index
		vp->spotEllipse[0] = (float)((vpnode[0x1E] >> 3) & 0x1FFF);						// spotlight X position (fractional component?)
		vp->spotEllipse[1] = (float)((vpnode[0x1D] >> 3) & 0x1FFF);						// spotlight Y
		vp->spotEllipse[2] = (float)((vpnode[0x1E] >> 16) & 0xFFFF);					// spotlight X size (16-bit? May have fractional component below bit 16)
		vp->spotEllipse[3] = (float)((vpnode[0x1D] >> 16) & 0xFFFF);					// spotlight Y size

		vp->spotRange[0] = 1.0f / (*(float *)&vpnode[0x21]);							// spotlight start
		vp->spotRange[1] = *(float *)&vpnode[0x1F];										// spotlight extent

		if (vp->spotRange[1] == 0) {													// if light extent = 0 light is effectively disabled
			spotColorIdx = 0;															
		}

		vp->spotColor[0] = color[spotColorIdx][0];										// spotlight color
		vp->spotColor[1] = color[spotColorIdx][1];
		vp->spotColor[2] = color[spotColorIdx][2];

		// Spotlight is applied on a per pixel basis, must scale its position and size to screen
		vp->spotEllipse[1] = 384.0f - vp->spotEllipse[1];
		vp->spotRange[1] += vp->spotRange[0];	// limit
		vp->spotEllipse[2] = 496.0f / sqrt(vp->spotEllipse[2]);	// spotlight appears to be specified in terms of physical resolution (unconfirmed)
		vp->spotEllipse[3] = 384.0f / sqrt(vp->spotEllipse[3]);

		// Scale the spotlight to the OpenGL viewport
		vp->spotEllipse[0] = vp->spotEllipse[0] * m_xRatio + m_xOffs;
		vp->spotEllipse[1] = vp->spotEllipse[1] * m_yRatio + m_yOffs;
		vp->spotEllipse[2] *= m_xRatio;
		vp->spotEllipse[3] *= m_yRatio;

		// Fog
		vp->fogParams[0] = (float)((vpnode[0x22] >> 16) & 0xFF) * (1.0f / 255.0f);	// fog color R
		vp->fogParams[1] = (float)((vpnode[0x22] >> 8) & 0xFF) * (1.0f / 255.0f);	// fog color G
		vp->fogParams[2] = (float)((vpnode[0x22] >> 0) & 0xFF) * (1.0f / 255.0f);	// fog color B
		vp->fogParams[3] = std::abs(*(float *)&vpnode[0x23]);						// fog density	- ocean hunter uses negative values, but looks the same
		vp->fogParams[4] = (float)(INT16)(vpnode[0x25] & 0xFFFF)*(1.0f / 255.0f);	// fog start

		vp->scrollFog = (float)(vpnode[0x20] & 0xFF) * (1.0f / 255.0f);				// scroll fog

		{
			//test fog paramaters
			float lightFogColour[3];
			int fogColourIdx;

			fogColourIdx = (vpnode[0x20] >> 8) & 7;

			lightFogColour[0] = color[fogColourIdx][0];
			lightFogColour[1] = color[fogColourIdx][1];
			lightFogColour[2] = color[fogColourIdx][2];

			float fogAttenuation = ((vpnode[0x24] >> 16) & 0xFF) / 255.f;
			float fogAmbient	 = ((vpnode[0x25] >> 16) & 0xFF) / 255.f;
			int debug = 0;
		}
		
		if (std::isinf(vp->fogParams[3]) || std::isnan(vp->fogParams[3]) || std::isinf(vp->fogParams[4]) || std::isnan(vp->fogParams[4])) {	// Star Wars Trilogy
			vp->fogParams[3] = vp->fogParams[4] = 0.0f;
		}

		// Unknown light/fog parameters
		float scrollAtt = (float)(vpnode[0x24] & 0xFF) * (1.0f / 255.0f);	// scroll attenuation

		// Clear texture offsets before proceeding
		m_nodeAttribs.Reset();

		// Set up coordinate system and base matrix
		InitMatrixStack(matrixBase, m_modelMat);

		// Safeguard: weird coordinate system matrices usually indicate scenes that will choke the renderer
		if (NULL != m_matrixBasePtr)
		{
			float	m21, m32, m13;

			// Get the three elements that are usually set and see if their magnitudes are 1
			m21 = m_matrixBasePtr[6];
			m32 = m_matrixBasePtr[10];
			m13 = m_matrixBasePtr[5];

			m21 *= m21;
			m32 *= m32;
			m13 *= m13;

			if ((m21>1.05) || (m21<0.95))
				return;
			if ((m32>1.05) || (m32<0.95))
				return;
			if ((m13>1.05) || (m13<0.95))
				return;
		}

		// Descend down the node link: Use recursive traversal
		DescendNodePtr(nodeAddr);
	}

	// render next viewport
	if (vpnode[0x01] != 0x01000000) {
		RenderViewport(vpnode[0x01]);
	}
}

void CNew3D::CopyVertexData(const R3DPoly& r3dPoly, std::vector<Poly>& polyArray)
{
	//====================
	Poly		p;
	V3::Vec3	normal;
	float		dotProd;
	bool		clockWise;
	//====================

	V3::createNormal(r3dPoly.v[0].pos, r3dPoly.v[1].pos, r3dPoly.v[2].pos, normal);

	dotProd		= V3::dotProduct(normal, r3dPoly.faceNormal);
	clockWise	= dotProd >= 0.0;

	if (clockWise) {
		p.p1 = r3dPoly.v[0];
		p.p2 = r3dPoly.v[1];
		p.p3 = r3dPoly.v[2];
	}
	else {
		p.p1 = r3dPoly.v[2];
		p.p2 = r3dPoly.v[1];
		p.p3 = r3dPoly.v[0];
	}

	//multiply face attributes with vertex attributes if required
	for (int i = 0; i < 4; i++) {
		p.p1.color[i] = p.p1.color[i] * r3dPoly.faceColour[i];
		p.p2.color[i] = p.p2.color[i] * r3dPoly.faceColour[i];
		p.p3.color[i] = p.p3.color[i] * r3dPoly.faceColour[i];
	}
	
	polyArray.emplace_back(p);

	if (r3dPoly.number == 4) {

		if (clockWise) {
			p.p1 = r3dPoly.v[0];
			p.p2 = r3dPoly.v[2];
			p.p3 = r3dPoly.v[3];
		}
		else {
			p.p1 = r3dPoly.v[0];
			p.p2 = r3dPoly.v[3];
			p.p3 = r3dPoly.v[2];
		}

		//multiply face attributes with vertex attributes if required
		for (int i = 0; i < 4; i++) {
			p.p1.color[i] = p.p1.color[i] * r3dPoly.faceColour[i];
			p.p2.color[i] = p.p2.color[i] * r3dPoly.faceColour[i];
			p.p3.color[i] = p.p3.color[i] * r3dPoly.faceColour[i];
		}

		polyArray.emplace_back(p);
	}
}

// non smooth texturing on the pro-1000 seems to sample like gl_nearest
// ie not outside of the texture coordinates, but with bilinear filtering
// this is about as close as we can emulate in hardware
// if we don't do this with gl_repeat enabled, it will wrap around and sample the 
// other side of the texture which produces ugly seems
void CNew3D::OffsetTexCoords(R3DPoly& r3dPoly, float offset[2])
{
	for (int i = 0; i < 2; i++) {

		float min =  std::numeric_limits<float>::max();
		float max = -std::numeric_limits<float>::max();

		if (!offset[i]) continue;

		for (int j = 0; j < r3dPoly.number; j++) {
			min = std::min(r3dPoly.v[j].texcoords[i], min);
			max = std::max(r3dPoly.v[j].texcoords[i], max);
		}

		float fTemp;
		float iTemp;
		bool fractMin;
		bool fractMax;
		
		fTemp		= std::modf(min, &iTemp);
		fractMin	= fTemp > 0;

		fTemp		= std::modf(max, &iTemp);
		fractMax	= fTemp > 0;

		for (int j = 0; j < r3dPoly.number && min != max; j++) {

			if (!fractMin) {
				if (r3dPoly.v[j].texcoords[i] == min) {
					r3dPoly.v[j].texcoords[i] += offset[i];
				}
			}

			if (!fractMax) {
				if (r3dPoly.v[j].texcoords[i] == max) {
					r3dPoly.v[j].texcoords[i] -= offset[i];
				}
			}
		}
	}
}

void CNew3D::SetMeshValues(SortingMesh *currentMesh, PolyHeader &ph)
{
	//copy attributes
	currentMesh->doubleSided	= false;			// we will double up polys
	currentMesh->textured		= ph.TexEnabled();
	currentMesh->alphaTest		= ph.AlphaTest();
	currentMesh->textureAlpha	= ph.TextureAlpha();
	currentMesh->polyAlpha		= ph.PolyAlpha();
	currentMesh->lighting		= ph.LightEnabled() && !ph.FixedShading();

	if (ph.Layered() || (!ph.TexEnabled() && ph.PolyAlpha())) {
		currentMesh->layered = true;
	}

	if (currentMesh->lighting) {
		if (ph.SpecularEnabled()) {
			currentMesh->specular = true;
			currentMesh->shininess = 0;// ph.Shininess();
			currentMesh->specularCoefficient = 0; // ph.SpecularValue();
		}
	}

	currentMesh->fogIntensity = ph.LightModifier();

	if (ph.TexEnabled()) {
		currentMesh->format = m_texSheet.GetTexFormat(ph.TexFormat(), ph.AlphaTest());

		if (currentMesh->format == 7) {
			currentMesh->alphaTest = false;	// alpha test is a 1 bit test, this format needs a lower threshold, since it has 16 levels of transparency
		}

		currentMesh->x				= ph.X();
		currentMesh->y				= ph.Y();
		currentMesh->width			= ph.TexWidth();
		currentMesh->height			= ph.TexHeight();
		currentMesh->mirrorU		= ph.TexUMirror();
		currentMesh->mirrorV		= ph.TexVMirror();
		currentMesh->microTexture	= ph.MicroTexture();
		currentMesh->microTextureID	= ph.MicroTextureID();
	}
}

void CNew3D::CacheModel(Model *m, const UINT32 *data)
{
	Vertex			prev[4];
	UINT16			prevTexCoords[4][2];
	PolyHeader		ph;
	int				numPolys	= 0;
	UINT64			lastHash	= -1;
	SortingMesh*	currentMesh = nullptr;
	
	std::map<UINT64, SortingMesh> sMap;

	if (data == NULL)
		return;

	ph = data; 
	int numTriangles = ph.NumTrianglesTotal();

	// Cache all polygons
	do {

		R3DPoly		p;					// current polygon
		float		uvScale;
		int			i, j;

		if (ph.header[6] == 0) {
			break;
		}

		if (ph.Disabled() || !numPolys && (ph.NumSharedVerts() != 0)) {
			continue;
		}

		// create a hash value based on poly attributes -todo add more attributes
		auto hash = ph.Hash();

		if (hash != lastHash) {

			if (sMap.count(hash) == 0) {

				sMap[hash] = SortingMesh();

				currentMesh = &sMap[hash];

				//make space for our vertices
				currentMesh->polys.reserve(numTriangles);

				//set mesh values
				SetMeshValues(currentMesh, ph);
			}

			currentMesh = &sMap[hash];
		}

		// Obtain basic polygon parameters
		p.number	= ph.NumVerts();
		uvScale		= ph.UVScale();

		ph.FaceNormal(p.faceNormal);

		// Fetch reused vertices according to bitfield, then new verts
		i = 0;
		j = 0;
		for (i = 0; i < 4; i++)		// up to 4 reused vertices
		{
			if (ph.SharedVertex(i))
			{
				p.v[j] = prev[i];

				//check if we need to recalc tex coords - will only happen if tex tiles are different + sharing vertices
				if (hash != lastHash) {
					if (currentMesh->textured) {
						Texture::GetCoordinates(currentMesh->width, currentMesh->height, prevTexCoords[i][0], prevTexCoords[i][1], uvScale, p.v[j].texcoords[0], p.v[j].texcoords[1]);
					}
				}

				j++;
			}
		}

		lastHash = hash;

		// copy face attributes

		if ((ph.header[1] & 2) == 0) {
			int colorIdx = ph.ColorIndex();
			p.faceColour[2] = (m_polyRAM[m_colorTableAddr + colorIdx] & 0xFF) / 255.f;
			p.faceColour[1] = ((m_polyRAM[m_colorTableAddr + colorIdx] >> 8) & 0xFF) / 255.f;
			p.faceColour[0] = ((m_polyRAM[m_colorTableAddr + colorIdx] >> 16) & 0xFF) / 255.f;
		}
		else {
			if (ph.ColorDisabled()) {		// no colours were set
				p.faceColour[0] = 1.0f;
				p.faceColour[1] = 1.0f;
				p.faceColour[2] = 1.0f;
			}
			else {
				p.faceColour[0] = ((ph.header[4] >> 24)) / 255.f;
				p.faceColour[1] = ((ph.header[4] >> 16) & 0xFF) / 255.f;
				p.faceColour[2] = ((ph.header[4] >> 8) & 0xFF) / 255.f;
			}
		}

		p.faceColour[3] = ph.Transparency() / 255.f;
				
		// if we have flat shading, we can't re-use normals from shared vertices
		for (i = 0; i < p.number && !ph.SmoothShading(); i++) {
			p.v[i].normal[0] = p.faceNormal[0];
			p.v[i].normal[1] = p.faceNormal[1];
			p.v[i].normal[2] = p.faceNormal[2];
		}

		UINT32* vData = ph.StartOfData();	// vertex data starts here

		// remaining vertices are new and defined here
		for (; j < p.number; j++)	
		{
			// Fetch vertices
			UINT32 ix = vData[0];
			UINT32 iy = vData[1];
			UINT32 iz = vData[2];
			UINT32 it = vData[3];

			// Decode vertices
			p.v[j].pos[0] = (GLfloat)(((INT32)ix) >> 8) * m_vertexFactor;
			p.v[j].pos[1] = (GLfloat)(((INT32)iy) >> 8) * m_vertexFactor;
			p.v[j].pos[2] = (GLfloat)(((INT32)iz) >> 8) * m_vertexFactor;

			// Per vertex normals
			if (ph.SmoothShading()) {
				p.v[j].normal[0] = (INT8)(ix & 0xFF) / 128.f;
				p.v[j].normal[1] = (INT8)(iy & 0xFF) / 128.f;
				p.v[j].normal[2] = (INT8)(iz & 0xFF) / 128.f;
			}

			if (ph.FixedShading() && ph.TexEnabled() && !ph.SmoothShading()) {		// fixed shading seems to be disabled if actual normals are set
				float offset = !ph.LightEnabled() ? 1.f : 0.f;						// if lighting is disabled colour seems to be an offset 
				float shade = (((ix + 128) & 0xFF) / 255.f) + offset;
				p.v[j].color[0] = shade;											// hardware doesn't really have per vertex colours, only per poly
				p.v[j].color[1] = shade;
				p.v[j].color[2] = shade;
				p.v[j].color[3] = 1;
			}
			else {
				p.v[j].color[0] = 1;
				p.v[j].color[1] = 1;
				p.v[j].color[2] = 1;
				p.v[j].color[3] = 1;
			}

			float texU, texV = 0;

			// tex coords
			if (currentMesh->textured) {
				Texture::GetCoordinates(currentMesh->width, currentMesh->height, (UINT16)(it >> 16), (UINT16)(it & 0xFFFF), uvScale, texU, texV);
			}

			p.v[j].texcoords[0] = texU;
			p.v[j].texcoords[1] = texV;

			//cache un-normalised tex coordinates
			prevTexCoords[j][0] = (UINT16)(it >> 16);
			prevTexCoords[j][1] = (UINT16)(it & 0xFFFF);

			vData += 4;
		}

		// check if we need to modify the texture coordinates
		{
			float offset[2] = { 0 };

			if (ph.TexEnabled()) {

				if (!ph.TexSmoothU() && !ph.TexUMirror()) {
					offset[0] = 0.5f / ph.TexWidth();	// half texel
				}

				if (!ph.TexSmoothV() && !ph.TexVMirror()) {
					offset[1] = 0.5f / ph.TexHeight();	// half texel
				}

				OffsetTexCoords(p, offset);
			}
		}

		// check if we need double up vertices for two sided lighting
		if (ph.DoubleSided()) {

			R3DPoly tempP = p;

			// flip normals
			V3::inverse(tempP.faceNormal);

			for (int i = 0; i < tempP.number; i++) {
				V3::inverse(tempP.v[i].normal);
			}

			CopyVertexData(tempP, currentMesh->polys);
		}

		// Copy this polygon into the model buffer
		CopyVertexData(p, currentMesh->polys);
		numPolys++;
		
		// Copy current vertices into previous vertex array
		for (i = 0; i < 4; i++) {
			prev[i] = p.v[i];
		}

	} while (ph.NextPoly());

	//sorted the data, now copy to main data structures

	// we know how many meshes we have so reserve appropriate space
	m->meshes->reserve(sMap.size());

	for (auto& it : sMap) {

		if (m->dynamic) {

			// calculate VBO values for current mesh
			it.second.vboOffset		= m_polyBufferRam.size() + MAX_ROM_POLYS;
			it.second.triangleCount = it.second.polys.size();

			// copy poly data to main buffer
			m_polyBufferRam.insert(m_polyBufferRam.end(), it.second.polys.begin(), it.second.polys.end());
		}
		else {
			// calculate VBO values for current mesh
			it.second.vboOffset		= m_polyBufferRom.size();
			it.second.triangleCount = it.second.polys.size();

			// copy poly data to main buffer
			m_polyBufferRom.insert(m_polyBufferRom.end(), it.second.polys.begin(), it.second.polys.end());
		}

		//copy the temp mesh into the model structure
		//this will lose the associated vertex data, which is now copied to the main buffer anyway
		m->meshes->push_back(it.second);
	}
}

float CNew3D::Determinant3x3(const float m[16]) {

	/*
		| A B C |
	M = | D E F |
		| G H I |

	then the determinant is calculated as follows:

	det M = A * (EI - HF) - B * (DI - GF) + C * (DH - GE)
	*/

	return m[0] * ((m[5] * m[10]) - (m[6] * m[9])) - m[4] * ((m[1] * m[10]) - (m[2] * m[9])) + m[8] * ((m[1] * m[6]) - (m[2] * m[5]));
}

bool CNew3D::IsDynamicModel(UINT32 *data)
{
	if (data == NULL) {
		return false;
	}

	PolyHeader p(data);

	do {

		if ((p.header[1] & 2) == 0) {		// model has rgb colour palette 
			return true;
		}

		if (p.header[6] == 0) {
			break;
		}

	} while (p.NextPoly());

	return false;
}

bool CNew3D::IsVROMModel(UINT32 modelAddr)
{
	return modelAddr >= 0x100000;
}

void CNew3D::CalcTexOffset(int offX, int offY, int page, int x, int y, int& newX, int& newY)
{
	newX = (x + offX) & 2047;	// wrap around 2048, shouldn't be required

	int oldPage = y / 1024;

	y -= (oldPage * 1024);	// remove page from tex y

	// calc newY with wrap around, wraps around in the same sheet, not into another memory sheet

	newY = (y + offY) & 1023;

	// add page to Y

	newY += ((oldPage + page) & 1) * 1024;		// max page 0-1
}

void CNew3D::CalcFrustumPlanes(Plane p[6], const float* matrix)
{
	// Left Plane
	p[0].a = matrix[3] + matrix[0];
	p[0].b = matrix[7] + matrix[4];
	p[0].c = matrix[11] + matrix[8];
	p[0].d = matrix[15] + matrix[12];
	p[0].Normalise();

	// Right Plane
	p[1].a = matrix[3] - matrix[0];
	p[1].b = matrix[7] - matrix[4];
	p[1].c = matrix[11] - matrix[8];
	p[1].d = matrix[15] - matrix[12];
	p[1].Normalise();

	// Bottom Plane
	p[2].a = matrix[3] + matrix[1];
	p[2].b = matrix[7] + matrix[5];
	p[2].c = matrix[11] + matrix[9];
	p[2].d = matrix[15] + matrix[13];
	p[2].Normalise();

	// Top Plane
	p[3].a = matrix[3] - matrix[1];
	p[3].b = matrix[7] - matrix[5];
	p[3].c = matrix[11] - matrix[9];
	p[3].d = matrix[15] - matrix[13];
	p[3].Normalise();

	// Near Plane
	p[4].a = matrix[3] + matrix[2];
	p[4].b = matrix[7] + matrix[6];
	p[4].c = matrix[11] + matrix[10];
	p[4].d = matrix[15] + matrix[14];
	p[4].Normalise();

	// Far Plane
	p[5].a = matrix[3] - matrix[2];
	p[5].b = matrix[7] - matrix[6];
	p[5].c = matrix[11] - matrix[10];
	p[5].d = matrix[15] - matrix[14];
	p[5].Normalise();
}

void CNew3D::CalcBox(float distance, BBox& box)
{
	//bottom left front
	box.points[0][0] = -distance;
	box.points[0][1] = -distance;
	box.points[0][2] = distance;
	box.points[0][3] = 1;

	//bottom left back
	box.points[1][0] = -distance;
	box.points[1][1] = -distance;
	box.points[1][2] = -distance;
	box.points[1][3] = 1;

	//bottom right back
	box.points[2][0] = distance;
	box.points[2][1] = -distance;
	box.points[2][2] = -distance;
	box.points[2][3] = 1;

	//bottom right front
	box.points[3][0] = distance;
	box.points[3][1] = -distance;
	box.points[3][2] = distance;
	box.points[3][3] = 1;

	//top left front
	box.points[4][0] = -distance;
	box.points[4][1] = distance;
	box.points[4][2] = distance;
	box.points[4][3] = 1;

	//top left back
	box.points[5][0] = -distance;
	box.points[5][1] = distance;
	box.points[5][2] = -distance;
	box.points[5][3] = 1;

	//top right back
	box.points[6][0] = distance;
	box.points[6][1] = distance;
	box.points[6][2] = -distance;
	box.points[6][3] = 1;

	//top right front
	box.points[7][0] = distance;
	box.points[7][1] = distance;
	box.points[7][2] = distance;
	box.points[7][3] = 1;
}

void CNew3D::MultVec(const float matrix[16], const float in[4], float out[4]) 
{
	for (int i = 0; i < 4; i++) {
		out[i] =
			in[0] * matrix[0 * 4 + i] +
			in[1] * matrix[1 * 4 + i] +
			in[2] * matrix[2 * 4 + i] +
			in[3] * matrix[3 * 4 + i];
	}
}

void CNew3D::TransformBox(const float *m, BBox& box)
{
	for (int i = 0; i < 8; i++) {
		float v[4];
		MultVec(m, box.points[i], v);
		box.points[i][0] = v[0];
		box.points[i][1] = v[1];
		box.points[i][2] = v[2];
	}
}

Clip CNew3D::ClipBox(BBox& box, Plane planes[6])
{
	int count = 0;

	for (int i = 0; i < 8; i++) {

		int temp = 0;

		for (int j = 0; j < 6; j++) {
			if (planes[j].DistanceToPoint(box.points[i]) >= 0) {
				temp++;
			}
		}

		if (temp == 6) count++;		// point is inside all 6 frustum planes
	}

	if (count == 8)	return Clip::INSIDE;
	if (count > 0)	return Clip::INTERCEPT;
	
	//if we got here all points are outside of the view frustum
	//check for all points being side same of any plane, means box outside of view

	for (int i = 0; i < 6; i++) {

		int temp = 0;

		for (int j = 0; j < 8; j++) {
			if (planes[i].DistanceToPoint(box.points[j]) >= 0) {
				float distance = planes[i].DistanceToPoint(box.points[j]);
				temp++;
			}
		}

		if (temp == 0) return Clip::OUTSIDE;
	}

	//if we got here, box is traversing view frustum

	return Clip::INTERCEPT;
}

} // New3D

