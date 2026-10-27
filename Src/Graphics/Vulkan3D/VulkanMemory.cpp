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
#include "VulkanMemory.h"
#include "OSD/Logger.h"

namespace Vulkan3D {

static constexpr VkDeviceSize kBlockBytes = 64ull * 1024ull * 1024ull;

bool CVulkanMemory::Init(VkPhysicalDevice physical, VkDevice device)
{
	m_physical = physical;
	m_device = device;
	vkGetPhysicalDeviceMemoryProperties(physical, &m_props);
	return true;
}

void CVulkanMemory::Shutdown()
{
	for (auto& block : m_blocks)
	{
		if (block.memory)
			vkFreeMemory(m_device, block.memory, nullptr);
	}
	m_blocks.clear();
	m_device = VK_NULL_HANDLE;
	m_physical = VK_NULL_HANDLE;
}

uint32_t CVulkanMemory::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const
{
	for (uint32_t i = 0; i < m_props.memoryTypeCount; i++)
	{
		if ((typeBits & (1u << i)) && (m_props.memoryTypes[i].propertyFlags & props) == props)
			return i;
	}
	return UINT32_MAX;
}

VkDeviceSize CVulkanMemory::TotalAllocated() const
{
	VkDeviceSize total = 0;
	for (const auto& block : m_blocks)
		total += block.size;
	return total;
}

VkDeviceSize CVulkanMemory::TotalFree() const
{
	VkDeviceSize total = 0;
	for (const auto& block : m_blocks)
		for (const auto& span : block.free)
			total += span.size;
	return total;
}

bool CVulkanMemory::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, AllocatedBuffer& out)
{
	out = AllocatedBuffer();
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = size;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(m_device, &info, nullptr, &out.buffer) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: vkCreateBuffer failed (%llu bytes).", (unsigned long long)size);
		return false;
	}

	VkMemoryRequirements req = {};
	vkGetBufferMemoryRequirements(m_device, out.buffer, &req);
	uint32_t typeIndex = FindMemoryType(req.memoryTypeBits, props);
	if (typeIndex == UINT32_MAX)
	{
		vkDestroyBuffer(m_device, out.buffer, nullptr);
		out.buffer = VK_NULL_HANDLE;
		ErrorLog("Vulkan: no memory type for buffer.");
		return false;
	}

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = req.size;
	alloc.memoryTypeIndex = typeIndex;
	if (vkAllocateMemory(m_device, &alloc, nullptr, &out.memory) != VK_SUCCESS)
	{
		vkDestroyBuffer(m_device, out.buffer, nullptr);
		out.buffer = VK_NULL_HANDLE;
		ErrorLog("Vulkan: vkAllocateMemory failed (%llu bytes).", (unsigned long long)req.size);
		return false;
	}
	if (vkBindBufferMemory(m_device, out.buffer, out.memory, 0) != VK_SUCCESS)
	{
		// Was unchecked. A buffer that cannot be bound is unusable, and silently continuing leaves
		// callers writing into a mapping that does not exist.
		vkFreeMemory(m_device, out.memory, nullptr);
		vkDestroyBuffer(m_device, out.buffer, nullptr);
		out = AllocatedBuffer();
		ErrorLog("Vulkan: vkBindBufferMemory failed (%llu bytes).", (unsigned long long)req.size);
		return false;
	}
	out.size = size;
	out.offset = 0;
	out.dedicated = true;
	if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		if (vkMapMemory(m_device, out.memory, 0, size, 0, &out.mapped) != VK_SUCCESS)
		{
			// Every write to a host-visible buffer goes through this pointer, so a failed map has to
			// stop here rather than leave callers dereferencing null.
			ErrorLog("Vulkan: vkMapMemory failed (%llu bytes).", (unsigned long long)size);
			vkFreeMemory(m_device, out.memory, nullptr);
			vkDestroyBuffer(m_device, out.buffer, nullptr);
			out = AllocatedBuffer();
			return false;
		}
	}
	return true;
}

