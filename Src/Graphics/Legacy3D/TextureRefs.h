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
 * CTextureRefs.h
 * 
 * Class that tracks unique texture references, eg in a cached model.
 */

#ifndef INCLUDED_TEXTUREREFS_H
#define INCLUDED_TEXTUREREFS_H



namespace Legacy3D {

#define TEXREFS_ARRAY_SIZE 12

// Hash entry that holds a texture reference in the hashset
struct HashEntry
{
	const unsigned texRef;	// Texture reference as a bitfield
	HashEntry *nextEntry;   // Next entry with the same hash
  
	HashEntry(unsigned theTexRef) : texRef(theTexRef), nextEntry(nullptr) { }
};

class CLegacy3D;

class CTextureRefs
{
public:
	/*
	 * CTextureRefs():
	 *
	 * Constructor.
	 */
    CTextureRefs();
    
	/*
	 * ~CTextureRefs():
	 *
	 * Destructor.
	 */
    ~CTextureRefs();

	/*
	 * GetSize():
	 *
	 * Returns number of unique texture references held.
	 */
    unsigned GetSize() const;

	/*
	 * Clear():
	 *
	 * Removes all texture references.
	 */
	void Clear();

	/*
	 * ContainsRef(fmt, x, y, width, height):
	 *
	 * Returns true if holds the given texture reference.
	 */
	bool ContainsRef(unsigned fmt, unsigned x, unsigned y, unsigned width, unsigned height);

	/*
	 * AddRef(fmt, x, y, width, height):
	 *
	 * Adds the given texture reference.  Returns false if it was not possible to add the reference (ie out of memory).
	 */
	bool AddRef(unsigned fmt, unsigned x, unsigned y, unsigned width, unsigned height);

	/*
	 * RemoveRef(fmt, x, y, width, height):
	 *
	 * Removes the given texture reference.  Return true if the reference was found.
	 */
	bool RemoveRef(unsigned fmt, unsigned x, unsigned y, unsigned width, unsigned height);

	/*
	 * RemoveRef(fmt, x, y, width, height):
	 *
	 * Decodes all texture references held, calling CLegacy3D::DecodeTexture for each one.
	 */
	void DecodeAllTextures(CLegacy3D *Render3D);

private:
	// Number of texture references held.
	unsigned m_size;

	// Pre-allocated array used to hold first TEXREFS_ARRAY_SIZE texture references.
	unsigned m_array[TEXREFS_ARRAY_SIZE];

	// Dynamically allocated hashset used to hold texture references when there are more than TEXREFS_ARRAY_SIZE.
	unsigned m_hashCapacity;
	HashEntry **m_hashEntries;
    
	/*
	 * UpdateHashCapacity(hashCapacity)
	 *
	 * Increases capacity of the hashset to given size.
	 */
	bool UpdateHashCapacity(unsigned hashCapacity);

	/*
	 * CreateHashEntry(texRef, hashCapacityUpdated)
	 *
	 * Creates and returns a new hash entry, updating the capacity if required (hashCapacityUpdated is set to true).
	 */
	HashEntry *CreateHashEntry(unsigned texRef, bool &hashCapacityUpdated);

	/*
	 * DeleteHashEntry(entry)
	 *
	 * Deletes the given hash entry and its storage.
	 */
	void DeleteHashEntry(HashEntry *entry);
	
	/*
	 * DeleteAllHashEntries()
	 *
	 * Deletes all hash entries and their storage.
	 */
    void DeleteAllHashEntries();
	
	/*
	 * AddToHash(texRef)
	 *
	 * Adds the given texture reference (as a bitfield) to the hashset.
	 */
	bool AddToHash(unsigned texRef);

	/*
	 * RemoveFromHash(texRef)
	 *
	 * Removes the given texture reference (as a bitfield) from the hashset.
	 */
	bool RemoveFromHash(unsigned texRef);

	/*
	 * HashContains(texRef)
	 *
	 * Returns true if given texture reference (as a bitfield) is held in the hashset.
	 */
    bool HashContains(unsigned texRef) const;
};

} // Legacy3D

#endif	// INCLUDED_TEXTUREREFS_H
