#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_BREAKPOINT_H
#define INCLUDED_BREAKPOINT_H

#include "AddressTable.h"
#include "Types.h"

namespace Debugger
{
	class CCPUDebug;

	/*
	 * Base class for a breakpoint.
	 */
	class CBreakpoint : public CAddressRef
	{
	protected:
		CBreakpoint(CCPUDebug *bpCPU, int bpAddr, char bpSymbol, const char *bpType);

	public:
		const char symbol;
		const char *type;

		unsigned num;

		bool active;

		bool Check(UINT32 pc, UINT32 opcode);

		virtual bool CheckBreak(UINT32 pc, UINT32 opcode) = 0;
		
		virtual void Reset();

		virtual bool GetInfo(char *str) = 0;
	};

	/*
	 * Simple breakpoint that will always halt execution when hit.
	 */
	class CSimpleBreakpoint : public CBreakpoint
	{
	public:
		CSimpleBreakpoint(CCPUDebug *bpCPU, int bpAddr);

		bool CheckBreak(UINT32 pc, UINT32 opcode);

		bool GetInfo(char *str);
	};

	/*
	 * Count breakpoint that will halt execution after it has been hit a certain number of times.
	 */
	class CCountBreakpoint : public CBreakpoint
	{
	private:
		int m_count;
		int m_counter;

	public:
		CCountBreakpoint(CCPUDebug *bpCPU, int bpAddr, int count);

		bool CheckBreak(UINT32 pc, UINT32 opcode);

		void Reset();

		bool GetInfo(char *str);
	};

	class CPrintBreakpoint : public CBreakpoint
	{
	public:
		CPrintBreakpoint(CCPUDebug *bpCPU, int bpAddr);

		bool CheckBreak(UINT32 pc, UINT32 opcode);

		void Reset();

		bool GetInfo(char *str);
	};

	//class CConditionBreakpoint : public CBrekapoint
	//{
	//	// TODO
	//}
}

#endif	// INCLUDED_BREAKPOINT_H
#endif  // SUPERMODEL_DEBUGGER