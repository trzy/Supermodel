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
 * r3d.c
 *
 * Real3D Model 3 graphics system emulation. The 3D hardware in Model 3 is
 * supposedly based on the Pro-1000.
 */

/*
 * RAM Size:
 * ---------
 *
 * It appears that there is 2MB total of "culling RAM." 1MB appears at
 * 0x8C000000 and the other at 0x8E000000. Step 1.0 and 1.5 appear to have
 * 1MB of polygon RAM, but Step 2.0 (and probably 2.1) clearly uses 2MB.
 */

#include "model3.h"

/******************************************************************/
/* Privates                                                       */
/******************************************************************/

static UINT8    *culling_ram_8e;    // culling RAM at 0x8E000000
static UINT8    *culling_ram_8c;    // culling RAM at 0x8C000000
static UINT8    *polygon_ram;       // polygon RAM at 0x98000000
static UINT8	*vrom;

static UINT8    texture_buffer_ram[1*1024*1024];

static UINT32	vrom_texture_address;
static UINT32	vrom_texture_header;

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

/*
 * void r3d_init(UINT8 *culling_ram_8e_ptr, UINT8 *culling_ram_8c_ptr,
 *               UINT8 *polygon_ram_ptr, UINT8 *vrom_ptr);
 *
 * Initializes the Real3D graphics emulation.
 *
 * Parameters:
 *      culling_ram_8e_ptr = Pointer to 0x8E000000 culling RAM.
 *      culling_ram_8c_ptr = Pointer to 0x8C000000 culling RAM.
 *      polygon_ram_ptr    = Pointer to polygon RAM.
 *      vrom_ptr           = Pointer to VROM.
 */

void r3d_init(UINT8 *culling_ram_8e_ptr, UINT8 *culling_ram_8c_ptr,
              UINT8 *polygon_ram_ptr, UINT8 *vrom_ptr)
{
    culling_ram_8e = culling_ram_8e_ptr;
    culling_ram_8c = culling_ram_8c_ptr;
    polygon_ram = polygon_ram_ptr;
	vrom = vrom_ptr;
}

/*
 * void r3d_shutdown(void);
 *
 * Shuts down the Real3D emulation.
 */

void r3d_shutdown(void)
{
}

/*
 * void r3d_reset(void);
 *
 * Resets the Real3D graphics hardware. RAM is cleared in order to prevent
 * the renderer from drawing garbage and possibly locking up as a result.
 */

void r3d_reset(void)
{
    memset(culling_ram_8e, 0, 1*1024*1024);
    memset(culling_ram_8c, 0, 2*1024*1024);
    memset(polygon_ram, 0, 2*1024*1024);
	tap_reset();
}

/*
 * void r3d_save_state(FILE *fp);
 *
 * Saves the state of the Real3D graphics hardware to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void r3d_save_state(FILE *fp)
{
    fwrite(culling_ram_8e, sizeof(UINT8), 1*1024*1024, fp);
    fwrite(culling_ram_8c, sizeof(UINT8), 2*1024*1024, fp);
    fwrite(polygon_ram, sizeof(UINT8), 2*1024*1024, fp);
}

/*
 * void r3d_load_state(FILE *fp);
 *
 * Loads the state of the Real3D graphics hardware from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void r3d_load_state(FILE *fp)
{
    fread(culling_ram_8e, sizeof(UINT8), 1*1024*1024, fp);
    fread(culling_ram_8c, sizeof(UINT8), 2*1024*1024, fp);
    fread(polygon_ram, sizeof(UINT8), 2*1024*1024, fp);
}

/******************************************************************/
/* Access                                                         */
/******************************************************************/

/*
 * UINT32 r3d_read_32(UINT32 a);
 *
 * Reads a 32-bit word from the Real3D regions.
 *
 * Parameters:
 *      a = Address.
 *
 * Returns:
 *      Data read.
 */

