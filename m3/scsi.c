/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * scsi.c
 *
 * NCR 53C810 SCSI controller emulation.
 *
 * Multi-byte data written is assumed to be little endian and is therefore
 * reversed because the PowerPC operates in big endian mode in the Model 3.
 *
 * NOTE: scsi_run() is often called with an argument of 100. This is
 * actually incorrect. It should run until an interrupt condition. A count of
 * 100 is used as a runaway counter to prevent lock-ups.
 */

#include "model3.h"

/*
 * Read/Write Handlers
 *
 * These are used as an interface to the rest of the system.
 */

static UINT8    (*read_8)(UINT32);
static UINT16   (*read_16)(UINT32);
static UINT32   (*read_32)(UINT32);
static void     (*write_8)(UINT32, UINT8);
static void     (*write_16)(UINT32, UINT16);
static void     (*write_32)(UINT32, UINT32);

/*
 * 53C810 Context
 *
 * Offsets within the register array correspond to register addresses but the
 * endianness in which they are stored is little endian. Macros are provided
 * to access them.
 */

#define REG8(r)     scsi_regs[r]
#define REG16(r)    *((UINT16 *) &scsi_regs[r])
#define REG32(r)    *((UINT32 *) &scsi_regs[r])

static UINT8    scsi_regs[0x60];
static BOOL     scripts_exec;   // true if SCRIPTS code should be executed

/*
 * SCRIPTS Emulation
 */

#include "scsiop.h"

/*
 * UINT8 scsi_read_8(UINT32 addr);
 *
 * Reads from the SCSI register space.
 *
 * Parameters:
 *      addr = Register address (only lower 8 bits matter.)
 *
 * Returns:
 *      Returns data from the register read.
 */

UINT8 scsi_read_8(UINT32 addr)
{
    UINT8   reg;

    addr &= 0xFF;
    if (addr > 0x5F)
        error("%08X: SCSI invalid read from %02X", ppc_get_reg(PPC_REG_PC), addr);

    reg = REG8(addr);

    switch (addr)
    {
    case 0x14:  
        REG8(addr) &= 0xFE; // clear DIP
        break;
    default:
        break;
    }

    return reg;
}

/*
 * UINT32 scsi_read_32(UINT32 addr);
 *
 * Reads from the SCSI register space.
 *
 * Parameters:
 *      addr = Register address (only lower 8 bits matter.)
 *
 * Returns:
 *      Returns data from the register read.
 */

UINT32 scsi_read_32(UINT32 addr)
{
    UINT32  reg;

//    error("SCSI 32-bit read from %08X", addr);

    addr &= 0xFF;
    if (addr > 0x5F)
        error("%08X: SCSI invalid read from %02X", ppc_get_reg(PPC_REG_PC), addr);

    reg = REG32(addr);

    switch (addr)
    {
    case 0x14:  
        error("%08X: SCSI 32-bit read from reg %02X -- don't know what to do", ppc_get_reg(PPC_REG_PC), addr);
        break;
    default:
        // NOTE: some registers might have to be handled to clear certain
        // bits
        break;
    }

    return reg;
}


/*
 * void scsi_write_8(UINT32 addr, UINT8 data);
 *
 * Writes a byte to the SCSI register space.
 *
 * Parameters:
 *      addr = Register address (only lower 8 bits matter.)
 *      data = Data to write.
 */

void scsi_write_8(UINT32 addr, UINT8 data)
{
    addr &= 0xFF;
    if (addr > 0x5F)
        error("%08X: SCSI invalid write8 to %02X", ppc_get_reg(PPC_REG_PC), addr);

    message(0, "%08X: SCSI %02X = %02X", ppc_get_reg(PPC_REG_PC), addr, data);

    REG8(addr) = data;

    switch (addr)
    {
    case 0x3B:  // DCNTL: DMA Control

        /*
         *   7      6      5      4      3      2      1      0
         * +------+------+------+------+------+------+------+------+
         * | CLSE | PFF  | PFEN | SSM  | IRQM | STD  | IRQD | COM  |
         * +------+------+------+------+------+------+------+------+
         *
         * CLSE     Cache Line Size Enable
         *
         * PFF      Prefetch Flush
         *
         * PFEN     Prefetch Enable
         *
         * SSM      Single Step Mode
         *
         *          Setting this bit causes the SCSI to stop after executing
         *          each SCRIPTS instruction and generate a single step
         *          interrupt. To restart the SCSI after the interrupt has
         *          been generated, read ISTAT and DSTAT to recognize and
         *          clear the interrupt, then set STD bit in this register
         *          to resume execution.
         *
         * IRQM     IRQ Mode
         *
         * STD      Start DMA Operation
         *
         *          The SCSI fetches an instruction and executes it when this
         *          bit is set. It is required when the SCSI is in one of the
         *          following modes:
         *
         *              - Manual start mode: Bit 0 in the DMODE register is
         *                set.
         *              - Single step mode: Bit 4 in the DCNTL register is
         *                set.
         *
         *          When in manual mode, setting this bit starts instruction
         *          execution -- it remains set until an interrupt occurs. In
         *          single step mode, it restarts execution after an
         *          interrupt.
         *
         * IRQD     IRQ Disable
         *
         * COM      LSI53C700 Family Compatibility
         */

        /*
         * Single-stepping is checked for in scsi_run() (the number of
         * instructions to execute will be reduced to 1 there.)
         */

        if ((REG8(0x38) & 0x01) ||          // MAN=1, start SCRIPTS on STD=1
            (REG8(0x3B) & 0x10))            // single step, resume execution
        {
            scripts_exec = data & 0x04;
            scsi_run(100);
        }
        break;
    }
}

