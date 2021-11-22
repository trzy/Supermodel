/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson 
 **
 ** This file is part of Supermodel.
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
 
/*
 * CTextureRefs.cpp
 *
 * Class that tracks unique texture references, eg in a cached model.
 *
 * Texture references are stored internally as a 27-bit field (3 bits for format, 6 bits each for x, y, width & height) to save space.
 * 
 * A pre-allocated array is used for storing up to TEXREFS_ARRAY_SIZE texture references.  When that limit is exceeded, it switches
 * to using a hashset to store the texture references, but this requires extra memory allocation.
 */

#include "TextureRefs.h"

#include "Supermodel.h"
#include "Legacy3D.h"

namespace Legacy3D {

CTextureRefs::CTextureRefs() : m_size(0), m_hashCapacity(0), m_hashEntries(NULL)
{
	//
}

CTextureRefs::~CTextureRefs()
{
	DeleteAllHashEntries();
}

unsigned CTextureRefs::GetSize() const
{
	return m_size;
}

void CTextureRefs::Clear()
{
	// Delete all hash entries
	DeleteAllHashEntries();
	m_size = 0;
	m_hashCapacity = 0;
	m_hashEntries = NULL;
}

bool CTextureRefs::ContainsRef(unsigned fmt, unsigned x, unsigned y, unsigned width, unsigned height)
{
	// Pack texture reference into bitfield
	unsigned texRef = (fmt&7)<<24|(x&0x7E0)<<13|(y&0x7E0)<<7|(width&0x7E0)<<1|(height&0x7E0)>>5;
	
	// Check if using array or hashset
	if (m_size <= TEXREFS_ARRAY_SIZE)
	{
		// See if texture reference held in array
		for (unsigned i = 0; i < m_size; i++)
		{
			if (texRef == m_array[i])
				return true;
		}
		return false;
	}
	else
		// See if texture reference held in hashset
		return HashContains(texRef);
}

bool CTextureRefs::AddRef(unsigned fmt, unsigned x, unsigned y, unsigned width, unsigned height)
{
	// Pack texture reference into bitfield
	unsigned texRef = (fmt&7)<<24|(x&0x7E0)<<13|(y&0x7E0)<<7|(width&0x7E0)<<1|(height&0x7E0)>>5;

	// Check if using array or hashset
	if (m_size <= TEXREFS_ARRAY_SIZE)
	{
		// See if already held in array, if so nothing to do
		for (unsigned i = 0; i < m_size; i++)
		{
			if (texRef == m_array[i])
				return true;
		}
		// If not, check if array is full
		if (m_size == TEXREFS_ARRAY_SIZE)
		{
			// If so, set initial hashset capacity to 47 to initialize it
			UpdateHashCapacity(47);
			// Copy array into hashset
			for (unsigned i = 0; i < TEXREFS_ARRAY_SIZE; i++)
				AddToHash(m_array[i]);
			// Add texture reference to hashset
			AddToHash(texRef);
		}
		else
		{
			// Add texture reference to array
			m_array[m_size] = texRef;
			m_size++;
		}
		return true;
	}
	else
		// Add texture reference to hashset
		return AddToHash(texRef);
}

bool CTextureRefs::RemoveRef(unsigned fmt, unsigned x, unsigned y, unsigned width, unsigned height)
{
	// Pack texture reference into bitfield
	unsigned texRef = (fmt&7)<<24|(x&0x7E0)<<13|(y&0x7E0)<<7|(width&0x7E0)<<1|(height&0x7E0)>>5;

	// Check if using array or hashset
	if (m_size <= TEXREFS_ARRAY_SIZE)
	{
		for (unsigned i = 0; i < m_size; i++)
		{
			if (texRef == m_array[i])
			{
				for (unsigned j = i + 1; j < m_size; j++)
					m_array[j - 1] = m_array[j];
				m_size--;
				return true;
			}
		}
		return false;
	}
	else 
	{
		// Remove texture reference from hashset
		bool removed = RemoveFromHash(texRef);

		// See if should switch back to array
		if (m_size == TEXREFS_ARRAY_SIZE)
		{
			// Loop through all hash entries and copy texture references into array
			unsigned j = 0;
			for (unsigned i = 0; i < m_hashCapacity; i++)
			{
				for (HashEntry *entry = m_hashEntries[i]; entry; entry = entry->nextEntry)
					m_array[j++] = entry->texRef;
			}
			// Delete all hash entries
			DeleteAllHashEntries();
		}
		return removed;
	}
}

void CTextureRefs::DecodeAllTextures(CLegacy3D *Render3D)
{
	// Check if using array or hashset
	if (m_size <= TEXREFS_ARRAY_SIZE)
	{
		// Loop through elements in array and call CLegacy3D::DecodeTexture
		for (unsigned i = 0; i < m_size; i++)
		{
			// Unpack texture reference from bitfield 
			unsigned texRef = m_array[i];
			unsigned fmt = texRef>>24;
			unsigned x = (texRef>>13)&0x7E0;
			unsigned y = (texRef>>7)&0x7E0;
			unsigned width = (texRef>>1)&0x7E0;
			unsigned height = (texRef<<5)&0x7E0;
			Render3D->DecodeTexture(fmt, x, y, width, height);
		}
	}
	else
	{
		// Loop through all hash entriesa and call CLegacy3D::DecodeTexture
		for (unsigned i = 0; i < m_hashCapacity; i++)
		{
			for (HashEntry *entry = m_hashEntries[i]; entry; entry = entry->nextEntry)
			{
				// Unpack texture reference from bitfield 
				unsigned texRef = entry->texRef;
				unsigned fmt = texRef>>24;
				unsigned x = (texRef>>13)&0x7E0;
				unsigned y = (texRef>>7)&0x7E0;
				unsigned width = (texRef>>1)&0x7E0;
				unsigned height = (texRef<<5)&0x7E0;
				Render3D->DecodeTexture(fmt, x, y, width, height);
			}
		}
	}
}

bool CTextureRefs::UpdateHashCapacity(unsigned capacity)
{
	unsigned oldCapacity = m_hashCapacity;
	HashEntry **oldEntries = m_hashEntries;
	// Update capacity and create new empty entries array
	m_hashCapacity = capacity;
	m_hashEntries = new(std::nothrow) HashEntry*[capacity];
	if (!m_hashEntries)
		return false;
	memset(m_hashEntries, 0, capacity * sizeof(HashEntry*));
	if (oldEntries)
	{
		// Redistribute entries into new entries array
		for (unsigned i = 0; i < oldCapacity; i++)
		{
			HashEntry *entry = oldEntries[i];
			while (entry)
			{
				HashEntry *nextEntry = entry->nextEntry;
				unsigned hash = entry->texRef % capacity;
				entry->nextEntry = m_hashEntries[hash];
				m_hashEntries[hash] = entry;
				entry = nextEntry;
			}
		}
		// Delete old entries array
		delete[] oldEntries;
	}
	return true;
}

HashEntry *CTextureRefs::CreateHashEntry(unsigned texRef, bool &hashCapacityUpdated)
{
	// Update size and increase hash capacity if required
	m_size++;
	hashCapacityUpdated = m_size >= m_hashCapacity;
	if (hashCapacityUpdated)
	{
		if (m_hashCapacity < 89)
			UpdateHashCapacity(89); // Capacity of 89 gives good sequence of mostly prime capacities (89, 179, 359, 719, 1439, 2879 etc)
		else
			UpdateHashCapacity(2 * m_hashCapacity + 1);
	}
	return new(std::nothrow) HashEntry(texRef);
}

void CTextureRefs::DeleteHashEntry(HashEntry *entry)
{
	// Update size and delete hash entry
	m_size--;
	delete entry;
}

void CTextureRefs::DeleteAllHashEntries()
{
	if (!m_hashEntries)
		return;
	// Delete all hash entries and their storage
	for (unsigned i = 0; i < m_hashCapacity; i++)
	{
		HashEntry *entry = m_hashEntries[i];
		if (entry)
			delete entry;
	}
	delete[] m_hashEntries;
}

bool CTextureRefs::AddToHash(unsigned texRef)
{
	// Convert texture reference to hash value
	unsigned hash = texRef % m_hashCapacity;
	// Loop through linked list for hash value and see if have texture reference already
	HashEntry *headEntry = m_hashEntries[hash];
	HashEntry *entry = headEntry;
	while (entry && texRef != entry->texRef)
		entry = entry->nextEntry;
	// If found, nothing to do
	if (entry)
		return true;
	// Otherwise, create new hash entry for texture reference
	bool hashCapacityUpdated;
	entry = CreateHashEntry(texRef, hashCapacityUpdated);
	// If couldn't create entry (ie out of memory), let caller know
	if (!entry)
		return false;
	if (hashCapacityUpdated)
	{
		// If hash capacity was increased recalculate hash value
		hash = texRef % m_hashCapacity;
		headEntry = m_hashEntries[hash];
	}
	// Store hash entry in linked list for hash value
	entry->nextEntry = headEntry;
	m_hashEntries[hash] = entry;
	return true;
}

bool CTextureRefs::RemoveFromHash(unsigned texRef)
{
	// Convert texture reference to hash value
	unsigned hash = texRef % m_hashCapacity;
	// Loop through linked list for hash value and see if have texture reference
	HashEntry *entry = m_hashEntries[hash];
	HashEntry *prevEntry = NULL;
	while (entry && texRef != entry->texRef)
	{
		prevEntry = entry;
		entry = entry->nextEntry;
    }
	// If not found, nothing to do
	if (!entry)
		return false;
	// Otherwise, remove entry from linked list for hash value
	if (prevEntry)	
		prevEntry->nextEntry = entry->nextEntry;
	else
		m_hashEntries[hash] = entry->nextEntry;
	// Delete hash entry storage
	DeleteHashEntry(entry);
	return true;
}

bool CTextureRefs::HashContains(unsigned texRef) const
{
	// Convert texture reference to hash value
	unsigned hash = texRef % m_hashCapacity;
	// Loop through linked list for hash value and see if have texture reference
	HashEntry *entry = m_hashEntries[hash];
	while (entry && texRef != entry->texRef)
		entry = entry->nextEntry;
	return !!entry;
}

} // Legacy3D
