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
#ifndef INCLUDED_VULKAN3D_2D_H
#define INCLUDED_VULKAN3D_2D_H

#include "Graphics/IRender2D.h"
#include "VulkanMemory.h"
#include "Util/NewConfig.h"

namespace Vulkan3D {

class CVulkanContext;

class CVulkan2D final : public IRender2D
{
public:
	CVulkan2D(const Util::Config::Node& config, CVulkanContext& context);
	~CVulkan2D() override;

	void BeginFrame(void) override;
	void PreRenderFrame(void) override;
	void RenderFrameBottom(void) override;
	void RenderFrameTop(void) override;
	void EndFrame(void) override;
	void WriteVRAM(unsigned addr, uint32_t data) override;
	void AttachRegisters(const uint32_t* regPtr) override;
	void AttachPalette(const uint32_t* palPtr[2]) override;
	void AttachVRAM(const uint8_t* vramPtr) override;
	void AttachDrawBuffers(std::shared_ptr<TileGenBuffer> bottom, std::shared_ptr<TileGenBuffer> top) override;
	Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget, UpscaleMode upscaleMode) override;

	void SetSceneColor(VkImageView view);

private:
	bool CreatePipeline();
	void UploadLayer(int index);
	void DrawLayer(uint32_t layer, int x, int y, int w, int h);
	void DrawCompositePass(VkCommandBuffer cmd, VkImageView targetView);

	void PictureRect(int& x, int& y, int& w, int& h) const;
	const Util::Config::Node& m_config;
	CVulkanContext& m_context;
	unsigned m_xOff = 0, m_yOff = 0, m_xRes = 496, m_yRes = 384, m_totalX = 496, m_totalY = 384;
	unsigned m_correction = 0;
	UpscaleMode m_upscaleMode = UpscaleMode::Bilinear;
	bool m_wideBg = false;

	// The widescreen option, read here because the compositor needs it as well as the 3D renderer:
	// it decides how much of the output the picture is allowed to occupy, and therefore what the
	// scissor clips every layer to. Kept in step with CVulkan3D's copy of the same setting - both read
	// it from the same configuration at setup, so they cannot disagree.
	bool m_wide = false;
	bool m_drawBottom = false;
	bool m_drawTop = false;
	bool m_uploaded[2] = {};
	std::shared_ptr<TileGenBuffer> m_buffers[2];

	AllocatedImage m_layers[2];
	VkImageView m_sceneView = VK_NULL_HANDLE;

	VkSampler m_sampler = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
	VkDescriptorPool m_pool = VK_NULL_HANDLE;
	VkDescriptorSet m_set = VK_NULL_HANDLE;
	VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkShaderModule m_vert = VK_NULL_HANDLE;
	VkShaderModule m_frag = VK_NULL_HANDLE;
};

} // Vulkan3D

#endif
