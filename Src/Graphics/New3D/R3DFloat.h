#ifndef _R3DFLOAT_H_
#define _R3DFLOAT_H_

namespace R3DFloat
{
	static const UINT16 Pro16BitMax = 0x7fff;

	float	GetFloat16(UINT16 f);
	float	GetFloat32(UINT32 f);

	UINT32	ConvertProFloat(UINT32 a1);			// return float in hex or integer format
	UINT32	Convert16BitProFloat(UINT32 a1);
	float	ToFloat(UINT32 a1);					// integer float to actual IEEE 754 float	
}

#endif