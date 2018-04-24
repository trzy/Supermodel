#include "Mat4.h"
#include <cmath>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265359
#endif

namespace New3D {

Mat4::Mat4()
{
	LoadIdentity();
}

void Mat4::LoadIdentity()
{
	float *m = currentMatrix;

	m[0] = 1; m[4] = 0; m[8 ] = 0; m[12] = 0;
	m[1] = 0; m[5] = 1; m[9 ] = 0; m[13] = 0;
	m[2] = 0; m[6] = 0; m[10] = 1; m[14] = 0;
	m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
}

void Mat4::MultiMatrices(const float a[16], const float b[16], float r[16]) 
{
#define A(row,col)  a[(col<<2)+row]
#define B(row,col)  b[(col<<2)+row]
#define P(row,col)  r[(col<<2)+row]

	int i;
	for (i = 0; i < 4; i++) {
		const float ai0 = A(i, 0), ai1 = A(i, 1), ai2 = A(i, 2), ai3 = A(i, 3);
		P(i, 0) = ai0 * B(0, 0) + ai1 * B(1, 0) + ai2 * B(2, 0) + ai3 * B(3, 0);
		P(i, 1) = ai0 * B(0, 1) + ai1 * B(1, 1) + ai2 * B(2, 1) + ai3 * B(3, 1);
		P(i, 2) = ai0 * B(0, 2) + ai1 * B(1, 2) + ai2 * B(2, 2) + ai3 * B(3, 2);
		P(i, 3) = ai0 * B(0, 3) + ai1 * B(1, 3) + ai2 * B(2, 3) + ai3 * B(3, 3);
	}

#undef A
#undef B
#undef p
}

void Mat4::Copy(const float in[16], float out[16])
{
	for (int i = 0; i<16; i++) {
		out[i] = in[i];
	}
}

void Mat4::Translate(float x, float y, float z) 
{
	//==========
	float m[16];
	//==========

	m[0] = 1;
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;

	m[4] = 0;
	m[5] = 1;
	m[6] = 0;
	m[7] = 0;

	m[8] = 0;
	m[9] = 0;
	m[10] = 1;
	m[11] = 0;

	m[12] = x;
	m[13] = y;
	m[14] = z;
	m[15] = 1;

	Mat4::MultiMatrices(currentMatrix, m, currentMatrix);
}

void Mat4::Scale(float x, float y, float z) 
{
	//==========
	float m[16];
	//==========

	m[0] = x;
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;

	m[4] = 0;
	m[5] = y;
	m[6] = 0;
	m[7] = 0;

	m[8] = 0;
	m[9] = 0;
	m[10] = z;
	m[11] = 0;

	m[12] = 0;
	m[13] = 0;
	m[14] = 0;
	m[15] = 1;

	Mat4::MultiMatrices(currentMatrix, m, currentMatrix);
}

void Mat4::Rotate(float angle, float x, float y, float z) 
{
	//===========
	float c;
	float s;
	float m[16];
	//===========

	// normalise vector first
	{
		//========
		float len;
		//========

		len = std::sqrt(x * x + y * y + z * z);
		
		x /= len;
		y /= len;
		z /= len;
	}

	c = std::cos(angle*3.14159265f / 180.0f);
	s = std::sin(angle*3.14159265f / 180.0f);

	m[0] = (x*x) * (1 - c) + c;
	m[1] = (y*x) * (1 - c) + (z*s);
	m[2] = (x*z) * (1 - c) - (y*s);
	m[3] = 0;

	m[4] = (x*y) * (1 - c) - (z*s);
	m[5] = (y*y) * (1 - c) + c;
	m[6] = (y*z) * (1 - c) + (x*s);
	m[7] = 0;

	m[8] = (x*z) * (1 - c) + (y*s);
	m[9] = (y*z) * (1 - c) - (x*s);
	m[10] = (z*z) * (1 - c) + c;
	m[11] = 0;

	m[12] = 0;
	m[13] = 0;
	m[14] = 0;
	m[15] = 1;

	Mat4::MultiMatrices(currentMatrix, m, currentMatrix);
}

void Mat4::Frustum(float left, float right, float bottom, float top, float nearVal, float farVal) 
{
	//=====================
	float m[16];
	float x, y, a, b, c, d;
	//=====================

	x = (2.0F*nearVal) / (right - left);
	y = (2.0F*nearVal) / (top - bottom);
	a = (right + left) / (right - left);
	b = (top + bottom) / (top - bottom);
	c = -(farVal + nearVal) / (farVal - nearVal);
	d = -(2.0F*farVal*nearVal) / (farVal - nearVal);

	m[0] = x;
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;

	m[4] = 0;
	m[5] = y;
	m[6] = 0;
	m[7] = 0;

	m[8] = a;
	m[9] = b;
	m[10] = c;
	m[11] = -1;

	m[12] = 0;
	m[13] = 0;
	m[14] = d;
	m[15] = 0;

	Mat4::MultiMatrices(currentMatrix, m, currentMatrix);
}

void Mat4::Perspective(float fovy, float aspect, float zNear, float zFar)
{
	//=========
	float ymax;
	float xmax;
	//=========

	ymax = zNear * tanf(fovy * (float)M_PI / 360.0f);
	xmax = ymax * aspect;

	Frustum(-xmax, xmax, -ymax, ymax, zNear, zFar);
}

void Mat4::Ortho(float left, float right, float bottom, float top, float nearVal, float farVal)
{
	//================
	float m[16];
	float tx, ty, tz;
	//================

	tx = -(right + left) / (right - left);
	ty = -(top + bottom) / (top - bottom);
	tz = -(farVal + nearVal) / (farVal - nearVal);

	m[0] = 2/(right-left);
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;

	m[4] = 0;
	m[5] = 2/(top-bottom);
	m[6] = 0;
	m[7] = 0;

	m[8] = 0;
	m[9] = 0;
	m[10] = -2/(farVal-nearVal);
	m[11] = 0;

	m[12] = tx;
	m[13] = ty;
	m[14] = tz;
	m[15] = 1;

	Mat4::MultiMatrices(currentMatrix, m, currentMatrix);
}

void Mat4::Transpose(float m[16]) 
{
	std::swap(m[1], m[4]);
	std::swap(m[2], m[8]);
	std::swap(m[3], m[12]);
	std::swap(m[6], m[9]);
	std::swap(m[7], m[13]);
	std::swap(m[11], m[14]);
}

void Mat4::MultMatrix(const float *m)
{
	if (!m) {
		return;
	}

	Mat4::MultiMatrices(currentMatrix, m, currentMatrix);
}

void Mat4::LoadMatrix(const float *m)
{
	if (!m) {
		return; 
	}

	Mat4::Copy(m, currentMatrix);
}

void Mat4::LoadTransposeMatrix(const float *m)
{
	if (!m) {
		return;
	}

	Mat4::LoadMatrix(m);
	Mat4::Transpose(currentMatrix);
}

void Mat4::MultTransposeMatrix(const float *m)
{
	if (!m) {
		return;
	}

	//=============
	float copy[16];
	//=============

	Mat4::Copy(m, copy);
	Mat4::Transpose(copy);
	Mat4::MultMatrix(copy);
}

void Mat4::PushMatrix()
{
	//==============
	Mat4Container m;
	//==============

	if (m_vMat4.size() > 128) {
		return;	// check for overflow
	}

	Mat4::Copy(currentMatrix, m.mat);

	m_vMat4.push_back(m);
}

void Mat4::PopMatrix()
{
	if (m_vMat4.empty()) {
		return;	// check for underflow
	}

	Mat4::Copy(m_vMat4.back().mat, currentMatrix);

	m_vMat4.pop_back();
}

// flush the matrix stack
void Mat4::Release()
{
	m_vMat4.clear();
}

}// New3D