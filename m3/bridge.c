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
 * bridge.c
 *
 * MPC105 and MPC106 PCI bridge/memory controller emulation.
 *
 * Warning: Portability hazard. Code assumes it is running on a little endian
 * machine.
 *
 * NOTE: Data written to the bridge controller is byte-reversed by the PPC.
 * This is to make it little endian. 
 */

#include "model3.h"

/******************************************************************/
/* Internal Bridge State                                          */
/******************************************************************/

/*
 * Macros for Register Access
 *
 * Assuming a little endian system (portability hazard!)
 */

#define DWORD(addr) *((UINT32 *) &regs[addr])
#define WORD(addr)  *((UINT16 *) &regs[addr])
#define BYTE(addr)  regs[addr]

/*
 * Bridge State
 *
 * This code is intended for use on little-endian systems. Data from the
 * registers should be returned as if a big endian read was performed (thus
 * returning the little endian word 0x1057 as 0x5710.)
 *
 * The DWORD() and WORD() macros only work on little endian systems and are
 * intended to set up the bridge state. Reads and writes using the handlers
 * are done in big endian fashion by manually breaking down the data into
 * bytes.
 *
 * To make this code fully portable, it would probably be enough to remove
 * the DWORD() and WORD() macros.
 */

static UINT8    regs[0x100];
static UINT8    config_data[4]; // data for last CONFIG_ADDR command
static UINT32   reg_ptr;        // current register address

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

/*
 * Callback for Bus Commands
 *
 * Note that there are no callbacks for reads/writes to CONFIG_DATA yet as
 * the need for them has not yet appeared.
 */

static UINT32   (*config_addr_callback)(UINT32);

/*
 * UINT8 bridge_read_8(UINT32 addr);
 *
 * Reads directly from a register. The lower 8 bits of the address are the
 * register to read from. This is intended for the MPC105 which can map its
 * internal registers to 0xF8FFF0xx.
 *
 * Parameters:
 *      addr = Address (only lower 8 bits matter.)
 *
 * Returns:
 *      Data read.
 */

UINT8 bridge_read_8(UINT32 addr)
{
    LOG("RB REG%02X", addr & 0xff);

    return BYTE(addr & 0xff);
}

/*
 * UINT16 bridge_read_16(UINT32 addr);
 *
 * Reads directly from a register. The lower 8 bits of the address are the
 * register to read from. This is intended for the MPC105 which can map its
 * internal registers to 0xF8FFF0xx.
 *
 * Bit 0 is masked off to ensure 16-bit alignment (I'm not sure how a real
 * PCIBMC would handle this.)
 *
 * Parameters:
 *      addr = Address (only lower 8 bits matter.)
 *
 * Returns:
 *      Data read.
 */

UINT16 bridge_read_16(UINT32 addr)
{
    LOG("RH REG%02X", addr & 0xff);
    addr &= 0xfe;
    return (BYTE(addr + 0) << 8) | BYTE(addr + 1);
}

/*
 * UINT32 bridge_read_32(UINT32 addr);
 *
 * Reads directly from a register. The lower 8 bits of the address are the
 * register to read from. This is intended for the MPC105 which can map its
 * internal registers to 0xF8FFF0xx.
 *
 * The lower 2 bits are masked off to ensure 32-bit alignment.
 *
 * Parameters:
 *      addr = Address (only lower 8 bits matter.)
 *
 * Returns:
 *      Data read.
 */

UINT32 bridge_read_32(UINT32 addr)
{
    LOG("RW REG%02X", addr & 0xff);
    addr &= 0xfc;
    return (BYTE(addr + 0) << 24) | (BYTE(addr + 1) << 16) |
           (BYTE(addr + 2) << 8) | BYTE(addr + 3);
}

/*
 * void bridge_write_8(UINT32 addr, UINT8 data);
 *
 * Writes directly to a register. Only the lower 8 bits matter. This is
 * intended for the MPC105.
 *
 * Arguments:
 *      addr = Address (only lower 8 bits matter.)
 *      data = Data to write.
 */

