#include "New3D.h"
#include "PolyHeader.h"
#include "Texture.h"
#include "Vec.h"
#include <cmath>  // needed by gcc

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
	m_texSheet.Invalidate(x, y, width, height);
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
					auto tex = m_texSheet.BindTexture(m_textureRAM, mesh.format, mesh.mirrorU, mesh.mirrorV, mesh.x + m.textureOffsetX, mesh.y + m.textureOffsetY, mesh.width, mesh.height);
					if (tex) {
						tex->BindTexture();
						tex->SetWrapMode(mesh.mirrorU, mesh.mirrorV);
					}
				}
				
				m_r3dShader.SetMeshUniforms(&mesh);
				glDrawArrays(GL_TRIANGLES, mesh.vboOffset*3, mesh.triangleCount*3);			// times 3 to convert triangles to vertices
			}
		}
	}

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

void CNew3D::RenderFrame(void)
{
	// release any resources from last frame
	m_polyBufferRam.clear();	// clear dyanmic model memory buffer
	m_nodes.clear();		// memory will grow during the object life time, that's fine, no need to shrink to fit
	m_modelMat.Release();	// would hope we wouldn't need this but no harm in checking
	m_nodeAttribs.Reset();

	glDepthFunc		(GL_LEQUAL);
	glEnable		(GL_DEPTH_TEST);
	glActiveTexture	(GL_TEXTURE0);
	glEnable		(GL_CULL_FACE);
	glFrontFace		(GL_CW);

	for (int pri = 0; pri <= 3; pri++) {
		RenderViewport(0x800000, pri);		// build model structure
	}

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
	glColorPointer		(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, color));
	
	m_r3dShader.SetShader(true);

	for (int pri = 0; pri <= 3; pri++) {
		glClear		(GL_DEPTH_BUFFER_BIT);
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


		if (m_romMap.count(modelAddr)) {
			m->meshes = m_romMap[modelAddr];
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

	if (!cached) {
		CacheModel(m, modelAddress);
	}

	return true;
}

// Descends into a 10-word culling node
void CNew3D::DescendCullingNode(UINT32 addr)
{
	const UINT32	*node, *lodTable;
	UINT32			matrixOffset, node1Ptr, node2Ptr;
	float			x, y, z;
	int				tx, ty;

	if (m_nodeAttribs.StackLimit()) {
		return;
	}

	node = TranslateCullingAddress(addr);

	if (NULL == node) {
		return;
	}

	// Extract known fields
	node1Ptr		= node[0x07 - m_offset];
	node2Ptr		= node[0x08 - m_offset];
	matrixOffset	= node[0x03 - m_offset] & 0xFFF;

	x = *(float *)&node[0x04 - m_offset];	// magic numbers everywhere !
	y = *(float *)&node[0x05 - m_offset];
	z = *(float *)&node[0x06 - m_offset];

	m_nodeAttribs.Push();	// save current attribs

	if (!m_offset)	// Step 1.5+
	{
		tx = 32 * ((node[0x02] >> 7) & 0x3F);
		ty = 32 * (node[0x02] & 0x3F) + ((node[0x02] & 0x4000) ? 1024 : 0);	// TODO: 5 or 6 bits for Y coord?

		// apply texture offsets, else retain current ones
		if ((node[0x02] & 0x8000))	{
			m_nodeAttribs.currentTexOffsetX = tx;
			m_nodeAttribs.currentTexOffsetY = ty;
			m_nodeAttribs.currentTexOffset	= node[0x02] & 0x7FFF;
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

	// Descend down first link
	if ((node[0x00] & 0x08))	// 4-element LOD table
	{
		lodTable = TranslateCullingAddress(node1Ptr);

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
		DescendNodePtr(node1Ptr);
	}

	// Proceed to second link
	m_modelMat.PopMatrix();

	// seems to indicate second link is invalid (fixes circular references)
	if ((node[0x00] & 0x07) != 0x06) {
		DescendNodePtr(node2Ptr);
	}

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
	case 0x01:	// model (perhaps bit 1 is a flag in this case?)
	case 0x03:
		DrawModel(nodeAddr & 0xFFFFFF);
		break;
	case 0x04:	// pointer list
		DescendPointerList(nodeAddr & 0xFFFFFF);
		break;
	default:
		//printf("ATTENTION: Unknown pointer format: %08X\n\n", nodeAddr);
		break;
	}
}

void CNew3D::DescendPointerList(UINT32 addr)
{
	const UINT32*	list;
	UINT32			nodeAddr;
	int				listEnd;

	if (m_listDepth > 2) {	// several Step 2.1 games require this safeguard
		return;
	}

	list = TranslateCullingAddress(addr);

	if (NULL == list) {
		return;
	}

	m_listDepth++;

	// Traverse the list forward and print it out
	listEnd = 0;

	while (1)
	{
		if ((list[listEnd] & 0x02000000)) {	// end of list (?)
			break;
		}

		if ((list[listEnd] == 0) || (((list[listEnd]) >> 24) != 0)) {
			listEnd--;	// back up to last valid list element
			break;
		}

		listEnd++;
	}

	for (int i = 0; i <= listEnd; i++) {

		nodeAddr = list[i] & 0x00FFFFFF;	// clear upper 8 bits to ensure this is processed as a culling node

		if (!(list[i] & 0x01000000)) {	//Fighting Vipers

			if ((nodeAddr != 0) && (nodeAddr != 0x800800)) {
				DescendCullingNode(nodeAddr);
			}
		}
	}


	/*
	// Traverse the list backward and descend into each pointer
	while (listEnd >= 0)
	{
		nodeAddr = list[listEnd] & 0x00FFFFFF;	// clear upper 8 bits to ensure this is processed as a culling node

		if (!(list[listEnd] & 0x01000000)) {	//Fighting Vipers
			
			if ((nodeAddr != 0) && (nodeAddr != 0x800800)) {
				DescendCullingNode(nodeAddr);
			}
		}

		listEnd--;
	}
	*/

	m_listDepth--;
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

	if (m_step > 0x10) {
		mat.LoadMatrix(m);
	}
	else {
		// Scaling seems to help w/ Step 1.0's extremely large coordinates
		GLfloat s = 1.0f / 2048.0f;	// this will fuck up normals
		mat.LoadIdentity();
		mat.Scale(s, s, s);
		mat.MultMatrix(m);
	}

	// Set matrix base address and apply matrix #0 (coordinate system matrix)
	m_matrixBasePtr = (float *)TranslateCullingAddress(matrixBaseAddr);
	MultMatrix(0, mat);
}

// Draws viewports of the given priority
void CNew3D::RenderViewport(UINT32 addr, int pri)
{
	GLfloat	color[8][3] =	// RGB1 translation
	{
		{ 0.0, 0.0, 0.0 },	// off
		{ 0.0, 0.0, 1.0 },	// blue
		{ 0.0, 1.0, 0.0 },	// green
		{ 0.0, 1.0, 1.0 },	// cyan
		{ 1.0, 0.0, 0.0 }, 	// red
		{ 1.0, 0.0, 1.0 },	// purple
		{ 1.0, 1.0, 0.0 },	// yellow
		{ 1.0, 1.0, 1.0 }	// white
	};
	const UINT32	*vpnode;
	UINT32			nextAddr, nodeAddr, matrixBase;
	int				curPri;
	int				vpX, vpY, vpWidth, vpHeight;
	int				spotColorIdx;
	GLfloat			scrollFog, scrollAtt;
	Viewport*		vp;

	// Translate address and obtain pointer
	vpnode = TranslateCullingAddress(addr);

	if (NULL == vpnode) {
		return;
	}

	if (vpnode[0] & 0x20) {
		return;		//viewport disabled
	}

	curPri		= (vpnode[0x00] >> 3) & 3;	// viewport priority
	nextAddr	= vpnode[0x01] & 0xFFFFFF;	// next viewport
	nodeAddr	= vpnode[0x02];				// scene database node pointer

	// Recursively process next viewport
	if (vpnode[0x01] == 0) {				// memory probably hasn't been set up yet, abort
		return;
	}
	if (vpnode[0x01] != 0x01000000) {
		RenderViewport(vpnode[0x01], pri);
	}

	// If the priority doesn't match, do not process
	if (curPri != pri) {
		return;
	}

	// create node object 
	m_nodes.emplace_back(Node());
	m_nodes.back().models.reserve(2048);			// create space for models

	// get pointer to its viewport
	vp = &m_nodes.back().viewport;

	vp->priority = pri;

	// Fetch viewport parameters (TO-DO: would rounding make a difference?)
	vpX			= (int)(((vpnode[0x1A] & 0xFFFF) / 16.0f) + 0.5f);		// viewport X (12.4 fixed point)
	vpY			= (int)(((vpnode[0x1A] >> 16) / 16.0f) + 0.5f);			// viewport Y (12.4)
	vpWidth		= (int)(((vpnode[0x14] & 0xFFFF) / 4.0f) + 0.5f);		// width (14.2)
	vpHeight	= (int)(((vpnode[0x14] >> 16) / 4.0f) + 0.5f);			// height (14.2)
	matrixBase	= vpnode[0x16] & 0xFFFFFF;								// matrix base address

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

	float near	= 0.1f;
	float far	= 1e5;

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

	// Lighting (note that sun vector points toward sun -- away from vertex)
	vp->lightingParams[0] = *(float *)&vpnode[0x05];								// sun X
	vp->lightingParams[1] = *(float *)&vpnode[0x06];								// sun Y
	vp->lightingParams[2] = *(float *)&vpnode[0x04];								// sun Z
	vp->lightingParams[3] = *(float *)&vpnode[0x07];								// sun intensity
	vp->lightingParams[4] = (float)((vpnode[0x24] >> 8) & 0xFF) * (1.0f / 255.0f);	// ambient intensity
	vp->lightingParams[5] = 0.0;	// reserved

	// Spotlight
	spotColorIdx = (vpnode[0x20] >> 11) & 7;					// spotlight color index
	vp->spotEllipse[0] = (float)((vpnode[0x1E] >> 3) & 0x1FFF);	// spotlight X position (fractional component?)
	vp->spotEllipse[1] = (float)((vpnode[0x1D] >> 3) & 0x1FFF);	// spotlight Y
	vp->spotEllipse[2] = (float)((vpnode[0x1E] >> 16) & 0xFFFF);	// spotlight X size (16-bit? May have fractional component below bit 16)
	vp->spotEllipse[3] = (float)((vpnode[0x1D] >> 16) & 0xFFFF);	// spotlight Y size

	vp->spotRange[0] = 1.0f / (*(float *)&vpnode[0x21]);		// spotlight start
	vp->spotRange[1] = *(float *)&vpnode[0x1F];				// spotlight extent
	vp->spotColor[0] = color[spotColorIdx][0];				// spotlight color
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
	vp->fogParams[3] = *(float *)&vpnode[0x23];									// fog density
	vp->fogParams[4] = (float)(INT16)(vpnode[0x25] & 0xFFFF)*(1.0f / 255.0f);	// fog start

	
	if (std::isinf(vp->fogParams[3]) || std::isnan(vp->fogParams[3]) || std::isinf(vp->fogParams[4]) || std::isnan(vp->fogParams[4])) {	// Star Wars Trilogy
		vp->fogParams[3] = vp->fogParams[4] = 0.0f;
	}

	// Unknown light/fog parameters
	scrollFog = (float)(vpnode[0x20] & 0xFF) * (1.0f / 255.0f);	// scroll fog
	scrollAtt = (float)(vpnode[0x24] & 0xFF) * (1.0f / 255.0f);	// scroll attenuation

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

	m_listDepth = 0;

	// Descend down the node link: Use recursive traversal
	DescendNodePtr(nodeAddr);
}

void CNew3D::CopyVertexData(R3DPoly& r3dPoly, std::vector<Poly>& polyArray)
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

		polyArray.emplace_back(p);
	}
}

