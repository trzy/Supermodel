#ifndef _R3DDATA_H_
#define _R3DDATA_H_

struct LOD
{
	float startRange;		// possibly specified as angles also, yeah who knows >_<
	float deleteRange;
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