void bridge_write_8(UINT32 addr, UINT8 data)
{
    LOG("REG%02X=%02X", addr & 0xff, data);
    BYTE(addr & 0xff) = data;
}

/*
 * void bridge_write_16(UINT32 addr, UINT16 data);
 *
 * Writes directly to a register. Only the lower 8 bits matter. This is
 * intended for the MPC105.
 *
 * Bit 0 is masked off to ensure 16-bit alignment.
 *
 * Arguments:
 *      addr = Address (only lower 8 bits matter.)
 *      data = Data to write.
 */

void bridge_write_16(UINT32 addr, UINT16 data)
{
    LOG("REG%02X=%02X%02X", addr & 0xff, data & 0xff, (data >> 8) & 0xff);
    addr &= 0xfe;
    BYTE(addr + 0) = data >> 8;
    BYTE(addr + 1) = (UINT8) data;
}

/*
 * void bridge_write_32(UINT32 addr, UINT32 data);
 *
 * Writes directly to a register. Only the lower 8 bits matter. This is
 * intended for the MPC105.
 *
 * The lower 2 bits are masked off to ensure 32-bit alignment.
 *
 * Arguments:
 *      addr = Address (only lower 8 bits matter.)
 *      data = Data to write.
 */

void bridge_write_32(UINT32 addr, UINT32 data)
{
    LOG("REG%02X=%02X%02X%02X%02X", addr & 0xff, data & 0xff, (data >> 8) & 0xff, (data >> 16) & 0xff, (data >> 24) & 0xff);
    addr &= 0xfc;
    BYTE(addr + 0) = data >> 24;
    BYTE(addr + 1) = data >> 16;
    BYTE(addr + 2) = data >> 8;
    BYTE(addr + 3) = data;
}

/*
 * UINT8 bridge_read_config_data_8(UINT32 addr);
 *
 * Reads from CONFIG_DATA offset 0 to 3. Only the lower 2 bits of the address
 * are checked to determine which byte to read. I'm not sure whether this is
 * correct behavior (perhaps all byte offsets return the same data.)
 *
 * Parameters:
 *      addr = Address to read from (only lower 2 bits matter.)
 *
 * Returns:
 *      Data read.
 */

UINT8 bridge_read_config_data_8(UINT32 addr)
{
    return config_data[addr & 3];
//    return BYTE(reg_ptr + (addr & 3));
}

/*
 * UINT16 bridge_read_config_data_16(UINT32 addr);
 *
 * Reads from CONFIG_DATA offset 0 or 2. Only bit 1 of the address is checked
 * to determine which word to read. I'm not sure whether this is correct
 * behavior for word reads (both words might return the same data, for
 * example.)
 *
 * Parameters:
 *      addr = Address to read from (only bit 1 matters.)
 *
 * Returns:
 *      Data read.
 */

UINT16 bridge_read_config_data_16(UINT32 addr)
{
    return (config_data[(addr & 2) + 0] << 8) | config_data[(addr & 2) + 1];
//    return (BYTE(reg_ptr + (addr & 2) + 0) << 8) |
//           BYTE(reg_ptr + (addr & 2) + 1);
}

/*
 * UINT32 bridge_read_config_data_32(UINT32 addr);
 *
 * Reads from CONFIG_DATA. The address argument is ignored because it is
 * assumed that this function is only called for valid CONFIG_DATA addresses.
 *
 * Parameters:
 *      addr = Ignored.
 *
 * Returns:
 *      Data read.
 */

UINT32 bridge_read_config_data_32(UINT32 addr)
{
    return (config_data[0] << 24) | (config_data[1] << 16) |
           (config_data[2] << 8) | config_data[3];
//    return (BYTE(reg_ptr + 0) << 24) | (BYTE(reg_ptr + 1) << 16) |
//           (BYTE(reg_ptr + 2) << 8) | BYTE(reg_ptr + 3);
}

