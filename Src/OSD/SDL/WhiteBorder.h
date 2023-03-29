/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2023 The Supermodel Team
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

#ifndef INCLUDED_WHITEBORDER_H
#define INCLUDED_WHITEBORDER_H

#include "Supermodel.h"
#include "Graphics/New3D/New3D.h"
#include "Inputs/Inputs.h"
#include "basicDrawable.h"


class CWhiteBorder : public CBasicDrawable
{
private:
	const Util::Config::Node& m_config;
	bool m_drawWhiteBorder;
	unsigned int m_xRes;
	unsigned int m_yRes;
	unsigned int m_xOffset;
	unsigned int m_yOffset;
	unsigned int m_width;
	
	void GenQuad(std::vector<CBasicDrawable::BasicVertex>& verts, unsigned int x, unsigned int y, unsigned int w, unsigned int h);
public:
	CWhiteBorder(const Util::Config::Node& config);
	~CWhiteBorder();
	bool Init();
	void Update(unsigned int xOffset, unsigned int yOffset, unsigned int xRes, unsigned int yRes, unsigned int totalXRes, unsigned int totalYRes);
	bool IsEnabled();
	unsigned int Width();
};

#endif

