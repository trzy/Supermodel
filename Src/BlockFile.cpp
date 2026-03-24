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
 * BlockFile.cpp
 *
 * Block format container file management. Implementation of the CBlockFile
 * class.
 */

#include "BlockFile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "Supermodel.h"


/******************************************************************************
 Output Functions
******************************************************************************/

void CBlockFile::ReadString(std::string *str, uint32_t length)
{
  if (backend == Backend::None)
    return;

  str->clear();

  bool keep_loading = true;
  for (uint32_t i = 0; i < length; i++)
  {
    char c = 0;
    if (ReadBytes(&c, sizeof(c)) != sizeof(c))
      break;

    if (keep_loading)
    {
      if (!c)
        keep_loading = false;
      else
        *str += c;
    }
  }
}

unsigned CBlockFile::ReadBytes(void *data, uint32_t numBytes)
{
  if (backend == Backend::File)
  {
    if (NULL == fp)
      return 0;
    return fread(data, sizeof(uint8_t), numBytes, fp);
  }

  if (backend == Backend::MemoryRead)
  {
    if (memoryPos >= memoryData.size())
      return 0;

    const size_t remaining = memoryData.size() - memoryPos;
    const size_t bytesToRead = std::min<size_t>(numBytes, remaining);
    if (bytesToRead == 0)
      return 0;

    std::memcpy(data, memoryData.data() + memoryPos, bytesToRead);
    memoryPos += bytesToRead;
    return static_cast<unsigned>(bytesToRead);
  }

  return 0;
}

unsigned CBlockFile::ReadDWord(uint32_t *data)
{
  return ReadBytes(data, sizeof(uint32_t));
}
  
void CBlockFile::UpdateBlockSize(void)
{
  long int curPos;
  uint32_t newBlockSize;

  if (backend == Backend::File)
  {
    if (NULL == fp)
      return;

    curPos = ftell(fp); // save current file position
    fseek(fp, blockStartPos, SEEK_SET);
    newBlockSize = static_cast<uint32_t>(curPos - blockStartPos);
    fwrite(&newBlockSize, sizeof(uint32_t), 1, fp);
    fseek(fp, curPos, SEEK_SET); // go back
    return;
  }

  if (backend == Backend::MemoryWrite)
  {
    curPos = static_cast<long int>(memoryPos);
    if (blockStartPos < 0)
      return;

    const size_t blockPos = static_cast<size_t>(blockStartPos);
    if (blockPos + sizeof(uint32_t) > memoryData.size())
      return;

    newBlockSize = static_cast<uint32_t>(curPos - blockStartPos);
    std::memcpy(memoryData.data() + blockPos, &newBlockSize, sizeof(newBlockSize));
  }
}

void CBlockFile::WriteByte(uint8_t data)
{
  WriteBytes(&data, sizeof(data));
}

void CBlockFile::WriteDWord(uint32_t data)
{
  WriteBytes(&data, sizeof(data));
}

void CBlockFile::WriteBytes(const void *data, uint32_t numBytes)
{
  if (backend == Backend::File)
  {
    if (NULL == fp)
      return;
    fwrite(data, sizeof(uint8_t), numBytes, fp);
    UpdateBlockSize();
    return;
  }

  if (backend == Backend::MemoryWrite)
  {
    const size_t start = memoryPos;
    const size_t end = start + numBytes;
    if (end > memoryData.size())
      memoryData.resize(end);

    std::memcpy(memoryData.data() + start, data, numBytes);
    memoryPos = end;
    fileSize = static_cast<long int>(memoryData.size());
    UpdateBlockSize();
  }
}

void CBlockFile::WriteBlockHeader(const std::string &name, const std::string &comment)
{
  if (backend == Backend::None)
    return;
  
  // Record current block starting position
  if (backend == Backend::File)
    blockStartPos = ftell(fp);
  else
    blockStartPos = static_cast<long int>(memoryPos);

  // Write the total block length field
  WriteDWord(0);  // will be automatically updated as we write the file
  
  // Write name and comment lengths
  WriteDWord(name.size() + 1);
  WriteDWord(comment.size() + 1);
  Write(name);
  Write(comment);
  
  // Record the start of the current data section
  if (backend == Backend::File)
    dataStartPos = ftell(fp);
  else
    dataStartPos = static_cast<long int>(memoryPos);
} 


/******************************************************************************
 Block Format Container File Implementation
 
 Files are just a consecutive array of blocks that must be searched.
 
 Block Format
 ------------
 blockLength  (uint32_t)  Total length of block in bytes.
 nameLength   (uint32_t)  Length of name field including terminating 0 (up to
              1025).
 commentLength  (uint32_t)  Same as above, but for comment string.
 name     ...     Name string (null-terminated, up to 1025 bytes).
 comment    ...     Comment string (same as above).
 data     ...     Raw data (blockLength - total header size).
******************************************************************************/

unsigned CBlockFile::Read(void *data, uint32_t numBytes)
{
  if (mode == 'r')
    return ReadBytes(data, numBytes);
  return 0;
}

unsigned CBlockFile::Read(bool *value)
{
  uint8_t byte;
  unsigned numBytes = Read(&byte, sizeof(byte));
  if (numBytes >  0)
    *value = byte != 0;
  return numBytes;
}

