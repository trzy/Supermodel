/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson 
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * 53C810.h
 * 
 * Header file defining the C53C810 class (NCR 53C810 SCSI controller).
 */

#ifndef INCLUDED_53C810_H
#define INCLUDED_53C810_H

#include "CPU/IRQController.h"
#include "PCI.h"
#include "BlockFile.h"
#include "CPU/Bus.h"
#include "SCSIBus.h"

/*
 * struct NCR53C810Context:
 *
 * Context information for an NCR 53C810 device. Used internally by the C53C810
 * class (defined below).
 */
struct NCR53C810Context
{
	UINT8	regs[0x60];

	// Registers defined below should not be read from here, not regs[]
	UINT32	regTEMP;	// TEMP
	UINT32	regDSP;		// DSP: DMA SCRIPTS Pointer
	UINT32	regDSPS;	// DSPS: DMA SCRIPTS Pointer Save
	UINT32	regDBC;		// DBC: DMA Byte Counter (24 bits)
	UINT8	regDCMD;	// DCMD: DMA Command
	UINT8	regDCNTL;	// DCNTL: DMA Control
	UINT8	regDMODE;	// DMODE: DMA Mode
	UINT8	regDSTAT;	// DSTAT: DMA Status (read only)
	UINT8	regDIEN;	// DIEN: DMA Interrupt Enable
	UINT8	regISTAT;	// ISTAT: Interrupt Status

	// Operational status
	bool	halt;		// set true if halted by interrupt instruction
	bool	idle;		// set true if SCRIPTS have not been started since reset or were stopped by an unrecognized instruction
	bool	waiting;	// set true if the last instruction is waiting on the SCSI bus (re-executed by the next Run())
	bool	carry;		// ALU carry flag (register arithmetic, carry tests)

	// SCSI bus: the initiator on the bus, or null if no bus is attached (target mode)
	ISCSIInitiator	*initiator;

	// Big endian bus object for DMA memory access and instruction fetching
	IBus	*Bus;

	// IRQ handling
	IIRQController	*IRQ;		// IRQ controller
	uint16_t	scsiIRQ;	// IRQ line mask to use when calling IRQ controller
};

/*
 * C53C810:
 *
 * NCR 53C810 SCSI controller device.
 */
class C53C810: public IPCIDevice
{
public:
	/*
	 * SaveState(SaveState):
	 *
	 * Saves an image of the current device state.
	 *
	 * Parameters:
	 *		SaveState	Block file to save state information to.
	 */
	void SaveState(CBlockFile *SaveState);

	/*
	 * LoadState(SaveState):
	 *
	 * Loads and a state image.
	 *
	 * Parameters:
	 *		SaveState	Block file to load state information from.
	 */
	void LoadState(CBlockFile *SaveState);

	/*
	 * ReadRegister(reg):
	 *
	 * Read from an operating register (8 bits at a time). 
	 *
	 * Parameters:
	 *		reg		Register offset ranging from 0x00 to 0x5F. Anything higher
	 *				is ignored and returns 0.
	 */
	UINT8 ReadRegister(unsigned reg);

	/*
	 * WriteRegister(reg, data):
	 *
	 * Write to an operating register (8 bits at a time). When breaking multi-
	 * byte words into individual bytes, make sure to write the lowest address
	 * first and the highest last. Some special functions registers, like DSP,
	 * will initiate processing only when the highest byte is written.
	 *
	 * Parameters:
	 *		reg		Register offset ranging from 0x00 to 0x5F. Anything higher
	 *				is ignored.
	 *		data	Data to write.
	 */
	void WriteRegister(unsigned reg, UINT8 data);

