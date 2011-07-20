#ifdef SUPERMODEL_DEBUGGER
#ifndef INCLUDED_68KDEBUG_H
#define INCLUDED_68KDEBUG_H

#include "Debugger/CPUDebug.h"
#include "Types.h"

#include "CPU/68K/Turbo68K/Turbo68K.h"

#define M68KSPECIAL_SP 0
#define M68KSPECIAL_SR 1

#define USE_NATIVE_READ 0
#define USE_NATIVE_WRITE 0
	
namespace Debugger
{
	/*
	 * Base class CCPUDebug implementation for Motorola 68000 CPUs.
	 */
	class C68KDebug : public CCPUDebug
	{
	private:
		char m_drNames[8][3];
		char m_arNames[8][3];

		char m_mSlotStr[32][20];
		char m_sSlotStr[32][20];
		char m_regStr[16][12];

		bool FormatAddrMode(UINT32 addr, UINT32 opcode, int &offset, UINT8 addrMode, char sizeC, char *dest);

	protected:
		virtual UINT32 GetSP() = 0;

	public:
		C68KDebug();

		// CCPUDebug methods

		int Disassemble(UINT32 addr, char *mnemonic, char *operands);
		
		EOpFlags GetOpFlags(UINT32 addr, UINT32 opcode);

		bool GetJumpAddr(UINT32 addr, UINT32 opcode, UINT32 &jumpAddr);

		bool GetJumpRetAddr(UINT32 addr, UINT32 opcode, UINT32 &retAddr);

		bool GetReturnAddr(UINT32 addr, UINT32 opcode, UINT32 &retAddr);

		bool GetHandlerAddr(CException *ex, UINT32 &handlerAddr);

		bool GetHandlerAddr(CInterrupt *in, UINT32 &handlerAddr);
	};
}

#endif	// INCLUDED_68KDEBUG_H
#endif  // SUPERMODEL_DEBUGGER