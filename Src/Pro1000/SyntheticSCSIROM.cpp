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
 * SyntheticSCSIROM.cpp
 *
 * Builds a synthetic SCSI ROM for the Real3D Pro-1000. The format of the SCSI
 * ROM directory and the way that the firmware loads the ROM are described in
 * the "SCSI ROM" section of Pro1000.cpp.
 *
 * The real SCSI ROM contents are unknown (it was likely used to hold an
 * initial scene, perhaps a Real3D logo). Without the logo database, the
 * synthetic ROM has the minimum needed for rend_init to proceed: the magic
 * word, the update memory configuration, and a zeroed directory otherwise,
 * so that every block has offset 0 and size 0 and the view table is empty.
 * The rest of the ROM is erased (0xFF).
 *
 * The Logo Database
 * -----------------
 * The SDK's 3dlogo.cmem and 3dlogo.pmem files contain the logo model. A real
 * ROM was probably made from such data. A .cmem file starts with a header
 * (PRO_Culling_File_Header in API/Include/pro_model.hh), followed by the
 * culling data. A .pmem file contains only the polygon data. All words are
 * big endian. The header words that Supermodel uses (byte offsets):
 *
 *   +0x00  File version. Bits 31:16 are 0x6413 (0x7413 with NEW_MODEL).
 *          Orlando: 0x64130005. Bayou: 0x64130009.
 *   +0x04  Header size in bytes (the culling data starts here).
 *   +0x08  polygon_HW_base_address
 *   +0x0C  polygon_HW_ram_base_address
 *   +0x10  culling_HW_base_address
 *   +0x14  root_node_location (byte offset in the file)
 *   +0x18  Size of the culling data in words.
 *   +0x1C  Size of the polygon data in words.
 *
 * In all of the 575 .cmem files of the SDK, all base addresses are 0 and the
 * root node is the first word of the culling data. Thus the pointers in the
 * data are for culling word 0 and polygon word 0. The SDK relocates the data
 * when it loads a model (for example, Bayou puts its data at culling word
 * 0x40000). The texture maps that the header names are not in the ROM: the
 * ROM cannot load textures.
 *
 * If the Assets folder contains 3dlogo.cmem and 3dlogo.pmem, and the files
 * pass the checks in LoadLogoDatabase(), the culling data becomes the culling
 * block and the polygon data becomes polygon block B. rend_init copies them
 * to culling word 0 (0x8C000000) and polygon word 0 (0x98000000). The data is
 * not relocated, thus the base addresses must be 0.
 *
 * The logo files use 8-word culling nodes (the Model 3 Step 1.0 format). The
 * root node uses matrix 0 (no matrix) and LOD table entry 0. The files do
 * not contain a viewport and the ROM has no view table. Thus the firmware
 * loads the logo but nothing is drawn. If the hypothesis in "Update Memory
 * Configuration" (Pro1000.cpp) is correct, a ROM must put a viewport at
 * culling word 0 and the relocated logo data after it.
 */

#include "SyntheticSCSIROM.h"

#include "Supermodel.h"
#include "OSD/FileSystemPath.h"
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// Size of the directory at the start of the ROM, in bytes
static constexpr uint32_t DIRECTORY_SIZE = 0x100;

/*
 * Adds the logo database (3dlogo.cmem and 3dlogo.pmem in the Assets folder)
 * to the synthetic SCSI ROM. The culling data becomes the culling block and
 * the polygon data becomes polygon block B. Call this after the directory is
 * made. If the two files are not there, nothing changes. If a file is not
 * correct, an error is shown and nothing changes.
 *
 * Parameters:
 *    scsiROM   SCSI ROM image (PCI byte order).
 *    size      Size of the image in bytes.
 *
 * Returns:
 *    True if the logo database was added, otherwise false.
 */
