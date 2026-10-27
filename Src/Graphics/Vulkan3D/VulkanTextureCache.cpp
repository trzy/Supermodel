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
#include "VulkanTextureCache.h"
#include "OSD/Logger.h"
#include "Shaders/texdecode_comp.spv.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Vulkan3D {

static const int kMipX[12] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
static const int kMipY[12] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023, 1023 };

static uint32_t MipCount(int width, int height)
{
	uint32_t mips = 1;
	while ((width > 1 || height > 1) && mips < 12)
	{
		width = std::max(1, width / 2);
		height = std::max(1, height / 2);
		mips++;
	}
	return mips;
}

static void ExtractColour(int type, uint32_t value, uint8_t out[4])
{
	auto u8 = [](float c) { return (uint8_t)std::min(255, std::max(0, (int)(c * 255.0f + 0.5f))); };
	float r = 0, g = 0, b = 0, a = 0;
	switch (type)
	{
	case 0:
		r = ((value >> 10) & 0x1F) / 31.0f;
		g = ((value >> 5) & 0x1F) / 31.0f;
		b = (value & 0x1F) / 31.0f;
		a = 1.0f - (float)((value >> 15) & 1u);
		break;
	case 1:
		r = g = b = (value & 0xF) / 15.0f;
		a = ((value >> 4) & 0xF) / 15.0f;
		break;
	case 2:
		a = (value & 0xF) / 15.0f;
		r = g = b = ((value >> 4) & 0xF) / 15.0f;
		break;
	case 3:
		r = g = b = ((value >> 8) & 0xF) / 15.0f;
		a = ((value >> 12) & 0xF) / 15.0f;
		break;
	case 4:
		a = ((value >> 8) & 0xF) / 15.0f;
		r = g = b = ((value >> 12) & 0xF) / 15.0f;
		break;
	case 5:
		r = g = b = (value & 0xFF) / 255.0f;
		a = (r >= 1.0f) ? 0.0f : 1.0f;
		break;
	case 6:
		r = g = b = ((value >> 8) & 0xFF) / 255.0f;
		a = (r >= 1.0f) ? 0.0f : 1.0f;
		break;
	case 7:
		r = ((value >> 12) & 0xF) / 15.0f;
		g = ((value >> 8) & 0xF) / 15.0f;
		b = ((value >> 4) & 0xF) / 15.0f;
		a = (value & 0xF) / 15.0f;
		break;
	case 8: case 9: case 10: case 11:
	{
		int shift = (type - 8) * 4;
		r = g = b = ((value >> shift) & 0xF) / 15.0f;
		a = (r >= 1.0f) ? 0.0f : 1.0f;
		break;
	}
	default:
		break;
	}
	out[0] = u8(r);
	out[1] = u8(g);
	out[2] = u8(b);
	out[3] = u8(a);
}

uint64_t CVulkanTextureCache::MakeKey(int page, int x, int y, int width, int height, int format, bool inverted)
{
	uint64_t key = 0;
	key |= (uint64_t)(page & 1);
	key |= (uint64_t)(inverted ? 1 : 0) << 1;
	key |= (uint64_t)(format & 0x1F) << 2;
	key |= (uint64_t)(width & 0xFFFF) << 8;
	key |= (uint64_t)(height & 0xFFFF) << 24;
	key |= (uint64_t)(x & 0xFFF) << 40;
	key |= (uint64_t)(y & 0xFFF) << 52;
	return key;
}