UINT32 r3d_read_32(UINT32 a)
{
    static UINT32   _84000000 = 0;

    switch (a)
    {
    case 0x84000000:    // loop around 0x1174EC in Lost World expects bit
        return (_84000000 ^= 0xFFFFFFFF);
    case 0x84000004:    // unknown, Lost World reads these
    case 0x84000008:
    case 0x8400000C:
    case 0x84000010:
    case 0x84000014:
    case 0x84000018:
    case 0x8400001C:
    case 0x84000020:
        return 0xFFFFFFFF;
    }

    error("Unknown R3D read: %08X: %08X\n", ppc_get_reg(PPC_REG_PC), a);
	return(0);
}

/*
 * void r3d_write_32(UINT32 a, UINT32 d);
 *
 * Writes a 32-bit word to the Real3D regions.
 *
 * Parameters:
 *      a = Address.
 *      d = Data to write.
 */

void r3d_write_32(UINT32 a, UINT32 d)
{
    static UINT32   last_addr;
    UINT            size_x, size_y;

    if (a >= 0x8E000000 && a <= 0x8E0FFFFF)         // culling RAM
    {
        *(UINT32 *) &culling_ram_8e[a & 0xFFFFF] = BSWAP32(d);
        return;
    }
    else if (a >= 0x8C000000 && a <= 0x8C1FFFFF)    // culling RAM
    {
        *(UINT32 *) &culling_ram_8c[a & 0x1FFFFF] = BSWAP32(d);
        return;
    }
	else if (a >= 0x8C100000 && a <= 0x8C1FFFFF)	// culling RAM (Scud Race)
	{
		/*
		 * probabily this is a mirror of 0x8C000000 - 0x8C0FFFFF,
		 * as the other is never used when this one is.
		 * i'm leaving everything as is, though, so that you can study them separately.
		 */

        *(UINT32 *) &culling_ram_8c[a & 0x1FFFFF] = BSWAP32(d);
        return;
	}
    else if (a >= 0x98000000 && a <= 0x981FFFFF)    // polygon RAM
    {
        *(UINT32 *) &polygon_ram[a & 0x1FFFFF] = BSWAP32(d);
        return;
    }
    else if (a >= 0x94000000 && a <= 0x940FFFFF)    // texture buffer
    {
        d = BSWAP32(d);
        *(UINT32 *) &texture_buffer_ram[a & 0xFFFFF] = d;

        if (a == 0x94000004)    // get size and calculate last word written
        {
            size_x = (d >> 14) & 3;
            size_y = (d >> 17) & 3;
            size_x = 32 << size_x;
            size_y = 32 << size_y;

			if(!(d & 0x00800000))
				last_addr = (0x94000008 + size_x * size_y - 1) & ~3;		// 8-bit texture
			else
				last_addr = (0x94000008 + size_x * size_y * 2 - 2) & ~3;	// 16-bit texture

            message(0, "texture transfer started: %dx%d, %08X", size_x, size_y, d);
        }
        else if (a == last_addr)    // last word written
        {
            osd_renderer_upload_texture(*(UINT32 *) &texture_buffer_ram[4], *(UINT32 *) &texture_buffer_ram[0], &texture_buffer_ram[8], 1);
            message(0, "texture upload complete");
        }

        return;
    }

    switch (a)
    {
    case 0x88000000:    // 0xDEADDEAD
        message(0, "88000000 = %08X", BSWAP32(d));
        return;
    case 0x90000000:    // VROM texture address
        vrom_texture_address = BSWAP32(d);
        message(0, "VROM texture address = %08X", BSWAP32(d));
        return;
    case 0x90000004:    
        vrom_texture_header = BSWAP32(d);
        message(0, "VROM texture header = %08X", BSWAP32(d));
        return;
    case 0x90000008:
        osd_renderer_upload_texture(vrom_texture_header, BSWAP32(d), &vrom[(vrom_texture_address & 0x7FFFFF) * 4], 0);
        message(0, "VROM texture length = %08X", BSWAP32(d));
        return;
    case 0x9000000C:    // ? Virtual On 2: These are almost certainly for VROM textures as well (I was too lazy to check :P)
        vrom_texture_address = BSWAP32(d);
        message(0, "90000000C = %08X", BSWAP32(d));
        return;
    case 0x90000010:    // ?
        vrom_texture_header = BSWAP32(d);
        message(0, "900000010 = %08X", BSWAP32(d));
        return;
    case 0x90000014:    // ?
        osd_renderer_upload_texture(vrom_texture_header, BSWAP32(d), &vrom[(vrom_texture_address & 0x7FFFFF) * 4], 0);
        message(0, "900000014 = %08X", BSWAP32(d));
        return;
    case 0x9C000000:    // ?
        message(0, "9C000000 = %08X", BSWAP32(d));
        return;
    case 0x9C000004:    // ?
        message(0, "9C000004 = %08X", BSWAP32(d));
        return;
    case 0x9C000008:    // ?
        message(0, "9C000008 = %08X", BSWAP32(d));
        return;
    }

    error("Unknown R3D write: %08X: %08X = %08X\n", ppc_get_reg(PPC_REG_PC), a, d);
}

