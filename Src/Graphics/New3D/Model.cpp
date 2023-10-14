#include "Model.h"

namespace New3D {

NodeAttributes::NodeAttributes()
{
	currentTexOffsetX	= 0;
	currentTexOffsetY	= 0;
	currentPage			= 0;
	currentClipStatus	= Clip::INTERCEPT;
	currentModelScale	= 1.0f;
	currentModelAlpha	= 1.0;
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

	na.page			= currentPage;
	na.texOffsetX	= currentTexOffsetX;
	na.texOffsetY	= currentTexOffsetY;
	na.clip			= currentClipStatus;
	na.modelScale	= currentModelScale;
	na.modelAlpha	= currentModelAlpha;

	m_vecAttribs.emplace_back(na);

	return true;
}

bool NodeAttributes::Pop()
{
	if (m_vecAttribs.empty()) {
		return false;	// check for underflow
	}

	auto &last = m_vecAttribs.back();

	currentPage			= last.page;
	currentTexOffsetX	= last.texOffsetX;
	currentTexOffsetY	= last.texOffsetY;
	currentClipStatus	= last.clip;
	currentModelScale	= last.modelScale;
	currentModelAlpha	= last.modelAlpha;

	m_vecAttribs.pop_back();

	return true;
}

bool NodeAttributes::StackLimit()
{
	return m_vecAttribs.size() >= 1024;
}

void NodeAttributes::Reset()
{
	currentPage			= 0;
	currentTexOffsetX	= 0;
	currentTexOffsetY	= 0;
	currentClipStatus	= Clip::INTERCEPT;
	currentModelScale	= 1.0f;
	currentModelAlpha	= 1.0f;

	m_vecAttribs.clear();
}

} // New3D
