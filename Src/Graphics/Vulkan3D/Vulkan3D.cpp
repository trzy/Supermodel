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
#include "Vulkan3D.h"
#include "Vulkan2D.h"
#include "OSD/Logger.h"
#include "Shaders/real3d_vert.spv.h"
#include "Shaders/real3d_frag.spv.h"
#include "Shaders/fog_vert.spv.h"
#include "Shaders/fog_frag.spv.h"

#include <cstring>
#include <cmath>

namespace Vulkan3D {

static const int kMicroX[8] = { 0, 0, 128, 128, 0, 0, 128, 128 };
static const int kMicroY[8] = { 0, 128, 0, 128, 256, 384, 256, 384 };
static constexpr float kNearPlane = 1e-3f;

static uint32_t AlignUp(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1u) & ~(alignment - 1u);
}

CVulkan3D::CVulkan3D(const Util::Config::Node& config, const std::string& gameName, CVulkanContext& context, CVulkan2D& output)
	: m_config(config), m_gameName(gameName), m_context(context), m_output(output), m_sunClamp(true), m_blockCulling(false)
{
	m_wide = config["WideScreen"].ValueAs<bool>();
	m_noWhiteFlash = config["NoWhiteFlash"].ValueAs<bool>();
	m_diagnostics = config["VulkanDiagnostics"].ValueAsDefault<bool>(false);

	// Hardware MSAA level: 1 (or 0) disables it, 2, 4 and 8 select that many samples. Only powers
	// of two between 1 and 8 are accepted; anything else falls back to no multisampling.
	const int msaa = config["MSAA"].ValueAsDefault<int>(0);
	if (msaa > 1 && msaa <= 8 && (msaa & (msaa - 1)) == 0)
	{
		// Ask the driver whether it really supports this sample count for both attachment formats
		// before committing to it. Nothing checked this before, so an unsupported combination would
		// have been requested blind - and the result of that is confusing rather than obvious: the
		// multisampled scene never appears while the 2D layers still draw, so only the text and UI
		// are visible.
		//
		// Done here, in the constructor, because the pipelines are built in Init() with
		// m_samples baked into their multisample state; choosing a different value later would leave
		// the pipelines disagreeing with their render targets.
		VkPhysicalDevice physical = context.PhysicalDevice();
		VkImageFormatProperties colorProps = {};
		VkImageFormatProperties depthProps = {};
		const bool colorOk = physical != VK_NULL_HANDLE &&
			vkGetPhysicalDeviceImageFormatProperties(physical, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &colorProps) == VK_SUCCESS;
		const bool depthOk = physical != VK_NULL_HANDLE &&
			vkGetPhysicalDeviceImageFormatProperties(physical, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &depthProps) == VK_SUCCESS;
		const VkSampleCountFlagBits wanted = (VkSampleCountFlagBits)msaa;
		const bool colorSupports = colorOk && (colorProps.sampleCounts & wanted) != 0;
		const bool depthSupports = depthOk && (depthProps.sampleCounts & wanted) != 0;

		InfoLog("Vulkan: MSAA %dx requested - colour format supported: %s, depth format supported: %s.",
			msaa, colorSupports ? "yes" : "NO", depthSupports ? "yes" : "NO");

		if (colorSupports && depthSupports)
		{
			m_sampleBits = (uint32_t)msaa;
			m_samples = wanted;
		}
		else
		{
			ErrorLog("Vulkan: %dx MSAA is not supported for both attachments; using 1x.", msaa);
		}
	}
}

CVulkan3D::~CVulkan3D()
{
	/*
	 * Progress is logged at each stage, not just at the end.
	 *
	 * A crash in here left the log's last line as "3D targets destroyed" with no way to tell which of
	 * the six steps that follow was responsible - and heap corruption is reported when the heap is next
	 * touched, not where the bad write happened, so the crash point and the cause are already known to
	 * be able to differ. Naming each stage turns the next occurrence into a one-line answer instead of
	 * a bisection.
	 */
	VkDevice device = m_context.Device();
	if (device)
		vkDeviceWaitIdle(device);
	m_textures.Shutdown();
	DestroyTargets();
	InfoLog("Vulkan3D: teardown - textures and targets released.");
	m_context.Memory().DestroyBuffer(m_rom);
	m_context.Memory().DestroyBuffer(m_materials);
	m_context.Memory().DestroyBuffer(m_los);
	InfoLog("Vulkan3D: teardown - buffers released.");
	if (!device)
		return;
	if (m_opaque)
		vkDestroyPipeline(device, m_opaque, nullptr);
	if (m_translucent)
		vkDestroyPipeline(device, m_translucent, nullptr);
	if (m_overlayOpaque)
		vkDestroyPipeline(device, m_overlayOpaque, nullptr);
	if (m_overlayTranslucent)
		vkDestroyPipeline(device, m_overlayTranslucent, nullptr);
	if (m_layout)
		vkDestroyPipelineLayout(device, m_layout, nullptr);
	if (m_fog)
		vkDestroyPipeline(device, m_fog, nullptr);
	if (m_fogLayout)
		vkDestroyPipelineLayout(device, m_fogLayout, nullptr);
	if (m_fogVert)
		vkDestroyShaderModule(device, m_fogVert, nullptr);
	if (m_fogFrag)
		vkDestroyShaderModule(device, m_fogFrag, nullptr);
	if (m_vert)
		vkDestroyShaderModule(device, m_vert, nullptr);
	if (m_frag)
		vkDestroyShaderModule(device, m_frag, nullptr);
	InfoLog("Vulkan3D: teardown - pipelines and shaders released.");
}

void CVulkan3D::AttachMemory(const uint32_t* cullingRAMLoPtr, const uint32_t* cullingRAMHiPtr, const uint32_t* polyRAMPtr, const uint32_t* vromPtr, const uint16_t* textureRAMPtr)
{
	m_scene.AttachMemory(cullingRAMLoPtr, cullingRAMHiPtr, polyRAMPtr, vromPtr);
	m_textures.AttachMemory(textureRAMPtr);
}

void CVulkan3D::SetStepping(Game::Stepping stepping)
{
    int step = 0x10;
    switch (stepping)
    {
    case Game::Stepping::MODEL3_1_5: step = 0x15; break;
    case Game::Stepping::MODEL3_2_0: step = 0x20; break;
    case Game::Stepping::MODEL3_2_1: step = 0x21; break;
    default: break;
    }
    m_scene.SetStepping(step);
}

void CVulkan3D::SetSunClamp(bool enable) { m_sunClamp = enable; }
void CVulkan3D::SetBlockCulling(bool enable) { m_blockCulling = enable; }
float CVulkan3D::GetLosValue(int layer)
{
	if (layer < 0 || layer > 3)
		return 0;
	std::lock_guard<std::mutex> guard(m_losMutex);
	return m_losValue[layer];
}

void CVulkan3D::UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height)
{
	m_textures.UploadTextures(level, x, y, width, height);
}

