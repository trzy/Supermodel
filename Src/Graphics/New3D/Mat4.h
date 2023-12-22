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