void CVulkanMemory::DestroyBuffer(AllocatedBuffer& buffer)
{
	if (m_device == VK_NULL_HANDLE)
		return;
	if (buffer.mapped && buffer.memory)
		vkUnmapMemory(m_device, buffer.memory);
	if (buffer.buffer)
		vkDestroyBuffer(m_device, buffer.buffer, nullptr);
	if (buffer.dedicated && buffer.memory)
		vkFreeMemory(m_device, buffer.memory, nullptr);
	buffer = AllocatedBuffer();
}

bool CVulkanMemory::Suballocate(uint32_t typeIndex, VkDeviceSize size, VkDeviceSize alignment, VkDeviceMemory& memory, VkDeviceSize& offset)
{
	for (auto& block : m_blocks)
	{
		if (block.typeIndex != typeIndex)
			continue;
		for (size_t i = 0; i < block.free.size(); i++)
		{
			VkDeviceSize begin = (block.free[i].offset + alignment - 1) & ~(alignment - 1);
			VkDeviceSize end = block.free[i].offset + block.free[i].size;
			if (begin >= end || end - begin < size)
				continue;

			VkDeviceSize left = begin - block.free[i].offset;
			VkDeviceSize right = end - (begin + size);
			block.free.erase(block.free.begin() + (ptrdiff_t)i);
			if (right)
				block.free.insert(block.free.begin() + (ptrdiff_t)i, Block::Span{ begin + size, right });
			if (left)
				block.free.insert(block.free.begin() + (ptrdiff_t)i, Block::Span{ begin - left, left });
			memory = block.memory;
			offset = begin;
			return true;
		}
	}

	VkDeviceSize blockSize = kBlockBytes;
	if (size + alignment > blockSize)
		blockSize = size + alignment;

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = blockSize;
	alloc.memoryTypeIndex = typeIndex;
	Block block;
	block.size = blockSize;
	block.typeIndex = typeIndex;
	if (vkAllocateMemory(m_device, &alloc, nullptr, &block.memory) != VK_SUCCESS)
	{
		/*
		 * This is the allocation that fails when VRAM runs out, and it used to fail silently: the
		 * return value was propagated but nothing was logged, so a renderer that had run out of memory
		 * looked identical in the log to one that simply had nothing to draw. Everything created
		 * through this allocator - render targets, MSAA images, staging buffers - arrives here, so one
		 * line covers the whole surface.
		 *
		 * The totals are included because they separate the two very different causes: a large total
		 * with room still free points at fragmentation or a single oversized request, whereas a total
		 * pressed against the device limit means the resolution, the MSAA level or both are simply too
		 * much for this GPU.
		 */
		ErrorLog("Vulkan: vkAllocateMemory failed for a %llu MB block (request was %llu bytes). "
			"%llu MB already held across %u block(s), %llu MB of that free.",
			(unsigned long long)(blockSize / (1024 * 1024)), (unsigned long long)size,
			(unsigned long long)(TotalAllocated() / (1024 * 1024)), (unsigned)m_blocks.size(),
			(unsigned long long)(TotalFree() / (1024 * 1024)));
		return false;
	}
	block.free.push_back(Block::Span{ 0, blockSize });
	m_blocks.push_back(block);

	// Logged on success too. A block is only created when no existing one can satisfy the request, so
	// this line appears exactly when the renderer's memory footprint grows - which makes it the record
	// of how much a given resolution and MSAA level actually costs.
	InfoLog("Vulkan: device memory block #%u allocated, %llu MB (%llu MB held in total).",
		(unsigned)m_blocks.size(), (unsigned long long)(blockSize / (1024 * 1024)),
		(unsigned long long)(TotalAllocated() / (1024 * 1024)));

	return Suballocate(typeIndex, size, alignment, memory, offset);
}