Result CVulkan3D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned)
{
	m_xOff = xOffset;
	m_yOff = yOffset;
	m_xRes = xRes;
	m_yRes = yRes;
	m_totalX = totalXRes;
	m_totalY = totalYRes;
	m_wide = m_config["WideScreen"].ValueAs<bool>();
	m_scene.SetViewport(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes, m_wide);
	m_targetGeneration = ~0ull;

	if (m_layout)
		return Result::OKAY;
	if (!m_textures.Init(m_context, m_diagnostics))
	{
		ErrorLog("Vulkan3D: texture cache init failed.");
		return Result::FAIL;
	}

	VkDeviceSize align = m_context.StorageAlignment();
	m_materialSlice = AlignUp((uint32_t)(kMaxMaterials * sizeof(R3DMaterial)), (uint32_t)align);
	if (!m_context.Memory().CreateBuffer(m_materialSlice * kMaxFramesInFlight,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_materials))
	{
		ErrorLog("Vulkan3D: material buffer allocation failed.");
		return Result::FAIL;
	}
	/*
	 * Sized from kMaxRomVerts rather than a round number.
	 *
	 * This buffer receives the geometry cache's accumulated ROM vertices, and the cache is allowed to
	 * grow to kMaxRomVerts before it flushes itself. A flat 64 MB held only 1048576 of those vertices
	 * at 64 bytes each - a third short of what the cache could hand over - so once a level had been
	 * played for a couple of minutes the buffer filled, the upload was rejected, and every mesh beyond
	 * the cut silently stopped being drawn. Because the missing geometry was simply absent rather than
	 * corrupted, it presented as large black areas and looked like a texture fault.
	 *
	 * Deriving the size from the same constant the cache uses is what makes the two limits impossible
	 * to disagree.
	 */
	const VkDeviceSize romBufferBytes = (VkDeviceSize)kMaxRomVerts * sizeof(R3DVertex);
	if (!m_context.Memory().CreateBuffer(romBufferBytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_rom))
	{
		ErrorLog("Vulkan3D: ROM vertex buffer allocation failed (%llu MB for %u vertices).",
			(unsigned long long)(romBufferBytes / (1024 * 1024)), (unsigned)kMaxRomVerts);
		return Result::FAIL;
	}
	if (!m_context.Memory().CreateBuffer(sizeof(float) * 4 * kMaxFramesInFlight,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_los))
	{
		ErrorLog("Vulkan3D: line-of-sight buffer allocation failed.");
		return Result::FAIL;
	}

	m_textures.WriteDynamicBuffers(m_context.Ring().Buffer(), sizeof(ViewportUBO), m_materials.buffer, kMaxMaterials * sizeof(R3DMaterial));

	VkDevice device = m_context.Device();
	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = real3d_vert_spv_size;
	moduleInfo.pCode = real3d_vert_spv;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &m_vert) != VK_SUCCESS)
	{
		ErrorLog("Vulkan3D: vertex shader failed to load.");
		return Result::FAIL;
	}
	moduleInfo.codeSize = real3d_frag_spv_size;
	moduleInfo.pCode = real3d_frag_spv;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &m_frag) != VK_SUCCESS)
	{
		ErrorLog("Vulkan3D: fragment shader failed to load.");
		return Result::FAIL;
	}

	VkPushConstantRange range = {};
	range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	range.size = sizeof(R3DPush);
	VkDescriptorSetLayout setLayout = m_textures.Layout();
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &setLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &range;
	if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
	{
		ErrorLog("Vulkan3D: pipeline layout creation failed.");
		return Result::FAIL;
	}
	if (!CreatePipeline(false, false) || !CreatePipeline(true, false) ||
	    !CreatePipeline(false, true) || !CreatePipeline(true, true))
	{
		ErrorLog("Vulkan3D: graphics pipeline creation failed.");
		return Result::FAIL;
	}
	if (!CreateFogPipeline())
	{
		ErrorLog("Vulkan3D: fog pipeline creation failed.");
		return Result::FAIL;
	}
	InfoLog("Vulkan3D: renderer ready.");
	return Result::OKAY;
}

bool CVulkan3D::CreatePipeline(bool translucent, bool overlay)
{
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = m_vert;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = m_frag;
	stages[1].pName = "main";

	VkVertexInputBindingDescription binding = {};
	binding.binding = 0;
	binding.stride = sizeof(R3DVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription attrs[8] = {};
	attrs[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(R3DVertex, pos) };
	attrs[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(R3DVertex, normal) };
	attrs[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(R3DVertex, uv) };
	attrs[3] = { 3, 0, VK_FORMAT_R32_UINT, offsetof(R3DVertex, colour) };
	attrs[4] = { 4, 0, VK_FORMAT_R32_UINT, offsetof(R3DVertex, materialFlags) };
	attrs[5] = { 5, 0, VK_FORMAT_R32_SFLOAT, offsetof(R3DVertex, fixedShade) };
	attrs[6] = { 6, 0, VK_FORMAT_R32_SFLOAT, offsetof(R3DVertex, textureNP) };
	attrs[7] = { 7, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(R3DVertex, faceNormal) };
	VkPipelineVertexInputStateCreateInfo vertex = {};
	vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex.vertexBindingDescriptionCount = 1;
	vertex.pVertexBindingDescriptions = &binding;
	vertex.vertexAttributeDescriptionCount = 8;
	vertex.pVertexAttributeDescriptions = attrs;

	VkPipelineInputAssemblyStateCreateInfo assembly = {};
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
	// Overlay (decal) polygons share exactly the same 3D coordinates as the base surface
	// they sit on, so a plain GREATER compare would let the base win the tie. nudge the
	// overlay slightly towards the viewer so the decal always passes the depth test.
	raster.depthBiasEnable = overlay ? VK_TRUE : VK_FALSE;
	raster.depthBiasConstantFactor = overlay ? 1.0f : 0.0f;
	raster.depthBiasSlopeFactor = overlay ? 1.0f : 0.0f;
	raster.depthBiasClamp = 0.0f;
	VkPipelineMultisampleStateCreateInfo multi = {};
	multi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multi.rasterizationSamples = m_samples;
	VkPipelineDepthStencilStateCreateInfo depth = {};
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthTestEnable = VK_TRUE;
	// New3D draws both transparency layers with depth writes enabled (SetRenderStates sets
	// glDepthMask(GL_TRUE) and only switches the compare to GREATER). Without the write, polygons
	// later in the same layer paint over nearer ones regardless of depth; on the Carnotaurus that
	// let the dark far side of the mouth cover the snout as a black patch.
	depth.depthWriteEnable = VK_TRUE;
	depth.depthCompareOp = overlay ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_GREATER;
	VkPipelineColorBlendAttachmentState blendAtt = {};
	blendAtt.blendEnable = translucent ? VK_TRUE : VK_FALSE;
	blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAtt.colorBlendOp = VK_BLEND_OP_ADD;
	blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
	blendAtt.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo blend = {};
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blendAtt;
	VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic = {};
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynamics;

	VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
	VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
	VkPipelineRenderingCreateInfo rendering = {};
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &colorFormat;
	rendering.depthAttachmentFormat = depthFormat;

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.pNext = &rendering;
	info.stageCount = 2;
	info.pStages = stages;
	info.pVertexInputState = &vertex;
	info.pInputAssemblyState = &assembly;
	info.pViewportState = &viewport;
	info.pRasterizationState = &raster;
	info.pMultisampleState = &multi;
	info.pDepthStencilState = &depth;
	info.pColorBlendState = &blend;
	info.pDynamicState = &dynamic;
	info.layout = m_layout;
	VkPipeline* dest;
	if (overlay)
		dest = translucent ? &m_overlayTranslucent : &m_overlayOpaque;
	else
		dest = translucent ? &m_translucent : &m_opaque;
	VkResult pipeResult = vkCreateGraphicsPipelines(m_context.Device(), VK_NULL_HANDLE, 1, &info, nullptr, dest);
	if (pipeResult != VK_SUCCESS)
		ErrorLog("Vulkan3D: vkCreateGraphicsPipelines (%s, %s) failed (%d).", translucent ? "translucent" : "opaque", overlay ? "overlay" : "base", (int)pipeResult);
	return pipeResult == VK_SUCCESS;
}

// Ambient / scroll fog background pass. This mirrors New3D, which draws a
// full-viewport quad into the opaque layer before any 3D geometry. Depth test
// and writes are off, so the quad only seeds the background colour; the 3D
// geometry is then drawn straight over it.
bool CVulkan3D::CreateFogPipeline()
{
	VkDevice device = m_context.Device();

	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = fog_vert_spv_size;
	moduleInfo.pCode = fog_vert_spv;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &m_fogVert) != VK_SUCCESS)
		return false;
	moduleInfo.codeSize = fog_frag_spv_size;
	moduleInfo.pCode = fog_frag_spv;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &m_fogFrag) != VK_SUCCESS)
		return false;

	VkPushConstantRange range = {};
	range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	range.size = sizeof(R3DFogPush);
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &range;
	if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_fogLayout) != VK_SUCCESS)
		return false;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = m_fogVert;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = m_fogFrag;
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
	multi.rasterizationSamples = m_samples;
	VkPipelineDepthStencilStateCreateInfo depth = {};
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthTestEnable = VK_FALSE;
	depth.depthWriteEnable = VK_FALSE;
	depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	VkPipelineColorBlendAttachmentState blendAtt = {};
	blendAtt.blendEnable = VK_FALSE;
	blendAtt.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo blend = {};
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blendAtt;
	VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic = {};
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynamics;

	VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
	VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
	VkPipelineRenderingCreateInfo rendering = {};
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &colorFormat;
	rendering.depthAttachmentFormat = depthFormat;

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.pNext = &rendering;
	info.stageCount = 2;
	info.pStages = stages;
	info.pVertexInputState = &vertex;
	info.pInputAssemblyState = &assembly;
	info.pViewportState = &viewport;
	info.pRasterizationState = &raster;
	info.pMultisampleState = &multi;
	info.pDepthStencilState = &depth;
	info.pColorBlendState = &blend;
	info.pDynamicState = &dynamic;
	info.layout = m_fogLayout;
	VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &m_fog);
	if (result != VK_SUCCESS)
		ErrorLog("Vulkan3D: vkCreateGraphicsPipelines (fog) failed (%d).", (int)result);
	return result == VK_SUCCESS;
}

