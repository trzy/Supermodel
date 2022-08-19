#include "Types.h"
#include "R3DFloat.h"
#include "Util\GenericValue.h"

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
	return uint_as_float(a1);
}