void CVulkanMemory::FreeSuballocation(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
{
	for (auto& block : m_blocks)
	{
		if (block.memory != memory)
			continue;
		block.free.push_back(Block::Span{ offset, size });
		// Coalesce any overlapping or adjacent spans. Order does not matter.
		bool merged = true;
		while (merged)
		{
			merged = false;
			for (size_t i = 0; i < block.free.size() && !merged; i++)
			{
				for (size_t j = i + 1; j < block.free.size(); j++)
				{
					Block::Span& a = block.free[i];
					Block::Span& b = block.free[j];
					Block::Span lo = a.offset < b.offset ? a : b;
					Block::Span hi = a.offset < b.offset ? b : a;
					if (lo.offset + lo.size >= hi.offset)
					{
						VkDeviceSize end = hi.offset + hi.size;
						if (lo.offset + lo.size > end)
							end = lo.offset + lo.size;
						block.free[i] = Block::Span{ lo.offset, end - lo.offset };
						block.free.erase(block.free.begin() + (ptrdiff_t)j);
						merged = true;
						break;
					}
				}
			}
		}
		return;
	}
}

bool CVulkanMemory::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, AllocatedImage& out, uint32_t samples)
{
	out = AllocatedImage();
	out.format = format;
	out.width = width;
	out.height = height;
	out.mipLevels = mipLevels;
	out.samples = samples ? samples : 1;

	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = { width, height, 1 };
	info.mipLevels = mipLevels;
	info.arrayLayers = 1;
	info.samples = (VkSampleCountFlagBits)out.samples;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (vkCreateImage(m_device, &info, nullptr, &out.image) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: vkCreateImage %ux%u failed.", width, height);
		return false;
	}

	VkMemoryRequirements req = {};
	vkGetImageMemoryRequirements(m_device, out.image, &req);
	uint32_t typeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (typeIndex == UINT32_MAX)
		typeIndex = FindMemoryType(req.memoryTypeBits, 0);
	if (typeIndex == UINT32_MAX)
	{
		vkDestroyImage(m_device, out.image, nullptr);
		out.image = VK_NULL_HANDLE;
		ErrorLog("Vulkan: no memory type for a %ux%u image (format %d, %u sample(s)).",
			width, height, (int)format, out.samples);
		return false;
	}

	VkDeviceSize alignment = req.alignment ? req.alignment : 1;
	if (!Suballocate(typeIndex, req.size, alignment, out.memory, out.offset))
	{
		vkDestroyImage(m_device, out.image, nullptr);
		out.image = VK_NULL_HANDLE;
		// Suballocate has already logged why. Naming the image here is what turns "an allocation
		// failed" into "the 3840x2160 4x MSAA colour target is what could not be placed".
		ErrorLog("Vulkan: could not place a %ux%u image needing %llu bytes.", width, height,
			(unsigned long long)req.size);
		return false;
	}
	VkDeviceSize boundOffset = out.offset;
	if (vkBindImageMemory(m_device, out.image, out.memory, out.offset) != VK_SUCCESS)
	{
		FreeSuballocation(out.memory, out.offset, req.size);
		vkDestroyImage(m_device, out.image, nullptr);
		out = AllocatedImage();
		// Was silent, and this one only fails for a genuine driver-side problem, so it is worth knowing
		// about rather than being absorbed into a generic failure.
		ErrorLog("Vulkan: vkBindImageMemory failed for a %ux%u image at offset %llu.",
			width, height, (unsigned long long)boundOffset);
		return false;
	}
	out.size = req.size;
	out.dedicated = false;
	return true;
}

void CVulkanMemory::DestroyImage(AllocatedImage& image)
{
	if (m_device == VK_NULL_HANDLE)
		return;
	if (image.view)
		vkDestroyImageView(m_device, image.view, nullptr);
	if (image.image)
		vkDestroyImage(m_device, image.image, nullptr);
	if (!image.dedicated && image.memory && image.size)
		FreeSuballocation(image.memory, image.offset, image.size);
	if (image.dedicated && image.memory)
		vkFreeMemory(m_device, image.memory, nullptr);
	image = AllocatedImage();
}

} // Vulkan3D