// Selects the fog parameters and records the background quad(s), mirroring
// New3D's DrawScrollFog() / DrawAmbientFog().
//
// Scroll fog wins when any viewport in the lowest priority layer that has one
// sets it: the colour and scroll amount come from that viewport, while the
// attenuation / ambient / spotlight-on-fog parameters come from any viewport
// sharing the same fog colour that actually enables fogging. Otherwise the
// lowest fogAmbient in the first priority layer containing viewports darkens
// the background with a black quad.
void CVulkan3D::DrawFog(VkCommandBuffer cmd)
{
	const New3D::Viewport* scrollNode = nullptr;

	// Highest scroll fog value, starting from the lowest priority layer.
	for (int layer = 0; layer < 4 && scrollNode == nullptr; layer++)
	{
		float best = 0.0f;
		for (const auto& node : m_scene.Nodes())
		{
			const New3D::Viewport& vp = node.viewport;
			if (vp.priority != layer || node.models.empty())
				continue;
			if (vp.scrollFog > best)
			{
				best = vp.scrollFog;
				scrollNode = &vp;
			}
		}
	}

	// Ambient fog: lowest fogAmbient in the first priority layer with viewports.
	const New3D::Viewport* ambientNode = nullptr;
	if (scrollNode == nullptr)
	{
		for (int layer = 0; layer < 4; layer++)
		{
			bool hasPriority = false;
			const New3D::Viewport* best = nullptr;
			float bestAmbient = 1.0f;
			for (const auto& node : m_scene.Nodes())
			{
				const New3D::Viewport& vp = node.viewport;
				if (vp.priority != layer || node.models.empty())
					continue;
				hasPriority = true;
				if (vp.scrollFog > 0.0f)
					continue;
				// Without a fog density or start the effect is disabled (Lost World)
				if (vp.fogDensity <= 0.0f && vp.fogStart <= 0.0f)
					continue;
				if (vp.fogAmbient < bestAmbient)
				{
					bestAmbient = vp.fogAmbient;
					best = &vp;
				}
			}
			if (best != nullptr)
				ambientNode = best;
			if (ambientNode != nullptr || hasPriority)
				break;
		}
	}

	if (scrollNode == nullptr && ambientNode == nullptr)
		return;

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fog);

	auto drawFogQuad = [&](const New3D::Viewport& vp, const float rgba[4], float attenuation, float ambient,
		const float spotFogColor[3], const float spotColour[4])
	{
		int vx = vp.x;
		int vy = (int)m_color.height - (vp.y + vp.height);
		int vw = vp.width;
		int vh = vp.height;
		if (vw <= 0 || vh <= 0)
			return;
		if (vx < 0) { vw += vx; vx = 0; }
		if (vy < 0) { vh += vy; vy = 0; }
		if (vx + vw > (int)m_color.width)
			vw = (int)m_color.width - vx;
		if (vy + vh > (int)m_color.height)
			vh = (int)m_color.height - vy;
		if (vw <= 0 || vh <= 0)
			return;

		R3DFogPush push = {};
		push.fogColour[0] = rgba[0];
		push.fogColour[1] = rgba[1];
		push.fogColour[2] = rgba[2];
		push.fogColour[3] = rgba[3];
		push.fogAmbientAtt[0] = ambient;
		push.fogAmbientAtt[1] = attenuation;
		push.spotEllipse[0] = spotColour[0];
		push.spotEllipse[1] = (float)m_color.height - spotColour[1];
		push.spotEllipse[2] = spotColour[2];
		push.spotEllipse[3] = spotColour[3];
		push.spotFogColor[0] = spotFogColor[0];
		push.spotFogColor[1] = spotFogColor[1];
		push.spotFogColor[2] = spotFogColor[2];
		vkCmdPushConstants(cmd, m_fogLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

		VkViewport view = {};
		view.x = (float)vx;
		view.y = (float)vy;
		view.width = (float)vw;
		view.height = (float)vh;
		view.maxDepth = 1.0f;
		VkRect2D scissor = { { vx, vy }, { (uint32_t)vw, (uint32_t)vh } };
		vkCmdSetViewport(cmd, 0, 1, &view);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdDraw(cmd, 4, 1, 0, 0);
	};

	if (scrollNode != nullptr)
	{
		// Find a viewport with the same fog colour that actually enables fogging.
		// That is where the attenuation, ambient and spotlight parameters come from.
		const New3D::Viewport* source = scrollNode;
		bool found = false;
		for (const auto& node : m_scene.Nodes())
		{
			const New3D::Viewport& vp = node.viewport;
			if (vp.fogColour[0] != scrollNode->fogColour[0] ||
				vp.fogColour[1] != scrollNode->fogColour[1] ||
				vp.fogColour[2] != scrollNode->fogColour[2])
				continue;

			bool spotOnFog = false;
			for (float c : vp.spotFogColor)
				if (c > 0.0f) { spotOnFog = true; break; }

			bool fog = vp.fogDensity > 0.0f || vp.fogStart > 0.0f || vp.scrollAtt > 0.0f;
			bool spotlightFog = vp.fogAttenuation > 0.0f && spotOnFog;

			if (fog || spotlightFog)
			{
				source = &vp;
				found = true;
				break;
			}
		}
		if (!found)
			return;

		// If regular fog is enabled the hardware seems to only apply that and
		// skips the spotlight on fog (ECA).
		float attenuation = source->fogAttenuation;
		bool regularFog = source->fogDensity > 0.0f || source->fogStart > 0.0f || source->scrollAtt > 0.0f;
		bool spotOnFog = false;
		for (float c : source->spotFogColor)
			if (c > 0.0f) { spotOnFog = true; break; }
		if (regularFog && spotOnFog)
			attenuation = 0.0f;

		float rgba[4] = { scrollNode->fogColour[0], scrollNode->fogColour[1], scrollNode->fogColour[2], scrollNode->scrollFog };
		drawFogQuad(*scrollNode, rgba, attenuation, source->fogAmbient, source->spotFogColor, source->spotEllipse);
	}
	else
	{
		// Ambient fog is a black quad whose alpha dims whatever is behind it.
		float rgba[4] = { 0.0f, 0.0f, 0.0f, 1.0f - ambientNode->fogAmbient };
		drawFogQuad(*ambientNode, rgba, 0.0f, 1.0f, ambientNode->spotFogColor, ambientNode->spotEllipse);
	}
}

