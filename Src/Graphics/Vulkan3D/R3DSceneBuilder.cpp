/**
 ** Supermodel-VK
 ** A Vulkan-focused fork of Supermodel, the Sega Model 3 arcade emulator.
 ** Copyright 2026 The Supermodel-VK authors
 **
 ** This file is part of Supermodel-VK, and is derived from Supermodel,
 ** Copyright 2003-2026 The Supermodel Team.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
#include "R3DSceneBuilder.h"
#include "VulkanCommon.h"
#include "../New3D/PolyHeader.h"
#include "../New3D/R3DFloat.h"
#include "../New3D/Vec.h"
#include "Util/BitCast.h"
#include "OSD/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <tuple>

#define BYTE_TO_FLOAT(B)	((2.0f * (B) + 1.0f) * (float)(1.0 / 255.0))
#define NEAR_PLANE 1e-3f
#define CMINDEX(y, x)	((x) * 4 + (y))

// The flush threshold now comes from VulkanCommon.h so that it cannot disagree with the size of the
// GPU vertex buffer it feeds. It used to be a local 1500000 while the buffer held only 1048576, and
// the resulting overflow silently discarded geometry part way through a level.

namespace Vulkan3D {

void CR3DSceneBuilder::AttachMemory(const uint32_t* cullingLo, const uint32_t* cullingHi, const uint32_t* polyRam, const uint32_t* vrom)
{
	m_cullingRAMLo = cullingLo;
	m_cullingRAMHi = cullingHi;
	m_polyRAM = polyRam;
	m_vrom = vrom;
}

void CR3DSceneBuilder::SetStepping(int stepping)
{
	m_step = stepping;
	if (m_step != 0x10 && m_step != 0x15 && m_step != 0x20 && m_step != 0x21)
		m_step = 0x10;
	if (m_step > 0x10)
	{
		m_offset = 0;
		m_vertexFactor = 1.0f / 2048.0f;
		m_textureNPFactor = 1.0f / 16384.0f;
	}
	else
	{
		m_offset = 2;
		m_vertexFactor = 1.0f / 128.0f;
		m_textureNPFactor = 1.0f / 4096.0f;
	}
}

void CR3DSceneBuilder::SetViewport(unsigned xOff, unsigned yOff, unsigned xRes, unsigned yRes, unsigned totalX, unsigned totalY, bool wideScreen)
{
	m_xOffs = xOff;
	m_yOffs = yOff;
	m_xRes = xRes;
	m_yRes = yRes;
	m_totalXRes = totalX;
	m_totalYRes = totalY;
	m_wideScreen = wideScreen;
	m_xRatio = xRes * (1.0f / 496.0f);
	m_yRatio = yRes * (1.0f / 384.0f);
}

void CR3DSceneBuilder::BeginFrame()
{
	m_polyBufferRam.clear();
	m_nodes.clear();
	m_modelMat.Release();
	m_nodeAttribs.Reset();
}

void CR3DSceneBuilder::Build()
{
	RenderViewport(0x800000);
}

const uint32_t* CR3DSceneBuilder::TranslateCullingAddress(uint32_t addr) const
{
	addr &= 0x00FFFFFF;
	if (addr >= 0x800000 && addr < 0x840000)
		return &m_cullingRAMHi[addr & 0x3FFFF];
	if (addr < 0x100000)
		return &m_cullingRAMLo[addr];
	return nullptr;
}

const uint32_t* CR3DSceneBuilder::TranslateModelAddress(uint32_t modelAddr) const
{
	modelAddr &= 0x00FFFFFF;
	if (modelAddr < 0x100000)
		return &m_polyRAM[modelAddr];
	return &m_vrom[modelAddr];
}

bool CR3DSceneBuilder::IsVROMModel(uint32_t modelAddr) const
{
	return modelAddr >= 0x100000;
}

bool CR3DSceneBuilder::IsDynamicModel(uint32_t* data) const
{
	if (!data)
		return false;
	New3D::PolyHeader p(data);
	do
	{
		if ((p.header[1] & 2) == 0)
			return true;
	} while (p.NextPoly());
	return false;
}

bool CR3DSceneBuilder::DrawModel(uint32_t modelAddr)
{
	const uint32_t* modelAddress = TranslateModelAddress(modelAddr);
	m_nodes.back().models.emplace_back();
	New3D::Model* m = &m_nodes.back().models.back();
	bool cached = false;

	if (IsVROMModel(modelAddr) && !IsDynamicModel(const_cast<uint32_t*>(modelAddress)))
	{
		m->meshes = m_romMap[modelAddr];
		if (m->meshes)
			cached = true;
		else
		{
			m->meshes = std::make_shared<std::vector<New3D::Mesh>>();
			m_romMap[modelAddr] = m->meshes;
		}
		m->dynamic = false;
	}
	else
		m->meshes = std::make_shared<std::vector<New3D::Mesh>>();

	for (int i = 0; i < 16; i++)
		m->modelMat[i] = m_modelMat.currentMatrix[i];
	m->textureOffsetX = m_nodeAttribs.currentTexOffsetX;
	m->textureOffsetY = m_nodeAttribs.currentTexOffsetY;
	m->page = m_nodeAttribs.currentPage;
	m->scale = m_nodeAttribs.currentModelScale;
	m->alpha = m_nodeAttribs.currentModelAlpha;
	if (!cached)
		CacheModel(m, modelAddress);
	return true;
}

void CR3DSceneBuilder::DescendCullingNode(uint32_t addr)
{
	enum class NodeType { undefined = -1, viewport = 0, rootNode = 1, cullingNode = 2 };
	if (m_nodeAttribs.StackLimit())
		return;
	const uint32_t* node = TranslateCullingAddress(addr);
	if (!node)
		return;

	NodeType nodeType = (NodeType)(node[0x00] & 3);
	uint32_t child1Ptr = node[0x07 - m_offset] & 0x7FFFFFF;
	uint32_t sibling2Ptr = node[0x08 - m_offset] & 0x1FFFFFF;
	uint32_t matrixOffset = node[0x03 - m_offset] & 0xFFF;
	bool resetMatrix = (node[0x00] & 0x80) != 0;
	uint8_t lodTablePointer = (node[0x03 - m_offset] >> 12) & 0x7F;

	if (nodeType == NodeType::viewport)
		return;
	if ((0x300 & node[0]) == 0x300)
		return;

	if ((node[0x00] & 0x07) != 0x06)
	{
		if (!(sibling2Ptr & 0x1000000) && sibling2Ptr)
			DescendCullingNode(sibling2Ptr);
	}
	if (node[0x00] & 0x04)
	{
		m_colorTableAddr = ((node[0x03 - m_offset] >> 19) << 0) | ((node[0x07 - m_offset] >> 28) << 13) | ((node[0x08 - m_offset] >> 25) << 17);
		m_colorTableAddr &= 0x000FFFFF;
	}

	if (!m_nodeAttribs.Push())
		return;
	if (!m_offset)
	{
		if (node[0x01] & 1)
			m_nodeAttribs.currentModelScale = Util::Uint32AsFloat(node[0x01] & ~3u);
		if (node[0x01] & 2)
			m_nodeAttribs.currentDisableCulling = true;
		if (node[0x02] & 0x8000)
		{
			m_nodeAttribs.currentTexOffsetX = 32 * (int)((node[0x02] >> 7) & 0x3F);
			m_nodeAttribs.currentTexOffsetY = 32 * (int)(node[0x02] & 0x1F);
			m_nodeAttribs.currentPage = (node[0x02] & 0x4000) >> 14;
		}
	}

	m_modelMat.PushMatrix();
	if (node[0x00] & 0x10)
	{
		m_modelMat.Translate(Util::Uint32AsFloat(node[0x04 - m_offset]), Util::Uint32AsFloat(node[0x05 - m_offset]), Util::Uint32AsFloat(node[0x06 - m_offset]));
	}
	else if (matrixOffset)
		MultMatrix(matrixOffset, m_modelMat);
	if (resetMatrix)
		ResetMatrix(m_modelMat);

	float x = m_modelMat.currentMatrix[12];
	float y = m_modelMat.currentMatrix[13];
	float z = m_modelMat.currentMatrix[14];
	float cullRadius = R3DFloat::GetFloat16((uint16_t)(node[9 - m_offset] & 0xFFFF)) * m_nodeAttribs.currentModelScale;
	float blendRadius = R3DFloat::GetFloat16((uint16_t)(node[9 - m_offset] >> 16)) * m_nodeAttribs.currentModelScale;

	bool outside = (z * m_planes.bnlu - x * m_planes.bnlv * m_planes.correction) > cullRadius ||
		(z * m_planes.bntu + y * m_planes.bntw) > cullRadius ||
		(z * m_planes.bnru - x * m_planes.bnrv * m_planes.correction) > cullRadius ||
		(z * m_planes.bnbu + y * m_planes.bnbw) > cullRadius;

	float lodScale = m_nodeAttribs.currentDisableCulling ? std::numeric_limits<float>::max() : (blendRadius / std::hypot(x, y, z));
	lodScale = std::clamp(lodScale, 0.0f, std::numeric_limits<float>::max());

	if (m_LODBlendTable && (m_nodeAttribs.currentDisableCulling || (!outside && lodScale >= m_LODBlendTable->table[lodTablePointer].lod[3].deleteSize)))
	{
		const LOD* lod = m_LODBlendTable->table[lodTablePointer].lod;
		if (node[0x00] & 0x08)
		{
			const uint32_t* lodPtr = TranslateCullingAddress(child1Ptr);
			if (lodPtr)
			{
				int modelLOD;
				for (modelLOD = 0; modelLOD < 3; modelLOD++)
				{
					if (lodScale >= lod[modelLOD].deleteSize && (lodPtr[modelLOD] & 0x1000000))
						break;
				}
				float tempAlpha = m_nodeAttribs.currentModelAlpha;
				float nodeAlpha = std::clamp(lod[modelLOD].blendFactor * (lodScale - lod[modelLOD].deleteSize), 0.0f, 1.0f);
				if (nodeAlpha > (31.0f / 32.0f))
					nodeAlpha = 1.0f;
				else if (nodeAlpha < (1.0f / 32.0f))
					nodeAlpha = 0.0f;
				m_nodeAttribs.currentModelAlpha *= nodeAlpha;
				if (node[0x03 - m_offset] & 0x20000000)
				{
					DescendCullingNode(lodPtr[modelLOD] & 0xFFFFFF);
					if (nodeAlpha < 1.0f && modelLOD != 3)
					{
						m_nodeAttribs.currentModelAlpha = (1.0f - nodeAlpha) * tempAlpha;
						DescendCullingNode(lodPtr[modelLOD + 1] & 0xFFFFFF);
					}
				}
				else
				{
					DrawModel(lodPtr[modelLOD] & 0xFFFFFF);
					if (nodeAlpha < 1.0f && modelLOD != 3)
					{
						m_nodeAttribs.currentModelAlpha = (1.0f - nodeAlpha) * tempAlpha;
						DrawModel(lodPtr[modelLOD + 1] & 0xFFFFFF);
					}
				}
			}
		}
		else
		{
			float nodeAlpha = std::clamp(lod[3].blendFactor * (lodScale - lod[3].deleteSize), 0.0f, 1.0f);
			m_nodeAttribs.currentModelAlpha *= nodeAlpha;
			DescendNodePtr(child1Ptr);
		}
	}

	m_modelMat.PopMatrix();
	m_nodeAttribs.Pop();
}

void CR3DSceneBuilder::DescendNodePtr(uint32_t nodeAddr)
{
	if ((nodeAddr & 0x00FFFFFF) == 0)
		return;
	switch ((nodeAddr >> 24) & 0x5)
	{
	case 0x00: DescendCullingNode(nodeAddr & 0xFFFFFF); break;
	case 0x01: DrawModel(nodeAddr & 0xFFFFFF); break;
	case 0x04: DescendPointerList(nodeAddr & 0xFFFFFF); break;
	default: break;
	}
}

void CR3DSceneBuilder::DescendPointerList(uint32_t addr)
{
	const uint32_t* list = TranslateCullingAddress(addr);
	if (!list)
		return;
	for (int index = 0;; index++)
	{
		if (list[index] & 0x01000000)
			break;
		DescendCullingNode(list[index] & 0x00FFFFFF);
		if (list[index] & 0x02000000)
			break;
	}
}

void CR3DSceneBuilder::MultMatrix(uint32_t matrixOffset, New3D::Mat4& mat)
{
	if (!m_matrixBasePtr)
		return;
	const float* src = &m_matrixBasePtr[matrixOffset * 12];
	float m[16];
	m[CMINDEX(0, 0)] = src[3]; m[CMINDEX(0, 1)] = src[4]; m[CMINDEX(0, 2)] = src[5]; m[CMINDEX(0, 3)] = src[0];
	m[CMINDEX(1, 0)] = src[6]; m[CMINDEX(1, 1)] = src[7]; m[CMINDEX(1, 2)] = src[8]; m[CMINDEX(1, 3)] = src[1];
	m[CMINDEX(2, 0)] = src[9]; m[CMINDEX(2, 1)] = src[10]; m[CMINDEX(2, 2)] = src[11]; m[CMINDEX(2, 3)] = src[2];
	m[CMINDEX(3, 0)] = 0; m[CMINDEX(3, 1)] = 0; m[CMINDEX(3, 2)] = 0; m[CMINDEX(3, 3)] = 1;
	mat.MultMatrix(m);
}

void CR3DSceneBuilder::InitMatrixStack(uint32_t matrixBaseAddr, New3D::Mat4& mat)
{
	float m[16] = {
		0, 0, -1, 0,
		1, 0, 0, 0,
		0, -1, 0, 0,
		0, 0, 0, 1
	};
	// The array above is row-major in the source listing of New3D, which stores
	// column-major. Rebuild it the same way New3D does.
	m[CMINDEX(0, 0)] = 0; m[CMINDEX(0, 1)] = 1; m[CMINDEX(0, 2)] = 0; m[CMINDEX(0, 3)] = 0;
	m[CMINDEX(1, 0)] = 0; m[CMINDEX(1, 1)] = 0; m[CMINDEX(1, 2)] = -1; m[CMINDEX(1, 3)] = 0;
	m[CMINDEX(2, 0)] = -1; m[CMINDEX(2, 1)] = 0; m[CMINDEX(2, 2)] = 0; m[CMINDEX(2, 3)] = 0;
	m[CMINDEX(3, 0)] = 0; m[CMINDEX(3, 1)] = 0; m[CMINDEX(3, 2)] = 0; m[CMINDEX(3, 3)] = 1;
	mat.LoadMatrix(m);
	m_matrixBasePtr = (const float*)TranslateCullingAddress(matrixBaseAddr);
	MultMatrix(0, mat);
}

void CR3DSceneBuilder::ResetMatrix(New3D::Mat4& mat) const
{
	float m[16];
	std::memcpy(m, mat.currentMatrix, sizeof(m));
	std::swap(m[1], m[4]);
	std::swap(m[2], m[8]);
	std::swap(m[6], m[9]);
	m[12] = m[13] = m[14] = 0;
	m[15] = 1;
	float s1 = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
	float s2 = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
	float s3 = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
	if (s1) { m[0] /= s1; m[1] /= s1; m[2] /= s1; }
	if (s2) { m[4] /= s2; m[5] /= s2; m[6] /= s2; }
	if (s3) { m[8] /= s3; m[9] /= s3; m[10] /= s3; }
	mat.MultMatrix(m);
}

void CR3DSceneBuilder::RenderViewport(uint32_t addr)
{
	static const float color[8][3] = {
		{ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 1 },
		{ 1, 0, 0 }, { 1, 0, 1 }, { 1, 1, 0 }, { 1, 1, 1 }
	};
	if ((addr & 0x00FFFFFF) == 0)
		return;
	const uint32_t* vpnode = TranslateCullingAddress(addr);
	if (!vpnode)
		return;

	bool vpDisabled = (vpnode[0] & 0x20) != 0;
	m_nodes.emplace_back();
	m_nodes.back().models.reserve(2048);
	New3D::Viewport* vp = &m_nodes.back().viewport;
	vp->priority = (vpnode[0] >> 3) & 0x3;
	vp->select = (vpnode[0] >> 8) & 0x3;
	vp->number = (vpnode[0] >> 10);
	vp->vpX = (vpnode[0x1A] & 0xFFFF) * (1.0f / 16.0f);
	vp->vpY = (vpnode[0x1A] >> 16) * (1.0f / 16.0f);
	vp->vpWidth = (vpnode[0x14] & 0xFFFF) * (1.0f / 4.0f);
	vp->vpHeight = (vpnode[0x14] >> 16) * (1.0f / 4.0f);
	uint32_t matrixBase = vpnode[0x16] & 0xFFFFFF;
	m_LODBlendTable = (LODBlendTable*)TranslateCullingAddress(vpnode[0x17] & 0xFFFFFF);

	float cv = Util::Uint32AsFloat(vpnode[0x8]);
	float cw = Util::Uint32AsFloat(vpnode[0x9]);
	float io = Util::Uint32AsFloat(vpnode[0xa]);
	float jo = Util::Uint32AsFloat(vpnode[0xb]);
	m_planes.bnlu = Util::Uint32AsFloat(vpnode[0xc]);
	m_planes.bnlv = Util::Uint32AsFloat(vpnode[0xd]);
	m_planes.bntu = Util::Uint32AsFloat(vpnode[0xe]);
	m_planes.bntw = Util::Uint32AsFloat(vpnode[0xf]);
	m_planes.bnru = Util::Uint32AsFloat(vpnode[0x10]);
	m_planes.bnrv = Util::Uint32AsFloat(vpnode[0x11]);
	m_planes.bnbu = Util::Uint32AsFloat(vpnode[0x12]);
	m_planes.bnbw = Util::Uint32AsFloat(vpnode[0x13]);
	m_planes.correction = 1.0f;

	vp->angle_left = (0.0f - jo) / cv;
	vp->angle_right = (1.0f - jo) / cv;
	vp->angle_bottom = -(1.0f - io) / cw;
	vp->angle_top = -(0.0f - io) / cw;
	vp->cota = Util::Uint32AsFloat(vpnode[0x3]);
	CalcViewport(vp);

	vp->lightingParams[0] = Util::Uint32AsFloat(vpnode[0x05]);
	vp->lightingParams[1] = -Util::Uint32AsFloat(vpnode[0x06]);
	vp->lightingParams[2] = -Util::Uint32AsFloat(vpnode[0x04]);
	vp->lightingParams[3] = std::max(0.f, std::min(Util::Uint32AsFloat(vpnode[0x07]), 1.0f));
	vp->lightingParams[4] = ((vpnode[0x24] >> 8) & 0xFF) * (1.0f / 255.0f);
	vp->lightingParams[5] = 0;
	vp->sunClamp = true;
	vp->intensityClamp = (m_step == 0x10);
	vp->step15Luminous = (m_step == 0x15);

	int spotColorIdx = (vpnode[0x20] >> 11) & 7;
	int spotFogColorIdx = (vpnode[0x20] >> 8) & 7;
	vp->spotEllipse[0] = (float)(int16_t)(vpnode[0x1E] & 0xFFFF) * (1.0f / 8.0f);
	vp->spotEllipse[1] = (float)(int16_t)(vpnode[0x1D] & 0xFFFF) * (1.0f / 8.0f);
	vp->spotEllipse[2] = (float)((vpnode[0x1E] >> 16) & 0xFFFF);
	vp->spotEllipse[3] = (float)((vpnode[0x1D] >> 16) & 0xFFFF);
	vp->spotRange[0] = 1.0f / Util::Uint32AsFloat(vpnode[0x21]);
	vp->spotRange[1] = Util::Uint32AsFloat(vpnode[0x1F]);
	vp->spotColor[0] = color[spotColorIdx][0];
	vp->spotColor[1] = color[spotColorIdx][1];
	vp->spotColor[2] = color[spotColorIdx][2];
	vp->spotFogColor[0] = color[spotFogColorIdx][0];
	vp->spotFogColor[1] = color[spotFogColorIdx][1];
	vp->spotFogColor[2] = color[spotFogColorIdx][2];
	vp->spotEllipse[1] = 384.0f - vp->spotEllipse[1];
	vp->spotEllipse[2] = std::max(1.0f, vp->spotEllipse[2]);
	vp->spotEllipse[3] = std::max(1.0f, vp->spotEllipse[3]);
	vp->spotEllipse[2] = std::round(2047.0f / vp->spotEllipse[2]);
	vp->spotEllipse[3] = std::round(2047.0f / vp->spotEllipse[3]);
	vp->spotEllipse[0] = vp->spotEllipse[0] * m_xRatio + (float)m_xOffs;
	vp->spotEllipse[1] = vp->spotEllipse[1] * m_yRatio + (float)m_yOffs;
	vp->spotEllipse[2] *= m_xRatio;
	vp->spotEllipse[3] *= m_yRatio;

	vp->losPosX = (int)(((vpnode[0x1c] & 0xFFFF) / 16.0f) + 0.5f);
	vp->losPosY = (int)(((vpnode[0x1c] >> 16) / 16.0f) + 0.5f);
	vp->fogColour[0] = ((vpnode[0x22] >> 16) & 0xFF) * (1.0f / 255.0f);
	vp->fogColour[1] = ((vpnode[0x22] >> 8) & 0xFF) * (1.0f / 255.0f);
	vp->fogColour[2] = (vpnode[0x22] & 0xFF) * (1.0f / 255.0f);
	vp->fogDensity = std::fabs(Util::Uint32AsFloat(vpnode[0x23]));
	vp->fogStart = (float)(int16_t)(vpnode[0x25] & 0xFFFF) * (1.0f / 255.0f);
	vp->fogAttenuation = ((vpnode[0x24] >> 16) & 0xFF) * (1.0f / 255.0f);
	vp->fogAmbient = ((vpnode[0x25] >> 16) & 0xFF) * (1.0f / 255.0f);
	if (std::isinf(vp->fogDensity) || std::isnan(vp->fogDensity))
		vp->fogDensity = 0;
	vp->scrollFog = (vpnode[0x20] & 0xFF) * (1.0f / 255.0f);
	vp->scrollAtt = (vpnode[0x24] & 0xFF) * (1.0f / 255.0f);

	m_nodeAttribs.Reset();
	InitMatrixStack(matrixBase, m_modelMat);
	if (!vpDisabled)
	{
		uint32_t childptr = vpnode[0x02];
		if (((childptr >> 24) & 0x5) == 0)
			DescendNodePtr(vpnode[0x02]);
	}
	if (vpnode[0x01] != 0x01000000)
		RenderViewport(vpnode[0x01]);
}

void CR3DSceneBuilder::CopyVertexData(const New3D::R3DPoly& r3dPoly, std::vector<New3D::FVertex>& vertexArray)
{
	if (r3dPoly.faceColour[3] == 0)
		return;
	vertexArray.emplace_back(r3dPoly, 0);
	vertexArray.emplace_back(r3dPoly, 1);
	vertexArray.emplace_back(r3dPoly, 2);
	if (r3dPoly.number == 4)
	{
		vertexArray.emplace_back(r3dPoly, 0);
		vertexArray.emplace_back(r3dPoly, 2);
		vertexArray.emplace_back(r3dPoly, 3);
	}
}

void CR3DSceneBuilder::GetCoordinates(int width, int height, uint16_t uIn, uint16_t vIn, float uvScale, float& uOut, float& vOut) const
{
	uOut = width ? (uIn * uvScale) / width : 0;
	vOut = height ? (vIn * uvScale) / height : 0;
}

int CR3DSceneBuilder::GetTexFormat(int originalFormat, bool contour) const
{
	if (!contour)
		return originalFormat;
	switch (originalFormat)
	{
	case 1: case 2: case 3: case 4:
		return originalFormat + 7;
	default:
		return originalFormat;
	}
}

void CR3DSceneBuilder::SetMeshValues(New3D::SortingMesh* currentMesh, New3D::PolyHeader& ph)
{
	currentMesh->textured = ph.TexEnabled();
	currentMesh->alphaTest = ph.AlphaTest();
	currentMesh->textureAlpha = ph.TextureAlpha();
	currentMesh->polyAlpha = ph.PolyAlpha();
	currentMesh->lighting = ph.LightEnabled();
	currentMesh->fixedShading = ph.FixedShading() && !ph.SmoothShading();
	currentMesh->highPriority = ph.HighPriority();
	currentMesh->transLSelect = ph.TranslucencyPatternSelect();
	currentMesh->layered = ph.Layered();
	currentMesh->specular = ph.SpecularEnabled();
	currentMesh->shininess = ph.Shininess();
	currentMesh->specularValue = ph.SpecularValue();
	currentMesh->fogIntensity = ph.LightModifier();
	currentMesh->translatorMap = ph.TranslatorMap();
	currentMesh->noLosReturn = ph.NoLosReturn();
	currentMesh->smoothShading = ph.SmoothShading();
	if (!currentMesh->textured)
		return;

	currentMesh->format = GetTexFormat(ph.TexFormat(), ph.AlphaTest());
	if (currentMesh->format == 7)
		currentMesh->alphaTest = false;
	currentMesh->x = ph.X();
	currentMesh->y = ph.Y();
	currentMesh->width = ph.TexWidth();
	currentMesh->height = ph.TexHeight();
	currentMesh->microTexture = ph.MicroTexture();
	currentMesh->inverted = ph.TranslatorMapOffset() == 2;
	currentMesh->page = ph.Page();

	bool smoothU = ph.TexSmoothU();
	bool smoothV = ph.TexSmoothV();
	if (ph.AlphaTest())
		smoothU = smoothV = false;
	using Wrap = New3D::Mesh::TexWrapMode;
	currentMesh->wrapModeU = ph.TexUMirror() ? (smoothU ? Wrap::mirror : Wrap::mirrorClamp) : (smoothU ? Wrap::repeat : Wrap::repeatClamp);
	currentMesh->wrapModeV = ph.TexVMirror() ? (smoothV ? Wrap::mirror : Wrap::mirrorClamp) : (smoothV ? Wrap::repeat : Wrap::repeatClamp);
	if (currentMesh->microTexture)
	{
		currentMesh->microTextureID = ph.MicroTextureID();
		currentMesh->microTextureMinLOD = (float)ph.MicroTextureMinLOD();
	}
}

void CR3DSceneBuilder::CacheModel(New3D::Model* m, const uint32_t* data)
{
	if (!data)
		return;
	uint16_t texCoords[4][2] = {};
	New3D::PolyHeader ph;
	uint64_t lastHash = ~(uint64_t)0;
	New3D::SortingMesh* currentMesh = nullptr;
	std::unordered_map<uint64_t, New3D::SortingMesh> meshes;
	ph = data;
	int numTriangles = ph.NumTrianglesTotal();

	do
	{
		if (ph.header[6] == 0)
			break;
		uint64_t hash = ph.Hash();
		if (hash != lastHash)
		{
			auto found = meshes.find(hash);
			if (found == meshes.end())
			{
				currentMesh = &meshes.insert({ hash, New3D::SortingMesh() }).first->second;
				currentMesh->verts.reserve((size_t)numTriangles * 3);
				SetMeshValues(currentMesh, ph);
			}
			else
				currentMesh = &found->second;
		}
		if (!currentMesh)
			break;

		New3D::R3DPoly p;
		p.number = ph.NumVerts();
		float uvScale = ph.UVScale();
		ph.FaceNormal(p.faceNormal);
		int j = 0;
		for (int i = 0; i < 4; i++)
		{
			if (!ph.SharedVertex(i))
				continue;
			p.v[j] = m_prev[i];
			texCoords[j][0] = m_prevTexCoords[i][0];
			texCoords[j][1] = m_prevTexCoords[i][1];
			if (hash != lastHash && currentMesh->textured)
				GetCoordinates(currentMesh->width, currentMesh->height, texCoords[j][0], texCoords[j][1], uvScale, p.v[j].texcoords[0], p.v[j].texcoords[1]);
			j++;
		}
		lastHash = hash;

		if (!ph.PolyColor())
		{
			int colorIdx = ph.ColorIndex();
			uint32_t colour = m_polyRAM[m_colorTableAddr + colorIdx];
			p.faceColour[2] = (uint8_t)(colour & 0xFF);
			p.faceColour[1] = (uint8_t)((colour >> 8) & 0xFF);
			p.faceColour[0] = (uint8_t)((colour >> 16) & 0xFF);
		}
		else
		{
			p.faceColour[0] = (uint8_t)(ph.header[4] >> 24);
			p.faceColour[1] = (uint8_t)((ph.header[4] >> 16) & 0xFF);
			p.faceColour[2] = (uint8_t)((ph.header[4] >> 8) & 0xFF);
		}
		p.faceColour[3] = ph.Transparency();
		if (ph.Discard1() && !ph.Discard2())
			p.faceColour[3] = (uint8_t)(p.faceColour[3] / 2);

		for (int i = 0; i < p.number && !ph.SmoothShading(); i++)
		{
			p.v[i].normal[0] = p.faceNormal[0];
			p.v[i].normal[1] = p.faceNormal[1];
			p.v[i].normal[2] = p.faceNormal[2];
		}
		p.textureNP = ph.TextureNP() * m_textureNPFactor;
		uint32_t* vData = ph.StartOfData();
		for (; j < p.number; j++)
		{
			uint32_t ix = vData[0], iy = vData[1], iz = vData[2], it = vData[3];
			p.v[j].pos[0] = ((int32_t)ix >> 8) * m_vertexFactor;
			p.v[j].pos[1] = ((int32_t)iy >> 8) * m_vertexFactor;
			p.v[j].pos[2] = ((int32_t)iz >> 8) * m_vertexFactor;
			p.v[j].pos[3] = 1.0f;
			if (ph.SmoothShading())
			{
				p.v[j].normal[0] = BYTE_TO_FLOAT((int8_t)(ix & 0xFF));
				p.v[j].normal[1] = BYTE_TO_FLOAT((int8_t)(iy & 0xFF));
				p.v[j].normal[2] = BYTE_TO_FLOAT((int8_t)(iz & 0xFF));
			}
			if (ph.FixedShading() && !ph.SmoothShading())
			{
				if (ph.SpecularEnabled())
					p.v[j].fixedShade = (ix & 0xFF) * (1.0f / 255.0f);
				else
					p.v[j].fixedShade = BYTE_TO_FLOAT((int8_t)(ix & 0xFF));
			}
			float texU = 0, texV = 0;
			if (currentMesh->textured)
				GetCoordinates(currentMesh->width, currentMesh->height, (uint16_t)(it >> 16), (uint16_t)it, uvScale, texU, texV);
			p.v[j].texcoords[0] = texU;
			p.v[j].texcoords[1] = texV;
			texCoords[j][0] = (uint16_t)(it >> 16);
			texCoords[j][1] = (uint16_t)it;
			vData += 4;
		}

		if (ph.DoubleSided() && !ph.Discard())
		{
			New3D::R3DPoly flipped = p;
			New3D::V3::inverse(flipped.faceNormal);
			for (int i = 0; i < flipped.number; i++)
				New3D::V3::inverse(flipped.v[i].normal);
			CopyVertexData(flipped, currentMesh->verts);
		}
		if (!ph.Discard())
			CopyVertexData(p, currentMesh->verts);
		for (int i = 0; i < 4; i++)
		{
			m_prev[i] = p.v[i];
			m_prevTexCoords[i][0] = texCoords[i][0];
			m_prevTexCoords[i][1] = texCoords[i][1];
		}
	} while (ph.NextPoly());

	m->meshes->reserve(meshes.size());
	for (auto& it : meshes)
	{
		if (m->dynamic)
		{
			it.second.vboOffset = (int)m_polyBufferRam.size();
			it.second.vertexCount = (int)it.second.verts.size();
			m_polyBufferRam.insert(m_polyBufferRam.end(), it.second.verts.begin(), it.second.verts.end());
		}
		else
		{
			if (m_polyBufferRom.size() + it.second.verts.size() >= kMaxRomVerts)
			{
				/*
				 * The cache has reached its ceiling, so it is emptied and rebuilt from scratch. The
				 * vertex buffer on the GPU is discarded with it and refills over the following frames.
				 *
				 * Worth logging rather than passing over in silence, for two reasons. It is the only
				 * visible consequence of a limit being reached - the models that were being built go
				 * missing for a frame - so a pop during play is otherwise inexplicable. And the figure
				 * it reports is what says whether the ceiling is set sensibly: a flush every few minutes
				 * in normal play means it is too low, and one that never happens means the headroom is
				 * being paid for without being used.
				 */
				InfoLog("Vulkan: ROM geometry cache reached its %u vertex limit and was flushed after "
					"%zu vertices; static models will be missing for this frame.",
					(unsigned)kMaxRomVerts, m_polyBufferRom.size());
				m_polyBufferRom.clear();
				m_romMap.clear();
				m_romReset = true;
				return;
			}
			it.second.vboOffset = (int)m_polyBufferRom.size();
			it.second.vertexCount = (int)it.second.verts.size();
			m_polyBufferRom.insert(m_polyBufferRom.end(), it.second.verts.begin(), it.second.verts.end());
		}
		m->meshes->push_back(it.second);
	}
}

