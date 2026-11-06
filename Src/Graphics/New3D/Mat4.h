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

#ifndef _MAT4_H_
#define _MAT4_H_

#include <vector>

namespace New3D {

class Mat4
{
public:

	Mat4();

	void LoadIdentity			();
	void Translate				(float x, float y, float z);
	void Rotate					(float angle, float x, float y, float z);
	void Scale					(float x, float y, float z);
	void Frustum				(float left, float right, float bottom, float top, float nearVal, float farVal);
	void FrustumRZ				(float left, float right, float bottom, float top, float nearVal);
	void Perspective			(float fovy, float aspect, float zNear, float zFar);
	void Ortho					(float left, float right, float bottom, float top, float nearVal, float farVal);
	void MultMatrix				(const float *m);
	void LoadMatrix				(const float *m);
	void LoadTransposeMatrix	(const float *m);
	void MultTransposeMatrix	(const float *m);
	void PushMatrix				();
	void PopMatrix				();
	void Release				();

	operator float*				()       { return currentMatrix; }
	operator const float*		() const { return currentMatrix; }
	
	float currentMatrix[16];

private:

	void MultiMatrices			(const float a[16], const float b[16], float r[16]);
	void Copy					(const float in[16], float out[16]);
	void Transpose				(float m[16]);

	struct Mat4Container {
		float mat[16];	// we must wrap the matrix inside a struct otherwise vector doesn't work
	};

	std::vector<Mat4Container> m_vMat4;
};

} // New3D

#endif