bool CVulkanTextureCache::Init(CVulkanContext& context, bool diagnostics)
{
	m_context = &context;
	m_diagnostics = diagnostics;
	if (!CreateDescriptors() || !CreateRawPages())
		return false;
	m_gpuDecode = CreateComputePipeline();
	if (!m_gpuDecode)
		InfoLog("Vulkan: texture decode will run on the CPU (push descriptors or storage images unavailable).");

	// Slot 0 is a 1x1 white stand-in so a missing bindless index still samples.
	m_slots.resize(1);
	Slot& white = m_slots[0];
	white.live = true;
	white.width = white.height = 1;
	if (!context.Memory().CreateImage(1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, white.image))
		return false;
	white.image.view = context.CreateView(white.image.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	white.mipViews.push_back(white.image.view);
	return true;
}

void CVulkanTextureCache::Shutdown()
{
	if (!m_context || !m_context->Device())
		return;
	VkDevice device = m_context->Device();
	vkDeviceWaitIdle(device);
	for (auto& slot : m_slots)
		DestroySlot(slot);
	m_slots.clear();
	m_lookup.clear();
	for (int i = 0; i < 2; i++)
	{
		m_context->DestroyView(m_raw[i].view);
		m_raw[i].view = VK_NULL_HANDLE;
		m_context->Memory().DestroyImage(m_raw[i]);
	}
	if (m_computePipeline)
		vkDestroyPipeline(device, m_computePipeline, nullptr);
	if (m_computeLayout)
		vkDestroyPipelineLayout(device, m_computeLayout, nullptr);
	if (m_computeSetLayout)
		vkDestroyDescriptorSetLayout(device, m_computeSetLayout, nullptr);
	if (m_computeModule)
		vkDestroyShaderModule(device, m_computeModule, nullptr);
	for (auto sampler : m_samplers)
		if (sampler)
			vkDestroySampler(device, sampler, nullptr);
	if (m_rawSampler)
		vkDestroySampler(device, m_rawSampler, nullptr);
	if (m_pool)
		vkDestroyDescriptorPool(device, m_pool, nullptr);
	if (m_layout)
		vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
	m_computePipeline = VK_NULL_HANDLE;
	m_computeLayout = VK_NULL_HANDLE;
	m_computeSetLayout = VK_NULL_HANDLE;
	m_computeModule = VK_NULL_HANDLE;
	m_pool = VK_NULL_HANDLE;
	m_layout = VK_NULL_HANDLE;
	m_context = nullptr;
}

void CVulkanTextureCache::AttachMemory(const uint16_t* textureRam)
{
	m_textureRam = textureRam;
}

void CVulkanTextureCache::UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height)
{
	(void)level;
	std::lock_guard<std::mutex> guard(m_dirtyMutex);
	m_uploadRequests++;
	if (width >= 2048 && height >= 2048)
	{
		/*
		 * A whole-page upload, which is how the ROM's own texture data arrives at startup.
		 *
		 * Both the dirty and the ever-written sets are filled, and the second one is the part that is
		 * easy to leave out: this branch returns before the per-tile loop below, so a version that only
		 * marked m_dirty would make every ROM-supplied texture look like a region the game had never
		 * touched. That turns the "was it written" diagnostic into a confident wrong answer, which is
		 * worse than not having it - it would have pointed away from this file.
		 */
		m_dirty[0].set();
		m_dirty[1].set();
		m_everDirty[0].set();
		m_everDirty[1].set();
		m_bulkUploads++;
		return;
	}
	int page = (int)y / kPageHeight;
	if (page < 0 || page > 1)
	{
		// The game asked us to upload a region that lies outside both texture pages. Dropping it is
		// correct, but it used to happen without a word, and a silently ignored upload is
		// indistinguishable from a texture that was never written at all. Logged for the first one only
		// - if this is systematic the count in the periodic report says so without flooding the log.
		if (++m_droppedRequests == 1)
			ErrorLog("Vulkan: texture upload at (%u,%u) size %ux%u lies outside the texture pages and "
				"was dropped (further drops are counted, not logged).", x, y, width, height);
		return;
	}
	int localY = (int)y - page * kPageHeight;
	int x0 = std::max(0, (int)x / kTile);
	int y0 = std::max(0, localY / kTile);
	int x1 = std::min(kTilesX - 1, (int)(x + width - 1) / kTile);
	int y1 = std::min(kTilesY - 1, (localY + (int)height - 1) / kTile);
	for (int ty = y0; ty <= y1; ty++)
		for (int tx = x0; tx <= x1; tx++)
		{
			m_dirty[page].set((size_t)ty * kTilesX + tx);
			// Remembered separately from the dirty set, which is cleared as uploads are handed over.
			// This is the record of what the game has ever written, and it is what tells a region the
			// game never filled from one whose upload was lost.
			m_everDirty[page].set((size_t)ty * kTilesX + tx);
		}
}

bool CVulkanTextureCache::CreateDescriptors()
{
	VkDevice device = m_context->Device();
	VkDescriptorSetLayoutBinding bindings[4] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[0].descriptorCount = 16;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	// The two packed Real3D texture pages. These are the only textures the
	// fragment shader samples; the hardware picks the rectangle and mip out of
	// them, exactly like New3D does.
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 2;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo = {};
	flagInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flagInfo.bindingCount = 0;
	flagInfo.pBindingFlags = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.flags = 0;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings = bindings;
	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: texture descriptor layout creation failed.");
		return false;
	}

	VkDescriptorPoolSize sizes[4] = {};
	sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	sizes[0].descriptorCount = 16;
	sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	sizes[1].descriptorCount = 1;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	sizes[2].descriptorCount = 1;
	sizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sizes[3].descriptorCount = 2;
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 4;
	poolInfo.pPoolSizes = sizes;
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: descriptor pool creation failed.");
		return false;
	}

	VkDescriptorSetAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = m_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &m_layout;
	if (vkAllocateDescriptorSets(device, &alloc, &m_set) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: descriptor set allocation failed.");
		return false;
	}

	VkSamplerCreateInfo sampler = {};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.maxLod = 16.0f;
	VkSamplerAddressMode address[4] = {
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
	};
	for (int v = 0; v < 4; v++)
	{
		for (int u = 0; u < 4; u++)
		{
			sampler.addressModeU = address[u];
			sampler.addressModeV = address[v];
			sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			if (vkCreateSampler(device, &sampler, nullptr, &m_samplers[v * 4 + u]) != VK_SUCCESS)
				return false;
		}
	}
	VkDescriptorImageInfo infos[16] = {};
	for (int i = 0; i < 16; i++)
		infos[i].sampler = m_samplers[i];
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_set;
	write.dstBinding = 0;
	write.descriptorCount = 16;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.pImageInfo = infos;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.unnormalizedCoordinates = VK_FALSE;
	return vkCreateSampler(device, &sampler, nullptr, &m_rawSampler) == VK_SUCCESS;
}