/******************************************************************/
/* Real3D TAP Port                                                */
/******************************************************************/

/*
 * State (corresponding to fsm[][] Y) and Instruction Names
 */

static char *state_name[] = { "Test-Logic/Reset", "Run-Test/Idle", "Select-DR-Scan",
                              "Capture-DR", "Shift-DR", "Exit1-DR", "Pause-DR",
                              "Exit2-DR", "Update-DR", "Select-IR-Scan",
                              "Capture-IR", "Shift-IR", "Exit1-IR", "Pause-IR",
                              "Exit2-IR", "Update-IR"
                            };

/*
 * TAP Finite State Machine
 *
 * Y are states and X are outgoing paths. Constructed from information on page
 * 167 of the 3D-RAM manual.
 */

#define NEXT(new_state) fsm[state][new_state]

static INT  state;  // current state
static INT  fsm[][2] =  {
                            {  1,  0 },  // 0  Test-Logic/Reset
                            {  1,  2 },  // 1  Run-Test/Idle
                            {  3,  9 },  // 2  Select-DR-Scan
                            {  4,  5 },  // 3  Capture-DR
                            {  4,  5 },  // 4  Shift-DR
                            {  6,  8 },  // 5  Exit1-DR
                            {  6,  7 },  // 6  Pause-DR
                            {  4,  8 },  // 7  Exit2-DR
                            {  1,  2 },  // 8  Update-DR
                            { 10,  0 },  // 9  Select-IR-Scan
                            { 11, 12 },  // 10 Capture-IR
                            { 11, 12 },  // 11 Shift-IR
                            { 13, 15 },  // 12 Exit1-IR
                            { 13, 14 },  // 13 Pause-IR
                            { 11, 15 },  // 14 Exit2-IR
                            {  1,  2 }   // 15 Update-IR
                        };

/*
 * TAP Registers
 */

static UINT64   current_instruction;    // latched IR (not always equal to IR)
static UINT64   ir;                     // instruction register (46 bits)

static UINT8    id_data[32];            // ASIC ID code data buffer
static INT      id_size;                // size of ID data in bits
static INT      ptr;                    // current bit ptr for data

static BOOL     tdo;                    // bit shifted out to TDO

/*
 * rotate_right():
 *
 * Rotates a 32-bit word right by 1 bit.
 */

static UINT32 rotate_right(UINT32 data)
{
    return ((data >> 1) | ((data & 1) << 31));
}

/*
 * insert_bit():
 *
 * Inserts a bit into an arbitrarily long bit field. Bit 0 is assumed to be
 * the MSB of the first byte in the buffer.
 */

static void insert_bit(UINT8 *buf, INT bit_num, INT bit)
{
    INT bit_in_byte;

    bit_in_byte = 7 - (bit_num & 7);

    buf[bit_num / 8] &= ~(1 << bit_in_byte);
    buf[bit_num / 8] |= (bit << bit_in_byte);
}

/*
 * insert_id():
 *
 * Inserts a 32-bit ID code into the ID bit field.
 */

static void insert_id(UINT32 id, INT start_bit)
{
    INT i;

    for (i = 31; i >= 0; i--)
        insert_bit(id_data, start_bit++, (id >> i) & 1);
}

