#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_EXCEPTION_H
#define INCLUDED_EXCEPTION_H

#include "Types.h"

namespace Debugger
{
	class CCPUDebug;

	/*
	 * Class that represents a CPU exception.
	 */
	class CException
	{
	public:
		CCPUDebug *cpu;
		const char *id;
		const UINT16 code;
		const char *name;

		bool trap;
		unsigned long count;
		
		CException(CCPUDebug *exCPU, const char *exId, UINT16 exCode, const char *exName);
	};
}

#endif	// INCLUDED_EXCEPTION_H
#endif  // SUPERMODEL_DEBUGGER