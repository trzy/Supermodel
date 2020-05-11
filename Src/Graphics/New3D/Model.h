#ifndef _MODEL_H_
#define _MODEL_H_

#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <string.h>
#include "Texture.h"
#include "Mat4.h"

namespace New3D {

struct ClipVertex
{
	float pos[4];
};

struct ClipPoly
{
	ClipVertex list[12];		// what's the max number we can hit for a triangle + 4 planes?
	int count = 0;
};


struct Vertex					// half vertex
{
	float pos[4];
	float normal[3];
	float texcoords[2];
	float fixedShade;

	static bool Equal(const Vertex& p1, const Vertex& p2)
	{
		if (p1.pos[0] == p2.pos[0] &&
			p1.pos[1] == p2.pos[1] &&
			p1.pos[2] == p2.pos[2])
		{
			return true;
		}

		return false;
	}

	static void Average(const Vertex& p1, const Vertex& p2, Vertex& p3)
	{
		p3.pos[3] = 1.0f;	//always 1
		p3.fixedShade = (p1.fixedShade + p2.fixedShade) / 2.0f;

		for (int i = 0; i < 3; i++)	{ p3.pos[i] = (p1.pos[i] + p2.pos[i]) / 2.0f; }
		for (int i = 0; i < 3; i++)	{ p3.normal[i] = (p1.normal[i] + p2.normal[i]) / 2.0f; }
		for (int i = 0; i < 2; i++)	{ p3.texcoords[i] = (p1.texcoords[i] + p2.texcoords[i]) / 2.0f; }
	}
};

struct R3DPoly
{
	Vertex v[4];			// just easier to have them as an array
	float faceNormal[3];	// we need this to help work out poly winding, i assume the h/w uses this instead of calculating normals itself
	UINT8 faceColour[4];	// per face colour
	int number = 4;
};

struct FVertex : Vertex			// full vertex including face attributes
{
	float faceNormal[3];
	UINT8 faceColour[4];

	FVertex& operator=(const Vertex& vertex) 
	{
		memcpy(this, &vertex, sizeof(Vertex));
		return *this;
	}

	FVertex() {}
	FVertex(const R3DPoly& r3dPoly, int index) 
	{
		for (int i = 0; i < 4; i++) { faceColour[i] = r3dPoly.faceColour[i]; }
		for (int i = 0; i < 3; i++) { faceNormal[i] = r3dPoly.faceNormal[i]; }

		*this = r3dPoly.v[index];
	}

	FVertex(const R3DPoly& r3dPoly, int index1, int index2)		// average of 2 points
	{
		Vertex::Average(r3dPoly.v[index1], r3dPoly.v[index2], *this);

		// copy face attributes
		for (int i = 0; i < 4; i++) { faceColour[i] = r3dPoly.faceColour[i]; }
		for (int i = 0; i < 3; i++) { faceNormal[i] = r3dPoly.faceNormal[i]; }
	}

	static void Average(const FVertex& p1, const FVertex& p2, FVertex& p3)
	{
		Vertex::Average(p1, p2, p3);
		for (int i = 0; i < 4; i++) { p3.faceColour[i] = p1.faceColour[i]; }
		for (int i = 0; i < 3; i++) { p3.faceNormal[i] = p1.faceNormal[i]; }
	}
};

enum class Layer { colour, trans1, trans2, trans12 /*both 1&2*/, all, none };

struct Mesh
{
	//helper funcs
	bool Render(Layer layer)
	{
		switch (layer)
		{
		case Layer::colour:
			if (polyAlpha) {
				return false;
			}
			break;
		case Layer::trans1:
			if ((!textureAlpha && !polyAlpha) || transLSelect) {
				return false;
			}
			break;
		case Layer::trans2:
			if ((!textureAlpha && !polyAlpha) || !transLSelect) {
				return false;
			}
			break;
		default:					// not using these types
			return false;
		}

		return true;
	}

	enum TexWrapMode : int { repeat = 0, repeatClamp, mirror, mirrorClamp };

	// texture
	int format, x, y, width, height = 0;
	TexWrapMode wrapModeU;
	TexWrapMode wrapModeV;
	bool inverted = false;

	// microtexture
	bool	microTexture		= false;
	int		microTextureID		= 0;
	float	microTextureScale	= 0;

	// attributes
	bool textured		= false;
	bool polyAlpha		= false;		// specified in the rgba colour
	bool textureAlpha	= false;		// use alpha in texture
	bool alphaTest		= false;		// discard fragment based on alpha (ogl does this with fixed function)
	bool layered		= false;		// stencil poly
	bool highPriority	= false;		// rendered over the top
	bool transLSelect	= false;		// actually the transparency layer, false = layer 0, true = layer 1
	bool translatorMap	= false;		// colours are multiplied by 16

	// lighting
	bool fixedShading	= false;
	bool lighting		= false;
	bool specular		= false;
	float shininess		= 0;
	float specularValue = 0;
	
	// fog
	float fogIntensity = 1.0f;

	// opengl resources
	int vboOffset		= 0;			// this will be calculated later
	int vertexCount		= 0;			// /3 for triangles /4 for quads
};

struct SortingMesh : public Mesh		// This struct temporarily holds the model data, before it gets copied to the main buffer
{
	std::vector<FVertex> verts;
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

	//model scale step 1.5+
	float scale = 1.0f;
};

struct Viewport
{
	int		vpX;					// these are the original hardware values
	int		vpY;
	int		vpWidth;
	int		vpHeight;
	float	angle_left;
	float	angle_right;
	float	angle_top;
	float	angle_bottom;

	Mat4	projectionMatrix;		// projection matrix, we will calc this later when we have scene near/far vals

	float	lightingParams[6];		// lighting parameters (see RenderViewport() and vertex shader)
	bool	sunClamp;				// unknown how this is set
	bool	intensityClamp;			// unknown how this is set
	float	spotEllipse[4];			// spotlight ellipse (see RenderViewport())
	float	spotRange[2];			// Z range
	float	spotColor[3];			// color
	float	fogParams[7];			// fog parameters (...)
	float	scrollFog;				// a transparency value that determines if fog is blended over the bottom 2D layer
	int		losPosX, losPosY;		// line of sight position
	int		x, y;					// viewport coordinates (scaled and in OpenGL format)
	int		width, height;			// viewport dimensions (scaled for display surface size)
	int		priority;				// priority
	int		select;					// viewport select?
	int		number;					// viewport number
	float	spotFogColor[3];		// spotlight color on fog
	float	scrollAtt;

	int		hardwareStep;			// not really a viewport param but will do here
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
	float currentModelScale;

private:

	struct NodeAttribs
	{
		int texOffsetX;
		int texOffsetY;
		int page;
		Clip clip;
		float modelScale;
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