/*
 * shift():
 *
 * Shifts the data buffer right (towards LSB at byte 0) by 1 bit. The size of
 * the number of bits must be specified. The bit shifted out of the LSB is
 * returned.
 */

static BOOL shift(UINT8 *data, INT num_bits)
{
    INT     i;
    BOOL    shift_out, shift_in;

    /*
     * This loop takes care of all the fully-filled bytes
     */

    shift_in = 0;
    for (i = 0; i < num_bits / 8; i++)
    {
        shift_out = data[i] & 1;
        data[i] >>= 1;
        data[i] |= (shift_in << 7);
        shift_in = shift_out;   // carry over to next element's MSB
    }

    /*
     * Take care of the last partial byte (if there is one)
     */

    if ((num_bits & 7) != 0)
    {
        shift_out = (data[i] >> (8 - (num_bits & 7))) & 1;
        data[i] >>= 1;
        data[i] |= (shift_in << 7);
    }

    return shift_out;
}

static void shift_data(UINT32 *data, INT num_bytes)
{
    INT     i;
    BOOL    shift_out, shift_in;

    shift_in = 0;
    for (i = 0; i < num_bytes; i++)
    {
        shift_out = data[i] & 1;
        data[i] >>= 1;
        data[i] |= (shift_in << 31);
        shift_in = shift_out;   // so we add it to the next element's MSB
    }
}

/*
 * BOOL tap_read(void);
 *
 * Reads TDO.
 *
 * Returns:
 *      TDO.
 */

BOOL tap_read(void)
{
    return tdo;
}

/*
 * void tap_write(BOOL tck, BOOL tms, BOOL tdi, BOOL trst);
 *
 * Writes to the TAP. State changes only occur on the rising edge of the clock
 * (tck = 1.)
 *
 * Parameters:
 *      tck  = Clock.
 *      tms  = Test mode select.
 *      tdi  = Serial data input. Must be 0 or 1 only!
 *      trst = Reset.
 */

void tap_write(BOOL tck, BOOL tms, BOOL tdi, BOOL trst)
{
    if (!tck)
        return;

    state = NEXT(tms);

    switch (state)
    {
    case 3:     // Capture-DR

        if (current_instruction == 0x0c631f8c7ffe)
        {           
            insert_id(0x116C7057, 1 + 0 * 32);
            insert_id(0x216C3057, 1 + 1 * 32);
            insert_id(0x116C4057, 1 + 2 * 32);
            insert_id(0x216C5057, 1 + 3 * 32);
            insert_id(0x116C6057, 1 + 4 * 32 + 1);
            insert_id(0x116C6057, 1 + 5 * 32 + 1);
        }

        break;

    case 4:     // Shift-DR

        tdo = shift(id_data, id_size);
        break;

    case 10:    // Capture-IR

        /*
         * Load lower 2 bits with 01 as per IEEE 1149.1-1990
         */

        ir = 1;
        break;

    case 11:    // Shift-IR

        /*
         * Shift IR towards output and load in new data from TDI
         */

        tdo = ir & 1;   // shift LSB to output
        ir >>= 1;
        ir |= ((UINT64) tdi << 45);
        break;

    case 15:    // Update-IR

        /*
         * Latch IR (technically, this should occur on the falling edge of
         * TCK)
         */

        ir &= 0x3fffffffffff;
        current_instruction = ir;
        {
            UINT8   *i = (UINT8 *) &ir;
//            LOG("model3.log", "current instruction set: %02X%02X%02X%02X%02X%02X\n", i[5], i[4], i[3], i[2], i[1], i[0]);
        }


        break;

    default:
        break;
    }

#if 0
    if (state == 4)
        LOG("model3.log", "state: Shift-DR %d\n", tdi);
    else if (state == 11)
        LOG("model3.log", "state: Shift-IR %d\n", tdi);
    else
        LOG("model3.log", "state: %s\n", state_name[state]);
#endif
}


/*
 * void tap_reset(void);
 *
 * Resets the TAP (simulating a power up or SCAN_RST signal.)
 */

void tap_reset(void)
{
    id_size = 197;  // 197 bits

    state = 0;  // test-logic/reset
}
