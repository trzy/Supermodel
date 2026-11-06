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

#ifndef _TILEGENBUFFER_H_
#define _TILEGENBUFFER_H_

#include "Types.h"
#include <cstring>

struct TileGenBuffer
{
	TileGenBuffer() : data{ 0 } {}

	UINT32* GetLine(int number) { return data + (512 * number); };
	void	Clear() { std::memset(data, 0, sizeof(data)); }

	UINT32 data[512 * 384];
};

#endif
