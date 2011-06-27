#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_DEBUGGER_H
#define INCLUDED_DEBUGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <algorithm>
using namespace std;

#include "Types.h"

#if defined(SUPERMODEL_WIN32) || defined(SUPERMODEL_UNIX) || defined(SUPERMODEL_OSX)
#define DEBUGGER_HASBLOCKFILE
#define DEBUGGER_HASLOGGER
#endif

#define DEBUGGER_STATEFILE_VERSION 0

#ifdef DEBUGGER_HASBLOCKFILE
#include "BlockFile.h"
#endif // DEBUGGER_HASBLOCKFILE

#ifdef DEBUGGER_HASLOGGER
#include "Logger.h"
#endif // DEBUGGER_HASLOGGER

#ifndef stricmp
#ifdef _MSC_VER	// MS VisualC++
	#define stricmp	_stricmp
#else           // Assume GC
	#define stricmp	strcasecmp
#endif // _MSC_VER
#endif // stricmp

namespace Debugger
{
	class CCPUDebug;
	class CCodeAnalyser;
	class CException;
	class CInterrupt;
	class CIO;
	class CWatch;
	class CBreakpoint;
	class CRegMonitor;

	enum EHaltReason
	{
		HaltNone  = 0,
		HaltState = 1,
		HaltUser  = 2,
		HaltStep  = 4,
		HaltCount = 8,
		HaltUntil = 16
	};

	enum EFormat
	{
		Hex = 0,
		Hex0x,
		HexDollar,
		HexPostH,
		Decimal,
		Binary,
		ASCII
	};

	/*
	 * Base class for a debugger.
	 * Provides common methods for loading/saving debugger state, formatting/parsing data and general control of the debugger.
	 * Also holds references to the CCPUDebug objects, one for each debuggable CPU, and contains pure virtual methods for sub-classes to 
	 * implement which act as callbacks for the various debugging events that can occur.
	 */
#ifdef DEBUGGER_HASLOGGER
	class CDebugger : public CLogger
#else
	class CDebugger
#endif // DEBUGGER_HASLOGGER
	{
	private:
		bool m_exit;
		bool m_pause;

	protected:
#ifdef DEBUGGER_HASBLOCKFILE
		virtual bool LoadState(CBlockFile *state);

		virtual bool SaveState(CBlockFile *state);
#endif

	public:
		vector<CCPUDebug*> cpus;

		UINT64 frameCount;

#ifdef DEBUGGER_HASLOGGER
		bool logDebug;
		bool logInfo;
		bool logError;
#endif // DEBUGGER_HASLOGGER

		static unsigned GetDataSize(UINT64 data);

		static const char *GetSizeString(unsigned dataSize);

		static UINT64 MaskData(unsigned dataSize, UINT64 data);

		static UINT64 GetSlottedData(UINT32 addr, unsigned dataSize, UINT64 data, UINT32 slotAddr, unsigned slotDataSize);

		static bool ParseInt(const char *str, int *val);

		CDebugger();

		virtual ~CDebugger();

		void AddCPU(CCPUDebug *cpu);

		void RemoveCPU(CCPUDebug *cpu);

		CCPUDebug *GetCPU(const char *name);

		void SetExit();

		void SetPause(bool pause);

		void ForceBreak(bool user);

		void ClearBreak();

		void SetContinue();

		bool CheckExit();

		bool CheckPause();
		
		//
		// Console output
		//

		void WriteOut(CCPUDebug *cpu, const char *typeStr, const char *fmtStr, ...);

#ifdef DEBUGGER_HASLOGGER
		//
		// CLogger logging methods
		//
		virtual void DebugLog(const char *fmt, va_list vl);

		virtual void InfoLog(const char *fmt, va_list vl);

		virtual void ErrorLog(const char *fmt, va_list vl);
#endif // DEBUGGER_HASLOGGER

		//
		// Formatting/parsing
		//

		// TODO - add in bufSize
		virtual bool ParseData(const char *str, EFormat format, unsigned dataSize, UINT64 *data);

		virtual void FormatData(char *str, EFormat format, unsigned dataSize, INT64 data);

		virtual void FormatData(char *str, EFormat format, unsigned dataSize, UINT64 data);

		//
		// State loading/saving
		//

		virtual bool HasState();

		virtual bool LoadState(const char *fileName);

		virtual bool SaveState(const char *fileName);

		//
		// Public virtual methods for sub-classes to implement
		//

		virtual void Attach();

		virtual void Detach();

		virtual void Reset();

		virtual void Poll();

		virtual void AnalysisUpdated(CCodeAnalyser *analyser) = 0;

		virtual void ExceptionTrapped(CException *ex) = 0;

		virtual void InterruptTrapped(CInterrupt *in) = 0;

		virtual void MemWatchTriggered(CWatch *watch, UINT32 addr, unsigned dataSize, UINT64 data, bool isRead) = 0;

		virtual void IOWatchTriggered(CWatch *watch, CIO *io, UINT64 data, bool isInput) = 0;

		virtual void BreakpointReached(CBreakpoint *bp) = 0;

		virtual void MonitorTriggered(CRegMonitor *regMon) = 0;

		virtual void ExecutionHalted(CCPUDebug *cpu, EHaltReason reason) = 0;

		virtual void WaitCommand(CCPUDebug *cpu) = 0;

		virtual void WriteOut(CCPUDebug *cpu, const char *typeStr, const char *fmtStr, va_list vl) = 0;
		
		virtual void FlushOut(CCPUDebug *cpu) = 0;
	};

	//
	// Inlined methods
	//

	inline bool CDebugger::CheckExit()
	{
		return m_exit;
	}

	inline bool CDebugger::CheckPause()
	{
		return m_pause;
	}
}

#endif	// INCLUDED_DEBUGGER_H
#endif  // SUPERMODEL_DEBUGGER