void CR3DSceneBuilder::CalcViewport(New3D::Viewport* vp)
{
	float l = vp->angle_left, r = vp->angle_right, t = vp->angle_top, b = vp->angle_bottom;
	vp->projectionMatrix.LoadIdentity();

	/*
	 * Diagnostic for the widescreen branch.
	 *
	 * Added because an A/B test produced pixel-identical frames with WideScreen off and on, even though
	 * the log confirmed the setting reached the renderer - which cannot happen if this branch is being
	 * taken. The branch is gated on the *game's* viewport being full-screen, so if the state being
	 * viewed has a smaller viewport both runs fall through to the same path and the test says nothing.
	 *
	 * The only way to tell which is happening is to state it, so the viewport dimensions and whether
	 * the branch applied are reported once per distinct viewport rather than per frame.
	 *
	 * "Once per distinct viewport" has to mean every distinct viewport, not the most recent one. A
	 * single frame walks several of them - the game switches between a full-screen view and smaller
	 * ones - so remembering only the previous set of dimensions reports a change on every frame, in
	 * both directions, and the line becomes 98% of the log. Measured on Daytona 2: 11,830 of 12,014
	 * lines were this one message alternating between two viewports, which buries everything else in
	 * the file. The set below reports each combination once for the life of the session, which is what
	 * the sentence above always said.
	 *
	 * A tuple rather than a packed integer: the previous key packed four coordinates into one uint32_t
	 * with shifts and an xor, which collides - 495x0 and 0x495 are different viewports with the same
	 * key. A collision loses a line rather than adding one, so it was not the cause of the flood, but
	 * it is the same kind of mistake and it costs nothing to not make it.
	 */
	{
		// Named rather than `const auto`: decltype of a const variable is a const-qualified type, and
		// std::set<const std::tuple<...>> is ill-formed - std::allocator refuses a const value type.
		using ViewportKey = std::tuple<int, int, int, int>;
		const ViewportKey key = std::make_tuple((int)vp->vpX, (int)vp->vpY, (int)vp->vpWidth, (int)vp->vpHeight);
		static std::set<ViewportKey> s_reportedViewports;

		if (s_reportedViewports.insert(key).second)
		{
			bool qualifies = (vp->vpX == 0 && vp->vpWidth >= 495 && vp->vpY == 0 && vp->vpHeight >= 383);
			InfoLog("Vulkan: viewport %.2fx%.2f at (%.2f,%.2f) - widescreen %s "
				"(WideScreen=%d, full-screen viewport=%d).",
				vp->vpWidth, vp->vpHeight, vp->vpX, vp->vpY,
				(m_wideScreen && qualifies) ? "ACTIVE" : "NOT active",
				m_wideScreen ? 1 : 0, qualifies ? 1 : 0);
		}
	}

	if (m_wideScreen && vp->vpX == 0 && vp->vpWidth >= 495 && vp->vpY == 0 && vp->vpHeight >= 383)
	{
		/*
		 * The aspect the widened viewport is actually drawn at, which is not the window's aspect.
		 *
		 * This block makes the 3D layer m_totalXRes wide by vpHeight*m_yRatio tall, and below it
		 * derives the frustum widening from the window (m_totalXRes x m_totalYRes) instead. Those two
		 * rectangles are identical whenever the rendered image fills the window, which is every case
		 * except integer scaling - and under integer scaling they differ by a factor of
		 * m_totalYRes/(384*multiple), i.e. 2160/(384*5) = 1.125 on a 4K panel at 5x. The frustum was
		 * therefore widened 12.5% too little, which magnifies the scene horizontally by the same
		 * amount: WideScreen plus IntegerScale stretched the 3D layer while the 2D layer composited
		 * over it stayed at the correct scale, so the two layers disagreed.
		 *
		 * Using the output height is a no-op for every non-integer-scale configuration: a full-screen
		 * viewport has vpHeight == 384, so vpHeight*m_yRatio == m_yRes, and the expression reduces to
		 * the same m_totalXRes/m_xRes it always did.
		 */
		const float outHeight = (vp->vpHeight > 0.0f && m_yRatio > 0.0f)
			? vp->vpHeight * m_yRatio
			: (float)m_totalYRes;
		const float correction = ((float)m_totalXRes / outHeight) / ((float)m_xRes / (float)m_yRes);
		m_planes.correction = 1.0f / correction;
		vp->x = 0;
		vp->y = m_yOffs + (int)((384.0f - (vp->vpY + vp->vpHeight)) * m_yRatio);
		vp->width = (int)m_totalXRes;
		vp->height = (int)(vp->vpHeight * m_yRatio);
		vp->projectionMatrix.FrustumRZ(l * correction, r * correction, b, t, NEAR_PLANE);
	}
	else
	{
		vp->x = m_xOffs + (int)(vp->vpX * m_xRatio);
		vp->y = m_yOffs + (int)((384.0f - (vp->vpY + vp->vpHeight)) * m_yRatio);
		vp->width = (int)(vp->vpWidth * m_xRatio);
		vp->height = (int)(vp->vpHeight * m_yRatio);
		vp->projectionMatrix.FrustumRZ(l, r, b, t, NEAR_PLANE);
	}
}

} // Vulkan3D