void CNew3D::CacheModel(Model *m, const UINT32 *data)
{
	Vertex			prev[4];
	PolyHeader		ph;
	int				numPolys	= 0;
	bool			done		= false;
	UINT64			lastHash	= -1;
	SortingMesh*	currentMesh = nullptr;
	
	std::map<UINT64, SortingMesh> sMap;

	if (data == NULL)
		return;

	ph = data; 
	int numTriangles = ph.NumTrianglesTotal();

	// Cache all polygons
	while (!done)
	{
		R3DPoly		p;					// current polygon
		GLfloat		uvScale;
		int			i, j;
		bool		validPoly = true;

		ph = data;

		if (ph.header[6] == 0) {
			break;
		}

		if ((ph.header[0] & 0x100) && (ph.header[0] & 0x200)) {	// assuming these two bits mean z and colour writes are disabled
			validPoly = false;
		}
		else {
			if (!numPolys && (ph.NumSharedVerts() != 0)) {			// sharing vertices, but we haven't started the model yet
				printf("incomplete data\n");
				validPoly = false;
			}
		}

		// Set current header pointer (header is 7 words)
		data += 7;	// data will now point to first vertex

		// create a hash value based on poly attributes -todo add more attributes
		auto hash = ph.Hash();

		if (hash != lastHash && validPoly) {

			if (sMap.count(hash) == 0) {

				sMap[hash] = SortingMesh();

				currentMesh = &sMap[hash];

				//make space for our vertices
				currentMesh->polys.reserve(numTriangles);

				//copy attributes
				currentMesh->doubleSided	= ph.DoubleSided();
				currentMesh->textured		= ph.TexEnabled();
				currentMesh->alphaTest		= ph.AlphaTest();
				currentMesh->textureAlpha	= ph.TextureAlpha();
				currentMesh->polyAlpha		= ph.PolyAlpha();
				currentMesh->lighting		= ph.LightEnabled();

				if (!ph.Luminous()) {
					currentMesh->fogIntensity = 1.0f;
				}
				else {
					currentMesh->fogIntensity = ph.LightModifier();
				}

				if (ph.TexEnabled()) {
					currentMesh->format		= ph.TexFormat();
					currentMesh->x			= ph.X();
					currentMesh->y			= ph.Y();
					currentMesh->width		= ph.TexWidth();
					currentMesh->height		= ph.TexHeight();
					currentMesh->mirrorU	= ph.TexUMirror();
					currentMesh->mirrorV	= ph.TexVMirror();
				}
			}

			currentMesh = &sMap[hash];
		}

		if (validPoly) {
			lastHash = hash;
		}

		// Obtain basic polygon parameters
		done		= ph.LastPoly();
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
				++j;
			}
		}

		for (; j < p.number; j++)	// remaining vertices are new and defined here
		{
			// Fetch vertices
			UINT32 ix = data[0];
			UINT32 iy = data[1];
			UINT32 iz = data[2];
			UINT32 it = data[3];

			// Decode vertices
			p.v[j].pos[0] = (GLfloat)(((INT32)ix) >> 8) * m_vertexFactor;
			p.v[j].pos[1] = (GLfloat)(((INT32)iy) >> 8) * m_vertexFactor;
			p.v[j].pos[2] = (GLfloat)(((INT32)iz) >> 8) * m_vertexFactor;

			p.v[j].normal[0] = p.faceNormal[0] + (GLfloat)(INT8)(ix & 0xFF);	// vertex normals are offset from polygon normal - we can normalise them in the shader
			p.v[j].normal[1] = p.faceNormal[1] + (GLfloat)(INT8)(iy & 0xFF);
			p.v[j].normal[2] = p.faceNormal[2] + (GLfloat)(INT8)(iz & 0xFF);

			if ((ph.header[1] & 2) == 0) {
				UINT32 colorIdx = ((ph.header[4] >> 8) & 0x7FF);
				p.v[j].color[2] = (m_polyRAM[0x400 + colorIdx] & 0xFF);
				p.v[j].color[1] = (m_polyRAM[0x400 + colorIdx] >> 8) & 0xFF;
				p.v[j].color[0] = (m_polyRAM[0x400 + colorIdx] >> 16) & 0xFF;
			}
			else if (ph.FixedShading()) {
				UINT8 shade = ph.ShadeValue();
				p.v[j].color[0] = shade;
				p.v[j].color[1] = shade;
				p.v[j].color[2] = shade;
			}
			else {
				p.v[j].color[0] = (ph.header[4] >> 24);
				p.v[j].color[1] = (ph.header[4] >> 16) & 0xFF;
				p.v[j].color[2] = (ph.header[4] >> 8) & 0xFF;
			}

			if ((ph.header[6] & 0x00800000)) {	// if set, polygon is opaque
				p.v[j].color[3] = 255;
			}
			else {
				p.v[j].color[3] = ph.Transparency();
			}

			float texU, texV = 0;

			// tex coords
			if (validPoly && currentMesh->textured) {
				Texture::GetCoordinates(currentMesh->width, currentMesh->height, (UINT16)(it >> 16), (UINT16)(it & 0xFFFF), uvScale, texU, texV);
			}

			p.v[j].texcoords[0] = texU;
			p.v[j].texcoords[1] = texV;

			data += 4;
		}

		// Copy current vertices into previous vertex array
		for (i = 0; i < 4 && validPoly; i++) {
			prev[i] = p.v[i];
		}

		// Copy this polygon into the model buffer
		if (validPoly) {
			CopyVertexData(p, currentMesh->polys);
			numPolys++;
		}
	}

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

} // New3D
