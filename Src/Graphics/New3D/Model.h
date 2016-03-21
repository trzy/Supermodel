#ifndef _MODEL_H_
#define _MODEL_H_

#include "types.h"
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include "Texture.h"
#include "Mat4.h"

namespace New3D {

struct Vertex
{
	float pos[3];
	float normal[3];
	float texcoords[2];
	UINT8 color[4];			//rgba
};

struct Poly		// our polys are always 3 triangles, unlike the real h/w
{
	Vertex p1;
	Vertex p2;
	Vertex p3;
};

struct R3DPoly
{
	Vertex v[4];			// just easier to have them as an array
	float faceNormal[3];	// we need this to help work out poly winding, i assume the h/w uses this instead of calculating normals itself
	int number = 4;
};

struct Mesh
{
	std::shared_ptr<Texture> texture;

	// attributes
	bool doubleSided	= false;
	bool textured		= false;
	bool polyAlpha		= false;		// specified in the rgba colour
	bool textureAlpha	= false;		// use alpha in texture
	bool alphaTest		= false;		// discard fragment based on alpha (ogl does this with fixed function)
	bool lighting		= false;
	bool testBit		= false;
	bool clockWise		= true;			// we need to check if the matrix will change the winding

	float fogIntensity = 1.0f;

	// texture
	bool mirrorU		= false;
	bool mirrorV		= false;

	// opengl resources
	int vboOffset		= 0;			// this will be calculated later
	int triangleCount	= 0;
};

struct SortingMesh : public Mesh		// This struct temporarily holds the model data, before it gets copied to the main buffer
{
	std::vector<Poly> polys;
};

struct Model
{
	std::vector<Mesh> meshes;

	//matrices
	float modelMat[16];



	// misc
	int lutIdx = 0;
};

struct Viewport
{
	Mat4	projectionMatrix;		// projection matrix
	float	lightingParams[6];		// lighting parameters (see RenderViewport() and vertex shader)
	float	spotEllipse[4];			// spotlight ellipse (see RenderViewport())
	float	spotRange[2];			// Z range
	float	spotColor[3];			// color
	float	fogParams[5];			// fog parameters (...)
	int		x, y;					// viewport coordinates (scaled and in OpenGL format)
	int		width, height;			// viewport dimensions (scaled for display surface size)
	int		priority;
};

class NodeAttributes
{
public:

	NodeAttributes();

	bool Push();
	bool Pop();
	bool StackLimit();
	void Reset();

	int currentTexOffsetX;
	int currentTexOffsetY;
	int currentTexOffset;		// raw value

private:

	struct NodeAttribs
	{
		int texOffsetX;
		int texOffsetY;
		int texOffset;
	};
	std::vector<NodeAttribs> m_vecAttribs;
};

struct Node
{
	Viewport viewport;
	std::vector<Model> models;
};

} // New3D


#endif