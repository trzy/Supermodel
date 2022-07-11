#include "Vec.h"
#include <cmath>
#include <algorithm>

namespace New3D {

void V3::subtract(const Vec3 a, const Vec3 b, Vec3 out) {

	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

void V3::subtract(Vec3 a, const Vec3 b) {

	a[0] -= b[0];
	a[1] -= b[1];
	a[2] -= b[2];
}

void V3::add(const Vec3 a, const Vec3 b, Vec3 out) {

	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

void V3::add(Vec3 a, const Vec3 b) {

	a[0] += b[0];
	a[1] += b[1];
	a[2] += b[2];
}

void V3::divide(Vec3 a, float number) {

	multiply(a,1.f/number);
}

void V3::multiply(Vec3 a, float number) {

	a[0] *= number;
	a[1] *= number;
	a[2] *= number;
}

void V3::multiply(Vec3 a, const Vec3 b) {

	a[0] *= b[0];
	a[1] *= b[1];
	a[2] *= b[2];
}

void V3::multiply(const Vec3 a, const Vec3 b, Vec3 out) {

	out[0] = a[0] * b[0];
	out[1] = a[1] * b[1];
	out[2] = a[2] * b[2];
}

void V3::crossProduct(const Vec3 v1, const Vec3 v2, Vec3 cross) {
									
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];						
}

float V3::dotProduct(const Vec3 v1, const Vec3 v2) {

	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void V3::copy(const Vec3 in, Vec3 out) {

	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void V3::inverse(Vec3 v) {

	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

float V3::length(const Vec3 v) {

	//===========
	float length;
	//===========
	
	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = std::sqrt(length);

	return length;
}

void V3::normalise(Vec3 v) {

	//========
	float inv_len;
	//========

	inv_len = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	inv_len = 1.f/std::sqrt(inv_len);

	multiply(v,inv_len);
}

void V3::multiplyAdd(const Vec3 a, float scale, const Vec3 b, Vec3 out) {

	out[0] = a[0] + scale*b[0];
	out[1] = a[1] + scale*b[1];
	out[2] = a[2] + scale*b[2];
}

void V3::reset(Vec3 v) {

	v[0] = 0.f;
	v[1] = 0.f;
	v[2] = 0.f;
}

void V3::set(Vec3 v, float value) {

	v[0] = value;
	v[1] = value;
	v[2] = value;
}

void V3::set(Vec3 v, float x, float y, float z) {

	v[0] = x;
	v[1] = y;
	v[2] = z;
}

void V3::reflect(const Vec3 a, const Vec3 b, Vec3 out) {

	//===========
	float	temp;
	Vec3	v;
	//===========

	//Vect2 = Vect1 - 2 * WallN * (WallN DOT Vect1)

	V3::copy(a,v);

	temp = V3::dotProduct(a,b) * 2.f;

	V3::multiply(v,temp);
	V3::subtract(b,v,out);
}

void V3::createNormal(const Vec3 a, const Vec3 b, const Vec3 c, Vec3 outNormal) {

	//======
	Vec3 v1;
	Vec3 v2;
	//======

	V3::subtract	(a,b,v1);
	V3::subtract	(c,b,v2);
	V3::crossProduct(v1,v2,outNormal);
}

void V3::_max(Vec3 a, const Vec3 compare) {

	a[0] = std::max(compare[0], a[0]);
	a[1] = std::max(compare[1], a[1]);
	a[2] = std::max(compare[2], a[2]);
}

void V3::_min(Vec3 a, const Vec3 compare) {

	a[0] = std::min(compare[0], a[0]);
	a[1] = std::min(compare[1], a[1]);
	a[2] = std::min(compare[2], a[2]);
}

bool V3::cmp(const Vec3 a, float b) {

	if(a[0]!=b) return false;
	if(a[1]!=b) return false;
	if(a[2]!=b) return false;

	return true;
}

bool V3::cmp(const Vec3 a, const Vec3 b) {

	if(a[0]!=b[0]) return false;
	if(a[1]!=b[1]) return false;
	if(a[2]!=b[2]) return false;

	return true;
}

void V3::clamp(Vec3 a, float _min, float _max) {
	
	a[0] = std::min(std::max(_min, a[0]), _max);
	a[1] = std::min(std::max(_min, a[1]), _max);
	a[2] = std::min(std::max(_min, a[2]), _max);
}

} // New3D
