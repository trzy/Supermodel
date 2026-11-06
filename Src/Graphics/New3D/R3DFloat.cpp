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

#include "Types.h"
#include "R3DFloat.h"
#include "Util/BitCast.h"

#include <cstdint>

float R3DFloat::GetFloat16(UINT16 f)
{
	return ToFloat(Convert16BitProFloat(f));
}

float R3DFloat::GetFloat32(UINT32 f)
{
	return ToFloat(ConvertProFloat(f));
}

UINT32 R3DFloat::ConvertProFloat(UINT32 a1)
{
	UINT32 exponent = (a1 & 0x7E000000) >> 25;

	if (exponent <= 31) {	// positive
		exponent += 127;
	}
	else {					// negative exponent
		exponent += 127 - 64;
	}

	UINT32 mantissa = (a1 & 0x1FFFFFF) >> 2;

	return (a1 & 0x80000000) | (exponent << 23) | mantissa;
}

UINT32 R3DFloat::Convert16BitProFloat(UINT32 a1)
{
	return ConvertProFloat(a1 << 15);
}

float R3DFloat::ToFloat(UINT32 a1)
{
	return Util::Uint32AsFloat(a1);
}
