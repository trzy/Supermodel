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
