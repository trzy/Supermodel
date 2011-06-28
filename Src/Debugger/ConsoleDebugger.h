#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_CONSOLEDEBUGGER_H
#define INCLUDED_CONSOLEDEBUGGER_H

#include "Debugger.h"
#include "CodeAnalyser.h"
#include "Types.h"

#include <stdarg.h>
#include <stdio.h>

#define NUM_LISTAUTOLABELS (sizeof(s_listAutoLabels) / sizeof(ELabelFlags))

namespace Debugger
{
	class CCPUDebug;
	class CException;
	class CWatch;
	class CBreakpoint;
	class CRegister;
	class CRegMonitor;

	/*
	 * Base class for a console-based debugger.
	 * TODO - a lot of commands are not yet implemented/missing and the main loop is a big ugly mess :-) ! 
	 */
	class CConsoleDebugger : public CDebugger
	{
	private:
		static ELabelFlags s_listAutoLabels[];

		CCPUDebug *m_cpu;

		bool m_nextFrame;
		unsigned m_nextFrameCount;

		UINT32 m_listDism;
		UINT32 m_listMem;
		
		bool m_analyseCode;
		EFormat m_addrFmt;
		EFormat m_portFmt;
		EFormat m_dataFmt;
		bool m_showLabels;
		bool m_labelsOverAddr;
		bool m_showOpCodes;
		unsigned m_memBytesPerRow;

		FILE *m_file;

	protected:
		void Read(char *str, size_t maxLen);

		void Print(const char *fmtStr, ...);

		void Error(const char *fmtStr, ...);

		void PrintVL(const char *fmtStr, va_list vl);

		void Flush();

		bool CheckToken(const char *token, const char *simple, const char *full);

		bool CheckToken(const char *token, const char *simple, const char *full, char *modifier, size_t modSize, const char *defaultMod);

		void Truncate(char *dst, size_t maxLen, const char *src);

		void UpperFirst(char *dst, const char *src);

		void FormatOpCodes(char *str, int addr, int codesLen);

		bool GetLabelText(char *str, int maxLen, UINT32 addr);

		bool SetBoolConfig(const char *str, bool &cfg);

		bool SetNumConfig(const char *str, unsigned &cfg);

		const char *GetFmtConfig(EFormat fmt);

		bool SetFmtConfig(const char *str, EFormat &cfg);

		bool ParseAddress(CCPUDebug *cpu, const char *str, UINT32 *addr);

		bool ParseDataSize(const char *str, unsigned &dataSize);

		bool ParseCPU(const char *str, CCPUDebug *&cpu);

		bool ParseRegister(const char *str, CRegister *&reg);

		void ListCPUs();

		void ListRegisters();

		void ListExceptions();

		void ListInterrupts();

		void ListIOs();

		void ListRegions();

		void ListLabels(bool customLabels, ELabelFlags autoLabelFlags);

		void GetAllMemWatches(vector<CWatch*> &watches);

		void ListMemWatches();

		void GetAllPortWatches(vector<CWatch*> &watches);

		void ListPortWatches();

		void ListBreakpoints();

		void ListMonitors();

		UINT32 ListDisassembly(UINT32 start, UINT32 end, unsigned numInstrs);

		UINT32 ListMemory(UINT32 start, UINT32 end, unsigned bytesPerRow);

		virtual void ApplyConfig();

		virtual void Attached();

		virtual void Detaching();

		virtual void AnalysisUpdated(CCodeAnalyser *analyser);

		virtual void ExceptionTrapped(CException *ex);

		virtual void InterruptTrapped(CInterrupt *ex);

		virtual void MemWatchTriggered(CWatch *watch, UINT32 addr, unsigned dataSize, UINT64 data, bool isRead);

		virtual void IOWatchTriggered(CWatch *watch, CIO *io, UINT64 data, bool isInput);

		virtual void BreakpointReached(CBreakpoint *bp);

		virtual void MonitorTriggered(CRegMonitor *regMon);

		virtual void ExecutionHalted(CCPUDebug *cpu, EHaltReason reason);

		virtual void WaitCommand(CCPUDebug *cpu);

		virtual bool ProcessToken(const char *token, const char *cmd);

		virtual void WriteOut(CCPUDebug *cpu, const char *typeStr, const char *fmtStr, va_list vl);
		
		virtual void FlushOut(CCPUDebug *cpu);

	public:
		CConsoleDebugger();

		void Attach();

		void Detach();

		virtual void Poll();
	};
}

#endif	// INCLUDED_CONSOLEDEBUGGER_H
#endif  // SUPERMODEL_DEBUGGER