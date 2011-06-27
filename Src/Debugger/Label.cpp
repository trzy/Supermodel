#ifdef SUPERMODEL_DEBUGGER

#include "Label.h"

#include <string.h>

namespace Debugger
{
	CLabel::CLabel(CCPUDebug *lCPU, UINT32 lAddr, const char *lName) : CAddressRef(lCPU, lAddr)
	{
		strncpy(nameStr, lName, MAX_LABEL_LENGTH);
		nameStr[MAX_LABEL_LENGTH] = '\0';
		name = nameStr;
	}

	CLabel::CLabel(CCPUDebug *lCPU, UINT32 lAddrStart, UINT32 lAddrEnd, const char *lName) :
		CAddressRef(lCPU, lAddrStart, lAddrEnd - lAddrStart + 1)
	{
		strncpy(nameStr, lName, MAX_LABEL_LENGTH);
		nameStr[MAX_LABEL_LENGTH] = '\0';
		name = nameStr;
	}

	CComment::CComment(CCPUDebug *cCPU, UINT32 cAddr, const char *cText) : CAddressRef(cCPU, cAddr)
	{
		size_t size = strlen(cText);
		char *textStr = new char[size + 1];
		strncpy(textStr, cText, size);
		textStr[size] = '\0';
		text = textStr;
	}

	CComment::~CComment()
	{
		delete text;
	}

	CRegion::CRegion(CCPUDebug *rCPU, UINT32 rAddrStart, UINT32 rAddrEnd, bool rIsCode, bool rIsReadOnly, const char *rName) : 
		CLabel(rCPU, rAddrStart, rAddrEnd, rName), isCode(rIsCode), isReadOnly(rIsReadOnly), prevRegion(NULL), nextRegion(NULL)
	{
		//
	}
}

#endif  // SUPERMODEL_DEBUGGER
