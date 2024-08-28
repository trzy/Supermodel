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