static bool LoadLogoDatabase(uint8_t *scsiROM, size_t size)
{
  const std::string assets = FileSystemPath::GetPath(FileSystemPath::Assets);
  const std::string cmemPath = assets + "3dlogo.cmem";
  const std::string pmemPath = assets + "3dlogo.pmem";

  auto readFile = [](const std::string &path, std::vector<uint8_t> *data) -> bool
  {
    std::ifstream file(path, std::ios::binary);
    if (!file)
      return false;
    data->assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
  };

  std::vector<uint8_t> cmem;
  std::vector<uint8_t> pmem;
  bool haveCMEM = readFile(cmemPath, &cmem);
  bool havePMEM = readFile(pmemPath, &pmem);
  if (!haveCMEM && !havePMEM)
    return false;
  if (!haveCMEM || !havePMEM)
  {
    ErrorLog("Pro-1000: Logo database not loaded: %s is missing.", haveCMEM ? pmemPath.c_str() : cmemPath.c_str());
    return false;
  }

  auto bigEndian32 = [](const uint8_t *p) -> uint32_t
  {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
  };

  // Header words that are used (see "The Logo Database")
  constexpr size_t MIN_HEADER_SIZE = 0x20;
  if (cmem.size() < MIN_HEADER_SIZE || (cmem.size() % 4) != 0 || (pmem.size() % 4) != 0)
  {
    ErrorLog("Pro-1000: Logo database not loaded: file sizes of %s and %s are not correct.", cmemPath.c_str(), pmemPath.c_str());
    return false;
  }
  uint32_t version = bigEndian32(&cmem[0x00]);
  uint32_t headerSize = bigEndian32(&cmem[0x04]);
  uint32_t polygonBase = bigEndian32(&cmem[0x08]);
  uint32_t polygonRAMBase = bigEndian32(&cmem[0x0C]);
  uint32_t cullingBase = bigEndian32(&cmem[0x10]);
  uint32_t cullingWords = bigEndian32(&cmem[0x18]);
  uint32_t polygonWords = bigEndian32(&cmem[0x1C]);

  if ((version >> 16) != 0x6413 && (version >> 16) != 0x7413)
  {
    ErrorLog("Pro-1000: Logo database not loaded: %s has an unknown file version (%08X).", cmemPath.c_str(), version);
    return false;
  }
  if (headerSize < MIN_HEADER_SIZE || (headerSize % 4) != 0 || headerSize > cmem.size() || (cmem.size() - headerSize) / 4 != cullingWords)
  {
    ErrorLog("Pro-1000: Logo database not loaded: the header size or culling data size in %s is not correct.", cmemPath.c_str());
    return false;
  }
  if (pmem.size() / 4 != polygonWords)
  {
    ErrorLog("Pro-1000: Logo database not loaded: the size of %s does not agree with the polygon data size in %s.", pmemPath.c_str(), cmemPath.c_str());
    return false;
  }

  // The firmware copies the blocks to culling and polygon memory word 0, and
  // the data is not relocated. Thus the data must be made for these addresses.
  if (polygonBase != 0 || polygonRAMBase != 0 || cullingBase != 0)
  {
    ErrorLog("Pro-1000: Logo database not loaded: %s has base addresses that are not 0.", cmemPath.c_str());
    return false;
  }

  // Blocks follow the directory
  const uint32_t cullingOffset = DIRECTORY_SIZE;
  const uint32_t cullingSize = cullingWords * 4;
  const uint32_t polygonOffset = cullingOffset + cullingSize;
  const uint32_t polygonSize = polygonWords * 4;
  if (uint64_t(polygonOffset) + polygonSize > size)
  {
    ErrorLog("Pro-1000: Logo database not loaded: it is too large for the SCSI ROM (%u bytes).", cullingSize + polygonSize);
    return false;
  }

  // The image is in PCI byte order: directory and block data are little
  // endian words. The renderer receives each block word as it is in the
  // file.
  auto putLittleEndian32 = [scsiROM](uint32_t offset, uint32_t value)
  {
    for (int b = 0; b < 4; b++)
      scsiROM[offset + b] = uint8_t(value >> (8 * b));
  };
  for (uint32_t i = 0; i < cullingWords; i++)
    putLittleEndian32(cullingOffset + 4 * i, bigEndian32(&cmem[headerSize + 4 * i]));
  for (uint32_t i = 0; i < polygonWords; i++)
    putLittleEndian32(polygonOffset + 4 * i, bigEndian32(&pmem[4 * i]));

  putLittleEndian32(0x24, cullingOffset);
  putLittleEndian32(0x28, cullingSize);
  putLittleEndian32(0x2C, polygonOffset);
  putLittleEndian32(0x30, polygonSize);

  InfoLog("Pro-1000: Logo database: %s (version %08X, %u culling words), %s (%u polygon words).", cmemPath.c_str(), version, cullingWords, pmemPath.c_str(), polygonWords);
  return true;
}

void BuildSyntheticSCSIROM(uint8_t *scsiROM, size_t size)
{
  memset(scsiROM, 0xFF, size);
  memset(scsiROM, 0, DIRECTORY_SIZE);
  const uint32_t magic = 0xCAFEBABE;
  scsiROM[0] = uint8_t(magic >> 0);
  scsiROM[1] = uint8_t(magic >> 8);
  scsiROM[2] = uint8_t(magic >> 16);
  scsiROM[3] = uint8_t(magic >> 24);

  // Update memory configuration (+0x3C, +0x40, +0x44), sent to Mercury by
  // rend_init. The values are the SDK defaults: ping-pong memory of 32K words
  // (PRO_PINGPONG_MEMSIZE, less 4 words) and update memory of 96K words, so
  // ping-pong plus update memory is 128K words. Model 3 Step 1.0 games use the
  // same values. See "SCSI ROM" and "Update Memory Configuration" in
  // Pro1000.cpp.
  const uint32_t updateConfiguration[3] = { 0x7FFC, 0x4, 0x20000 };
  for (int i = 0; i < 3; i++)
  {
    for (int b = 0; b < 4; b++)
      scsiROM[0x3C + 4 * i + b] = uint8_t(updateConfiguration[i] >> (8 * b));  // little endian
  }
  if (LoadLogoDatabase(scsiROM, size))
    InfoLog("Pro-1000: Using synthetic SCSI ROM with the logo database from the Assets folder.");
  else
    InfoLog("Pro-1000: Using synthetic SCSI ROM (magic and update memory configuration only).");
}
