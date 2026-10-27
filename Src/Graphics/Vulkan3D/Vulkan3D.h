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
#ifndef INCLUDED_VULKAN3D_H
#define INCLUDED_VULKAN3D_H

#include "Graphics/IRender3D.h"
#include "R3DSceneBuilder.h"
#include "VulkanTextureCache.h"
#include "Util/NewConfig.h"
#include <atomic>
#include <mutex>
#include <string>

namespace Vulkan3D {

class CVulkan2D;

class CVulkan3D final : public IRender3D
{
public:
	CVulkan3D(const Util::Config::Node& config, const std::string& gameName, CVulkanContext& context, CVulkan2D& output);
	~CVulkan3D() override;

	void RenderFrame(void) override;
	void BeginFrame(void) override;
	void EndFrame(void) override;
	void UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height) override;
	void AttachMemory(const uint32_t* cullingRAMLoPtr, const uint32_t* cullingRAMHiPtr, const uint32_t* polyRAMPtr, const uint32_t* vromPtr, const uint16_t* textureRAMPtr) override;
	void SetStepping(Game::Stepping stepping) override;
	Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget) override;
	void SetSunClamp(bool enable) override;
	void SetBlockCulling(bool enable) override;
	float GetLosValue(int layer) override;

	/*
	 * The MSAA level in force, which is not always the one asked for: a driver that cannot provide
	 * the sample count for both the colour and depth formats has it reduced to 1 during Init, and the
	 * settings window would still be showing the request. Read out for the statistics display, so that
	 * "the setting says 4x but nothing is being antialiased" is something a user can see.
	 */
	uint32_t SampleCount() const { return m_sampleBits; }

private:
	bool CreatePipeline(bool translucent, bool overlay);
	bool CreateFogPipeline();
	bool EnsureTargets();
	void DestroyTargets();
	R3DVertex ConvertVertex(const New3D::FVertex& src) const;
	void CollectTextures();
	void UploadGeometry(VkCommandBuffer cmd);
	void DrawScene(VkCommandBuffer cmd);
	void DrawFog(VkCommandBuffer cmd);
	void CopyLos(VkCommandBuffer cmd, int priority);
	void FillPush(R3DPush& push, const New3D::Model& model, const New3D::Viewport& vp, uint32_t materialIndex, bool discardAlpha) const;
	void FillMaterial(R3DMaterial& material, const New3D::Mesh& mesh, const New3D::Model& model);

	const Util::Config::Node& m_config;
	std::string m_gameName;
	CVulkanContext& m_context;
	CVulkan2D& m_output;
	CR3DSceneBuilder m_scene;
	CVulkanTextureCache m_textures;

	unsigned m_xOff = 0, m_yOff = 0, m_xRes = 496, m_yRes = 384, m_totalX = 496, m_totalY = 384;
	bool m_wide = false;
	bool m_noWhiteFlash = false;
	std::atomic<bool> m_sunClamp;
	std::atomic<bool> m_blockCulling;

	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkPipeline m_opaque = VK_NULL_HANDLE;
	VkPipeline m_translucent = VK_NULL_HANDLE;
	VkPipeline m_overlayOpaque = VK_NULL_HANDLE;
	VkPipeline m_overlayTranslucent = VK_NULL_HANDLE;
	VkShaderModule m_vert = VK_NULL_HANDLE;
	VkShaderModule m_frag = VK_NULL_HANDLE;

	// Ambient / scroll fog background pass (see DrawFog).
	VkPipelineLayout m_fogLayout = VK_NULL_HANDLE;
	VkPipeline m_fog = VK_NULL_HANDLE;
	VkShaderModule m_fogVert = VK_NULL_HANDLE;
	VkShaderModule m_fogFrag = VK_NULL_HANDLE;

	AllocatedImage m_color;
	AllocatedImage m_depth;
	// Hardware MSAA. When m_sampleBits > 1 the scene is rasterised into a multisampled m_color and
	// resolved into m_colorResolve, which is the image the 2D layer composites. Antialiasing is then
	// done by the rasteriser on geometry coverage, so polygon edges are smoothed without touching
	// texel sharpness - decals and small textures stay crisp, unlike a post-process blur.
	AllocatedImage m_colorResolve;
	uint32_t m_sampleBits = 1;
	VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
	VkImageLayout m_colorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout m_depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout m_resolveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	uint64_t m_targetGeneration = ~0ull;

	AllocatedBuffer m_rom;
	AllocatedBuffer m_materials;
	AllocatedBuffer m_los;
	VkDeviceSize m_materialSlice = 0;
	size_t m_romGpuVerts = 0;
	uint32_t m_materialCount = 0;
	VkDeviceSize m_ramOffset = 0;
	bool m_ramReady = false;

	// Diagnostics for the ROM vertex buffer, which is a fixed-size GPU buffer the CPU-side geometry
	// cache is allowed to outgrow. Both counters exist so the ceiling can be seen approaching in the
	// log rather than only inferred from missing geometry on screen.
	size_t m_romVertsSeen = 0;
	uint64_t m_romReportFrames = 0;
	bool m_diagnostics = false;
	uint64_t m_romFullReports = 0;

	/*
	 * Diagnostics for the "something was thrown away and never rebuilt" class of bug.
	 *
	 * A resize destroys and recreates the render targets, and anything still referring to what was
	 * destroyed fails without saying so - which is why a session that has been minimised and restored
	 * can look different from one that has not, with no error anywhere to explain it.
	 *
	 * Each counter below is a path that drops geometry or texture silently today:
	 *
	 *   m_skipRomBeyond        a mesh lies past the end of the uploaded ROM geometry
	 *   m_skipRamNotReady      a dynamic mesh was drawn before its ring allocation succeeded
	 *   m_skipDegenerateNode   a node's viewport collapsed, taking every model in that node with it
	 *
	 * Reported together and only when a value changes, so a normal run stays quiet while a
	 * reproduction names the path that lost the geometry instead of leaving it to be guessed at.
	 */
	uint64_t m_skipRomBeyond = 0;
	uint64_t m_skipRamNotReady = 0;
	uint64_t m_skipDegenerateNode = 0;
	uint64_t m_skipLoggedRom = 0;
	uint64_t m_skipLoggedRam = 0;
	uint64_t m_skipLoggedNode = 0;

	// How many wholly-unuploaded texture regions have had their coordinates reported. Bounded, because
	// the point is to see the shape of the first few rather than to fill the log.
	unsigned m_sampleMissingRegions = 0;

	bool m_drew = false;
	uint64_t m_frameSerial = 1;

	float m_losValue[4] = {};
	bool m_losReady[kMaxFramesInFlight] = {};
	std::mutex m_losMutex;
};

} // Vulkan3D

#endif
