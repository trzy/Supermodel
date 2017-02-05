#ifndef _MODEL_H_
#define _MODEL_H_

#include "Types.h"
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
	float color[4];			//rgba
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
	float faceColour[4];	// per face colour
	int number = 4;
};

struct Mesh
{
	// texture
	int format, x, y, width, height = 0;
	bool mirrorU = false;
	bool mirrorV = false;

	// microtexture
	bool	microTexture		= false;
	int		microTextureID		= 0;
	float	microTextureScale	= 0;

	// attributes
	bool doubleSided	= false;
	bool textured		= false;
	bool polyAlpha		= false;		// specified in the rgba colour
	bool textureAlpha	= false;		// use alpha in texture
	bool alphaTest		= false;		// discard fragment based on alpha (ogl does this with fixed function)
	bool clockWise		= true;			// we need to check if the matrix will change the winding
	bool layered		= false;		// stencil poly

	// lighting
	bool lighting		= false;
	bool specular		= false;
	float shininess		= 0;
	float specularCoefficient = 0;
	
	// fog
	float fogIntensity = 1.0f;

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
	std::shared_ptr<std::vector<Mesh>> meshes;	// this reason why this is a shared ptr to an array, is that multiple models might use the same meshes

	//which memory are we in
	bool dynamic = true;

	// texture offsets for model
	int textureOffsetX = 0;
	int textureOffsetY = 0;
	int page = 0;

	//matrices
	float modelMat[16];
	float determinant;				// we check if the determinant of the matrix is negative, if it is, the matrix will swap the axis order
};

struct Viewport
{
	Mat4	projectionMatrix;		// projection matrix
	float	lightingParams[6];		// lighting parameters (see RenderViewport() and vertex shader)
	float	spotEllipse[4];			// spotlight ellipse (see RenderViewport())
	float	spotRange[2];			// Z range
	float	spotColor[3];			// color
	float	fogParams[5];			// fog parameters (...)
	float	scrollFog;				// a transparency value that determines if fog is blended over the bottom 2D layer
	int		x, y;					// viewport coordinates (scaled and in OpenGL format)
	int		width, height;			// viewport dimensions (scaled for display surface size)
	int		priority;
};

enum class Clip { INSIDE, OUTSIDE, INTERCEPT, NOT_SET };

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
	int currentPage;
	Clip currentClipStatus;

private:

	struct NodeAttribs
	{
		int texOffsetX;
		int texOffsetY;
		int page;
		Clip clip;
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