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

#ifndef _R3DFLOAT_H_
#define _R3DFLOAT_H_

namespace R3DFloat
{
	constexpr UINT16	Pro16BitMax = 0x7fff;
	constexpr float	Pro16BitFltMin = 1e-7f;			// float min in IEEE 

	float	GetFloat16(UINT16 f);
	float	GetFloat32(UINT32 f);

	UINT32	ConvertProFloat(UINT32 a1);				// return float in hex or integer format
	UINT32	Convert16BitProFloat(UINT32 a1);
	float	ToFloat(UINT32 a1);						// integer float to actual IEEE 754 float	
}

#endif
