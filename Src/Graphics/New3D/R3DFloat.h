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