/*
 * void scsi_write_16(UINT32 addr, UINT16 data);
 *
 * Writes a word to the SCSI register space.
 *
 * Parameters:
 *      addr = Register address (only lower 8 bits matter.)
 *      data = Data to write.
 */

void scsi_write_16(UINT32 addr, UINT16 data)
{
    data = BSWAP16(data);
    message(0, "%08X: SCSI %02X = %04X", ppc_get_reg(PPC_REG_PC), addr, data);
}

/*
 * void scsi_write_32(UINT32 addr, UINT32 data);
 *
 * Writes a byte to the SCSI register space.
 *
 * Parameters:
 *      addr = Register address (only lower 8 bits matter.)
 *      data = Data to write.
 */

void scsi_write_32(UINT32 addr, UINT32 data)
{
    if ((addr & 0xff) > 0x5C)
        error("%08X: SCSI invalid write32 to %08X", ppc_get_reg(PPC_REG_PC), addr);
    addr &= 0xff;

    data = BSWAP32(data);
    REG32(addr) = data;

//    message(0, "%08X: SCSI %02X = %08X", ppc_get_reg(PPC_REG_PC), addr, data);

    switch (addr)
    {
    case 0x2C:  // DSP: DMA SCRIPTS Pointer
        if ((REG8(0x38) & 0x01) == 0)   // MAN=0, start SCRIPTS automatically
            scripts_exec = 1;
        scsi_run(100);
        break;
    default:
        break;
    }
}

/*
 * void scsi_save_state(FILE *fp);
 *
 * Saves the state of the SCSI controller to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void scsi_save_state(FILE *fp)
{
    fwrite(scsi_regs, sizeof(UINT8), 0x60, fp);
    fwrite(&scripts_exec, sizeof(BOOL), 1, fp);
}

/*
 * void scsi_load_state(FILE *fp);
 *
 * Loads the state of the SCSI controller from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void scsi_load_state(FILE *fp)
{
    fread(scsi_regs, sizeof(UINT8), 0x60, fp);
    fread(&scripts_exec, sizeof(BOOL), 1, fp);
}

/*
 * void scsi_reset(void);
 *
 * Initializes the SCSI controller to its power-on state. All bits which are
 * marked as undefined by the manual are simply set to 0 here.
 */

void scsi_reset(void)
{
    memset(scsi_regs, 0, sizeof(scsi_regs));
    scripts_exec = 0;

    REG8(0x00) = 0xc0;
    REG8(0x0C) = 0x80;
    REG8(0x0F) = 0x02;
    REG8(0x18) = 0xff;
    REG8(0x19) = 0xf0;
    REG8(0x1A) = 0x01;
    REG8(0x46) = 0x60;
    REG8(0x47) = 0x0f;
    REG8(0x4C) = 0x03;
}

/*
 * void scsi_init(UINT8 (*read8)(UINT32), UINT16 (*read16)(UINT32),
 *                UINT32 (*read32)(UINT32), void (*write8)(UINT32, UINT8),
 *                void (*write16)(UINT32, UINT16),
 *                void (*write32)(UINT32, UINT32));
 *
 * Initializes the SCSI controller emulation by building the SCRIPTS opcode
 * table and setting the memory access handlers.
 *
 * Parameters:
 *      read8   = Handler to use for 8-bit reads.
 *      read16  = Handler to use for 16-bit reads.
 *      read32  = Handler to use for 32-bit reads.
 *      write8  = Handler to use for 8-bit writes.
 *      write16 = Handler to use for 16-bit writes.
 *      write32 = Handler to use for 32-bit writes.
 */

void scsi_init(UINT8 (*read8)(UINT32), UINT16 (*read16)(UINT32),
               UINT32 (*read32)(UINT32), void (*write8)(UINT32, UINT8),
               void (*write16)(UINT32, UINT16),
               void (*write32)(UINT32, UINT32))
{
    read_8 = read8;
    read_16 = read16;
    read_32 = read32;
    write_8 = write8;
    write_16 = write16;
    write_32 = write32;

    build_opcode_table();
}

/*
 * void scsi_shutdown(void);
 *
 * Shuts down the SCSI emulation (actually does nothing.)
 */

void scsi_shutdown(void)
{
}
