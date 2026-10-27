// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 The Supermodel-VK authors
// Copyright 2003-2026 The Supermodel Team.
// Source and license details: Docs/Vulkan.md and Docs/LICENSE.txt.
#ifndef INCLUDED_VULKAN3D_RING_H
#define INCLUDED_VULKAN3D_RING_H

#include "VulkanMemory.h"

namespace Vulkan3D {

class CVulkanRingBuffer
{
public:
	bool Init(CVulkanMemory& memory, uint32_t frames, VkDeviceSize sliceBytes);
	void Shutdown(CVulkanMemory& memory);
	void BeginFrame(uint32_t frame);
	bool Alloc(VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize& offset, void*& ptr);
	VkBuffer Buffer() const { return m_buffer.buffer; }
	VkDeviceSize SliceBytes() const { return m_slice; }

private:
	AllocatedBuffer m_buffer;
	uint32_t m_frames = 0;
	uint32_t m_frame = 0;
	VkDeviceSize m_slice = 0;
	VkDeviceSize m_cursor = 0;
};

} // Vulkan3D

#endif
