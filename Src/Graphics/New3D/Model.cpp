#include "Model.h"

namespace New3D {

NodeAttributes::NodeAttributes()
{
	currentTexOffsetX = 0;
	currentTexOffsetY = 0;
	currentTexOffset = 0;
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

	na.texOffset = currentTexOffset;
	na.texOffsetX = currentTexOffsetX;
	na.texOffsetY = currentTexOffsetY;

	m_vecAttribs.push_back(na);

	return true;
}

bool NodeAttributes::Pop()
{
	if (m_vecAttribs.empty()) {
		return false;	// check for underflow
	}

	currentTexOffset = m_vecAttribs.back().texOffset;
	currentTexOffsetX = m_vecAttribs.back().texOffsetX;
	currentTexOffsetY = m_vecAttribs.back().texOffsetY;

	m_vecAttribs.pop_back();

	return true;
}

bool NodeAttributes::StackLimit()
{
	return m_vecAttribs.size() >= 1024;
}

void NodeAttributes::Reset()
{
	currentTexOffset = 0;
	currentTexOffsetX = 0;
	currentTexOffsetY = 0;
	m_vecAttribs.clear();
}

} // New3D
