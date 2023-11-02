#ifndef _R3DDATA_H_
#define _R3DDATA_H_

struct LOD
{
	float deleteSize;
	float blendFactor;
};

struct LODFeatureType
{
	LOD lod[4];
};

struct LODBlendTable
{
	LODFeatureType table[128];
};

#endif