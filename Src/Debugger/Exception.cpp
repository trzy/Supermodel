#ifdef SUPERMODEL_DEBUGGER

#include "Exception.h"

namespace Debugger
{
	CException::CException(CCPUDebug *exCPU, const char *exId, UINT16 exCode, const char *exName) : 
		cpu(exCPU), id(exId), code(exCode), name(exName), trap(false), count(0)
	{
		//
	}
}

#endif  // SUPERMODEL_DEBUGGER