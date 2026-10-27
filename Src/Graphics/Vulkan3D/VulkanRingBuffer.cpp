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
#include "VulkanRingBuffer.h"
#include "OSD/Logger.h"

namespace Vulkan3D {

bool CVulkanRingBuffer::Init(CVulkanMemory& memory, uint32_t frames, VkDeviceSize sliceBytes)
{
	m_frames = frames;
	m_slice = sliceBytes;
	m_frame = 0;
	m_cursor = 0;
	VkBufferUsageFlags usage =
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (!memory.CreateBuffer(sliceBytes * frames, usage,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_buffer))
	{
		ErrorLog("Vulkan: failed to allocate the per-frame ring buffer.");
		return false;
	}
	return true;
}

void CVulkanRingBuffer::Shutdown(CVulkanMemory& memory)
{
	memory.DestroyBuffer(m_buffer);
	m_frames = 0;
	m_slice = 0;
	m_cursor = 0;
}

void CVulkanRingBuffer::BeginFrame(uint32_t frame)
{
	m_frame = frame;
	m_cursor = 0;
}

bool CVulkanRingBuffer::Alloc(VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize& offset, void*& ptr)
{
	if (alignment == 0)
		alignment = 1;
	VkDeviceSize aligned = (m_cursor + alignment - 1) & ~(alignment - 1);
	if (aligned + size > m_slice)
	{
		ErrorLog("Vulkan: ring buffer exhausted (need %llu, %llu left).",
			(unsigned long long)size, (unsigned long long)(m_slice > aligned ? m_slice - aligned : 0));
		return false;
	}
	offset = m_frame * m_slice + aligned;
	ptr = static_cast<uint8_t*>(m_buffer.mapped) + offset;
	m_cursor = aligned + size;
	return true;
}

} // Vulkan3D
