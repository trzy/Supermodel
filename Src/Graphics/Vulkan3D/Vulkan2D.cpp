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
#include "Vulkan2D.h"
#include "VulkanContext.h"
#include "PictureLayout.h"
#include "OSD/Logger.h"
#include "Shaders/composite_vert.spv.h"
#include "Shaders/composite_frag.spv.h"
#include <cstring>

namespace Vulkan3D {
struct LayerPush { uint32_t layer; int32_t upscaleMode; };
constexpr uint32_t kCompositeLayerCount = 3;

CVulkan2D::CVulkan2D(const Util::Config::Node& config, CVulkanContext& context)
	: m_config(config), m_context(context)
{
	m_wideBg = config["WideBackground"].ValueAs<bool>();
	m_wide = config["WideScreen"].ValueAs<bool>();

}

CVulkan2D::~CVulkan2D()
{
	VkDevice device = m_context.Device();
	if (!device)
		return;
	vkDeviceWaitIdle(device);
	for (auto& layer : m_layers)
	{
		m_context.DestroyView(layer.view);
		layer.view = VK_NULL_HANDLE;
		m_context.Memory().DestroyImage(layer);
	}
	if (m_pipeline)
		vkDestroyPipeline(device, m_pipeline, nullptr);
	if (m_pipeLayout)
		vkDestroyPipelineLayout(device, m_pipeLayout, nullptr);
	if (m_vert)
		vkDestroyShaderModule(device, m_vert, nullptr);
	if (m_frag)
		vkDestroyShaderModule(device, m_frag, nullptr);
	if (m_pool)
		vkDestroyDescriptorPool(device, m_pool, nullptr);
	if (m_layout)
		vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
	if (m_sampler)
		vkDestroySampler(device, m_sampler, nullptr);
}

Result CVulkan2D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned, UpscaleMode upscaleMode)
{
	m_xOff = xOffset;
	m_yOff = yOffset;
	m_xRes = xRes;
	m_yRes = yRes;
	m_totalX = totalXRes;
	m_totalY = totalYRes;
	m_correction = (unsigned)((yRes / 384.0) * 2.0 + 0.5);
	m_upscaleMode = (xRes == 496 && yRes == 384) ? UpscaleMode::Nearest : upscaleMode;
	m_wideBg = m_config["WideBackground"].ValueAs<bool>();
	m_wide = m_config["WideScreen"].ValueAs<bool>();

	if (m_layers[0].image)
		return Result::OKAY;

	for (int i = 0; i < 2; i++)
	{
	if (!m_context.Memory().CreateImage(496, 384, 1, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, m_layers[i]))
	{
		ErrorLog("Vulkan2D: failed to allocate the tile layer image.");
		return Result::FAIL;
	}
		m_layers[i].view = m_context.CreateView(m_layers[i].image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	}
	if (!CreatePipeline())
	{
		ErrorLog("Vulkan2D: failed to create the composite pipeline.");
		return Result::FAIL;
	}

	VkDescriptorImageInfo infos[kCompositeLayerCount] = {};
	for (uint32_t i = 0; i < kCompositeLayerCount; i++)
	{
		infos[i].sampler = VK_NULL_HANDLE;
		infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	infos[0].imageView = m_layers[0].view;
	infos[1].imageView = m_layers[0].view;
	infos[2].imageView = m_layers[1].view;
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_set;
	write.dstBinding = 1;
	write.descriptorCount = kCompositeLayerCount;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = infos;
	vkUpdateDescriptorSets(m_context.Device(), 1, &write, 0, nullptr);
	return Result::OKAY;
}

bool CVulkan2D::CreatePipeline()
{
	VkDevice device = m_context.Device();
	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = composite_vert_spv_size;
	moduleInfo.pCode = composite_vert_spv;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &m_vert) != VK_SUCCESS)
	{
		ErrorLog("Vulkan2D: composite vertex shader failed to load.");
		return false;
	}
	moduleInfo.codeSize = composite_frag_spv_size;
	moduleInfo.pCode = composite_frag_spv;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &m_frag) != VK_SUCCESS)
	{
		ErrorLog("Vulkan2D: composite fragment shader failed to load.");
		return false;
	}

	VkSamplerCreateInfo sampler = {};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	// Required for the bloom: Vulkan defaults maxLod to 0, which would clamp every sample to mip
	// level 0 and make the glow do nothing. VK_LOD_CLAMP_NONE lets the shader pick any level.
	sampler.maxLod = VK_LOD_CLAMP_NONE;
	if (VkResult r = vkCreateSampler(device, &sampler, nullptr, &m_sampler); r != VK_SUCCESS)
	{
		ErrorLog("Vulkan2D: vkCreateSampler failed (%d).", (int)r);
		return false;
	}

	VkDescriptorSetLayoutBinding bindings[2] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[1].descriptorCount = kCompositeLayerCount;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo setInfo = {};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.bindingCount = 2;
	setInfo.pBindings = bindings;
	if (VkResult r = vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &m_layout); r != VK_SUCCESS)
	{
		ErrorLog("Vulkan2D: vkCreateDescriptorSetLayout failed (%d).", (int)r);
		return false;
	}

	VkDescriptorPoolSize sizes[2] = {};
	sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	sizes[0].descriptorCount = 1;
	sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	// Must match the layout's binding count above, not the number of layers that happen to be filled:
	// a pool smaller than the set it has to serve fails the allocation below, and the failure is total
	// (nothing is drawn) rather than partial.
	sizes[1].descriptorCount = kCompositeLayerCount;
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = sizes;
	if (VkResult r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool); r != VK_SUCCESS)
	{
		ErrorLog("Vulkan2D: vkCreateDescriptorPool failed (%d).", (int)r);
		return false;
	}
	VkDescriptorSetAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = m_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &m_layout;
	if (VkResult r = vkAllocateDescriptorSets(device, &alloc, &m_set); r != VK_SUCCESS)
	{
		// VK_ERROR_OUT_OF_POOL_MEMORY (or, on newer drivers, the INITIALIZATION_FAILED that a validation
		// error about pool sizes turns into) means the pool is smaller than the set it has to serve.
		ErrorLog("Vulkan2D: vkAllocateDescriptorSets failed (%d) - the descriptor pool is too small for "
			"the layout (%u sampled images per set).", (int)r, kCompositeLayerCount);
		return false;
	}

	VkDescriptorImageInfo sampInfo = {};
	sampInfo.sampler = m_sampler;
	VkWriteDescriptorSet sampWrite = {};
	sampWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	sampWrite.dstSet = m_set;
	sampWrite.dstBinding = 0;
	sampWrite.descriptorCount = 1;
	sampWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	sampWrite.pImageInfo = &sampInfo;
	vkUpdateDescriptorSets(device, 1, &sampWrite, 0, nullptr);

	VkPushConstantRange range = {};
	range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	range.size = sizeof(LayerPush);
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &m_layout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &range;
	if (VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipeLayout); r != VK_SUCCESS)
	{
		ErrorLog("Vulkan2D: vkCreatePipelineLayout failed (%d).", (int)r);
		return false;
	}

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = m_vert;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = m_frag;
	stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertex = {};
	vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo raster = {};
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo multi = {};
	multi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState blend = {};
	blend.blendEnable = VK_TRUE;
	blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.colorBlendOp = VK_BLEND_OP_ADD;
	blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.alphaBlendOp = VK_BLEND_OP_ADD;
	blend.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo blending = {};
	blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blending.attachmentCount = 1;
	blending.pAttachments = &blend;
	VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic = {};
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynamics;
	VkPipelineRenderingCreateInfo rendering = {};
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	VkFormat format = m_context.SwapFormat();
	rendering.pColorAttachmentFormats = &format;

	VkGraphicsPipelineCreateInfo pipe = {};
	pipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipe.pNext = &rendering;
	pipe.stageCount = 2;
	pipe.pStages = stages;
	pipe.pVertexInputState = &vertex;
	pipe.pInputAssemblyState = &assembly;
	pipe.pViewportState = &viewport;
	pipe.pRasterizationState = &raster;
	pipe.pMultisampleState = &multi;
	pipe.pColorBlendState = &blending;
	pipe.pDynamicState = &dynamic;
	pipe.layout = m_pipeLayout;
	VkResult pipeResult = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe, nullptr, &m_pipeline);
	if (pipeResult != VK_SUCCESS)
		ErrorLog("Vulkan2D: vkCreateGraphicsPipelines failed (%d).", (int)pipeResult);
	return pipeResult == VK_SUCCESS;
}

