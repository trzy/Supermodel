/**
 ** Supermodel-VK
 ** A Vulkan-focused fork of Supermodel, the Sega Model 3 arcade emulator.
 ** Copyright 2026 The Supermodel-VK authors
 **
 ** This file is part of Supermodel-VK, and is derived from Supermodel,
 ** Copyright 2003-2026 The Supermodel Team.
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
#pragma once
namespace Vulkan3D {
inline void ComputePictureRect(int extentWidth, int extentHeight,
                                 int xOffset, int yOffset, int xRes, int yRes,
                                 int correction, bool wideBackground, bool wideScreen,
                                 int& outX, int& outY, int& outW, int& outH)
{
	if (wideBackground || wideScreen)
	{
		int y = extentHeight - (yOffset + yRes) + correction;
		int h = yRes - 2 * correction;
		if (y < 0) { h += y; y = 0; }
		if (y + h > extentHeight) h = extentHeight - y;
		if (h < 0) h = 0;

		outX = 0;
		outY = y;
		outW = extentWidth;
		outH = h;
		return;
	}
	int x = xOffset + correction;
	int y = extentHeight - (yOffset + yRes) + correction;
	int w = xRes - 2 * correction;
	int h = yRes - 2 * correction;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > extentWidth) w = extentWidth - x;
	if (y + h > extentHeight) h = extentHeight - y;
	if (w < 0) w = 0;
	if (h < 0) h = 0;

	outX = x;
	outY = y;
	outW = w;
	outH = h;
}

}
