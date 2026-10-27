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
#ifndef INCLUDED_VULKAN3D_SCENE_H
#define INCLUDED_VULKAN3D_SCENE_H

#include "../New3D/Model.h"
#include "../New3D/PolyHeader.h"
#include "../New3D/R3DData.h"
// uint32_t is used throughout the interface below. MSVC gets it indirectly through one of the
// headers above, so this was missing without ever being noticed; gcc does not, and the whole file
// failed with "'uint32_t' does not name a type" on the first line that used it.
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Vulkan3D {

class CR3DSceneBuilder
{
public:
	void AttachMemory(const uint32_t* cullingLo, const uint32_t* cullingHi, const uint32_t* polyRam, const uint32_t* vrom);
	void SetStepping(int stepping);
	void SetViewport(unsigned xOff, unsigned yOff, unsigned xRes, unsigned yRes, unsigned totalX, unsigned totalY, bool wideScreen);
	void BeginFrame();
	void Build();

	std::vector<New3D::Node>& Nodes() { return m_nodes; }
	std::vector<New3D::FVertex>& RamVerts() { return m_polyBufferRam; }
	std::vector<New3D::FVertex>& RomVerts() { return m_polyBufferRom; }
	bool RomReset() const { return m_romReset; }
	void ClearRomReset() { m_romReset = false; }

private:
	const uint32_t* TranslateCullingAddress(uint32_t addr) const;
	const uint32_t* TranslateModelAddress(uint32_t modelAddr) const;
	bool DrawModel(uint32_t modelAddr);
	void DescendCullingNode(uint32_t addr);
	void DescendNodePtr(uint32_t nodeAddr);
	void DescendPointerList(uint32_t addr);
	void MultMatrix(uint32_t matrixOffset, New3D::Mat4& mat);
	void InitMatrixStack(uint32_t matrixBaseAddr, New3D::Mat4& mat);
	void ResetMatrix(New3D::Mat4& mat) const;
	void RenderViewport(uint32_t addr);
	void CopyVertexData(const New3D::R3DPoly& poly, std::vector<New3D::FVertex>& vertexArray);
	void GetCoordinates(int width, int height, uint16_t uIn, uint16_t vIn, float uvScale, float& uOut, float& vOut) const;
	int GetTexFormat(int originalFormat, bool contour) const;
	void SetMeshValues(New3D::SortingMesh* mesh, New3D::PolyHeader& ph);
	void CacheModel(New3D::Model* model, const uint32_t* data);
	bool IsDynamicModel(uint32_t* data) const;
	bool IsVROMModel(uint32_t modelAddr) const;
	void CalcViewport(New3D::Viewport* vp);

	int m_step = 0x10;
	int m_offset = 2;
	float m_vertexFactor = 1.0f / 128.0f;
	float m_textureNPFactor = 1.0f / 4096.0f;

	const uint32_t* m_cullingRAMLo = nullptr;
	const uint32_t* m_cullingRAMHi = nullptr;
	const uint32_t* m_polyRAM = nullptr;
	const uint32_t* m_vrom = nullptr;

	float m_xRatio = 1, m_yRatio = 1;
	unsigned m_xOffs = 0, m_yOffs = 0;
	unsigned m_xRes = 496, m_yRes = 384;
	unsigned m_totalXRes = 496, m_totalYRes = 384;
	bool m_wideScreen = false;

	const float* m_matrixBasePtr = nullptr;
	uint32_t m_colorTableAddr = 0x400;
	LODBlendTable* m_LODBlendTable = nullptr;
	New3D::NodeAttributes m_nodeAttribs;
	New3D::Mat4 m_modelMat;
	New3D::Vertex m_prev[4] = {};
	uint16_t m_prevTexCoords[4][2] = {};

	std::vector<New3D::Node> m_nodes;
	std::vector<New3D::FVertex> m_polyBufferRam;
	std::vector<New3D::FVertex> m_polyBufferRom;
	std::unordered_map<uint32_t, std::shared_ptr<std::vector<New3D::Mesh>>> m_romMap;
	bool m_romReset = false;

	struct
	{
		float bnlu, bnlv, bntu, bntw, bnru, bnrv, bnbu, bnbw, correction;
	} m_planes = {};
};

} // Vulkan3D

#endif