	/*
	 * ReadPCIConfigSpace(device, reg, bits, offset):
	 *
	 * Reads a PCI configuration space register. See CPCIDevice definition for
	 * more details.
	 *
	 * Parameters:
	 *		device	Device number (ignored, not needed).
	 *		reg		Register number.
	 *		bits	Bit width of access (8, 16, or 32 only).;
	 *		offset	Byte offset within register, aligned to the specified bit
	 *				width, and offset from the 32-bit aligned base of the
	 * 				register number.
	 *
	 * Returns:
	 *		Register data.
	 */
	UINT32 ReadPCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset) const;

	/*
	 * WritePCIConfigSpace(device, reg, bits, offset, data):
	 *
	 * Writes to a PCI configuration space register. See CPCIDevice definition
	 * for more details.
	 *
	 * Parameters:
	 *		device	Device number (ignored, not needed).
	 *		reg		Register number.
	 *		bits	Bit width of access (8, 16, or 32 only).
	 *		offset	Byte offset within register, aligned to the specified bit
	 *				width, and offset from the 32-bit aligned base of the
	 * 				register number.
	 *		data	Data.
	 */
	void WritePCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset, UINT32 data);

	/*
	 * GetBaseAddress(void):
	 * 
	 * Retrieves the base address of the SCSI device.
	 */
	UINT8 GetBaseAddress(void) const;

	/*
	 * Run(maxInstructions):
	 *
	 * Executes SCRIPTS until the SCRIPTS processor halts or the instruction
	 * limit is reached, whichever comes first. A limit is always required
	 * because SCRIPTS can loop indefinitely. SCRIPTS are also started
	 * immediately (with a fixed limit) when the registers that start them are
	 * written; this may be called periodically to continue execution.
	 *
	 * Parameters:
	 *		maxInstructions	Maximum number of instructions to execute.
	 *
	 * Returns:
	 *		Number of instructions executed.
	 */
	unsigned Run(unsigned maxInstructions);

	/*
	 * Reset(void):
	 *
	 * Resets the device. 
	 */
	void Reset(void);

	/*
	 * Init(BusObjectPtr, IRQObjectPtr, scsiIRQBit):
	 *
	 * One-time initialization of the context. Must be called prior to all
	 * other members.
	 *
	 * Parameters:
	 *		BusObjectPtr	Pointer to the bus that the 53C810 has control
	 *						over. Used to read/write memory.
	 *		IRQObjectPtr	Pointer to the IRQ controller. Used to trigger SCSI
	 *						and DMA interrupts.
	 *		scsiIRQBit		IRQ line mask to pass along to IRQ controller
	 *						when asserting interrupts.
	 */
	void Init(IBus *BusObjectPtr, IIRQController *IRQObjectPtr, uint16_t scsiIRQBit);

	/*
	 * AttachInitiator(initiator):
	 *
	 * Connects the controller to a SCSI bus that has an initiator on it. The
	 * controller can then operate as a target: it can be selected, transfer
	 * bytes, disconnect, and reselect. If no initiator is attached, the
	 * controller is never selected.
	 *
	 * Parameters:
	 *		initiator	The initiator, or null to disconnect from the bus. The
	 *					initiator must stay available while it is attached.
	 */
	void AttachInitiator(ISCSIInitiator *initiator);

	/*
	 * C53C810(void):
	 * ~C53C810(void):
	 *
	 * Constructor and destructor.
	 */
	C53C810(void);
	~C53C810(void);

private:
	// Private members
	Result	ExecuteInstruction(void);	// fetches and executes the instruction at DSP
	void	SingleStep(void);			// executes one instruction and raises the single step interrupt
	void	BuildOpTable(void);
	void	Insert(UINT8 mask, UINT8 op, Result (*Handler)(struct NCR53C810Context *));
	Result	(*OpTable[256])(struct NCR53C810Context *);

	// Context (register file)
	struct NCR53C810Context	Ctx;

	// IRQ line mask for this SCSI controller
	uint16_t	scsiIRQ;

	// Base address of the SCSI device (varies by game)
	UINT8	baseAddress;
};


#endif	// INCLUDED_53C810_H
