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
