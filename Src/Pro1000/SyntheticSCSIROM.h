/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * SyntheticSCSIROM.h
 *
 * Builds a synthetic SCSI ROM for the Real3D Pro-1000 when no ROM image is
 * available. See SyntheticSCSIROM.cpp.
 */

#ifndef INCLUDED_SYNTHETICSCSIROM_H
#define INCLUDED_SYNTHETICSCSIROM_H

#include <cstddef>
#include <cstdint>

/*
 * BuildSyntheticSCSIROM(scsiROM, size):
 *
 * Makes a synthetic SCSI ROM image. The image has the magic word, the update
 * memory configuration, and (if the Assets folder contains 3dlogo.cmem and
 * 3dlogo.pmem) the logo database. All other bytes are 0xFF. The image is in
 * PCI byte order (little endian words), as the real ROM is. At boot, the
 * firmware sends the update memory configuration to the Real3D configuration
 * registers. Thus this ROM sets them to valid values.
 *
 * Parameters:
 *    scsiROM   Buffer for the image.
 *    size      Size of the buffer in bytes. It must be at least 256 bytes
 *              (the directory).
 */
void BuildSyntheticSCSIROM(uint8_t *scsiROM, size_t size);

#endif  // INCLUDED_SYNTHETICSCSIROM_H
