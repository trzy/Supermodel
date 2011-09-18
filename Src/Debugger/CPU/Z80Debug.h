#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_Z80DEBUG_H
#define INCLUDED_Z80DEBUG_H

#include "Debugger/CPUDebug.h"
#include "CPU/Bus.h"
#include "Types.h"

class CZ80;

namespace Debugger
{
	/*
	 * CCPUDebug implementation for the Zilog Z80 emulator.
	 */
	class CZ80Debug : public CCPUDebug, public ::CBus
	{
	private:
		static UINT8 ReadReg8(CCPUDebug *cpu, unsigned reg8);
		
		static bool WriteReg8(CCPUDebug *cpu, unsigned reg8, UINT8 value);

		static UINT16 ReadReg16(CCPUDebug *cpu, unsigned reg16);

		static bool WriteReg16(CCPUDebug *cpu, unsigned reg16, UINT16 value);

		CZ80 *m_z80;
		::CBus *m_bus;

		char m_vecIds[256][5];
		char m_vecNames[256][22];
		char m_portNames[256][10];

	public:
		CZ80Debug(const char *name, CZ80 *z80);

		virtual ~CZ80Debug();

		// CZ80Debug methods

		void AttachToCPU();

		::CBus *AttachBus(::CBus *bus);

		void DetachFromCPU();
		
		::CBus *DetachBus();

		UINT32 GetResetAddr();

		bool UpdatePC(UINT32 pc);
		
		bool ForceException(CException *ex);

		bool ForceInterrupt(CInterrupt *in);

		UINT64 ReadMem(UINT32 addr, unsigned dataSize);

		bool WriteMem(UINT32 addr, unsigned dataSize, UINT64 data);

		UINT64 ReadPort(UINT16 portNum);

		bool WritePort(UINT16 portNum, UINT64 data);

		int Disassemble(UINT32 addr, char *mnemonic, char *operands);

		//int GetOpLength(UINT32 addr);

		EOpFlags GetOpFlags(UINT32 addr, UINT32 opcode);

		bool GetJumpAddr(UINT32 addr, UINT32 opcode, UINT32 &jumpAddr);
		
		bool GetJumpRetAddr(UINT32 addr, UINT32 opcode, UINT32 &retAddr);

		bool GetReturnAddr(UINT32 addr, UINT32 opcode, UINT32 &retAddr);

		bool GetHandlerAddr(CException *ex, UINT32 &handlerAddr);

		bool GetHandlerAddr(CInterrupt *in, UINT32 &handlerAddr);

		// CBus methods
		
		UINT8 Read8(UINT32 addr);

		void Write8(UINT32 addr, UINT8 data);

		UINT8 IORead8(UINT32 portNum);

		void IOWrite8(UINT32 portNum, UINT8 data);
	};
}

#endif	// INCLUDED_Z80DEBUG_H
#endif  // SUPERMODEL_DEBUGGER