void CBlockFile::Write(const void *data, uint32_t numBytes)
{
  if (mode == 'w')
    WriteBytes(data, numBytes);
}

void CBlockFile::Write(bool value)
{
  uint8_t byte = value ? 1 : 0;
  Write(&byte, sizeof(byte));
}

void CBlockFile::Write(const std::string &str)
{
  if (mode == 'w')
    WriteBytes(str.c_str(), str.length() + 1);
}

void CBlockFile::NewBlock(const std::string &name, const std::string &comment)
{
  if (mode == 'w')
    WriteBlockHeader(name, comment);
}

Result CBlockFile::FindBlock(const std::string &name)
{
  if (mode != 'r')
    return Result::FAIL;

  if (backend == Backend::File)
  {
    if (fp == NULL)
      return Result::FAIL;
    fseek(fp, 0, SEEK_SET);
  }
  else if (backend == Backend::MemoryRead)
  {
    memoryPos = 0;
  }
  else
  {
    return Result::FAIL;
  }
  
  long int  curPos = 0;
  while (curPos < fileSize)
  {
    blockStartPos = curPos;
    
    // Read header
    uint32_t block_length;
    uint32_t name_length;
    uint32_t comment_length;
    curPos += ReadDWord(&block_length);
    curPos += ReadDWord(&name_length);
    curPos += ReadDWord(&comment_length);
    std::string block_name;
    ReadString(&block_name, name_length);
    
    // Is this the block we want?
    if (block_name == name)
    {
      const long int nextDataPos = blockStartPos + 12 + name_length + comment_length;
      if (backend == Backend::File)
      {
        fseek(fp, nextDataPos, SEEK_SET); // move to beginning of data
        dataStartPos = ftell(fp);
      }
      else
      {
        memoryPos = static_cast<size_t>(nextDataPos);
        dataStartPos = nextDataPos;
      }
      return Result::OKAY;
    }
    
    // Move to next block
    curPos = blockStartPos + block_length;
    if (backend == Backend::File)
      fseek(fp, curPos, SEEK_SET);
    else
      memoryPos = static_cast<size_t>(curPos);
    if (block_length == 0)  // this would never advance
      break;
  }
  
  return Result::FAIL;
}

Result CBlockFile::Create(const std::string &file, const std::string &headerName, const std::string &comment)
{
  Close();

  fp = fopen(file.c_str(), "wb");
  if (NULL == fp)
    return Result::FAIL;

  backend = Backend::File;
  memoryData.clear();
  memoryPos = 0;
  blockStartPos = 0;
  dataStartPos = 0;
  fileSize = 0;
  mode = 'w';
  WriteBlockHeader(headerName, comment);
  return Result::OKAY;
}
  
Result CBlockFile::Load(const std::string &file)
{
  Close();

  fp = fopen(file.c_str(), "rb");
  if (NULL == fp)
    return Result::FAIL;

  backend = Backend::File;
  memoryData.clear();
  memoryPos = 0;
  blockStartPos = 0;
  dataStartPos = 0;
  mode = 'r';
  
  // TODO: is this a valid block file?
  
  // Get the file size
  fseek(fp, 0, SEEK_END);
  fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  
  return Result::OKAY;
}

Result CBlockFile::CreateMemory(const std::string &headerName, const std::string &comment)
{
  Close();

  backend = Backend::MemoryWrite;
  mode = 'w';
  fileSize = 0;
  blockStartPos = 0;
  dataStartPos = 0;
  memoryPos = 0;
  memoryData.clear();

  WriteBlockHeader(headerName, comment);
  return Result::OKAY;
}

Result CBlockFile::LoadMemory(const void *data, size_t size)
{
  if (data == nullptr && size > 0)
    return Result::FAIL;

  Close();

  backend = Backend::MemoryRead;
  mode = 'r';
  fileSize = static_cast<long int>(size);
  blockStartPos = 0;
  dataStartPos = 0;
  memoryPos = 0;
  memoryData.resize(size);
  if (size > 0)
    std::memcpy(memoryData.data(), data, size);

  return Result::OKAY;
}

const uint8_t *CBlockFile::GetMemoryData(void) const
{
  if (memoryData.empty())
    return nullptr;
  return memoryData.data();
}

size_t CBlockFile::GetMemorySize(void) const
{
  return memoryData.size();
}
  
void CBlockFile::Close(void)
{
  if (backend == Backend::File && fp != NULL)
    fclose(fp);

  fp = NULL;
  backend = Backend::None;
  memoryPos = 0;
  mode = 0;
  fileSize = 0;
  blockStartPos = 0;
  dataStartPos = 0;
}

CBlockFile::CBlockFile(void)
{
  backend = Backend::None;
  fp = NULL;
  memoryPos = 0;
  mode = 0;   // neither reading nor writing (do nothing)
  fileSize = 0;
  blockStartPos = 0;
  dataStartPos = 0;
}

CBlockFile::~CBlockFile(void)
{
  if (backend == Backend::File && fp != NULL) // in case user forgot
    fclose(fp);

  fp = NULL;
  backend = Backend::None;
  memoryPos = 0;
  mode = 0;
  fileSize = 0;
  blockStartPos = 0;
  dataStartPos = 0;
}
