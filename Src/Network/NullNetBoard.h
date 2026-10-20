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
 * NullNetBoard.h
 *
 * Net board used when Supermodel is built without network support
 * (SUPERMODEL_NO_NETWORK). It is never attached, which is what the real net
 * boards report when networking is disabled in the configuration.
 */

#ifndef INCLUDED_NULLNETBOARD_H
#define INCLUDED_NULLNETBOARD_H

#include "INetBoard.h"

class CNullNetBoard : public INetBoard
{
public:
  void SaveState(CBlockFile *) override {}
  void LoadState(CBlockFile *) override {}

  void RunFrame(void) override {}
  void Reset(void) override {}

  bool IsAttached(void) override { return false; }
  bool IsRunning(void) override { return false; }

  Result Init(UINT8 *, UINT8 *) override { return Result::OKAY; }

  void GetGame(const Game &) override {}

  UINT8 ReadCommRAM8(unsigned) override { return 0; }
  UINT16 ReadCommRAM16(unsigned) override { return 0; }
  UINT32 ReadCommRAM32(unsigned) override { return 0; }

  void WriteCommRAM8(unsigned, UINT8) override {}
  void WriteCommRAM16(unsigned, UINT16) override {}
  void WriteCommRAM32(unsigned, UINT32) override {}

  UINT16 ReadIORegister(unsigned) override { return 0; }
  void WriteIORegister(unsigned, UINT16) override {}
};

#endif  // INCLUDED_NULLNETBOARD_H