/*
 * void bridge_write_config_addr_32(UINT32 addr, UINT32 data);
 *
 * Writes to CONFIG_ADDR. The address argument is ignored because it is
 * assumed that this function is only called for valid CONFIG_ADDR addresses.
 *
 * Parameters:
 *      addr = Ignored.
 *      data = Data to write.
 */

void bridge_write_config_addr_32(UINT32 addr, UINT32 data)
{
    UINT32  callback_data;

    /*
     * I'm not sure how the bridge controller distinguishes between indirect
     * register accesses and accesses which must be passed to some device on
     * the PCI bus.
     *
     * Currently, I test to see if the command is of the format 0x800000XX,
     * and if so, I return put register data in the return buffer
     * (config_data), otherwise I invoke the callback.
     */
    

//    LOG("CONFIG_ADDR=%02X%02X%02X%02X @ %08X", data & 0xff, (data >> 8) & 0xff, (data >> 16) & 0xff, (data >> 24) & 0xff, PowerPC_ReadIntegerRegister(POWERPC_IREG_PC));
//    reg_ptr = data >> 24;   // remember, data comes in little endian form
//                            // see page 3-15 of MPC106UM/D REV.1

    if ((data & 0x00ffffff) == 0x00000080)  // indirect register access (little endian data, see page 3-15 of MPC106UM/D REV.1)
    {
        reg_ptr = data >> 24;
        config_data[0] = BYTE(reg_ptr + 0);
        config_data[1] = BYTE(reg_ptr + 1);
        config_data[2] = BYTE(reg_ptr + 2);
        config_data[3] = BYTE(reg_ptr + 3);
    }
    else                                    // pass it to the callback
    {
        if (config_addr_callback != NULL)
        {
            callback_data = config_addr_callback(BSWAP32(data));
            callback_data = BSWAP32(callback_data); // so we can return as little endian
        }
        else
            callback_data = 0xFFFFFFFF;

        config_data[0] = callback_data >> 24;
        config_data[1] = callback_data >> 16;
        config_data[2] = callback_data >> 8;
        config_data[3] = callback_data;
    }
}

/*
 * void bridge_write_config_data_8(UINT32 addr, UINT8 data);
 *
 * Writes to CONFIG_DATA. The lower 2 bits determine which byte offset to
 * write to.
 *
 * Parameters:
 *      addr = Address to write to (only lower 2 bits matter.)
 *      data = Data to write.
 */

void bridge_write_config_data_8(UINT32 addr, UINT8 data)
{
//    LOG("CONFIG_DATA=%02X @ %08X", data, PowerPC_ReadIntegerRegister(POWERPC_IREG_PC));
    BYTE(reg_ptr + (addr & 3)) = data;
    config_data[addr & 3] = data;
}

/*
 * void bridge_write_config_data_16(UINT32 addr, UINT16 data);
 *
 * Writes to CONFIG_DATA. Only bit 1 of the address is checked to determine
 * which word to write to.
 *
 * Parameters:
 *      addr = Address to write to (only bit 1 matters.)
 *      data = Data to write.
 */

void bridge_write_config_data_16(UINT32 addr, UINT16 data)
{
    BYTE(reg_ptr + (addr & 2) + 0) = data >> 8;
    BYTE(reg_ptr + (addr & 2) + 1) = (UINT8) data;
    config_data[(addr & 2) + 0] = data >> 8;
    config_data[(addr & 2) + 1] = (UINT8) data;
//    LOG("CONFIG_DATA=%04X @ %08X", WORD(reg_ptr), PowerPC_ReadIntegerRegister(POWERPC_IREG_PC));
}

/*
 * void bridge_write_config_data_32(UINT32 addr, UINT32 data);
 *
 * Writes to CONFIG_DATA. The address argument is ignored because it is
 * assumed that this function is only called for valid CONFIG_DATA addresses.
 *
 * Parameters:
 *      addr = Ignored.
 *      data = Data to write.
 */

