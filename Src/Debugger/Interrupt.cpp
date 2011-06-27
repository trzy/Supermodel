#ifdef SUPERMODEL_DEBUGGER

#include "Interrupt.h"

namespace Debugger
{
	CInterrupt::CInterrupt(CCPUDebug *intCPU, const char *intId, UINT16 intCode, const char *intName) : 
		cpu(intCPU), id(intId), code(intCode), name(intName), trap(false), count(0)
	{
		//
	}
}

#endif  // SUPERMODEL_DEBUGGER