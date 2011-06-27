#ifdef SUPERMODEL_DEBUGGER

#include "Breakpoint.h"

#include <stdio.h>

namespace Debugger
{
	CBreakpoint::CBreakpoint(CCPUDebug *bpCPU, int bpAddr, char bpSymbol, const char *bpType) : 
		CAddressRef(bpCPU, bpAddr), symbol(bpSymbol), type(bpType), active(true)
	{
		//
	}

	bool CBreakpoint::Check(UINT32 pc, UINT32 opcode)
	{
		return CheckAddr(pc) && active && CheckBreak(pc, opcode);
	}

	void CBreakpoint::Reset()
	{
		//
	}

	CSimpleBreakpoint::CSimpleBreakpoint(CCPUDebug *bpCPU, int bpAddr) : CBreakpoint(bpCPU, bpAddr, '*', "simple")
	{
		//
	}

	bool CSimpleBreakpoint::CheckBreak(UINT32 pc, UINT32 opcode) 
	{
		return true;
	}

	bool CSimpleBreakpoint::GetInfo(char *str)
	{
		return false;
	}

	CCountBreakpoint::CCountBreakpoint(CCPUDebug *bpCPU, int bpAddr, int count) : CBreakpoint(bpCPU, bpAddr, '+', "count"), 
		m_count(count), m_counter(0)
	{
		//
	}

	bool CCountBreakpoint::CheckBreak(UINT32 pc, UINT32 opcode) 
	{
		return ++m_counter == m_count;
	}

	void CCountBreakpoint::Reset()
	{
		m_counter = 0;
	}

	bool CCountBreakpoint::GetInfo(char *str)
	{
		sprintf(str, "%d / %d", m_counter, m_count); 
		return true;
	}
}

#endif  // SUPERMODEL_DEBUGGER