bool CVulkanTextureCache::CreateRawPages()
{
	for (int i = 0; i < 2; i++)
	{
		if (!m_context->Memory().CreateImage(kPageWidth, kPageHeight, 1, VK_FORMAT_R16_UINT,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, m_raw[i]))
			return false;
		m_raw[i].view = m_context->CreateView(m_raw[i].image, VK_FORMAT_R16_UINT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
		m_rawLayout[i] = VK_IMAGE_LAYOUT_UNDEFINED;

		// The images are being made from scratch, so nothing in them has been uploaded yet. Leaving the
		// markers set would have the diagnostic claim a region was filled when the new image is still
		// undefined - the exact false reassurance it exists to prevent.
		m_everUploaded[i].reset();
		// Sized here rather than in Init() so the guard's storage (16 KB per page) only exists when the
		// texture pages do, and so a resize cannot leave a checksum describing an image that is gone.
		if (m_diagnostics)
			m_gpuChecksum[i].assign((size_t)kTilesX * kTilesY, 0);
	}

	// Bind both packed texture pages to binding 3. The fragment shader samples
	// them directly (texelFetch) so no filtering state matters here.
	VkDescriptorImageInfo infos[2] = {};
	for (int i = 0; i < 2; i++)
	{
		infos[i].sampler = m_rawSampler;
		infos[i].imageView = m_raw[i].view;
		infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_set;
	write.dstBinding = 3;
	write.descriptorCount = 2;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = infos;
	vkUpdateDescriptorSets(m_context->Device(), 1, &write, 0, nullptr);
	return true;
}

bool CVulkanTextureCache::CreateComputePipeline()
{
	if (!m_context->HasPushDescriptors())
		return false;
	VkFormatProperties props = {};
	vkGetPhysicalDeviceFormatProperties(m_context->PhysicalDevice(), VK_FORMAT_R8G8B8A8_UNORM, &props);
	if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		return false;
	vkGetPhysicalDeviceFormatProperties(m_context->PhysicalDevice(), VK_FORMAT_R16_UINT, &props);
	if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		return false;

	VkDevice device = m_context->Device();
	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = texdecode_comp_spv_size;
	moduleInfo.pCode = texdecode_comp_spv;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &m_computeModule) != VK_SUCCESS)
		return false;

	VkDescriptorSetLayoutBinding bindings[2] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	VkDescriptorSetLayoutCreateInfo setInfo = {};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	setInfo.bindingCount = 2;
	setInfo.pBindings = bindings;
	if (vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &m_computeSetLayout) != VK_SUCCESS)
		return false;

	VkPushConstantRange range = {};
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	range.size = sizeof(int) * 8;
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &m_computeSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &range;
	VkResult layoutResult = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_computeLayout);
	if (layoutResult != VK_SUCCESS)
		return false;

	VkPipelineShaderStageCreateInfo stage = {};
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = m_computeModule;
	stage.pName = "main";
	VkComputePipelineCreateInfo pipe = {};
	pipe.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe.stage = stage;
	pipe.layout = m_computeLayout;
	return vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipe, nullptr, &m_computePipeline) == VK_SUCCESS;
}

