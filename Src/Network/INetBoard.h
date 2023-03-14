/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2020 Bart Trzynadlowski, Nik Henson, Ian Curtis,
 **                     Harry Tuttle, and Spindizzi
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

#ifndef INCLUDED_INETBOARD_H
#define INCLUDED_INETBOARD_H

#include "Types.h"
#include "Game.h"
#include "BlockFile.h"

class INetBoard
{
public:
  virtual ~INetBoard()
  {
  }

	virtual void SaveState(CBlockFile* SaveState) = 0;
	virtual void LoadState(CBlockFile* SaveState) = 0;

	virtual void RunFrame(void) = 0;
	virtual void Reset(void) = 0;

	virtual bool IsAttached(void) = 0;
	virtual bool IsRunning(void) = 0;

	virtual bool Init(UINT8* netRAMPtr, UINT8* netBufferPtr) = 0;

	virtual void GetGame(const Game&) = 0;

	virtual UINT8 ReadCommRAM8(unsigned addr) = 0;
	virtual UINT16 ReadCommRAM16(unsigned addr) = 0;
	virtual UINT32 ReadCommRAM32(unsigned addr) = 0;

	virtual void WriteCommRAM8(unsigned addr, UINT8 data) = 0;
	virtual void WriteCommRAM16(unsigned addr, UINT16 data) = 0;
	virtual void WriteCommRAM32(unsigned addr, UINT32 data) = 0;

	virtual UINT16 ReadIORegister(unsigned reg) = 0;
	virtual void WriteIORegister(unsigned reg, UINT16 data) = 0;
};

#endif