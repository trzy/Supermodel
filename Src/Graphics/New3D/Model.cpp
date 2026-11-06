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

#include "Model.h"

namespace New3D {

NodeAttributes::NodeAttributes()
{
	currentTexOffsetX		= 0;
	currentTexOffsetY		= 0;
	currentPage				= 0;
	currentModelScale		= 1.0f;
	currentModelAlpha		= 1.0;
	currentDisableCulling	= false;
}

bool NodeAttributes::Push()
{
	//=============
	NodeAttribs na;
	//=============

	// check for overflow
	if (m_vecAttribs.size() >= 128) {
		return false;
	}

	na.page				= currentPage;
	na.texOffsetX		= currentTexOffsetX;
	na.texOffsetY		= currentTexOffsetY;
	na.modelScale		= currentModelScale;
	na.modelAlpha		= currentModelAlpha;
	na.disableCulling	= currentDisableCulling;

	m_vecAttribs.emplace_back(na);

	return true;
}

bool NodeAttributes::Pop()
{
	if (m_vecAttribs.empty()) {
		return false;	// check for underflow
	}

	auto &last = m_vecAttribs.back();

	currentPage				= last.page;
	currentTexOffsetX		= last.texOffsetX;
	currentTexOffsetY		= last.texOffsetY;
	currentModelScale		= last.modelScale;
	currentModelAlpha		= last.modelAlpha;
	currentDisableCulling	= last.disableCulling;

	m_vecAttribs.pop_back();

	return true;
}

bool NodeAttributes::StackLimit() const
{
	return m_vecAttribs.size() >= 1024;
}

void NodeAttributes::Reset()
{
	currentPage				= 0;
	currentTexOffsetX		= 0;
	currentTexOffsetY		= 0;
	currentModelScale		= 1.0f;
	currentModelAlpha		= 1.0f;
	currentDisableCulling	= false;

	m_vecAttribs.clear();
}

} // New3D
