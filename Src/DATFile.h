/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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
 * DATFile.h
 *
 * Writes the ROM sets as a Logiqx XML DAT file, for ROM auditing tools such as
 * ClrMamePro and RomVault.
 */

#ifndef INCLUDED_DATFILE_H
#define INCLUDED_DATFILE_H

#include "GameLoader.h"
#include <string>

// Returns true on error
bool WriteDATFile(const std::string &filename, const GameLoader &loader);

#endif  // INCLUDED_DATFILE_H
