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

#ifndef VEC_H
#define VEC_H

namespace New3D {
namespace V3 
{
	typedef float Vec3[3];

	void	subtract		(const Vec3 a, const Vec3 b, Vec3 out);
	void	subtract		(Vec3 a, const Vec3 b);
	void	add				(const Vec3 a, const Vec3 b, Vec3 out);
	void	add				(Vec3 a, const Vec3 b);
	void	divide			(Vec3 a, float number);
	void	multiply		(Vec3 a, float number);
	void	multiply		(Vec3 a, const Vec3 b);
	void	multiply		(const Vec3 a, const Vec3 b, Vec3 out);
	void	crossProduct	(const Vec3 v1, const Vec3 v2, Vec3 cross);
	float	dotProduct		(const Vec3 v1, const Vec3 v2);
	void	copy			(const Vec3 in, Vec3 out);
	void	inverse			(Vec3 v);
	float	length			(const Vec3 v);
	void	normalise		(Vec3 v);
	void	multiplyAdd		(const Vec3 a, float scale, const Vec3 b, Vec3 out);
	void	reset			(Vec3 v);
	void	set				(Vec3 v, float value);
	void	set				(Vec3 v, float x, float y, float z);
	void	reflect			(const Vec3 a, const Vec3 b, Vec3 out);
	void	createNormal	(const Vec3 a, const Vec3 b, const Vec3 c, Vec3 outNormal);	// assume a,b,c are wound clockwise
	void	_max			(Vec3 a, const Vec3 compare);
	void	_min			(Vec3 a, const Vec3 compare);
	bool	cmp				(const Vec3 a, float b);
	bool	cmp				(const Vec3 a, const Vec3 b);
	void	clamp			(Vec3 a, float _min, float _max);
}

namespace V4
{
	typedef float Vec4[4];
}
} // New3D

#endif