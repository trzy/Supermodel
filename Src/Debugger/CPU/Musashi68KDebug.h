#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_MUSASHI68KDEBUG_H
#define INCLUDED_MUSASHI68KDEBUG_H

#include "68KDebug.h"
#include "CPU/Bus.h"
#include "CPU/68K/68K.h"
#include "Types.h"

using namespace Debugger;
// TODO - can't get this namespace to work with 68K.h for some reason - strange Visual Studio errors
//namespace Debugger
//{
	/*
	 * CCPUDebug implementation for the Musashi Motorola 68000 emulator.
	 */
	class CMusashi68KDebug : public C68KDebug, public ::CBus
	{
	private:
		static UINT32 GetReg(CCPUDebug *cpu, unsigned id);

		static bool SetReg(CCPUDebug *cpu, unsigned id, UINT32 data);

		M68KCtx *m_ctx;
		UINT32 m_resetAddr;

		M68KCtx m_savedCtx;

		::CBus *m_bus;

		void SetM68KContext();

		void UpdateM68KContext(M68KCtx *ctx);

		void RestoreM68KContext();

	protected:
		UINT32 GetSP();

	public:
		CMusashi68KDebug(const char *name, M68KCtx *m68K);

		virtual ~CMusashi68KDebug();

		// CCPUDebug methods

		void AttachToCPU();

		void DetachFromCPU();

		UINT32 GetResetAddr();

		bool UpdatePC(UINT32 pc);

		bool ForceException(CException *ex);

		bool ForceInterrupt(CInterrupt *in);

		UINT64 ReadMem(UINT32 addr, unsigned dataSize);

		bool WriteMem(UINT32 addr, unsigned dataSize, UINT64 data);

		// CBus methods
		
		UINT8 Read8(UINT32 addr);

		void Write8(UINT32 addr, UINT8 data);
		
		UINT16 Read16(UINT32 addr);

		void Write16(UINT32 addr, UINT16 data);
		
		UINT32 Read32(UINT32 addr);

		void Write32(UINT32 addr, UINT32 data);
	};

	// Inlined methods

	inline void CMusashi68KDebug::SetM68KContext()
	{
		M68KGetContext(&m_savedCtx);
		if (m_savedCtx.Debug != this)
			M68KSetContext(m_ctx);
	}

	inline void CMusashi68KDebug::UpdateM68KContext(M68KCtx *ctx)
	{
		m_ctx = ctx;
	}

	inline void CMusashi68KDebug::RestoreM68KContext()
	{
		if (m_savedCtx.Debug != this)
			M68KSetContext(&m_savedCtx);	
	}
//}

#endif	// INCLUDED_MUSASHI68KDEBUG_H
#endif  // SUPERMODEL_DEBUGGER