void CVulkan2D::SetSceneColor(VkImageView view)
{
	if (view == m_sceneView || !m_set)
		return;
	m_sceneView = view;
	VkDescriptorImageInfo info = {};
	info.imageView = view ? view : m_layers[0].view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_set;
	write.dstBinding = 1;
	write.dstArrayElement = 1;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &info;
	vkUpdateDescriptorSets(m_context.Device(), 1, &write, 0, nullptr);
}

void CVulkan2D::BeginFrame(void)
{
	m_drawBottom = false;
	m_drawTop = false;
}

void CVulkan2D::UploadLayer(int index)
{
	if (!m_buffers[index] || !m_context.IsRecording())
		return;
	VkCommandBuffer cmd = m_context.Cmd();
	VkImageLayout oldLayout = m_uploaded[index] ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	VkDeviceSize offset = 0;
	void* ptr = nullptr;
	VkDeviceSize bytes = 512ull * 384ull * 4ull;
	if (!m_context.Ring().Alloc(bytes, 4, offset, ptr))
		return;
	CmdTransition(cmd, m_layers[index].image, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	std::memcpy(ptr, m_buffers[index]->data, (size_t)bytes);
	VkBufferImageCopy copy = {};
	copy.bufferOffset = offset;
	copy.bufferRowLength = 512;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent = { 496, 384, 1 };
	vkCmdCopyBufferToImage(cmd, m_context.Ring().Buffer(), m_layers[index].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	CmdTransition(cmd, m_layers[index].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	m_uploaded[index] = true;
}

void CVulkan2D::PreRenderFrame(void)
{
	UploadLayer(0);
	UploadLayer(1);
}

void CVulkan2D::RenderFrameBottom(void) { m_drawBottom = true; }

void CVulkan2D::RenderFrameTop(void) { m_drawTop = true; }

void CVulkan2D::DrawLayer(uint32_t layer, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;

	VkExtent2D extent = m_context.Extent();

	// Viewport: the caller's rectangle, untouched.
	VkViewport viewport = {};
	viewport.x = (float)x;
	viewport.y = (float)y;
	viewport.width = (float)w;
	viewport.height = (float)h;
	viewport.maxDepth = 1.0f;

	// Scissor: the rectangle cropped to the visible picture, then clamped to the swapchain (Vulkan
	// rejects a negative scissor offset).
	int cx = x, cy = y, cw = w, ch = h;
	int px = 0, py = 0, pw = 0, ph = 0;
	PictureRect(px, py, pw, ph);
	if (cx < px) { cw -= px - cx; cx = px; }
	if (cy < py) { ch -= py - cy; cy = py; }
	if (cx + cw > px + pw) cw = px + pw - cx;
	if (cy + ch > py + ph) ch = py + ph - cy;

	if (cx < 0) { cw += cx; cx = 0; }
	if (cy < 0) { ch += cy; cy = 0; }
	if (cx >= (int)extent.width || cy >= (int)extent.height)
		return;
	if (cx + cw > (int)extent.width) cw = (int)extent.width - cx;
	if (cy + ch > (int)extent.height) ch = (int)extent.height - cy;
	if (cw <= 0 || ch <= 0)
		return;

	VkCommandBuffer cmd = m_context.Cmd();
	VkRect2D scissor = {};
	scissor.offset = { cx, cy };
	scissor.extent = { (uint32_t)cw, (uint32_t)ch };
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
	// Scale native-resolution tiles; the 3D scene already matches the output size.
	LayerPush push = { layer, layer == 1 ? (int32_t)UpscaleMode::Bilinear : (int32_t)m_upscaleMode };
	vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
	vkCmdDraw(cmd, 4, 1, 0, 0);
}

void CVulkan2D::EndFrame(void)
{
    if (!m_context.IsRecording() || !m_pipeline)
        return;
    VkCommandBuffer cmd = m_context.Cmd();
    // Discard the previous contents; the composite pass clears the entire image.
    CmdTransition(cmd, m_context.SwapImage(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    DrawCompositePass(cmd, m_context.SwapView());
    CmdTransition(cmd, m_context.SwapImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
    m_context.MarkSwapchainReady();
}

void CVulkan2D::DrawCompositePass(VkCommandBuffer cmd, VkImageView targetView)
{

	VkRenderingAttachmentInfo color = {};
	color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color.imageView = targetView;
	color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.clearValue.color = { { 0, 0, 0, 1 } };
	VkRenderingInfo rendering = {};
	rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering.renderArea.extent = m_context.Extent();
	rendering.layerCount = 1;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachments = &color;
	CmdBeginRendering(cmd, &rendering);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLayout, 0, 1, &m_set, 0, nullptr);

	VkExtent2D extent = m_context.Extent();
	int bottomX = 0, bottomY = 0, bottomW = (int)extent.width, bottomH = (int)extent.height;
	if (!(m_wideBg && m_drawBottom))
	{
		bottomX = (int)m_xOff - (int)m_correction;
		int glY = (int)m_yOff + (int)m_correction;
		bottomW = (int)m_xRes;
		bottomH = (int)m_yRes;
		bottomY = (int)extent.height - (glY + bottomH);
	}
	if (m_drawBottom && m_uploaded[0])
		DrawLayer(0, bottomX, bottomY, bottomW, bottomH);
	if (m_sceneView)
	{
		// The 3D scene is rendered into an image the size of the swapchain and is already
		// positioned/scaled correctly inside it (see CVulkan3D), so blit it 1:1.
		DrawLayer(1, 0, 0, (int)extent.width, (int)extent.height);
	}
	if (m_drawTop && m_uploaded[1])
	{
		int glY = (int)m_yOff + (int)m_correction;
		int topH = (int)m_yRes;
		int topY = (int)extent.height - (glY + topH);
		DrawLayer(2, (int)m_xOff - (int)m_correction, topY, (int)m_xRes, topH);
	}
	CmdEndRendering(cmd);
}

void CVulkan2D::PictureRect(int& x, int& y, int& w, int& h) const
{
	ComputePictureRect((int)m_context.Extent().width, (int)m_context.Extent().height,
		(int)m_xOff, (int)m_yOff, (int)m_xRes, (int)m_yRes,
		(int)m_correction, m_wideBg, m_wide,
		x, y, w, h);
}

void CVulkan2D::WriteVRAM(unsigned, uint32_t) {}

void CVulkan2D::AttachRegisters(const uint32_t*) {}

void CVulkan2D::AttachPalette(const uint32_t**) {}

void CVulkan2D::AttachVRAM(const uint8_t*) {}

void CVulkan2D::AttachDrawBuffers(std::shared_ptr<TileGenBuffer> bottom, std::shared_ptr<TileGenBuffer> top)
{
	m_buffers[0] = bottom;
	m_buffers[1] = top;
}

} // Vulkan3D