void CVulkanTextureCache::WriteDynamicBuffers(VkBuffer viewportBuffer, VkDeviceSize viewportRange, VkBuffer materialBuffer, VkDeviceSize materialRange)
{
	VkDescriptorBufferInfo buffers[2] = {};
	buffers[0].buffer = viewportBuffer;
	buffers[0].offset = 0;
	buffers[0].range = viewportRange;
	buffers[1].buffer = materialBuffer;
	buffers[1].offset = 0;
	buffers[1].range = materialRange;
	VkWriteDescriptorSet writes[2] = {};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = m_set;
	writes[0].dstBinding = 1;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	writes[0].pBufferInfo = &buffers[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = m_set;
	writes[1].dstBinding = 2;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	writes[1].pBufferInfo = &buffers[1];
	vkUpdateDescriptorSets(m_context->Device(), 2, writes, 0, nullptr);
}

void CVulkanTextureCache::WriteImageDescriptor(uint32_t slot)
{
	VkDescriptorImageInfo info = {};
	info.imageView = m_slots[slot].image.view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_set;
	write.dstBinding = 3;
	write.dstArrayElement = slot;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &info;
	vkUpdateDescriptorSets(m_context->Device(), 1, &write, 0, nullptr);
}

void CVulkanTextureCache::DestroySlot(Slot& slot)
{
	if (!m_context)
		return;
	for (auto view : slot.mipViews)
	{
		if (view && view != slot.image.view)
			m_context->DestroyView(view);
	}
	slot.mipViews.clear();
	m_context->DestroyView(slot.image.view);
	slot.image.view = VK_NULL_HANDLE;
	m_context->Memory().DestroyImage(slot.image);
	slot.live = false;
}

void CVulkanTextureCache::EvictOne(uint64_t frameSerial)
{
	uint32_t victim = 0;
	uint64_t oldest = UINT64_MAX;
	for (uint32_t i = 1; i < m_slots.size(); i++)
	{
		if (!m_slots[i].live)
			continue;
		if (m_slots[i].lastUsed != 0 && m_slots[i].lastUsed + kMaxFramesInFlight > frameSerial)
			continue;
		if (m_slots[i].lastUsed < oldest)
		{
			oldest = m_slots[i].lastUsed;
			victim = i;
		}
	}
	if (victim == 0)
		return;
	m_lookup.erase(m_slots[victim].key);
	DestroySlot(m_slots[victim]);
}

uint32_t CVulkanTextureCache::AllocateSlot(uint64_t key, int page, int x, int y, int width, int height, int format, bool inverted)
{
	if (width < 1) width = 1;
	if (height < 1) height = 1;
	uint32_t index = 0;
	for (uint32_t i = 1; i < m_slots.size(); i++)
	{
		if (!m_slots[i].live)
		{
			index = i;
			break;
		}
	}
	if (index == 0)
	{
		if (m_slots.size() >= kMaxTextureSlots)
			return 0;
		index = (uint32_t)m_slots.size();
		m_slots.emplace_back();
	}
	Slot& slot = m_slots[index];
	slot = Slot();
	slot.key = key;
	slot.page = page & 1;
	slot.x = x;
	slot.y = y;
	slot.width = width;
	slot.height = height;
	slot.format = format;
	slot.inverted = inverted;
	slot.stale = true;
	uint32_t mips = MipCount(width, height);
	if (!m_context->Memory().CreateImage((uint32_t)width, (uint32_t)height, mips, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, slot.image))
	{
		m_slots[index].live = false;
		return 0;
	}
	slot.image.view = m_context->CreateView(slot.image.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, mips);
	slot.mipViews.resize(mips);
	slot.mipViews[0] = m_context->CreateView(slot.image.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	for (uint32_t mip = 1; mip < mips; mip++)
		slot.mipViews[mip] = m_context->CreateView(slot.image.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, mip, 1);

	int mipW = width;
	int mipH = height;
	for (uint32_t mip = 0; mip < mips && mip < 12; mip++)
	{
		Rect rect;
		if (mip == 0)
		{
			rect.x = x;
			rect.y = y;
		}
		else
		{
			rect.x = kMipX[mip] + (x >> mip);
			rect.y = kMipY[mip] + (y >> mip);
		}
		rect.w = std::max(1, mipW);
		rect.h = std::max(1, mipH);
		slot.coverage.push_back(rect);
		mipW = std::max(1, mipW / 2);
		mipH = std::max(1, mipH / 2);
	}
	slot.live = true;
	slot.inShaderRead = false;
	m_lookup[key] = index;
	return index;
}

uint32_t CVulkanTextureCache::Acquire(int page, int x, int y, int width, int height, int format, bool inverted)
{
	if (width <= 0 || height <= 0)
		return 0;
	page &= 1;
	uint64_t key = MakeKey(page, x, y, width, height, format, inverted);
	auto found = m_lookup.find(key);
	if (found != m_lookup.end())
		return found->second;
	if (m_lookup.size() + 1 >= kMaxTextureSlots)
		EvictOne(m_frameSerial);
	return AllocateSlot(key, page, x, y, width, height, format, inverted);
}

void CVulkanTextureCache::MarkUsed(uint32_t slot, uint64_t frameSerial)
{
	if (slot < m_slots.size() && m_slots[slot].live)
		m_slots[slot].lastUsed = frameSerial;
}

CVulkanTextureCache::Coverage CVulkanTextureCache::RegionCoverage(int page, int x, int y, int width, int height) const
{
	// A degenerate or out-of-range region has nothing to sample, so reporting it as fine keeps the
	// diagnostic free of false positives that would bury a real one.
	if (page < 0 || page > 1 || width <= 0 || height <= 0)
		return Coverage::Full;

	int x0 = std::max(0, x / kTile);
	int y0 = std::max(0, y / kTile);
	int x1 = std::min(kTilesX - 1, (x + width - 1) / kTile);
	int y1 = std::min(kTilesY - 1, (y + height - 1) / kTile);
	if (x1 < x0 || y1 < y0)
		return Coverage::Full;

	unsigned tiles = 0;
	unsigned present = 0;
	for (int ty = y0; ty <= y1; ty++)
	{
		for (int tx = x0; tx <= x1; tx++)
		{
			++tiles;
			if (m_everUploaded[page].test((size_t)ty * kTilesX + tx))
				++present;
		}
	}
	if (present == 0)
		return Coverage::None;
	return present == tiles ? Coverage::Full : Coverage::Partial;
}

void CVulkanTextureCache::NoteTexturedReference(Coverage coverage)
{
	m_texturedRefs++;
	switch (coverage)
	{
	case Coverage::None:
		m_coverageNone++;
		m_coverageNoneInWindow++;
		break;
	case Coverage::Partial:
		m_coveragePartial++;
		m_coveragePartialInWindow++;
		break;
	default:
		m_coverageFull++;
		break;
	}
}

unsigned CVulkanTextureCache::RegionTileCount(int page, int x, int y, int width, int height) const
{
	if (page < 0 || page > 1 || width <= 0 || height <= 0)
		return 0;
	const int x0 = std::max(0, x / kTile);
	const int y0 = std::max(0, y / kTile);
	const int x1 = std::min(kTilesX - 1, (x + width - 1) / kTile);
	const int y1 = std::min(kTilesY - 1, (y + height - 1) / kTile);
	if (x1 < x0 || y1 < y0)
		return 0;
	return (unsigned)((x1 - x0 + 1) * (y1 - y0 + 1));
}

unsigned CVulkanTextureCache::RegionEverWrittenTiles(int page, int x, int y, int width, int height) const
{
	if (page < 0 || page > 1 || width <= 0 || height <= 0)
		return 0;
	const int x0 = std::max(0, x / kTile);
	const int y0 = std::max(0, y / kTile);
	const int x1 = std::min(kTilesX - 1, (x + width - 1) / kTile);
	const int y1 = std::min(kTilesY - 1, (y + height - 1) / kTile);
	if (x1 < x0 || y1 < y0)
		return 0;
	unsigned written = 0;
	for (int ty = y0; ty <= y1; ty++)
		for (int tx = x0; tx <= x1; tx++)
			if (m_everDirty[page].test((size_t)ty * kTilesX + tx))
				++written;
	return written;
}

// FNV-1a over the tile's 1024 texels. Chosen for being a handful of instructions per texel and having
// no ordering weakness of the kind a plain sum has, so a tile that merely has its texels permuted
// still reads as changed.
uint64_t CVulkanTextureCache::TileChecksum(int page, int tileX, int tileY) const
{
	if (!m_textureRam)
		return 0;
	uint64_t h = 1469598103934665603ull;
	const uint16_t* base = m_textureRam + page * (kPageWidth * kPageHeight);
	for (int y = 0; y < kTile; y++)
	{
		const uint16_t* row = base + (tileY * kTile + y) * kPageWidth + tileX * kTile;
		for (int x = 0; x < kTile; x++)
		{
			h ^= row[x];
			h *= 1099511628211ull;
		}
	}
	return h;
}

void CVulkanTextureCache::VerifyDirtyCoverage(const std::bitset<kTilesX * kTilesY> dirty[2], uint64_t frameSerial)
{
	if (!m_textureRam)
		return;
	if (m_gpuChecksum[0].empty())
		return;

	// A slice per frame rather than the whole page, so checking costs a fraction of a percent and
	// every tile is still covered within a handful of frames. A miss that matters is one that persists,
	// so catching it a few frames late is fine.
	static constexpr unsigned kGuardSlices = 16;
	const size_t slice = (size_t)(frameSerial % kGuardSlices);

	for (int page = 0; page < 2; page++)
	{
		if (m_gpuChecksum[page].empty())
			continue;
		for (int ty = 0; ty < kTilesY; ty++)
		{
			for (int tx = 0; tx < kTilesX; tx++)
			{
				size_t idx = (size_t)ty * kTilesX + tx;
				if ((idx % kGuardSlices) != slice)
					continue;
				// Dirty means it is about to be re-sent, so the GPU will be correct shortly and there
				// is nothing to report. This is the path that keeps the guard quiet in normal running.
				if (dirty[page].test(idx))
					continue;
				// A tile that has never been uploaded has no meaningful checksum yet, so comparing
				// against it would report the whole page as stale during startup.
				if (!m_everUploaded[page].test(idx))
					continue;

				uint64_t now = TileChecksum(page, tx, ty);
				if (m_gpuChecksum[page][idx] == now)
					continue;

				m_staleTiles++;
				// Logged on the change, not on inspection, so a tile that stays wrong is reported once
				// rather than every time the guard sweeps over it. The coordinates are what makes this
				// actionable: they name the exact region the GPU is holding stale data for, and the
				// pattern across several of them says whether this is one runaway upload or a systematic
				// miss. Capped so a wholesale failure cannot bury the log.
				if (m_staleReports < 24)
				{
					m_staleReports++;
					ErrorLog("Vulkan: texture tile (%d,%d) page %d is STALE - its contents changed but nothing "
						"marked it for upload. Pixels (%d,%d)..(%d,%d) of the raw page are wrong on the GPU.",
						tx, ty, page, tx * kTile, ty * kTile, tx * kTile + kTile - 1, ty * kTile + kTile - 1);
				}
				// Adopt the new value so this tile is not reported again until it changes once more; the
				// bug being hunted is the absence of an upload, and re-reporting the same tile every
				// sweep would hide how many distinct regions are affected.
				m_gpuChecksum[page][idx] = now;
			}
		}
	}
}

/*
 * A periodic one-line summary of texture traffic.
 *
 * Enabled only by VulkanDiagnostics. The normal file logger closes and reopens the file on each
 * line, so these reports must not perform synchronous file I/O during ordinary gameplay.
 */
void CVulkanTextureCache::ReportUploads()
{
	size_t total = (size_t)kTilesX * kTilesY;
	InfoLog("Vulkan: textures - %llu upload request(s), %llu dropped, %llu tile(s) copied to the GPU, %llu whole-page upload(s).",
		(unsigned long long)m_uploadRequests, (unsigned long long)m_droppedRequests,
		(unsigned long long)m_tilesUploaded, (unsigned long long)m_bulkUploads);
	InfoLog("Vulkan: textures - page 0 %.1f%% and page 1 %.1f%% of tiles have ever been uploaded.",
		100.0 * m_everUploaded[0].count() / total, 100.0 * m_everUploaded[1].count() / total);

	/*
	 * Only the None figure is a claim about black. Reported alongside the others so the three can be
	 * read as a breakdown rather than as one number whose meaning has to be remembered: Full means the
	 * draw sampled data that was all there, Partial means it sampled a region with unvisited corners -
	 * which is normal for a large texture and is not a fault - and None means nothing in the region had
	 * ever been uploaded, so that draw read untouched memory.
	 */
	InfoLog("Vulkan: textures - of %llu textured draw(s): %llu fully uploaded, %llu partly uploaded, "
		"%llu never uploaded (the last is the count that renders as black).",
		(unsigned long long)m_texturedRefs, (unsigned long long)m_coverageFull,
		(unsigned long long)m_coveragePartial, (unsigned long long)m_coverageNone);

	if (m_coverageNoneInWindow != 0)
	{
		InfoLog("Vulkan: textures - in the last interval: %llu draw(s) sampled nothing but unuploaded "
			"data (black) and %llu sampled a partly filled region.",
			(unsigned long long)m_coverageNoneInWindow, (unsigned long long)m_coveragePartialInWindow);
	}
	else
	{
		InfoLog("Vulkan: textures - in the last interval no draw sampled a wholly unuploaded region.");
	}
	// The count that matters most. Anything above zero here means the GPU is holding stale texture data
	// that nothing will ever replace, which is the mechanism behind corruption that appears partway
	// through a level and is cured by loading a save state (that path re-sends everything).
	if (m_staleTiles != 0)
	{
		InfoLog("Vulkan: textures - %llu tile(s) have gone stale so far (changed without being marked "
			"for upload); see the earlier [Error] lines for their coordinates.",
			(unsigned long long)m_staleTiles);
	}
	else
	{
		InfoLog("Vulkan: textures - no stale tiles: every change to texture memory has been uploaded.");
	}
	m_texturedRefs = 0;
	m_coverageFull = 0;
	m_coveragePartial = 0;
	m_coverageNone = 0;
	m_coverageNoneInWindow = 0;
	m_coveragePartialInWindow = 0;
}

void CVulkanTextureCache::RecordSync(VkCommandBuffer cmd, CVulkanRingBuffer& ring, uint64_t frameSerial)
{
	std::bitset<kTilesX * kTilesY> dirty[2];
	{
		std::lock_guard<std::mutex> guard(m_dirtyMutex);
		/*
		 * The marks are moved out and cleared only when there is somewhere to send them.
		 *
		 * This used to clear unconditionally, and RecordRawUploads() below returns immediately when
		 * the texture RAM pointer has not been attached yet. That combination destroyed the marks for
		 * writes that were never copied: the page keeps whatever it had, and because Model 3 games
		 * write their textures once at level load, nothing ever rewrites that region - so the surface
		 * stays black for the rest of the session with no error anywhere. It is the only path in this
		 * file that lost work without saying so; the ring-exhausted path puts its marks back and the
		 * out-of-range path counts itself.
		 *
		 * Leaving the marks in place costs nothing: they are still set, so the first frame that runs
		 * with memory attached uploads them.
		 */
		if (m_textureRam)
		{
			dirty[0] = m_dirty[0];
			dirty[1] = m_dirty[1];
			m_dirty[0].reset();
			m_dirty[1].reset();
		}
		else if (m_dirty[0].any() || m_dirty[1].any())
		{
			// Said out loud, because the old behaviour was silent and this is the state that loses data.
			if (m_noMemoryReports++ == 0)
				ErrorLog("Vulkan: texture RAM is not attached; texture uploads are being held back "
					"rather than dropped (further occurrences are counted, not logged).");
		}
	}

	// The fragment shader samples these pages directly, so they must always be
	// in a defined layout even before the emulated game uploads anything.
	for (int page = 0; page < 2; page++)
	{
		if (m_rawLayout[page] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			CmdTransition(cmd, m_raw[page].image, m_rawLayout[page], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			CmdTransition(cmd, m_raw[page].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			m_rawLayout[page] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	// Deliberately outside the dirty check below. The case this guard exists to catch is a change with
	// nothing marked dirty, and on a frame like that the block below is skipped entirely - so leaving
	// the call inside it would make the guard blind to exactly the fault it is looking for.
	VerifyDirtyCoverage(dirty, frameSerial);

	if (dirty[0].any() || dirty[1].any())
	{
		RecordRawUploads(cmd, ring, dirty);
		for (uint32_t i = 1; i < m_slots.size(); i++)
		{
			Slot& slot = m_slots[i];
			if (!slot.live)
				continue;
			for (const Rect& rect : slot.coverage)
			{
				int x0 = std::max(0, rect.x / kTile);
				int y0 = std::max(0, rect.y / kTile);
				int x1 = std::min(kTilesX - 1, (rect.x + rect.w - 1) / kTile);
				int y1 = std::min(kTilesY - 1, (rect.y + rect.h - 1) / kTile);
				bool hit = false;
				for (int ty = y0; ty <= y1 && !hit; ty++)
					for (int tx = x0; tx <= x1; tx++)
						if (dirty[slot.page].test((size_t)ty * kTilesX + tx))
						{
							hit = true;
							break;
						}
				if (hit)
				{
					slot.stale = true;
					break;
				}
			}
		}
	}

	if (!m_slots.empty() && m_slots[0].live && !m_slots[0].inShaderRead)
		CpuDecode(cmd, ring, m_slots[0]);

	if (m_gpuDecode && m_computePipeline)
		RecordDecode(cmd, frameSerial);
	else
	{
		for (uint32_t i = 0; i < m_slots.size(); i++)
		{
			Slot& slot = m_slots[i];
			if (!slot.live || !slot.stale)
				continue;
			bool inFlight = slot.lastUsed != 0 && slot.lastUsed + kMaxFramesInFlight > frameSerial;
			if (inFlight && i != 0)
				continue;
			CpuDecode(cmd, ring, slot);
		}
	}

	// File logging reopens the file on every line; periodic reports are opt-in.
	if (m_diagnostics && ++m_frames % 600 == 0)
		ReportUploads();
}

void CVulkanTextureCache::RecordRawUploads(VkCommandBuffer cmd, CVulkanRingBuffer& ring, const std::bitset<kTilesX * kTilesY> dirty[2])
{
	if (!m_textureRam)
		return;
	for (int page = 0; page < 2; page++)
	{
		if (!dirty[page].any())
			continue;

		/*
		 * Tiles that still owe the GPU an upload.
		 *
		 * Started as a copy of the dirty set and cleared as each run is handed over, so that whatever
		 * is left when the ring runs out can be put back - see the note at the bottom of the loop.
		 * Without this the dirty bits were already reset by RecordSync() and an abandoned run was
		 * lost for good: the page keeps its old contents until the game happens to rewrite that exact
		 * region, which for data written once at level load means never. The visible result is a
		 * texture that is stale, or black where it has never been filled in at all.
		 */
		std::bitset<kTilesX * kTilesY> pending = dirty[page];

		CmdTransition(cmd, m_raw[page].image, m_rawLayout[page], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		m_rawLayout[page] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		std::vector<VkBufferImageCopy> regions;
		bool ranOut = false;
		for (int ty = 0; ty < kTilesY && !ranOut; ty++)
		{
			int tx = 0;
			while (tx < kTilesX)
			{
				if (!pending.test((size_t)ty * kTilesX + tx))
				{
					tx++;
					continue;
				}
				int run = tx;
				while (run < kTilesX && pending.test((size_t)ty * kTilesX + run))
					run++;
				int texX = tx * kTile;
				int texY = ty * kTile;
				int texW = (run - tx) * kTile;
				int texH = kTile;
				VkDeviceSize bytes = (VkDeviceSize)texW * texH * sizeof(uint16_t);
				VkDeviceSize offset = 0;
				void* ptr = nullptr;
				if (!ring.Alloc(bytes, 4, offset, ptr))
				{
					// Stop here rather than returning: the transitions below still have to happen, and
					// the rest of this page - and the whole of the other one - must be retried instead
					// of being abandoned.
					ranOut = true;
					break;
				}
				const uint16_t* srcBase = m_textureRam + page * (kPageWidth * kPageHeight);
				uint16_t* dst = static_cast<uint16_t*>(ptr);
				for (int row = 0; row < texH; row++)
					std::memcpy(dst + row * texW, srcBase + (texY + row) * kPageWidth + texX, (size_t)texW * sizeof(uint16_t));
				VkBufferImageCopy copy = {};
				copy.bufferOffset = offset;
				copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copy.imageSubresource.layerCount = 1;
				copy.imageOffset = { texX, texY, 0 };
				copy.imageExtent = { (uint32_t)texW, (uint32_t)texH, 1 };
				regions.push_back(copy);
				for (int i = tx; i < run; i++)
				{
					pending.reset((size_t)ty * kTilesX + i);
					// Recorded here rather than where the upload is requested: a tile whose copy was
					// deferred has not reached the GPU yet, and the whole point of the diagnostic is to
					// find tiles that never did.
					size_t tileIdx = (size_t)ty * kTilesX + i;
					m_everUploaded[page].set(tileIdx);
					m_tilesUploaded++;
					// What the GPU now holds, for the stale-tile guard to compare against later. Set at
					// the same moment as the copy so the two can never disagree.
					if (!m_gpuChecksum[page].empty())
						m_gpuChecksum[page][tileIdx] = TileChecksum(page, i, ty);
				}
				tx = run;
			}
		}
		if (!regions.empty())
			vkCmdCopyBufferToImage(cmd, ring.Buffer(), m_raw[page].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)regions.size(), regions.data());
		// Unconditional, including on the exhausted path: the image must never be left in
		// TRANSFER_DST while the fragment shader samples it as shader-read.
		CmdTransition(cmd, m_raw[page].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		m_rawLayout[page] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (ranOut && pending.any())
		{
			// Put back what did not fit, so the next frame finishes the job.
			{
				std::lock_guard<std::mutex> guard(m_dirtyMutex);
				m_dirty[page] |= pending;
			}
			// Logged every time rather than once: if this is happening at all it is worth seeing how
			// often, because the ring is sized generously enough that it should be never.
			InfoLog("Vulkan: texture upload ran out of ring space on page %d (%d tile(s) deferred).",
				page, (int)pending.count());
		}
	}
}

void CVulkanTextureCache::RecordDecode(VkCommandBuffer cmd, uint64_t frameSerial)
{
	if (!vkCmdPushDescriptorSetKHR)
		return;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
	for (uint32_t index = 0; index < m_slots.size(); index++)
	{
		Slot& slot = m_slots[index];
		if (!slot.live || !slot.stale || index == 0)
			continue;
		bool inFlight = slot.lastUsed != 0 && slot.lastUsed + kMaxFramesInFlight > frameSerial;
		if (inFlight && index != 0)
			continue;
		if (m_rawLayout[slot.page] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && index != 0)
			continue;

		VkImageLayout oldLayout = slot.inShaderRead ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
		CmdTransition(cmd, slot.image.image, oldLayout, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 0, slot.image.mipLevels);

		for (uint32_t mip = 0; mip < slot.mipViews.size() && mip < slot.coverage.size(); mip++)
		{
			const Rect& rect = slot.coverage[mip];
			VkDescriptorImageInfo rawInfo = {};
			rawInfo.sampler = m_rawSampler;
			rawInfo.imageView = m_raw[slot.page].view;
			rawInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			VkDescriptorImageInfo destInfo = {};
			destInfo.imageView = slot.mipViews[mip];
			destInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkWriteDescriptorSet writes[2] = {};
			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstBinding = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[0].pImageInfo = &rawInfo;
			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstBinding = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writes[1].pImageInfo = &destInfo;
			vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computeLayout, 0, 2, writes);

			int push[8] = { rect.x, rect.y, rect.w, rect.h, slot.format, slot.inverted ? 1 : 0, 0, 0 };
			vkCmdPushConstants(cmd, m_computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
			vkCmdDispatch(cmd, (uint32_t)(rect.w + 7) / 8, (uint32_t)(rect.h + 7) / 8, 1);
		}
		CmdTransition(cmd, slot.image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 0, slot.image.mipLevels);
		slot.inShaderRead = true;
		slot.stale = false;
	}
}

void CVulkanTextureCache::CpuDecode(VkCommandBuffer cmd, CVulkanRingBuffer& ring, Slot& slot)
{
	if (!m_textureRam && slot.width != 1)
		return;
	VkImageLayout oldLayout = slot.inShaderRead ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	CmdTransition(cmd, slot.image.image, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 0, slot.image.mipLevels);

	if (slot.width == 1 && slot.height == 1 && slot.coverage.empty())
	{
		VkDeviceSize offset = 0;
		void* ptr = nullptr;
		if (!ring.Alloc(4, 4, offset, ptr))
			return;
		uint8_t* pixel = static_cast<uint8_t*>(ptr);
		pixel[0] = pixel[1] = pixel[2] = pixel[3] = 255;
		VkBufferImageCopy copy = {};
		copy.bufferOffset = offset;
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.layerCount = 1;
		copy.imageExtent = { 1, 1, 1 };
		vkCmdCopyBufferToImage(cmd, ring.Buffer(), slot.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	}
	else if (m_textureRam)
	{
		const uint16_t* page = m_textureRam + slot.page * (kPageWidth * kPageHeight);
		for (uint32_t mip = 0; mip < slot.coverage.size(); mip++)
		{
			const Rect& rect = slot.coverage[mip];
			VkDeviceSize bytes = (VkDeviceSize)rect.w * rect.h * 4;
			VkDeviceSize offset = 0;
			void* ptr = nullptr;
			if (!ring.Alloc(bytes, 4, offset, ptr))
				return;
			uint8_t* dst = static_cast<uint8_t*>(ptr);
			for (int row = 0; row < rect.h; row++)
			{
				for (int col = 0; col < rect.w; col++)
				{
					int sx = std::min(kPageWidth - 1, rect.x + col);
					int sy = std::min(kPageHeight - 1, rect.y + row);
					uint32_t value = page[sy * kPageWidth + sx];
					uint8_t colour[4];
					ExtractColour(slot.format, value, colour);
					if (slot.inverted)
					{
						colour[0] = (uint8_t)(255 - colour[0]);
						colour[1] = (uint8_t)(255 - colour[1]);
						colour[2] = (uint8_t)(255 - colour[2]);
					}
					std::memcpy(dst + ((size_t)row * rect.w + col) * 4, colour, 4);
				}
			}
			VkBufferImageCopy copy = {};
			copy.bufferOffset = offset;
			copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy.imageSubresource.mipLevel = mip;
			copy.imageSubresource.layerCount = 1;
			copy.imageExtent = { (uint32_t)rect.w, (uint32_t)rect.h, 1 };
			vkCmdCopyBufferToImage(cmd, ring.Buffer(), slot.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		}
	}
	CmdTransition(cmd, slot.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 0, slot.image.mipLevels);
	slot.inShaderRead = true;
	slot.stale = false;
}

} // Vulkan3D
