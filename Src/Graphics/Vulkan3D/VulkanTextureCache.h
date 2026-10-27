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
#ifndef INCLUDED_VULKAN3D_TEXTURECACHE_H
#define INCLUDED_VULKAN3D_TEXTURECACHE_H

#include "VulkanContext.h"
#include <bitset>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Vulkan3D {

class CVulkanTextureCache
{
public:
	static constexpr int kPageWidth = 2048;
	static constexpr int kPageHeight = 1024;
	static constexpr int kTile = 32;
	static constexpr int kTilesX = kPageWidth / kTile;
	static constexpr int kTilesY = kPageHeight / kTile;

	bool Init(CVulkanContext& context, bool diagnostics = false);
	void Shutdown();
	void AttachMemory(const uint16_t* textureRam);

	// Thread-safe. Marks 32x32 tiles dirty; does not call Vulkan.
	void UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height);

	// Returns a bindless slot. Creates the image if this key is new.
	uint32_t Acquire(int page, int x, int y, int width, int height, int format, bool inverted);
	void SetFrameSerial(uint64_t frame) { m_frameSerial = frame; }
	void MarkUsed(uint32_t slot, uint64_t frameSerial);

	/*
	 * How much of a sampled region had ever reached the GPU.
	 *
	 * Three cases rather than a yes/no, because the middle one is not evidence of anything: a texture
	 * is uploaded in the blocks the game writes, so its outer tiles can legitimately never be touched
	 * if they are never sampled. Only None means the sampler reads untouched memory for the whole
	 * region, which is the case that shows up as a black surface. An earlier yes/no version counted
	 * Partial as unuploaded and reported 30% of draws in a scene that plainly looked right, which is
	 * how a diagnostic stops being read.
	 */
	enum class Coverage { Full, Partial, None };

	/*
	 * Classifies a texture region by how much of it has made it to the GPU.
	 *
	 * The raw page image starts out undefined and is only filled for tiles the emulated game has
	 * written and told us about. A tile that never made it across samples as zero, which is black, and
	 * because textures are typically written once at level load nothing later will fix it - redrawing
	 * the area cannot help.
	 *
	 * So this answers, for a region the geometry is about to sample, whether that has happened. It
	 * separates the failures that look identical on screen: a texture that was never uploaded (black,
	 * and a bug on this side) from a surface that really is dark (a lighting or modulation problem, and
	 * the polygon data's fault). Coordinates are page-local, the same space the fragment shader
	 * addresses in FetchTexel().
	 */
	Coverage RegionCoverage(int page, int x, int y, int width, int height) const;

	/*
	 * How many of a region's tiles the game has ever asked to upload, whether or not the copy reached
	 * the GPU.
	 *
	 * This is the question that separates the two failures which look identical: a region the game
	 * never wrote at all - in which case sampling it and getting black is faithful behaviour, and the
	 * fault is elsewhere or not a fault - from one it wrote and the uploader lost. Comparing this
	 * against what arrived answers it without needing a second renderer to compare against.
	 */
	unsigned RegionEverWrittenTiles(int page, int x, int y, int width, int height) const;

	// Total tiles covering a region, so the figure above can be read as a fraction. Both are needed
	// because "0 of 64 written" and "0 of 4 written" mean different things.
	unsigned RegionTileCount(int page, int x, int y, int width, int height) const;

	// Called once per textured material drawn, so the periodic report can say how many draws sampled a
	// region that had never been uploaded, how many sampled a partly filled one, and how many sampled
	// fully uploaded data.
	void NoteTexturedReference(Coverage coverage);

	// Records raw-page uploads and decodes stale slots into the open command buffer.
	void RecordSync(VkCommandBuffer cmd, CVulkanRingBuffer& ring, uint64_t frameSerial);

	void WriteDynamicBuffers(VkBuffer viewportBuffer, VkDeviceSize viewportRange, VkBuffer materialBuffer, VkDeviceSize materialRange);

	VkDescriptorSetLayout Layout() const { return m_layout; }
	VkDescriptorSet Set() const { return m_set; }
	VkPipelineLayout ComputeLayout() const { return m_computeLayout; }

