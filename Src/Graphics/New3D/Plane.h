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

#ifndef _PLANE_H_
#define _PLANE_H_

#include <cmath>

struct Plane
{
	float a, b, c, d;

	void Normalise() {
		float temp = 1.f/std::sqrt((a * a) + (b * b) + (c * c));
		a *= temp;
		b *= temp;
		c *= temp;
		d *= temp;
	}

	float DistanceToPoint(const float v[3]) {
		return a*v[0] + b*v[1] + c*v[2] + d;
	}

	float DotProduct(const float v[3]) {
		return a*v[0] + b*v[1] + c*v[2];
	}
};

#endif
