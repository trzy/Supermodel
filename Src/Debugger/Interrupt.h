#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_INTERRUPT_H
#define INCLUDED_INTERRUPT_H

#include "Types.h"

namespace Debugger
{
	class CCPUDebug;

	/*
	 * Class that represents a CPU interrupt (just a specialised CPU exception).
	 */
	class CInterrupt
	{
	public:
		CCPUDebug *cpu;
		const char *id;
		const UINT16 code;
		const char *name;

		bool trap;
		unsigned long count;
		
		CInterrupt(CCPUDebug *intCPU, const char *intId, UINT16 intCode, const char *intName);
	};
}

#endif	// INCLUDED_INTERRUPT_H
#endif  // SUPERMODEL_DEBUGGER