void bridge_write_config_data_32(UINT32 addr, UINT32 data)
{
    BYTE(reg_ptr + 0) = data >> 24;
    BYTE(reg_ptr + 1) = data >> 16;
    BYTE(reg_ptr + 2) = data >> 8;
    BYTE(reg_ptr + 3) = data;
    config_data[0] = data >> 24;
    config_data[1] = data >> 16;
    config_data[2] = data >> 8;
    config_data[3] = data;
//    LOG("CONFIG_DATA=%08X @ %08X", DWORD(reg_ptr), PowerPC_ReadIntegerRegister(POWERPC_IREG_PC));
}

/*
 * void bridge_save_state(FILE *fp);
 *
 * Saves the bridge controller state by writing it out to a file.
 *
 * Parameters:
 *      fp = File to write to.
 */

void bridge_save_state(FILE *fp)
{
    fwrite(regs, sizeof(UINT8), 0x100, fp);
    fwrite(config_data, sizeof(UINT8), 4, fp);
    fwrite(&reg_ptr, sizeof(UINT32), 1, fp);
}

/*
 * void bridge_load_state(FILE *fp);
 *
 * Loads the bridge controller state by reading it in from a file.
 *
 * Parameters:
 *      fp = File to read from.
 */

void bridge_load_state(FILE *fp)
{
    fread(regs, sizeof(UINT8), 0x100, fp);
    fread(config_data, sizeof(UINT8), 4, fp);
    fread(&reg_ptr, sizeof(UINT32), 1, fp);
}

/*
 * void bridge_reset(INT device);
 *
 * Initializes the bridge controller's internal state.
 *
 * Memory control configuration 1 is to be set to 0xFFn20000, where n is data
 * sampled from signals sent by hardware to the MPC105 (see the MPC105
 * manual.) For Model 3, I'm guessing that n == 0.
 *
 * Parameters:
 *      device = Device ID of the bridge to emulate (1 = MPC105, 2 = MPC106.)
 *               If it is not one of these values, behavior is undefined.
 */

void bridge_reset(INT device)
{
    if (device == 1)
        LOG("Using MPC105");
    else if (device == 2)
        LOG("Using MPC106");
    else
        LOG("ERROR: Unknown bridge controller device ID (%d)", device);

    memset(regs, 0, sizeof(UINT8) * 0x100);
    memset(config_data, 0, sizeof(UINT8) * 4);
    reg_ptr = 0;

    /*
     * Set up common fields
     */

    WORD(0x00) = 0x1057;        // vendor ID (Motorola)
    WORD(0x02) = device;        // device ID
    WORD(0x04) = 0x0006;        // PCI command
    WORD(0x06) = 0x0080;        // PCI status
    BYTE(0x08) = 0x00;          // revision ID (?)
    BYTE(0x0b) = 0x06;          // class code
    DWORD(0xa8) = 0xff000010;   // processor interface configuration 1
    DWORD(0xac) = 0x000c060c;   // processor interface configuration 2
    BYTE(0xba) = 0x04;          // alternate OS-visible parameters 1
    BYTE(0xc0) = 0x01;          // error enabling 1
    DWORD(0xf0) = 0xff020000;   // memory control configuration 1
    DWORD(0xf4) = 0x00000003;   // memory control configuration 2
    DWORD(0xfc) = 0x00100000;   // memory control configuration 4

    /*
     * Additional settings for MPC106
     */

    if (device == 2)    // MPC106
    {
        BYTE(0x0c) = 0x08;          // cache line size
        BYTE(0x73) = 0xcd;          // output driver control
        DWORD(0xe0) = 0x0fff0042;   // emulation support configuration 1
        DWORD(0xe8) = 0x00000020;   // emulation support configuration 2
    }
}

/*
 * void bridge_init(UINT32 (*config_addr_callback_ptr)(UINT32));
 *
 * Initializes the bridge controller by setting up the CONFIG_ADDR callback.
 * bridge_reset() must still be used to put the controller in a usable (reset)
 * state.
 */

void bridge_init(UINT32 (*config_addr_callback_ptr)(UINT32))
{
    config_addr_callback = config_addr_callback_ptr;
}