void CVulkan3D::DestroyTargets()
{
	/*
	 * The handles are logged on the way out because that is the only way to tell afterwards whether a
	 * stale reference survived the rebuild.
	 *
	 * Vulkan handles are pointers and a freed one is very often handed straight back to the next
	 * creation. Anything that caches a view and decides whether to rewrite a descriptor by comparing
	 * handles therefore cannot tell "the same image" from "a different image that landed on the same
	 * address" - and the failure shows up much later, as the compositor sampling something that was
	 * destroyed. Naming the values is what makes that visible instead of theoretical.
	 */
	InfoLog("Vulkan: 3D targets destroyed (colour view %p, depth view %p, resolve view %p, generation %llu).",
		(void*)m_color.view, (void*)m_depth.view, (void*)m_colorResolve.view,
		(unsigned long long)m_context.SwapchainGeneration());

	m_context.DestroyView(m_color.view);
	m_color.view = VK_NULL_HANDLE;
	m_context.Memory().DestroyImage(m_color);
	m_context.DestroyView(m_depth.view);
	m_depth.view = VK_NULL_HANDLE;
	m_context.Memory().DestroyImage(m_depth);
	m_context.DestroyView(m_colorResolve.view);
	m_colorResolve.view = VK_NULL_HANDLE;
	m_context.Memory().DestroyImage(m_colorResolve);
	m_colorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_resolveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

bool CVulkan3D::EnsureTargets()
{
	VkExtent2D extent = m_context.Extent();
	if (extent.width == 0 || extent.height == 0)
		return false;
	if (m_color.image && m_color.width == extent.width && m_color.height == extent.height && m_targetGeneration == m_context.SwapchainGeneration())
		return true;
	if (m_context.Device())
		vkDeviceWaitIdle(m_context.Device());
	DestroyTargets();

	// Hardware MSAA: render multisampled and resolve into a single-sampled image that the 2D layer
	// can sample. MSAA smooths polygon coverage only, so texel content (car decals, small sprites)
	// keeps its full sharpness. The sample count was validated against the driver in the constructor.
	const VkImageUsageFlags colorUsage = m_samples > VK_SAMPLE_COUNT_1_BIT
		? (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		: (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	if (!m_context.Memory().CreateImage(extent.width, extent.height, 1, VK_FORMAT_R8G8B8A8_UNORM,
			colorUsage, m_color, (uint32_t)m_samples))
		return false;
	m_color.view = m_context.CreateView(m_color.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	// TRANSFER_SRC is only legal on a single-sampled image, so the depth image must not ask for it
	// when multisampling: only TRANSFER_DST is permitted on a multisampled image. The flag exists so
	// the line-of-sight readback can copy depth out, and that readback is skipped under MSAA anyway
	// (see CopyLos), so dropping it here costs nothing. Requesting it regardless is invalid and a
	// driver is within its rights to refuse to render into the attachment at all, which shows up as
	// the 3D scene vanishing while the 2D layers still draw.
	const VkImageUsageFlags depthUsage = (m_samples > VK_SAMPLE_COUNT_1_BIT)
		? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		: (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	if (!m_context.Memory().CreateImage(extent.width, extent.height, 1, VK_FORMAT_D32_SFLOAT,
			depthUsage, m_depth, (uint32_t)m_samples))
		return false;
	m_depth.view = m_context.CreateView(m_depth.image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1);
	if (m_samples > VK_SAMPLE_COUNT_1_BIT)
	{
		if (!m_context.Memory().CreateImage(extent.width, extent.height, 1, VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_colorResolve))
			return false;
		m_colorResolve.view = m_context.CreateView(m_colorResolve.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	}
	m_colorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_resolveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_targetGeneration = m_context.SwapchainGeneration();
	m_output.SetSceneColor(m_samples > VK_SAMPLE_COUNT_1_BIT ? m_colorResolve.view : m_color.view);
	InfoLog("Vulkan: 3D target %ux%u, %ux MSAA%s.", extent.width, extent.height, (unsigned)m_samples,
		m_samples > VK_SAMPLE_COUNT_1_BIT ? " (resolved before compositing)" : "");
	InfoLog("Vulkan: 3D targets rebuilt (colour view %p, depth view %p, resolve view %p -> compositor, generation %llu, ROM verts %zu, RAM ready %d).",
		(void*)m_color.view, (void*)m_depth.view, (void*)m_colorResolve.view,
		(unsigned long long)m_targetGeneration, m_romGpuVerts, m_ramReady ? 1 : 0);
	return m_color.view && m_depth.view;
}

R3DVertex CVulkan3D::ConvertVertex(const New3D::FVertex& src) const
{
	R3DVertex dst = {};
	dst.pos[0] = src.base.pos[0];
	dst.pos[1] = src.base.pos[1];
	dst.pos[2] = src.base.pos[2];
	dst.normal[0] = src.base.normal[0];
	dst.normal[1] = src.base.normal[1];
	dst.normal[2] = src.base.normal[2];
	dst.uv[0] = src.base.texcoords[0];
	dst.uv[1] = src.base.texcoords[1];
	dst.colour = (uint32_t)src.faceColour[0] | ((uint32_t)src.faceColour[1] << 8) | ((uint32_t)src.faceColour[2] << 16) | ((uint32_t)src.faceColour[3] << 24);
	dst.fixedShade = src.base.fixedShade;
	dst.textureNP = src.textureNP;
	dst.faceNormal[0] = src.faceNormal[0];
	dst.faceNormal[1] = src.faceNormal[1];
	dst.faceNormal[2] = src.faceNormal[2];
	return dst;
}

void CVulkan3D::BeginFrame(void)
{
	uint32_t frame = m_context.FrameIndex();
	if (m_losReady[frame] && m_los.mapped)
	{
		float values[4];
		std::memcpy(values, static_cast<uint8_t*>(m_los.mapped) + frame * sizeof(values), sizeof(values));
		std::lock_guard<std::mutex> guard(m_losMutex);
		for (int i = 0; i < 4; i++)
		{
			float depth = values[i];
			float z = depth / kNearPlane;
			auto* bytes = reinterpret_cast<unsigned char*>(&z);
			if (depth == 0.0f)
				bytes[0] |= 1;
			else
				bytes[0] &= 0xFE;
			m_losValue[i] = z;
		}
	}
	m_frameSerial++;
	m_materialCount = 0;
	m_ramReady = false;
	m_drew = false;
}

void CVulkan3D::EndFrame(void)
{
	if (!m_context.IsRecording() || !m_color.image)
		return;
	// Hand the scene colour over to the compositor.
	//
	// Under MSAA that has to be the resolved image, not the multisampled one. A multisampled image
	// cannot be read through a plain sampler2D (it needs texture2DMS), so sampling it anyway is
	// undefined behaviour - and the visible result was the 3D scene collapsing into a flat wash of
	// colour while the 2D layers drew normally. EnsureTargets set this correctly, but it returns
	// early once the targets exist, so it only ever ran on the frame the targets were created.
	// Setting it here too, where it runs every frame, is what keeps the compositor pointed at the
	// resolved image.
	if (m_samples > VK_SAMPLE_COUNT_1_BIT)
	{
		if (m_resolveLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			CmdTransition(m_context.Cmd(), m_colorResolve.image, m_resolveLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			m_resolveLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		m_output.SetSceneColor(m_colorResolve.view);
	}
	else
	{
		if (m_colorLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			CmdTransition(m_context.Cmd(), m_color.image, m_colorLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			m_colorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		m_output.SetSceneColor(m_color.view);
	}
}

void CVulkan3D::CollectTextures()
{
	m_textures.SetFrameSerial(m_frameSerial);
}

void CVulkan3D::UploadGeometry(VkCommandBuffer cmd)
{
	if (m_scene.RomReset())
	{
		m_romGpuVerts = 0;
		m_scene.ClearRomReset();
	}
	auto& rom = m_scene.RomVerts();
	if (rom.size() > m_romGpuVerts)
	{
		size_t count = rom.size() - m_romGpuVerts;
		VkDeviceSize bytes = count * sizeof(R3DVertex);
		VkDeviceSize offset = 0;
		void* ptr = nullptr;
		if ((m_romGpuVerts + count) * sizeof(R3DVertex) <= m_rom.size && m_context.Ring().Alloc(bytes, 16, offset, ptr))
		{
			R3DVertex* dst = static_cast<R3DVertex*>(ptr);
			for (size_t i = 0; i < count; i++)
				dst[i] = ConvertVertex(rom[m_romGpuVerts + i]);
			VkBufferCopy copy = {};
			copy.srcOffset = offset;
			copy.dstOffset = m_romGpuVerts * sizeof(R3DVertex);
			copy.size = bytes;
			vkCmdCopyBuffer(cmd, m_context.Ring().Buffer(), m_rom.buffer, 1, &copy);
			VkBufferMemoryBarrier2 barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.buffer = m_rom.buffer;
			barrier.offset = copy.dstOffset;
			barrier.size = bytes;
			VkDependencyInfo dep = {};
			dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			dep.bufferMemoryBarrierCount = 1;
			dep.pBufferMemoryBarriers = &barrier;
			CmdPipelineBarrier2(cmd, &dep);
			m_romGpuVerts += count;
		}
		else
		{
			/*
			 * The ROM vertex buffer could not take the new vertices, so they are dropped and every mesh
			 * that lives beyond them is skipped below. That is silently missing geometry - whole models
			 * vanishing part way through a level - and it is worth a line in the log because the visible
			 * symptom (large black or untextured areas) gives no hint that geometry is the cause.
			 *
			 * The two numbers that matter are how many vertices fit and how many the CPU side was
			 * prepared to hand over; those two limits are what have to agree. This repeats every frame
			 * until the buffer is flushed, so it is throttled to the first occurrence to keep the log
			 * readable while still making the moment visible.
			 */
			if (m_romFullReports++ == 0)
			{
				ErrorLog("Vulkan: ROM vertex buffer is FULL - %zu vertices fit in the %llu MB buffer "
					"(%zu bytes each). %zu new vertex/vertices dropped; every mesh beyond them will not be "
					"drawn, which shows up as missing geometry rather than as a texture fault.",
					m_rom.size / sizeof(R3DVertex), (unsigned long long)(m_rom.size / (1024 * 1024)),
					sizeof(R3DVertex), count);
			}
		}
	}

	// Report the fill level as it climbs. The ceiling is only reachable after a long stretch of play, and
	// the interesting question is not whether it is full but how close to full it normally gets - which
	// is also what decides the right size for the buffer.
	m_romVertsSeen = std::max(m_romVertsSeen, m_romGpuVerts);
	if (m_diagnostics && ++m_romReportFrames % 600 == 0 && m_romVertsSeen > 0)
	{
		InfoLog("Vulkan: ROM geometry - %zu of %zu vertices used so far (%.1f%% of the buffer's capacity).",
			m_romVertsSeen, m_rom.size / sizeof(R3DVertex),
			100.0 * (double)m_romVertsSeen / (double)(m_rom.size / sizeof(R3DVertex)));
	}

	auto& ram = m_scene.RamVerts();
	m_ramReady = false;
	if (!ram.empty())
	{
		VkDeviceSize bytes = ram.size() * sizeof(R3DVertex);
		void* ptr = nullptr;
		if (m_context.Ring().Alloc(bytes, 16, m_ramOffset, ptr))
		{
			R3DVertex* dst = static_cast<R3DVertex*>(ptr);
			for (size_t i = 0; i < ram.size(); i++)
				dst[i] = ConvertVertex(ram[i]);
			m_ramReady = true;
		}
	}
}

void CVulkan3D::FillMaterial(R3DMaterial& material, const New3D::Mesh& mesh, const New3D::Model& model)
{
	material = {};
	material.shininess = mesh.shininess;
	material.specularValue = mesh.specularValue;
	material.fogIntensity = mesh.fogIntensity;
	material.microMinLOD = mesh.microTextureMinLOD;
	material.texX = mesh.x + model.textureOffsetX;
	material.texY = mesh.y + model.textureOffsetY;
	material.texWidth = mesh.width;
	material.texHeight = mesh.height;
	material.texFormat = mesh.format;
	material.texturePage = (mesh.page ^ model.page) & 1;
	material.wrapModeX = (int32_t)mesh.wrapModeU;
	material.wrapModeY = (int32_t)mesh.wrapModeV;
	material.microTextureID = mesh.microTextureID & 7;
	if (mesh.textured)
		material.flags |= kMatTextured;
	if (mesh.alphaTest)
		material.flags |= kMatAlphaTest;
	if (mesh.textureAlpha)
		material.flags |= kMatTextureAlpha;
	if (mesh.lighting)
		material.flags |= kMatLighting;
	if (mesh.fixedShading)
		material.flags |= kMatFixedShade;
	if (mesh.specular)
		material.flags |= kMatSpecular;
	if (mesh.translatorMap)
		material.flags |= kMatTranslator;
	if (mesh.inverted)
		material.flags |= kMatInverted;
	if (mesh.smoothShading)
		material.flags |= kMatSmooth;
	if (mesh.polyAlpha)
		material.flags |= kMatPolyAlpha;
	if (mesh.microTexture)
		material.flags |= kMatMicro;
}

void CVulkan3D::FillPush(R3DPush& push, const New3D::Model& model, const New3D::Viewport& vp, uint32_t materialIndex, bool discardAlpha) const
{
	std::memcpy(push.modelView, model.modelMat, sizeof(push.modelView));
	std::memcpy(push.projection, vp.projectionMatrix.currentMatrix, sizeof(push.projection));
	push.projection[1] = -push.projection[1];
	push.projection[5] = -push.projection[5];
	push.projection[9] = -push.projection[9];
	push.projection[13] = -push.projection[13];
	push.lightDir[0] = vp.lightingParams[0];
	push.lightDir[1] = vp.lightingParams[1];
	push.lightDir[2] = vp.lightingParams[2];
	push.lightDir[3] = vp.lightingParams[3];
	push.lightParams[0] = vp.lightingParams[4];
	push.lightParams[1] = m_sunClamp ? 1.0f : 0.0f;
	push.lightParams[2] = vp.intensityClamp ? 1.0f : 0.0f;
	push.lightParams[3] = (vp.step15Luminous ? 21.0f : 0.0f);
	push.fogColour[0] = vp.fogColour[0];
	push.fogColour[1] = vp.fogColour[1];
	push.fogColour[2] = vp.fogColour[2];
	push.fogColour[3] = vp.fogDensity;
	push.fogParams[0] = vp.fogStart;
	push.fogParams[1] = vp.fogAttenuation;
	push.fogParams[2] = vp.fogAmbient;
	push.fogParams[3] = model.alpha;
	push.extra[0] = vp.cota;
	push.extra[1] = model.scale == 0.0f ? 1.0f : model.scale;
	push.extra[2] = discardAlpha ? 1.0f : 0.0f;
	push.extra[3] = UintToFloatBits(materialIndex);
}

void CVulkan3D::CopyLos(VkCommandBuffer cmd, int priority)
{
	// Line-of-sight readback needs the depth buffer as a transfer source, which a multisampled image
	// cannot be: Vulkan requires a multisample image to be resolved before it can be copied, and
	// D32_SFLOAT does not support resolve at all. Attempting the copy anyway is undefined behaviour
	// and it corrupted the frame - exactly the fault that made the picture go black with MSAA on.
	//
	// So MSAA and line-of-sight readback are mutually exclusive here, and MSAA wins: it is a visible
	// improvement every frame, whereas the line-of-sight test only feeds a visibility register that
	// most scenes do not consult. GetLosValue() keeps returning its last value.
	if (m_samples > VK_SAMPLE_COUNT_1_BIT)
		return;

	const New3D::Viewport* vp = nullptr;
	for (const auto& node : m_scene.Nodes())
	{
		if (node.viewport.priority == priority && (node.viewport.losPosX || node.viewport.losPosY))
		{
			vp = &node.viewport;
			break;
		}
	}
	if (!vp || !m_los.mapped)
		return;
	int inY = 384 - vp->losPosY;
	int glX = (int)m_xOff + (int)(vp->losPosX * (m_xRes / 496.0f));
	int glY = (int)m_yOff + (int)(inY * (m_yRes / 384.0f));
	int vkY = (int)m_color.height - 1 - glY;
	if (glX < 0 || vkY < 0 || glX >= (int)m_color.width || vkY >= (int)m_color.height)
		return;

	CmdTransition(cmd, m_depth.image, m_depthLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	m_depthLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	VkBufferImageCopy copy = {};
	copy.bufferOffset = (m_context.FrameIndex() * 4 + (uint32_t)priority) * sizeof(float);
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	copy.imageSubresource.layerCount = 1;
	copy.imageOffset = { glX, vkY, 0 };
	copy.imageExtent = { 1, 1, 1 };
	vkCmdCopyImageToBuffer(cmd, m_depth.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_los.buffer, 1, &copy);
	CmdTransition(cmd, m_depth.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	m_depthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

	// The copy lands after this frame's fence. Convert 1/w depth the same way New3D does
	// once the buffer is visible; until then the previous value stands. The bit below is
	// applied when the copy is read back, using a zero depth as "no geometry".
	m_losReady[m_context.FrameIndex()] = true;
	(void)kNearPlane;
}

void CVulkan3D::DrawScene(VkCommandBuffer cmd)
{
	VkCommandBuffer command = cmd;
	auto begin = [&](bool clear)
	{
		VkImageLayout old = m_colorLayout;
		VkImageLayout next = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		if (old != next)
		{
			CmdTransition(command, m_color.image, old, next, VK_IMAGE_ASPECT_COLOR_BIT);
			m_colorLayout = next;
		}
		if (m_depthLayout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		{
			CmdTransition(command, m_depth.image, m_depthLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
			m_depthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		}

		VkRenderingAttachmentInfo color = {};
		color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color.imageView = m_color.view;
		color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.clearValue.color.float32[3] = 0;
		if (m_samples > VK_SAMPLE_COUNT_1_BIT)
		{
			// Resolve the multisampled scene into m_colorResolve when this pass ends. resolveMode
			// AVERAGE is the standard box filter. The resolve target has to be in the resolve layout
			// for as long as the pass is open, so it is transitioned here rather than per pass.
			if (m_resolveLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			{
				CmdTransition(command, m_colorResolve.image, m_resolveLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
				m_resolveLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
			color.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
			color.resolveImageView = m_colorResolve.view;
			color.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		VkRenderingAttachmentInfo depth = {};
		depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth.imageView = m_depth.view;
		depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depth.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth.clearValue.depthStencil.depth = 0;
		VkRenderingInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		info.renderArea.extent = { m_color.width, m_color.height };
		info.layerCount = 1;
		info.colorAttachmentCount = 1;
		info.pColorAttachments = &color;
		info.pDepthAttachment = &depth;
		CmdBeginRendering(command, &info);
	};
	// Closes the scene pass and, when multisampling, prepares the resolved image for the 2D layer to
	// sample. Every path that ends the scene rendering must go through here.
	auto finishScene = [&]()
	{
		CmdEndRendering(command);
		if (m_samples > VK_SAMPLE_COUNT_1_BIT && m_resolveLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			CmdTransition(command, m_colorResolve.image, m_resolveLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			m_resolveLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	};
	auto clearDepth = [&]()
	{
		VkClearAttachment att = {};
		att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		att.clearValue.depthStencil.depth = 0;
		VkClearRect rect = {};
		rect.rect.extent = { m_color.width, m_color.height };
		rect.layerCount = 1;
		vkCmdClearAttachments(command, 1, &att, 1, &rect);
	};
	auto drawLayer = [&](int priority, bool overlay, New3D::Layer layer, bool discardAlpha, bool translucent) -> bool
	{
		bool hasOverlay = false;
		VkPipeline pipeline;
		if (overlay)
			pipeline = translucent ? m_overlayTranslucent : m_overlayOpaque;
		else
			pipeline = translucent ? m_translucent : m_opaque;
		vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		int currentNode = -1;
		bool dynamicBound = false;
		bool bound = false;
		VkDeviceSize boundOffset = ~0ull;
		for (size_t ni = 0; ni < m_scene.Nodes().size(); ni++)
		{
			auto& node = m_scene.Nodes()[ni];
			if (node.viewport.priority != priority || node.models.empty())
				continue;
			node.viewport.sunClamp = m_sunClamp;
			int vx = node.viewport.x;
			int vy = (int)m_color.height - (node.viewport.y + node.viewport.height);
			int vw = node.viewport.width;
			int vh = node.viewport.height;
			if (vw <= 0 || vh <= 0)
				continue;
			if (vx < 0) { vw += vx; vx = 0; }
			if (vy < 0) { vh += vy; vy = 0; }
			if (vx + vw > (int)m_color.width)
				vw = (int)m_color.width - vx;
			if (vy + vh > (int)m_color.height)
				vh = (int)m_color.height - vy;
			if (vw <= 0 || vh <= 0)
			{
				// Every model in this node goes with it. Silent until now, and this is the skip that
				// takes whole objects rather than a single mesh.
				m_skipDegenerateNode++;
				continue;
			}

			if (currentNode != (int)ni)
			{
				VkDeviceSize uboOffset = 0;
				void* ptr = nullptr;
				if (!m_context.Ring().Alloc(sizeof(ViewportUBO), m_context.UniformAlignment(), uboOffset, ptr))
					continue;
				ViewportUBO ubo = {};
				ubo.spotEllipse[0] = node.viewport.spotEllipse[0];
				ubo.spotEllipse[1] = (float)m_color.height - node.viewport.spotEllipse[1];
				ubo.spotEllipse[2] = node.viewport.spotEllipse[2];
				ubo.spotEllipse[3] = node.viewport.spotEllipse[3];
				ubo.spotRange[0] = node.viewport.spotRange[0];
				ubo.spotRange[1] = node.viewport.spotRange[1];
				ubo.spotColor[0] = node.viewport.spotColor[0];
				ubo.spotColor[1] = node.viewport.spotColor[1];
				ubo.spotColor[2] = node.viewport.spotColor[2];
				ubo.spotFogColor[0] = node.viewport.spotFogColor[0];
				ubo.spotFogColor[1] = node.viewport.spotFogColor[1];
				ubo.spotFogColor[2] = node.viewport.spotFogColor[2];
				std::memcpy(ptr, &ubo, sizeof(ubo));
				uint32_t offsets[2] = { (uint32_t)uboOffset, (uint32_t)(m_context.FrameIndex() * m_materialSlice) };
				VkDescriptorSet set = m_textures.Set();
				vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &set, 2, offsets);
				VkViewport view = {};
				view.x = (float)vx;
				view.y = (float)vy;
				view.width = (float)vw;
				view.height = (float)vh;
				view.maxDepth = 1;
				VkRect2D scissor = { { vx, vy }, { (uint32_t)vw, (uint32_t)vh } };
				vkCmdSetViewport(command, 0, 1, &view);
				vkCmdSetScissor(command, 0, 1, &scissor);
				currentNode = (int)ni;
			}

			for (auto& model : node.models)
			{
				if (!model.meshes)
					continue;
				for (auto& mesh : *model.meshes)
				{
					if (mesh.highPriority)
						hasOverlay = true;
					if (!mesh.Render(layer, model.alpha) || mesh.highPriority != overlay || mesh.vertexCount <= 0)
						continue;
					bool rom = !model.dynamic;
					if (rom && (size_t)(mesh.vboOffset + mesh.vertexCount) > m_romGpuVerts)
					{
						m_skipRomBeyond++;
						continue;
					}
					if (!rom && !m_ramReady)
					{
						// The RAM geometry for this frame never reached the ring, so every dynamic model
						// is dropped - characters and moving scenery, which is exactly what a player
						// notices first. Previously silent.
						m_skipRamNotReady++;
						continue;
					}
					if (m_materialCount >= kMaxMaterials)
						return hasOverlay;
					R3DMaterial material;
					FillMaterial(material, mesh, model);
					// Ask how much of the region this draw is about to sample ever actually reached the
					// GPU. Done here, per material, because this is the only place that knows both the
					// geometry and its texture rectangle; the result feeds the periodic report, which is
					// what turns "some things look black" into a count that either is or is not zero.
					//
					// Also the place to catch a texture reference that points somewhere the game never
					// wrote: the coordinates reported with it are what say whether the addressing is
					// wrong or the level genuinely has nothing there.
					if (m_diagnostics && (material.flags & kMatTextured) != 0)
					{
						const CVulkanTextureCache::Coverage coverage = m_textures.RegionCoverage(
							material.texturePage & 1, material.texX, material.texY,
							material.texWidth, material.texHeight);
						m_textures.NoteTexturedReference(coverage);

						// The first few wholly-unuploaded regions, with their coordinates and - the part
						// that decides which fault this is - whether the game ever wrote them.
						//
						// A count alone cannot distinguish a systematically wrong page or offset from a
						// game that samples padding, and neither can the coordinates by themselves. The
						// written/arrived comparison can: "0 of 64 written" means the game never filled
						// that area, so sampling it and getting black is faithful and the fault is
						// elsewhere; "37 of 64 written" means the data was there and this file lost it.
						if (coverage == CVulkanTextureCache::Coverage::None && m_sampleMissingRegions < 12)
						{
							m_sampleMissingRegions++;
							const unsigned tiles = m_textures.RegionTileCount(
								material.texturePage & 1, material.texX, material.texY, material.texWidth, material.texHeight);
							const unsigned written = m_textures.RegionEverWrittenTiles(
								material.texturePage & 1, material.texX, material.texY, material.texWidth, material.texHeight);
							// A rectangle that runs off the page is worth flagging on its own: the shader
							// addresses within one layer, so anything past the edge is undefined rather
							// than reaching the next page, and a game that relies on the wrap would look
							// like missing texture here and nowhere else.
							const bool overflows = (material.texX + material.texWidth > 2048) ||
								(material.texY + material.texHeight > 1024);
							InfoLog("Vulkan: unuploaded region %u - page %d, rect (%d,%d %dx%d)%s; "
								"game wrote %u of %u tile(s) in it, so this is %s.",
								m_sampleMissingRegions, material.texturePage & 1,
								material.texX, material.texY, material.texWidth, material.texHeight,
								overflows ? " WHICH RUNS PAST THE PAGE EDGE" : "",
								written, tiles,
								written == 0 ? "a region the game never filled (faithful, not a bug here)"
									: "a region that was written and did not reach the GPU (a bug here)");
						}
					}
					std::memcpy(static_cast<uint8_t*>(m_materials.mapped) + m_context.FrameIndex() * m_materialSlice + m_materialCount * sizeof(R3DMaterial), &material, sizeof(material));
					R3DPush push;
					FillPush(push, model, node.viewport, m_materialCount, discardAlpha);
					m_materialCount++;
					vkCmdPushConstants(command, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
					VkBuffer buffer = rom ? m_rom.buffer : m_context.Ring().Buffer();
					VkDeviceSize offset = rom ? 0 : m_ramOffset;
					if (!bound || dynamicBound == rom || boundOffset != offset)
					{
						vkCmdBindVertexBuffers(command, 0, 1, &buffer, &offset);
						bound = true;
						dynamicBound = !rom;
						boundOffset = offset;
					}
					vkCmdDraw(command, (uint32_t)mesh.vertexCount, 1, (uint32_t)mesh.vboOffset, 0);
				}
			}
		}
		return hasOverlay;
	};

	bool white = m_blockCulling && !m_noWhiteFlash;
	begin(true);
	if (white)
	{
		// Flash the screen white, as the arcade hardware does while it is block culling. Only the
		// game's own 4:3 viewport is blanked: the 3D target is the size of the swapchain and the
		// surrounding pillarbox area belongs to the 2D layer, so clearing the whole target would
		// turn those bars white for as long as the flash lasts (most visible on the startup text
		// screen, which is drawn before the first game frame).
		VkClearAttachment att = {};
		att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		att.colorAttachment = 0;
		att.clearValue.color.float32[0] = att.clearValue.color.float32[1] = att.clearValue.color.float32[2] = att.clearValue.color.float32[3] = 1;
		VkClearRect rect = {};
		// m_yOff is measured from the bottom, as in the OpenGL convention the renderers use.
		const int flashX = (int)m_xOff;
		const int flashY = (int)m_color.height - (int)(m_yOff + m_yRes);
		rect.rect.offset = { flashX, flashY };
		rect.rect.extent = { m_xRes, m_yRes };
		// Fall back to the whole target if the viewport is degenerate or out of bounds.
		if (m_xRes == 0 || m_yRes == 0 || m_xRes > m_color.width || m_yRes > m_color.height ||
			flashX < 0 || flashX + (int)m_xRes > (int)m_color.width || flashY < 0)
			rect.rect = { { 0, 0 }, { m_color.width, m_color.height } };
		rect.layerCount = 1;
		vkCmdClearAttachments(command, 1, &att, 1, &rect);
		finishScene();
		return;
	}

	// Seed the background with the fog colour before any geometry, so that the
	// horizon and empty areas match New3D instead of staying black.
	DrawFog(command);

	bool inRenderPass = true;
	for (int priority = 0; priority <= 3; priority++)
	{
		bool any = false;
		for (const auto& node : m_scene.Nodes())
			if (node.viewport.priority == priority && !node.models.empty())
				any = true;
		if (!any)
			continue;

		for (int pass = 0; pass < 2; pass++)
		{
			bool renderOverlay = (pass == 1);
			if (!inRenderPass)
			{
				begin(false);
				inRenderPass = true;
			}
			clearDepth();
			bool hasOverlay = drawLayer(priority, renderOverlay, New3D::Layer::colour, true, false);
			CmdEndRendering(command);
			inRenderPass = false;

			if (!renderOverlay)
				CopyLos(command, priority);

			begin(false);
			inRenderPass = true;
			drawLayer(priority, renderOverlay, New3D::Layer::trans1, false, true);
			drawLayer(priority, renderOverlay, New3D::Layer::trans2, false, true);
			CmdEndRendering(command);
			inRenderPass = false;

			if (!hasOverlay)
				break;
		}
	}

	if (inRenderPass)
		finishScene();

	// Hand the resolved scene to the compositor even when no render pass is left open.
	//
	// This has to happen unconditionally. Every pass in the loop above ends with CmdEndRendering and
	// clears inRenderPass, so the loop normally finishes with no pass open and finishScene() was
	// therefore never reached - which left the resolve target in COLOR_ATTACHMENT_OPTIMAL while the
	// compositor's descriptor declared it SHADER_READ_ONLY_OPTIMAL. Sampling with a mismatched layout
	// is undefined behaviour and the scene came out black, which is exactly what MSAA users saw.
	//
	// The same reasoning applies to the non-MSAA path, where the compositor samples m_color directly,
	// so both images are handed over here.
	if (m_samples > VK_SAMPLE_COUNT_1_BIT)
	{
		if (m_resolveLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			CmdTransition(command, m_colorResolve.image, m_resolveLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			m_resolveLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}
	else if (m_colorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		CmdTransition(command, m_color.image, m_colorLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		m_colorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
}

void CVulkan3D::RenderFrame(void)
{
	if (!m_context.IsRecording())
		return;
	if (!EnsureTargets())
		return;
	VkCommandBuffer cmd = m_context.Cmd();
	m_scene.BeginFrame();
	// Re-read every frame so the in-game overlay can toggle it live. It only changes the horizontal
	// frustum, which CalcViewport derives per frame from this flag.
	m_wide = m_config["WideScreen"].ValueAs<bool>();
	m_scene.SetViewport(m_xOff, m_yOff, m_xRes, m_yRes, m_totalX, m_totalY, m_wide);
	if (!(m_blockCulling && !m_noWhiteFlash))
		m_scene.Build();
	for (auto& node : m_scene.Nodes())
		node.viewport.sunClamp = m_sunClamp;
	CollectTextures();
	m_textures.RecordSync(cmd, m_context.Ring(), m_frameSerial);
	UploadGeometry(cmd);
	DrawScene(cmd);
	m_drew = true;

	/*
	 * Report any skip counter that changed since the last report.
	 *
	 * Logged on change rather than per frame: a skip that persists is one event, not thousands, and a
	 * skip that begins after a resize is the thing worth seeing. These three are the paths that drop
	 * geometry - or an entire node - with no other trace at all, so without this a renderer that has
	 * lost half the scene reads exactly like one that has not.
	 */
	if (m_skipRomBeyond != m_skipLoggedRom || m_skipRamNotReady != m_skipLoggedRam ||
		m_skipDegenerateNode != m_skipLoggedNode)
	{
		InfoLog("Vulkan: draw skips - %llu mesh(es) past the uploaded ROM geometry, %llu dynamic "
			"mesh(es) with no ring allocation, %llu node(s) with a collapsed viewport "
			"(ROM verts %zu, RAM verts %zu, RAM ready %d).",
			(unsigned long long)m_skipRomBeyond, (unsigned long long)m_skipRamNotReady,
			(unsigned long long)m_skipDegenerateNode, m_romGpuVerts, m_scene.RamVerts().size(),
			m_ramReady ? 1 : 0);
		m_skipLoggedRom = m_skipRomBeyond;
		m_skipLoggedRam = m_skipRamNotReady;
		m_skipLoggedNode = m_skipDegenerateNode;
	}
}

} // Vulkan3D
