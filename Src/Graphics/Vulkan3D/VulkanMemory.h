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
#ifndef INCLUDED_VULKAN3D_MEMORY_H
#define INCLUDED_VULKAN3D_MEMORY_H

#include "VulkanCommon.h"
#include <vector>

namespace Vulkan3D {

struct AllocatedBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDeviceSize offset = 0;
	VkDeviceSize size = 0;
	void* mapped = nullptr;
	bool dedicated = true;
};

struct AllocatedImage
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDeviceSize offset = 0;
	VkDeviceSize size = 0;
	VkFormat format = VK_FORMAT_UNDEFINED;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mipLevels = 1;
	uint32_t samples = 1;       // VK_SAMPLE_COUNT_1_BIT unless this is a multisampled target
	bool dedicated = false;
};

class CVulkanMemory
{
public:
	bool Init(VkPhysicalDevice physical, VkDevice device);
	void Shutdown();

	bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, AllocatedBuffer& out);
	void DestroyBuffer(AllocatedBuffer& buffer);

	// samples is a VkSampleCountFlagBits value. Multisampled targets cannot be sampled, copied or
	// blitted directly: render into one and resolve it into a single-sampled image for that.
	bool CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, AllocatedImage& out, uint32_t samples = 1);
	void DestroyImage(AllocatedImage& image);

	VkDevice Device() const { return m_device; }

	/*
	 * How much device memory this allocator holds in total, and how much of it is still unused.
	 *
	 * Both are reported whenever a new block is created, which is the only moment the allocator asks
	 * the driver for memory. That makes the total a running high-water mark, and it turns "did raising
	 * the resolution or turning MSAA on push us over the edge?" from a guess into a number in the log.
	 *
	 * The free figure is the more interesting one when something fails: a large total with plenty free
	 * means a request could not be satisfied by fragmentation or by one oversized allocation, which is
	 * a different problem - and a different fix - from genuinely having no memory left.
	 */
	VkDeviceSize TotalAllocated() const;
	VkDeviceSize TotalFree() const;

private:
	struct Block
	{
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize size = 0;
		uint32_t typeIndex = 0;
		struct Span
		{
			VkDeviceSize offset;
			VkDeviceSize size;
		};
		std::vector<Span> free;
	};

	uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
	bool Suballocate(uint32_t typeIndex, VkDeviceSize size, VkDeviceSize alignment, VkDeviceMemory& memory, VkDeviceSize& offset);
	void FreeSuballocation(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);

	VkPhysicalDevice m_physical = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkPhysicalDeviceMemoryProperties m_props = {};
	std::vector<Block> m_blocks;
};

} // Vulkan3D

#endif