private:
	struct Rect
	{
		int x, y, w, h;
	};
	struct Slot
	{
		uint64_t key = 0;
		AllocatedImage image;
		std::vector<VkImageView> mipViews;
		std::vector<Rect> coverage;
		int page = 0;
		int format = 0;
		bool inverted = false;
		bool stale = true;
		bool live = false;
		bool inShaderRead = false;
		uint64_t lastUsed = 0;
		int x = 0, y = 0, width = 0, height = 0;
	};

	bool CreateDescriptors();
	bool CreateRawPages();
	bool CreateComputePipeline();
	uint32_t AllocateSlot(uint64_t key, int page, int x, int y, int width, int height, int format, bool inverted);
	void DestroySlot(Slot& slot);
	void EvictOne(uint64_t frameSerial);
	void WriteImageDescriptor(uint32_t slot);
	void RecordRawUploads(VkCommandBuffer cmd, CVulkanRingBuffer& ring, const std::bitset<kTilesX * kTilesY> dirty[2]);
	void RecordDecode(VkCommandBuffer cmd, uint64_t frameSerial);
	void CpuDecode(VkCommandBuffer cmd, CVulkanRingBuffer& ring, Slot& slot);
	void ReportUploads();

	/*
	 * Guard for the one fault a dirty-bit uploader can have: a tile whose contents change without
	 * anything marking it. Such a tile is never re-sent, so the GPU keeps stale data for good - and
	 * because it is invisible to every counter that watches uploads, it cannot be found by asking how
	 * much was uploaded, only by comparing.
	 *
	 * A checksum per tile of what the GPU is believed to hold is compared against the live contents.
	 * Tiles are covered a slice at a time so the cost stays negligible, and a tile that is dirty this
	 * frame is skipped because it is about to be refreshed anyway.
	 */
	void VerifyDirtyCoverage(const std::bitset<kTilesX * kTilesY> dirty[2], uint64_t frameSerial);
	uint64_t TileChecksum(int page, int tileX, int tileY) const;
	static uint64_t MakeKey(int page, int x, int y, int width, int height, int format, bool inverted);

	CVulkanContext* m_context = nullptr;
	const uint16_t* m_textureRam = nullptr;
	std::mutex m_dirtyMutex;
	std::bitset<kTilesX * kTilesY> m_dirty[2];

	// Tiles that have actually been copied into a raw page image at least once. Deliberately updated
	// where the copy is recorded rather than where the upload is requested, so that a deferred upload
	// stays "never uploaded" until it really happens - otherwise the diagnostic would clear itself for
	// exactly the tiles it exists to find.
	std::bitset<kTilesX * kTilesY> m_everUploaded[2];

	/*
	 * Tiles the game has ever asked us to upload, whether or not the copy got there.
	 *
	 * Kept alongside m_everUploaded rather than derived from it, because the interesting case is the
	 * one where the two disagree: written but never uploaded is a bug in this file, never written at
	 * all means the game is sampling a region it did not fill and the black is faithful.
	 */
	std::bitset<kTilesX * kTilesY> m_everDirty[2];

	// What the GPU is believed to be holding, per tile. Written when a copy is recorded and compared
	// by VerifyDirtyCoverage() to catch tiles that changed without being marked.
	std::vector<uint64_t> m_gpuChecksum[2];

	// Counters behind the periodic report. Cheap to maintain and the only way to tell, after the fact,
	// whether texture data was arriving at all during a scene that rendered wrong.
	//
	// The draws that sample a region are split three ways rather than two, because "the region is not
	// fully uploaded" and "nothing in the region was ever uploaded" are completely different claims.
	// A large texture legitimately has unvisited corners, so counting it as unuploaded inflates the
	// figure until it stops meaning anything - the earlier single-counter version reported 30% of draws
	// in a scene that plainly looked mostly right. Only the None count is a statement about black.
	uint64_t m_uploadRequests = 0;
	uint64_t m_droppedRequests = 0;
	uint64_t m_tilesUploaded = 0;
	// Whole-page uploads, which is how the ROM's own texture data arrives. Worth counting because the
	// entire ROM-supplied texture set depends on this one call: if it never comes, or comes before
	// there is anywhere to put it, the pages stay empty and almost everything looks black.
	uint64_t m_bulkUploads = 0;
	bool m_diagnostics = false;
	uint64_t m_frames = 0;
	uint64_t m_texturedRefs = 0;
	uint64_t m_coverageFull = 0;
	uint64_t m_coveragePartial = 0;
	uint64_t m_coverageNone = 0;
	uint64_t m_coverageNoneInWindow = 0;
	uint64_t m_coveragePartialInWindow = 0;
	uint64_t m_staleTiles = 0;
	uint64_t m_staleReports = 0;
	// How many frames found texture writes waiting with no texture RAM attached. Should be zero in
	// normal running; anything else means the window in which uploads were held back.
	uint64_t m_noMemoryReports = 0;

	VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
	VkDescriptorPool m_pool = VK_NULL_HANDLE;
	VkDescriptorSet m_set = VK_NULL_HANDLE;
	VkSampler m_samplers[16] = {};
	VkSampler m_rawSampler = VK_NULL_HANDLE;

	AllocatedImage m_raw[2];
	VkImageLayout m_rawLayout[2] = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED };

	VkDescriptorSetLayout m_computeSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout m_computeLayout = VK_NULL_HANDLE;
	VkPipeline m_computePipeline = VK_NULL_HANDLE;
	VkShaderModule m_computeModule = VK_NULL_HANDLE;
	bool m_gpuDecode = false;
	uint64_t m_frameSerial = 0;

	std::vector<Slot> m_slots;
	std::unordered_map<uint64_t, uint32_t> m_lookup;
};

} // Vulkan3D

#endif
