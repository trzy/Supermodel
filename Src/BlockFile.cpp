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

#include <cstdio>
#include <cstring>
#include <cstdint>
#include "Supermodel.h"


/******************************************************************************
 Output Functions
******************************************************************************/

void CBlockFile::ReadString(std::string *str, uint32_t length)
{
  if (NULL == fp)
    return;
  str->clear();
  //TODO: use fstream to get rid of this ugly hack
  bool keep_loading = true;
  for (size_t i = 0; i < length; i++)
  {
    char c;
    fread(&c, sizeof(char), 1, fp);
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
  if (NULL == fp)
    return 0;
  return fread(data, sizeof(uint8_t), numBytes, fp);
}

unsigned CBlockFile::ReadDWord(uint32_t *data)
{
  if (NULL == fp)
    return 0;
  fread(data, sizeof(uint32_t), 1, fp);
  return 4;
}
  
void CBlockFile::UpdateBlockSize(void)
{
  long int  curPos;
  unsigned  newBlockSize;
  
  if (NULL == fp)
    return;
  curPos = ftell(fp);       // save current file position
  fseek(fp, blockStartPos, SEEK_SET);
  newBlockSize = curPos - blockStartPos;
  fwrite(&newBlockSize, sizeof(uint32_t), 1, fp);
  fseek(fp, curPos, SEEK_SET);  // go back
}

void CBlockFile::WriteByte(uint8_t data)
{
  if (NULL == fp)
    return;
  fwrite(&data, sizeof(uint8_t), 1, fp);
  UpdateBlockSize();
}

void CBlockFile::WriteDWord(uint32_t data)
{
  if (NULL == fp)
    return;
  fwrite(&data, sizeof(uint32_t), 1, fp);
  UpdateBlockSize();
}

void CBlockFile::WriteBytes(const void *data, uint32_t numBytes)
{
  if (NULL == fp)
    return;
  fwrite(data, sizeof(uint8_t), numBytes, fp);
  UpdateBlockSize();
}

void CBlockFile::WriteBlockHeader(const std::string &name, const std::string &comment)
{
  if (NULL == fp)
    return;
  
  // Record current block starting position
  blockStartPos = ftell(fp);

  // Write the total block length field
  WriteDWord(0);  // will be automatically updated as we write the file
  
  // Write name and comment lengths
  WriteDWord(name.size() + 1);
  WriteDWord(comment.size() + 1);
  Write(name);
  Write(comment);
  
  // Record the start of the current data section
  dataStartPos = ftell(fp);
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

bool CBlockFile::FindBlock(const std::string &name)
{
  if (mode != 'r')
    return FAIL;
    
  fseek(fp, 0, SEEK_SET);
  
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
      fseek(fp, blockStartPos + 12 + name_length + comment_length, SEEK_SET); // move to beginning of data
      dataStartPos = ftell(fp);
      return OKAY;
    }
    
    // Move to next block
    fseek(fp, blockStartPos + block_length, SEEK_SET);
    curPos = blockStartPos + block_length;
    if (block_length == 0)  // this would never advance
      break;
  }
  
  return FAIL;
}

bool CBlockFile::Create(const std::string &file, const std::string &headerName, const std::string &comment)
{
  fp = fopen(file.c_str(), "wb");
  if (NULL == fp)
    return FAIL;
  mode = 'w';
  WriteBlockHeader(headerName, comment);
  return OKAY;
}
  
bool CBlockFile::Load(const std::string &file)
{
  fp = fopen(file.c_str(), "rb");
  if (NULL == fp)
    return FAIL;
  mode = 'r';
  
  // TODO: is this a valid block file?
  
  // Get the file size
  fseek(fp, 0, SEEK_END);
  fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  
  return OKAY;
}
  
void CBlockFile::Close(void)
{
  if (fp != NULL)
    fclose(fp);
  fp = NULL;
  mode = 0;
}

CBlockFile::CBlockFile(void)
{
  fp = NULL;
  mode = 0;   // neither reading nor writing (do nothing)
}

CBlockFile::~CBlockFile(void)
{
  if (fp != NULL) // in case user forgot
    fclose(fp);
  fp = NULL;
  mode = 0;
}
