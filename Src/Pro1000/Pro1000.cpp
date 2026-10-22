/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * Pro1000.cpp
 *
 * Implementation of the CPro1000 class: a Real3D Pro-1000 image generator.
 *
 * The Pro-1000 was attached to a Windows NT host via SCSI. Applications ran on
 * the host and sent data to the image generator, which had its own display.
 * The Model 3 graphics hardware is derived from it. This file documents what
 * has been learned about the hardware from its firmware. Everything here was
 * reverse engineered and should be treated as a working hypothesis unless
 * stated otherwise.
 *
 *
 * Methods
 * -------
 * The firmware (p1_p4.bin, 0x7014C bytes) is a flat big endian PowerPC image
 * loaded at 0xFFF00000. Findings come from:
 *
 *  - Disassembly with ppcd (bin64/ppcd.exe roms/P1_P4.bin -o FFF00000).
 *  - Logging unmapped accesses (DebugLog, -log-level=all) and UART output.
 *  - SCCS ID strings: each C module embeds "@(#)<file>.c <ver> <date>" in its
 *    data section. Data adjacent to an ID string belongs to that module, which
 *    attributes tables (e.g., register address tables) to source files.
 *  - Address cross-referencing: code forms addresses with "lis rX,hi" followed
 *    by "addi/ori rY,rX,lo". Collecting all such pairs reveals which code
 *    references which string or table. Error messages often contain function
 *    names (e.g., "jtag_wrt with zero bits"), which locates those functions.
 *  - Datasheets for identified parts (National SCANPSC100F, Intel 8259A,
 *    NCR 53C8xx, 16550 UART).
 *  - The firmware's own address table (see Address Map below), found in
 *    init_rom.c's data. It names every hardware address used by the firmware.
 *
 * Most firmware code runs from RAM. To convert, RAM = ROM - 0xFFF10000 +
 * 0x3000. Addresses below are RAM addresses unless prefixed with "ROM".
 *
 *
 * SDK Documentation
 * -----------------
 * The Pro-1000 SDK CD (Owner's Manual v3.0e, Product Description v4.0e,
 * Developer's Guide v4.0e, API headers) describes the product from the user's
 * point of view. Hardware facts relevant to emulation:
 *
 *  - Models: PRO-1100/1110 (1 pixel processor, 50M pixel writes/s, 640x480),
 *    PRO-1200/1210 (2 pixel processors, 100M, 640x480 or dual 512x384/486),
 *    PRO-1400/1410 (4 pixel processors, 200M, 1024x768 or dual 640x480, soft
 *    edge blending). These correspond to the firmware's single pixel board,
 *    dual pixel board, and high res configurations.
 *  - Update rate: 30 or 60 Hz. Standard line rates are 640x480 and 1024x768
 *    non-interlaced at 60 Hz and 512/640/720x486 interlaced at 59.94 Hz and
 *    720x576 at 50 Hz ("application programmers should treat this as 60 Hz").
 *    The API's PRO_Line_Rate enumeration lists 8 line rates (VGA_640X480=1,
 *    SVGA_1024X768=2, NTSC_505X480=3, ..., PAL_720X576=8), matching the
 *    firmware's 8 sync files. Display timing is programmable.
 *  - Overload modes: off (frame lock: drop polygons to hold the update rate),
 *    on (halve the frame rate, e.g. 60 to 30 Hz), extended (render everything
 *    regardless of time). Overload state can be daisy chained between units.
 *  - Memory: 8MB polygon memory, expandable in 8MB modules to 32MB. Texture
 *    memory is 4M 16-bit texels (8MB), optionally 16M (32MB). Texel formats
 *    include 4/8-bit monochrome, 1-5-5-5, and 4-4-4-4. Maps are 32x32 to
 *    1024x1024. Up to 4096 matrices; matrix tree depth 15.
 *  - Rendering: 24-bit floating point Z-buffer, stencil for coplanar
 *    polygons, post-process edge anti-aliasing ("S-buffer"). Up to 4
 *    viewports per channel with priorities 0-3.
 *  - Video output: 8-bit DACs, gamma correction (256 entry 8-bit lookup table
 *    per color component), soft edge blending (1400 series only), foreground
 *    and background colors (see SCROLL register), dual channel output.
 *  - Host interface: differential fast-wide SCSI II at 20MB/s, up to 7 units
 *    per host adapter. The SCSI ID is set with DIP switches 1-4 (5-8 are
 *    reserved) behind the rear panel. The protocol's data structures are in
 *    API/Include/pro_io_structs.hh (Scsi_Head: address, word count, control,
 *    and valid/swapped/render done/operation failed flags; Hw_Config; sync
 *    and miscellaneous information).
 *  - Rear panel: two video outputs, genlock in/out/sync, overload in/out,
 *    an RS-232 port, a "HOST" port providing RS-232 syncs for the host, SCSI
 *    in/out, SCSI ID switches, and a reset button.
 *  - Power-on: the unit runs a confidence test and then displays a spinning
 *    Real 3D logo. The API describes the logo as the "default database"
 *    (PRO_ReloadLogo; monitor command LDDB). This is most likely the contents
 *    of the SCSI ROM. The SDK's DB_Demos/Orlando and DB_Demos/Bayou contain
 *    3dlogo.cmem and 3dlogo.pmem (see SCSI ROM).
 *  - The real-time clock count returned to the host is in 30 ns hardware
 *    clocks.
 *  - Demo databases (DB_Demos) are distributed precompiled as .cmem (culling
 *    memory), .pmem (polygon memory), and .tmap (texture) files.
 *
 *
 * Models
 * ------
 * Games.xml selects the model with the stepping field and the polygon memory
 * with the polygon_memory field (in MB):
 *
 *   stepping  Model     Firmware configuration  Config byte (0x611A8)
 *   1100      PRO-1100  Single pixel board      1
 *   1200      PRO-1200  Dual pixel board        2
 *   1400      PRO-1400  High res                4
 *
 * If there is no stepping, the model is the PRO-1100. The polygon memory is
 * 8, 16, 24, or 32 MB (8 MB on the base board and up to three 8 MB cards,
 * independent of the model). If there is no polygon_memory, it is 8 MB.
 *
 * The firmware finds the model from the JTAG scan chain (see JTAG), and
 * CPro1000::BuildScanChain() makes the chain of the model. The firmware finds
 * the polygon memory size in get_hw_config (0x6A48): for k = 1 to 3, it
 * writes 0xCAFEBABE to 0x98000000 + k * 8 MB with the CPU and reads the word
 * back through the Venus JTAG polygon memory access (0x1DDF4, see
 * CASIC::PolygonMemoryAccess() in JTAG.cpp). It stops at the first word that
 * is not correct. The number of correct words is the number of memory cards
 * (bits 7-6 of the byte at RAM 0x70004), and show_config_data prints 8 MB +
 * 8 MB for each card. CPro1000::Init() gives the size to CReal3D
 * (Real3DConfig::installedPolygonRAMSize): writes above it are ignored and
 * reads return 0.
 *
 * show_config_data also prints the system type (bits 4-2 of the byte at RAM
 * 0x70005, set in get_hw_config at 0x6C44):
 *
 *   Production High Resolution System  high res configuration
 *   Production System                  single or dual configuration, the ID
 *                                      code version fields of the Mars
 *                                      ASICs are not 0
 *   Pre-Production System              single or dual configuration, the
 *                                      Mars version fields are 0
 *
 * Supermodel sets the version fields to 0 (Pre-Production). With version 3
 * (the Model 3 value), the firmware prints "Production System". Other than
 * this message, the effect of the system type is not known.
 *
 * Only the PRO-1100 is emulated fully. For the PRO-1200 and PRO-1400, the
 * firmware detects the configuration and the memory, but the renderer does
 * not emulate the additional pixel processors, dual channel output, or high
 * res video timing.
 *
 *
 * ROM Layout
 * ----------
 * Names in parentheses are from the firmware's address table.
 *
 * ROM FFF00000-FFF0FFFF  Boot stub (CPU_ROM_BASE; reset vector CPU_BOOT_BASE)
 * ROM FFF10000-FFF7FFFF  Main firmware (LOOP_ROM_BASE-LOOP_ROM_TOP), copied to
 *                        RAM 00003000 (CODE_BASE)
 * ROM FFF80000           Logic sync memory contents (LOGIC_SYNC_INIT)
 * ROM FFF90000           Video sync memory contents (VIDEO_SYNC_INIT)
 * ROM FFF94000           Scroll register (SCROLL_INIT)
 * ROM FFF94004/06/08     PLL REF/FBK/CW registers (PLL_*_INIT)
 * ROM FFF9400A           Mode word (MODEWORD_INIT)
 * ROM FFF9400C           RIF mode word (REND_INIT_START)
 *
 * ROM FFFEFFFC           INIT ROM revision (54 in p2.bin)
 * ROM FFFFF000           Gamma bank 0 (magic 0xA569A569, then 256 RGB
 *                        triplets from FFFFF008), saved by monitor command GMS
 *
 * The ROM is flash and the firmware can reprogram it. The SDK's flash update
 * (burn.exe, "Loop Code Software Revision 5.4, Init ROM Revision 54")
 * consists of two images:
 *
 *   p1_p4.bin  Parts 1 and 4: boot stub and loop code, loaded at FFF00000
 *              (region "cpu_rom"). Ends at FFF7014C.
 *   p2.bin     Part 2: INIT ROM, loaded at FFF80000-FFFEFFFF (region
 *              "init_rom"). Identified by the revision word at FFFEFFFC and
 *              the contents at the addresses named in the firmware's address
 *              table (sync memories, scroll, PLL, mode words).
 *
 * FFFF0000-FFFFFFFF (gamma and soft edge blend data written by GMS/BMS) is not
 * part of the update and reads as 0xFF (erased flash), so the firmware reports
 * no gamma or blend data. The gap between the two images also reads as 0xFF.
 *
 *
 * Boot Sequence
 * -------------
 * Boot stub (ROM):
 *  1. Reads RIF mode word (RIF_MODE), checks bit 13, writes 0x2000 to
 *     RIF_MODE and to MAIN_RST_AUX (0x40000000).
 *  2. Initializes the UART and prints "go" four times.
 *  3. Copies ROM FFF10000-FFF7FFFC to RAM 0x3000.
 *  4. Copies 64KB from ROM FFF80000 to byte-wide logic sync memory
 *     (LSYNC_BASE, 0x60000000).
 *  5. Copies 16KB from ROM FFF90000 to halfword-wide video sync memory
 *     (VSYNC_BASE, 0x58000000).
 *  6. Loads SCROLL (0x62000000) and the PLL registers (0x74000000-0x74000010)
 *     from ROM FFF94000-FFF94008, and ORs ROM FFF9400C | 0x100 into RIF_MODE.
 *  7. Jumps to RAM 0x3000 (_start).
 *
 * _start (RAM 0x3000, startup.c):
 *  1. Prints RIF mode word (register), RIF mode word from ROM (FFF9400C),
 *     merges them, and prints PPC version.
 *  2. Initializes the 8259 (0x4AA8) and registers interrupt handlers (IR0,
 *     IR7, IR3).
 *  3. Configures PCI device 11 (SCSI, 0x4E0C).
 *  4. Resets the SCANPSC100F: writes 2 to MODE2, waits for the reset bit to
 *     clear, then writes 0x0C (status update). Registers IR4 (jtag_int).
 *  5. Initializes the SCSI controller ("ncrinit"), SCSI ID 0.
 *  6. Measures field time: clears the field counter at 0x5D960 and waits up to
 *     0x7F2C89 time base ticks for the IR0 handler to increment it. Without
 *     field syncs, it prints "NO FIELD SYNCS DETECTED", skips the steps
 *     below that touch the renderer, and goes on to read DIP switches
 *     (probably RIF register bits 23:16, active low), print the revision,
 *     and enter the main loop (". . . entering main loop . . .").
 *  7. With field syncs: prints "field time = N milliseconds", reads ASIC ID
 *     codes over JTAG and prints any that do not match (see JTAG section),
 *     falls back to the low res single pixel board configuration if
 *     detection fails, and calls write_scroll().
 *  8. Calls 0x5660 (sets up mode words via 0x130A4 and configures PCI
 *     device 12 via 0x4E78), then prints "_start: before get_hw_config".
 *     0x5660 waits for a GP sync (IR7) via 0x577C.
 *  9. get_hw_config (measures the polygon memory, see Models), rend_mw_init,
 *     then rend_init (0x51A8), which uploads
 *     initial renderer memory contents from the SCSI ROM (see SCSI ROM
 *     below). Without a valid SCSI ROM it prints "SCSI ROM magic does NOT
 *     validate" and aborts. rom_to_bm and rom_to_gm then look for blend and
 *     gamma data in the INIT ROM.
 *
 *
 * SCSI ROM
 * --------
 * A flash memory attached to the SCSI controller's expansion ROM interface
 * (SCSI_ROM_BASE, 0xC0000000, 512KB). It is called the SCSI ROM because of
 * where it is connected, not what it is for:
 *
 *  - The firmware's address table names 0xC0000000-0xC0080000 SCSI_ROM_BASE
 *    and SCSI_ROM_TOP.
 *  - The firmware maps it by programming the expansion ROM BAR of PCI device
 *    11 (the 53C825) to 0xC0000001.
 *  - Firmware messages refer to it as the SCSI ROM ("SCSI ROM magic does NOT
 *    validate", "test_flash_prog: erase SCSI ROM").
 *
 * On a PC SCSI card this interface holds the BIOS. Here it appears to be used
 * as general purpose flash storage readable over PCI, separate from the INIT
 * ROM in CPU flash.
 *
 * It is a data ROM. No code executes from it: none of the code that forms
 * 0xC0xxxxxx addresses branches indirectly. It begins with a directory of
 * little endian 32-bit words (the PowerPC byte swaps them when reading across
 * PCI). Offsets are relative to SCSI_ROM_BASE.
 *
 * Directory format. Each entry is one 32-bit word. Offsets are byte offsets
 * from SCSI_ROM_BASE. Sizes are in bytes and must be multiples of 4. Words
 * that are not in the table were not seen to be read.
 *
 *   +0x00  Magic, 0xCAFEBABE. rend_init and 0xA680 check it.
 *   +0x0C  PowerPC address (0x80000000-0x9FFFFFFC) to which 0xA680 writes
 *          one view table entry (12 words) each frame.
 *   +0x1C  Size of polygon block A.
 *   +0x20  Offset of polygon block A.
 *   +0x24  Offset of the culling block.
 *   +0x28  Size of the culling block.
 *   +0x2C  Offset of polygon block B.
 *   +0x30  Size of polygon block B.
 *   +0x34  Offset of the view table. Each entry is 12 words (48 bytes).
 *   +0x38  Number of entries in the view table. 0 disables 0xA680.
 *   +0x3C  Update memory configuration, word 0 (ping-pong memory size).
 *   +0x40  Update memory configuration, word 1.
 *   +0x44  Update memory configuration, word 2 (ping-pong memory size plus
 *          update memory size).
 *   +0x48  Number of words in each part of the polygon block A upload. It
 *          must not be 0 if polygon block A is not empty (0x615C does not
 *          stop).
 *
 * rend_init (0x51A8) loads the ROM in this sequence:
 *
 *  1. If the magic is not correct, it prints "SCSI ROM magic does NOT
 *     validate" and stops.
 *  2. It writes the three update memory configuration words to
 *     REND_UPDATE_START (0x9C000000). The write function sends them to
 *     Mercury through JTAG (see Update Memory Configuration). Then it writes
 *     REND_UPDATE_COMPL (0x88000000) and waits for GP sync (0x577C) four
 *     times.
 *  3. It copies the culling block to REND_CULL_BASE (0x8C000000) in four
 *     parts. Parts 1 to 3 each have size / 16 words (rounded down). Part 4
 *     has the remaining words.
 *  4. 0x615C copies polygon block A. First, it writes the word
 *     (n << 3) | 4 to REND_POLY_BASE (0x98000000), where n is the number of
 *     words in the block. Then it copies the block to 0x98000004, in parts
 *     of the number of words at +0x48 (the last part can be smaller). After
 *     that, it writes 0 to REND_ROM_UPDATE (0x90000000, the texture port).
 *  5. 0x12AF0 copies polygon block B to REND_POLY_BASE (0x98000000) in four
 *     parts, as in step 3. After each part, it waits for bit 2 of RAM
 *     0x62384 (the copy is complete, see below).
 *  6. It calls 0x10984.
 *
 * Polygon blocks A and B both start at 0x98000000. Thus block B overwrites
 * the start of block A. Probably a ROM uses only one of them.
 *
 * The CPU copies each part from the ROM to a RAM buffer (0x944C), word for
 * word with no byte swap. Thus the data is in the byte order of the renderer
 * (little endian). The .cmem and .pmem files are big endian. Then 0x10E3C
 * writes the buffer to the renderer. It uses the CPU for a part of at most
 * the number of words at RAM 0x5E060, and a 53C825 SCRIPTS memory move (DMA,
 * 0x45AC0) for a larger part. The main loop sets 0x5E060 to 40 words. During
 * rend_init it is still 0, thus all parts use the 53C825. At the end of the
 * move, the script interrupts with DSPS 0xFFFFFF0B, and the SCSI interrupt
 * handler (0x452C8) sets bit 2 of RAM 0x62384 (the copy is complete).
 *
 * After each part, the firmware calls 0x63DC (it waits a maximum of 8 ms for
 * bit 2 of 0x62384), writes REND_UPDATE_COMPL, and waits for GP sync. If
 * 0x63DC fails, it prints "SCSI_BLK_MV_DONE time-out for address ...". Only
 * 0x615C (polygon block A) stops on a failure (an error loop). In Supermodel,
 * this message can occur although the copy is complete: the 8259 interrupts
 * the CPU only between CPU time slices (see Interrupts), and 0x63DC stops
 * immediately when its two time base reads are equal.
 *
 * When there is no host, the firmware draws the scene in the ROM. 0xA908
 * calls 0xA680, which makes a render job from the ROM, and then 0xA984 (the
 * render cycle). 0xA680 does nothing if the magic is not correct or if the
 * view table is empty. Each job uses the next entry of the view table (the
 * index is at RAM 0x5E014 and goes back to 0 after the last entry). The 12
 * words of an entry are probably a view matrix or other viewpoint data. This
 * is probably how the spinning logo turns.
 *
 * The SCSI ROM is not part of the SDK flash update (burn.exe programs only
 * P1_P4.bin and P2.bin), and no file in the SDK contains a SCSI ROM image.
 * Its contents are unknown. All that is known is where the blocks are
 * uploaded: culling memory, polygon memory, and the configuration registers.
 * The SDK documentation suggests it is the default database, a spinning Real
 * 3D logo displayed at power-on. Host command 5 (PRO_ReloadLogo, a packet
 * write to 0x20000005) and the monitor command LDDB run rend_init again and
 * then go back to the idle loop. The firmware has a flash test that erases
 * and programs the SCSI ROM (test_flash_prog, 0x99AC), but no code calls it.
 *
 * The firmware writes only one word of the root viewport (the screen size,
 * 0x8E000050, from 0x10984). It is not known how the real ROM supplies the
 * rest of the viewport. See "Update Memory Configuration" for a hypothesis.
 *
 * Emulation: the ROM is mapped read only at SCSI_ROM_BASE regardless of the
 * expansion ROM BAR (the firmware always programs 0xC0000000). Writes (flash
 * programming) are not emulated. If the game defines a "scsi_rom" region, it
 * is loaded as a byte image in PCI address order. Otherwise a synthetic image
 * is created (BuildSyntheticSCSIROM() in SyntheticSCSIROM.cpp). It can
 * contain the logo database from the Assets folder, but it has no viewport.
 * Thus nothing is drawn.
 *
 *
 * Update Memory Configuration
 * ---------------------------
 * The Real3D keeps writes that occur after the ping-pong flip in update
 * memory, in low culling RAM (see "Update Buffers" in Real3D.cpp). Three
 * values set the areas. On Model 3, the CPU writes them to the configuration
 * registers at 0x9C000000, 0x9C000004, and 0x9C000008:
 *
 *   word 0  Ping-pong memory size in words.
 *   word 1  Unknown (flags?). Model 3 games and the SCSI ROM use small values.
 *   word 2  Ping-pong memory size plus update memory size in words.
 *
 * Supermodel keeps the high culling RAM update buffer in low culling RAM
 * words 0 to word 0 - 1 and the polygon RAM update buffer in words word 0 to
 * word 2 - 1. High culling RAM (0x8E000000) is a separate memory.
 *
 * The Pro-1000 firmware does not write these values to memory. Its function
 * that writes words to a renderer address (0x10E3C) sends a write to
 * 0x9C000000 to 0x11060. 0x11060 keeps the three words at RAM 0x5F2A0 and
 * sends them to Mercury through JTAG: instruction 0x09, with a 57-bit data
 * register. The register has three fields of 19 bits, from the most
 * significant bit: word 0, word 1, word 2.
 *
 * The firmware sends values two times:
 *
 *  - At boot, rend_init sends the words at +0x3C in the SCSI ROM.
 *  - The host sends them in a packet to 0x9C000000. The SDK demos send
 *    0x8000, 0, 0x20000. Bayou first sends 0x45, 0x7FF76, 0x7FFBB and then
 *    the SDK values (see below).
 *
 * The SDK defaults: ping-pong memory of 32K words at PRO_PINGPONG_BASE
 * (0x8E000000) and a total of 128K words. Model 3 Step 1.0 games use the
 * same values. These are the values in the synthetic SCSI ROM (word 0 is
 * 0x7FFC, 4 words less, as on Model 3).
 *
 * Ping-pong memory (not emulated): there are two physical ping-pong
 * memories. A write to 0x8E000000 goes only to the memory that is not drawn,
 * and data that must stay is written to both (the firmware writes each word
 * two times with a flip between; Model 3 games write such data exactly two
 * times in consecutive frames). See "TODO: Physical Ping-Pong Memory" in
 * Real3D.cpp for the evidence.
 *
 * Hypothesis (weak evidence): one of the two ping-pong memories is low
 * culling RAM words 0 to word 0 - 1, and culling address 0x800000 + i
 * (0x8E000000 + 4i) is word i of it. Evidence:
 *
 *  - With the configuration 0x45, 0x7FF76, 0x7FFBB, Bayou writes a 0x45-word
 *    scene to low culling words 0-0x44 (0x8C000000): a viewport (words
 *    0-0x2F, one viewport), a pointer list (0x30), matrix 0 (0x31-0x3C), and
 *    a culling node with no child (0x3D-0x44). Its pointers are 0x800030,
 *    0x800031, 0x80003D, and 0x800000. They are correct only if culling
 *    address 0x800000 + i is low culling word i.
 *  - The packet parser rejects writes to 0x8E000000 above word 0 ("PPO").
 *  - The high culling RAM data of Bayou ends at word 0x8000 (its word 0).
 *  - Hosts put their culling data above word 2 (Asia: 0x20000, Bayou:
 *    0x40000). Model 3 games (Scud Race, Virtua Fighter 3, Daytona USA 2)
 *    do not write low culling RAM below word 0x20000.
 *
 * But there is no evidence that the renderer draws this scene. If the
 * hypothesis is correct, the culling block of the SCSI ROM (loaded at
 * 0x8C000000) can contain the root viewport, and 0x10984 then sets its
 * screen size through 0x8E000050. But rend_init writes the culling block
 * only one time, through 0x8C000000, thus it is in only one ping-pong memory,
 * and the logo would not be shown in each frame. It is not known how the
 * hardware prevents this.
 *
 * Emulation: Mercury has the register (CASIC, instruction 9) on Model 3 and
 * on the Pro-1000, because the two use the same ASICs. An update of the
 * register writes the three values to the Real3D configuration registers. A
 * capture reads them. Thus the register adds no state. No Model 3 game that
 * was examined loads instruction 9 (they use 0x03, 0x08, 0x0A, 0x0B, 0x18,
 * and 0x1F with Mercury). Thus Model 3 operation does not change.
 *
 *
 * Host Commands
 * -------------
 * A packet write item to an address with bits 31:24 = 0x20 is a command, not
 * a write to memory (packet parser 0xAC18, write handler 0xBEF0, command
 * dispatcher 0x16000, jump table at 0x160A0). The firmware has a list of the
 * command names (0x52930):
 *
 *   0x20000001  mem_op_valid checking (PRO_Init)
 *   0x20000002  Prev_buff processing flag (PRO_Init, PRO_SetDebugOn/Off;
 *               sets a flag at RAM 0x5E058)
 *   0x20000003  Soft Edge Blending Option
 *   0x20000004  Gamma Correction Option (ls_ctrl_gamma; see JTAG)
 *   0x20000005  Reload Default Data base (PRO_ReloadLogo): runs rend_init,
 *               sets the video sync again, and goes back to the idle loop
 *   0x20000006  Load gamma memory bank 0
 *   0x20000007  Load gamma memory bank 1
 *   0x20000008  Save Soft Edge Blend Memory to ROM
 *   0x20000009  Invalidate Soft Edge Blend Memory in ROM
 *   0x2000000A  Update Specific Pixels in Soft Edge Blend Memory
 *   0x2000000B  Texture (0 = off, 1 = on, other = texture map size in words)
 *   0x2000000C  Texture Translucency On/Off
 *   0x2000000D  S-Buffer On/Off
 *   0x2000000E  Polygon Translucency On/Off
 *   0x2000000F  Contour Translucency On/Off
 *   0x20000010  Edges On/Off
 *   0x20000011  Set Cursor/Edge Color 0-7 (3 bits RGB)
 *   0x20000012  Select Sync File (1-8)
 *   0x20000013  Set Default Sync File (1-8)
 *   0x20000014  Stencil On/Off
 *   0x20000015  Shading On/Off
 *   0x20000016  Illumination On/Off
 *   0x20000017  Fading On/Off
 *   0x20000018  Save Gamma Memory to ROM
 *   0x20000019  Invalidate Gamma Memory in ROM
 *   0x2000001A  Set Drop SCSI BSY Time
 *   0x2000001B  Set up as master for 30 Hz genlock
 *   0x2000001C  Genlock as 30 Hz Slave
 *   0x2000001D  Burst Mode On/Off
 *   0x20000020  Do_update_complete On/Off (sets a flag at RAM 0x5E05C)
 *   0x200000FC  Read Soft Edge Blend Memory from ROM into Buffer to Host
 *   0x200000FD  Read Gamma Memory Bank 0 from ROM into Buffer to Host
 *   0x200000FE  Read Gamma Memory Bank 1 from ROM into Buffer to Host
 *   0x200000FF  Read Both Gamma Memory Banks from ROM into Buffer to Host
 *   0x74000000  Genlock Control
 *
 *
 * PowerPC Address Map
 * -------------------
 * The firmware contains a table of 76 named hardware addresses (RAM
 * 0x5F2B0-0x5FE90, entries of a 36-byte name followed by a 32-bit value).
 * Names below are from that table. Descriptions are inferred.
 *
 * 00000000-003FFFFF  DRAM_BASE/DRAM_TOP  RAM (4MB). DRAM_G1_TOP=0x02000000 is
 *                                        also defined; its meaning is unknown.
 * 00010000           LOW_MEM_TOP
 * 00003000           CODE_BASE           Main firmware copied here
 * 40000000           MAIN_RST_AUX        Reset auxiliary processor? 0x2000
 *                                        written at boot.
 * 42000000           AUX_INT_MAIN        Auxiliary to main interrupt?
 * 44000000           MAIN_INT_AUX        Main to auxiliary interrupt?
 * 48000000           AUX_INTACK          Auxiliary interrupt acknowledge
 * 50000000/08        AUX_INT_R0/R1       Auxiliary interrupt controller (a
 *                                        second 8259?)
 * 52000000           MAIN_INTACK         8259 INTA: each write is one pulse.
 *                                        [Emulated]
 * 54000000/08        MAIN_INT_R0/R1      Main 8259A. [Emulated]
 * 58000000           VSYNC_BASE          Video sync memory, 8K halfwords at
 *                                        stride 8
 * 60000000           LSYNC_BASE          Logic sync memory, 64K bytes at
 *                                        stride 8
 * 62000000           SCROLL              Pluto scroll register (also accessible
 *                                        via JTAG as "PL Scroll Register").
 *                                        Bit 26: 1 = background, 0 =
 *                                        foreground, bits 25-24: viewport
 *                                        select, bits 23-0: RGB color (from
 *                                        the SC monitor command's formatted
 *                                        dump). INIT ROM value is 0x040CF0F8.
 *                                        A solid color layer. A host packet
 *                                        write to 0x62000000 goes to
 *                                        write_scroll (0x145D4), which stores
 *                                        the word here (single and dual
 *                                        pixel boards) or sends it to Pluto
 *                                        through JTAG (high res, config byte
 *                                        4). Bayou sends
 *                                        0x07DCDCDC. [Emulated: see
 *                                        CPro1000Render2D]
 * 64000000-38        UART_*              16550 UART (RDTD_BUFFER, INT_ENABLE,
 *                                        INT_ID/FIFO_CNTRL, LINE_CNTRL,
 *                                        MODEM_CNTRL, LINE_STATUS,
 *                                        MODEM_STATUS, SCRATCH, DIV_LATCH0/1).
 *                                        [Emulated]
 * 68000000-38        JTAG_*              SCANPSC100F (TDO, TDI, TMS0, TMS1,
 *                                        CNT0, MODE0-2). [Emulated]
 * 70000000           RIF_MODE            RIF mode word. Bit 13 checked at boot,
 *                                        bit 14 toggled by boot stub, bit 7
 *                                        cleared around GP sync waits
 *                                        (0x577C) and while command 5 reloads
 *                                        the SCSI ROM. Bit names from the
 *                                        RIF_MODE monitor command (0x17BA0):
 *                                        23 BIST Enabled, 22 Flash ROM Write
 *                                        Enabled, 21 Renderer Init Disabled
 *                                        (DIP switch: _start skips rend_init),
 *                                        20 Unused Switch, 19-16 SCSI ID,
 *                                        15 Align Syncs (write only), 14 BIST
 *                                        LED (0 = pass), 13 Main CPU Flag,
 *                                        12-9 402K/806K/1.6M/3.3M resistor
 *                                        for external sync logic, 8 Kill
 *                                        Video Syncs, 7 Ovld Force, 6 Ovld No
 *                                        Report, 5-4 Ovld Mode, 3 Ovld
 *                                        Enable, 2 Select External Video
 *                                        Reference, 1 Sync Transition Select
 *                                        (1 = even only), 0 Rif On-Line.
 * 72000000           HARD_RESET          Written in a loop by boot stub on 'r'
 * 74000000/08/10     PLL_REF/FBK/CW      Video PLL registers
 * 78000000/08        MAIN_RD_INTA_R0/R1  Main interrupt acknowledge by read?
 *                                        Read by IR0 handler.
 * 7A000000/08        AUX_RD_INTA_R0/R1   Auxiliary equivalent
 * 80000000           REND_BASE           Renderer (Real3D), PCI device 12 BAR0
 * 84000000           REND_STATUS         Status registers. [Emulated, Model 3]
 * 88000000           REND_UPDATE_COMPL   Update complete (Model 3 "flush").
 *                                        [Emulated, Model 3]
 * 8C000000-8FFFFFFC  REND_CULL_BASE/TOP  Culling memory (64MB window).
 *                                        [Model 3 8C/8E mapping emulated]
 * 90000000           REND_ROM_UPDATE     Texture port (Model 3 Step 1.0 VROM
 *                                        texture port). The data is the word
 *                                        address of a texture block in
 *                                        polygon memory. Used by firmware
 *                                        (e.g., 0x63AC) and accepted from the
 *                                        host. [Emulated as on Model 3 Step
 *                                        1.0, with polygon memory as VROM;
 *                                        all accesses logged at info level]
 * 94000000           REND_TEX_UPDATE,    Texture FIFO. [Emulated, Model 3]
 *                    REND_TEX_CONTINUE,
 *                    REND_TRANS_UPDATE
 * 98000000-9FFFFFFC  REND_POLY_BASE/TOP  Polygon memory (128MB window). There
 *                                        is no VROM: model and texture word
 *                                        addresses at 0x100000 and above are
 *                                        also polygon memory. [Emulated:
 *                                        98000000-9BFFFFFF, 64MB, enough for
 *                                        all 24-bit word addresses]
 * 9C000000           REND_UPDATE_START   Model 3 configuration registers.
 *                                        [Emulated, Model 3]
 * C0000000-C007FFFF  SCSI_ROM_BASE/TOP   SCSI ROM, 512KB (see SCSI ROM below).
 *                                        [Emulated: read only, optional image
 *                                        "scsi_rom", otherwise synthetic]
 * D0000000           SCSI_MEM_BASE       SCSI controller registers. [Emulated]
 * F0800000           CONFIG_ADDR         MPC105 (same as Model 3 Step 1.0)
 * F0C00000           CONFIG_DATA
 * F8FFF000           BRIDGE_CSR_BASE     MPC105 registers
 * FFF00000-FFFFFFFF  CPU_ROM_BASE/TOP    Flash ROM (mapped in an 8MB window
 *                                        from 0xFF800000)
 *
 * Byte-wide peripherals (8259, UART, SCANPSC100F, logic sync memory) are connected
 * to D7:0 and decode address bits 3 and up: register n is the least
 * significant byte of the big endian word at base+n*8. Firmware uses both
 * byte (base+n*8+3) and word (base+n*8) accesses.
 *
 * DEADBEEF/CAFEBABE/DEADFACE test patterns written by the boot stub to 58, 60,
 * 62, 68, and 74 confirm these are distinct register/memory regions.
 *
 *
 * PCI
 * ---
 * The firmware never reads configuration space (no device probing). It
 * programs fixed device numbers through the MPC105 (CONFIG_ADDR values decoded
 * as bus 0, device 11 or 12):
 *
 *  Device 11 (SCSI, 0x4E0C):
 *    04 Command  = 0x0006 (memory space, bus master)
 *    0C          = 0x8000 (latency timer 0x80)
 *    14 BAR1     = 0xD0000000 (memory-mapped registers)
 *    30 ROM BAR  = 0xC0000001 (expansion ROM at 0xC0000000, enabled)
 *  Device 12 (probably Real3D, 0x4E78, not yet reached):
 *    04 Command  = 0x0142, status cleared
 *    10 BAR0     = 0x80000000
 *
 * The SCSI controller is an NCR 53C825 (scsi825.c, sslib25.c; "NCR825 is
 * revision 0 silicon"). It is emulated with the 53C810, which has a
 * compatible register set. Its flash ROM (expansion ROM) can be reprogrammed
 * ("test_flash_prog: erase SCSI ROM").
 *
 *
 * Interrupts
 * ----------
 * An 8259A (int_8259.c) drives the PowerPC external interrupt. Initialization
 * (0x4AA8): ICW1=0x13 (edge, single, ICW4), ICW2=0x00, ICW4=0x01 (8086 mode),
 * mask all, OCW2=0xC6 (IR6 lowest priority), OCW3=0x0B (read ISR).
 *
 * Dispatcher (0x4B24): writes 0x52000000 twice (INTA), reads ISR, scans from
 * IR7 down, calls handler[line], then issues a specific EOI (0x60|line).
 * Handlers are registered with 0x4BDC(handler, line), which also calls the
 * handler once and unmasks the line. 0x4C7C/0x4CF0 mask/unmask a line.
 *
 *  IR0  Field sync (0x4EBC): counts fields at 0x5D960, reads 0x78000000
 *       (MAIN_RD_INTA_R0).
 *       [Emulated: one line long pulse per frame at line 383, matching when
 *       Model 3 asserts its VBlank interrupt]
 *  IR3  SCSI (0x44868, ncrintr). [Emulated]
 *  IR4  SCANPSC100F INT (0x4F98, jtag_int). Dispatches to sub-handlers
 *       registered with 0x50B4(handler, MODE1 bit). [Emulated]
 *  IR5  UART (handler 0x6530, uart.c, installed by 0x6488). The firmware's
 *       main loop receives serial input by interrupt (IER = 0x01, receive
 *       data available), one character per interrupt, masking the UART
 *       interrupt with MCR OUT1 while servicing it. [Emulated]
 *  IR7  Geometry processor (GP) sync (0x6918, gp_sync_int.c): counts frames at
 *       0x5DB80, sets flag 0x5DB84, and keeps interval statistics (monitor
 *       "Delta Frame (GP) Sync Count"). Equivalent to the Model 3 "GP done"
 *       interrupt. 0x577C waits for it to change before returning.
 *       [Emulated: one line long pulse per frame at end of VBlank, matching
 *       when Model 3 asserts its GP done interrupt]
 *
 * The 8259 raises the CPU interrupt line only between CPU time slices (see
 * C8259::Tick()), modeling the time devices take to complete operations.
 * jtag_wrt, for example, sets a busy flag after starting a shift and would
 * miss an interrupt taken immediately.
 *
 *
 * JTAG
 * ----
 * The Real3D ASICs are configured through JTAG (mode words, ID codes) using a
 * National SCANPSC100F controller at 0x68000000, unlike Model 3 which
 * bit-bangs a TAP through system registers. jtag_wrt (0x12444, jtag_io.c)
 * navigates the TAP with TMS0 patterns (0x30 = to Shift-IR, 0x20 = to
 * Shift-DR, 0x01 = exit to Run-Test/Idle), shifts one byte per CNT32
 * interrupt using Auto TMS High on the final byte, and completes in its
 * interrupt handler (0x12630). jtag_read is at approximately 0x12790.
 *
 * ASIC name table (pointer table at 0x611AC): NOT_PRESENT, JUPITER, MERCURY,
 * VENUS, EARTH_0, MARS_00, MARS_01, EARTH_1, MARS_10, MARS_11, EARTH_2,
 * MARS_20, MARS_21, EARTH_3, MARS_30, MARS_31, PLUTO, NEPTUNE_0-3. This is a
 * naming table, not the scan chain order. Pluto is new relative to Model 3
 * and appears to handle video output/gamma. The chain depends on the hardware
 * configuration: single pixel board, dual pixel boards, or high resolution
 * (get_hw_config.c, auto_config.c). The 3D RAMs are not named.
 *
 * Configuration detection (0x17EB0, auto_config.c) reads the chain with all
 * devices in IDCODE (ASICs, 32 bits) or BYPASS (3D-RAMs, 1 bit) using a
 * bit-at-a-time DR read (0x1CD18, jtag_dr.c) that discards the first 3 bits
 * shifted out (the last three 3D-RAMs) and stores the rest LSB first. It tries
 * each configuration in turn and checks ID codes at the bit positions that
 * configuration implies (masked with 0x0FFFFFFF, ignoring the version field):
 *
 *   Config        Bits  Chain (TDI to TDO)                        Check
 *   Single pixel   196  J Me V E R Ma Ma R R R                    0x18204
 *   Dual pixel     296  J Me V (E R Ma Ma R R R) x2               0x1879C
 *   High res       496  J Me V (E R Ma Ma R R R) x4               0x18F58
 *
 * J = Jupiter, Me = Mercury, V = Venus, E = Earth, Ma = Mars, R = 3D-RAM. The
 * single and dual configurations are exactly the Model 3 Step 1.x and Step
 * 2.x chains. If all fail, the high res check prints "FAILED ID'S" with the
 * expected values and the single pixel board configuration is assumed.
 *
 * Expected ID codes (table at 0x61200): Jupiter 0x016C7057, Mercury
 * 0x016C3057, Venus 0x016C4057, Earth 0x016C5057, Mars 0x016C6057 (the Model
 * 3 ID codes with version field 0), then Jupiter 0x0D7F1057 and Earth
 * 0x0D7F0057. The single and dual pixel checks use the first five. The high
 * res check uses 0x0D7F1057 at the Jupiter position and 0x0D7F0057 at all
 * four Earth positions. "FAILED ID'S" prints both values for Jupiter and
 * Earth, but the high res check compares only with the second value. The
 * device table that the high res check makes (0x70B28, entries of type,
 * name index, and version) names these devices PLUTO and NEPTUNE_0-3. Thus
 * on the high res system, Pluto is at the Jupiter position and Neptune is at
 * each Earth position.
 *
 * CPro1000::BuildScanChain() makes the chain of the model (see Models) with
 * the Model 3 CASIC and C3DRAM device models. On the high res chain, the
 * Pluto and Neptune positions are CASIC objects with the Jupiter and Earth
 * names and the high res ID codes. The firmware prints "SINGLE PIXEL BOARD
 * DETECTED", "DUAL PIXEL BOARDS DETECTED", or "HI RES CONFIGURATION
 * DETECTED". Early attempts used only the 15 ASICs from the firmware's name
 * table (0x611AC) and omitted the 3D-RAMs; every missing 1-bit device
 * shifted the ID codes out of position and all three configuration checks
 * failed.
 *
 * Mode words: 0x130A4(index, value) writes the mode word (JTAG instruction 8)
 * of the device at position index in the chain (1 = Jupiter, 2 = Mercury).
 * The firmware keeps the current values in a table at RAM 0x606A4 (Jupiter,
 * Mercury, ...). 0x137BC(index, pointer) reads one. The values that a single
 * pixel board gets (from a trace of the emulated firmware):
 *
 *   When                     Changes
 *   Boot (0x5660)            Jupiter 0x06000000, Mercury 0x800, Venus 0,
 *                            Earth 0x00080000, Mars 0. Bit 0 of Jupiter and
 *                            Mercury is set and then cleared (a pulse).
 *   get_hw_config            Venus 0x6000 and Mars 0x80 for a short time.
 *   rend_init                Jupiter bit 4 (Disable Fog) is set while the
 *                            SCSI ROM is loaded and cleared after (high
 *                            res: device 2, bit 2).
 *   Main loop, logo          No changes.
 *   Host gamma control       Jupiter bit 25 (see below).
 *
 * Gamma bypass: host command 4 (a packet write of one word to 0x20000004,
 * SDK function ls_ctrl_gamma, "Gamma Correction Option") calls 0x1BC34(data
 * != 0). On a single pixel board, 0x1BC34(1) clears Jupiter mode word bit 25
 * (Gamma Bypass) and 0x1BC34(0) sets it. On the high res system (config
 * byte 4), it uses bit 7 of device 1, which is Pluto Gamma Bypass (device 1
 * is Pluto on the high res system, see JTAG). At boot the bit
 * is set (no gamma correction). The SDK demos clear it (gamma correction).
 * Supermodel does not emulate gamma correction. To emulate it, use this bit
 * and the gamma tables (rom_to_gm, ls_wrt_gamma). 0x1BB04, which the gamma
 * loading code (0x1C220) calls, sets and then clears Jupiter bit 20 (Enable
 * Memory Test). On the high res system, it uses bit 4 of device 1 (Pluto Test
 * Gamma Memory).
 *
 * Mode word bit names: the firmware has a table of bit names for each ASIC
 * (pointer arrays at RAM 0x6018C (Pluto), 0x6020C (Jupiter), 0x6028C
 * (Mercury), 0x6030C (Venus), 0x60390 (Earth and Neptune), 0x60410 (Mars)),
 * which the monitor uses to print mode words (0x15578 selects the table by
 * ASIC type). The Earth and Mars tables are printed with the name of the
 * device. Their assignment to Earth and Mars is from the type numbers and
 * the boot values. Bits that are not listed are "unused".
 *
 *   Jupiter  0 GLOBAL RESET, 1 Enable Two Channel, 2 Enable Interlace,
 *            3 Enable Two Processor, 4 Disable Fog, 5 Disable Illumination,
 *            6 Disable Scroll, 7 Disable S-Buffer, 8 Enable Scroll
 *            Smoothing, 9 Display Edges Only, 10 Slope, 11 Trap Pixel Data,
 *            12 Trap Line FIFOs, 13-18 Threshold[0-5], 19 Enable MISRs,
 *            20 Enable Memory Test, 21 Enable Edge Resolver Compare,
 *            22 Enable Blend Compare, 23 Enable Smudge Compare, 24 Enable
 *            Color Calc Test, 25 Gamma Bypass, 26 Dac Strobes Invert.
 *   Mercury  0 NOT Initialized, 1 Block CPU Write, 2 Block Culling, 3 Block
 *            Burst, 4 Block PingPong, 5 Force PingPong, 6 Block Poly
 *            Request, 7 Force Poly Request, 8 Block LOS, 9 MISR Enable,
 *            10 SDRAM Short Refresh, 11 Memory Pointer JTAG, 12-14 Stats
 *            Sel[0-2], 15 Burst Always, 16-17 Stack Mem Test[0-1], 18 Model
 *            FIFO Test, 19 Culling SDRAM Test, 20 Ignore Discard, 21 Model
 *            FIFO Ignore Discard, 22 Model FIFO Cycle, 23 LOS Test, 24 MISR
 *            MAC Sel, 25 MAC Test, 26 Datamux Test, 27 Counter Test, 28-29
 *            Mux Isolation Sel[0-1], 30 Fltfunc Test, 31 Do Nothing.
 *   Venus    4 Stop SDRAM writes/reads, 5 Always Available, 6 Test 1x,
 *            7 MISR Enable, 8-9 Stats Mode[0-1], 10 Projection Scale
 *            Disable, 11 Snapshot Enable, 12 Stats Disable, 13 Polygon Mem
 *            Test, 14 Texture Update, 15 Polygon FIFO Test, 16 No writes to
 *            rd FIFO, 17 No reads to rd FIFO, 18 Short Refresh, 19 Pmem ROM
 *            Checksum Enable, 20-22 Test Mode[0-2], 23 Counter Test.
 *   Earth    0 Two Processors, 1 Two Monitors, 2 Disable Texture Xluc,
 *            3 Disable Polygon Xluc (high res: Enable Polygon Xluc),
 *            4 Disable Contour Xluc, 5 Disable Stencil, 6-7 Test/Trap Input
 *            FIFO[0-1], 8-10 Stats Sel[0-2], 11 Enable LOS Test, 12-14 Trap
 *            Code[0-2], 15 Enable DBM Test, 16 Enable Video Mem Test,
 *            17 Cycle Video Mem, 18 Enable MISRs, 19 Disable DRAM Refresh,
 *            20 Disable Mars Freeze, 21 Select Database Color.
 *   Mars     0 Two Processors, 1 Disable Texture, 2 Disable Shading,
 *            3 Disable Illumination, 4 Disable Fading, 5 Enable Compare
 *            Test, 6 Enable MISRs, 7 Enable Memory Test, 8 Enable CDRAM
 *            Debug, 9 Short Refresh, 10 Disable Texture Updates, 11 Hi Res.
 *   Pluto    0 GLOBAL RESET, 1 Enable Dual Channel, 2 Enable Interlace,
 *            3 Enable MISRs, 4 Test Gamma Memory, 5 Test Soft Edge Blend
 *            Memory, 6 Fab Test, 7 Gamma Bypass, 8 Disable Soft Edge
 *            Blending, 9 Display Edges, 10-15 Threshold[0-5], 16 Slope,
 *            17 Trap Pixel Data, 18 Trap Line FIFOs, 19 Disable S-Buffer,
 *            20 Disable Background Color.
 *
 * Thus the boot values are: Jupiter 0x06000000 (Gamma Bypass, Dac Strobes
 * Invert), Mercury 0x800 (Memory Pointer JTAG), Earth 0x00080000 (Disable
 * DRAM Refresh). The bit 0 pulses are GLOBAL RESET (Jupiter) and NOT
 * Initialized (Mercury). get_hw_config sets Venus Polygon Mem Test and
 * Texture Update and Mars Enable Memory Test for a short time. Supermodel
 * uses Mercury bit 2 (Block Culling) as in this table. It uses Mars bit 18 to
 * clamp the sun on Model 3, but in this table Mars bit 18 is unused. The
 * firmware does not set Mercury Block PingPong or Force PingPong (only the
 * monitor can).
 *
 *
 * Serial Console
 * --------------
 * The UART carries boot messages and a command monitor (rs232_ui.c, menu.c)
 * with a "command> " prompt and about 80 commands, including memory display
 * and modification (RD, RC, RP, WC, WP), mode word access (MWR, MWW), JTAG
 * (WJIR, WJDR, RJDR), ID code and hardware configuration dumps (ID, HW,
 * RIF_MODE), and rendering feature toggles. The boot stub also polls for
 * input ('r' resets, ESC prints the scratch register).
 *
 * Lines typed at the emulator's console are received by the UART (followed by
 * a carriage return). Console input is shared with the debugger through
 * CConsoleInput: the UART claims it permanently and the debugger claims it
 * while execution is halted, so commands typed at the debugger prompt go to
 * the debugger and everything else goes to the Pro-1000. The UART's claim is
 * also what starts the console being read.
 *
 *
 * Rendering
 * ---------
 * The Real3D draws continuously, one frame for each field (hypothesis). No
 * command starts the drawing. Real3D.cpp ("Rendering") describes how
 * Supermodel draws and how the Pro-1000 differs from Model 3.
 *
 * Firmware sequence for each frame (main loop 0x9B6C, simplified):
 *  1. Wait for host data. A WRITE from the host (LUN 0) puts the update in
 *     the ring buffer and sets bit 1 of 0x62384.
 *  2. Render cycle (0xA984): set bits in RIF_MODE (0x70000000), process the
 *     packets in the ring buffer (0xAC18), and write REND_UPDATE_COMPL
 *     (0x88000000) at 0xC954 if the packets wrote data.
 *  3. Read the Real3D status registers into the status packet (Stat_Pckt),
 *     with "render done" and the ping-pong bit. The host reads this packet
 *     (READ, LUN 0) before it sends the next frame.
 *  4. Wait for the geometry processor sync (IR7) or the next field (IR0).
 *
 * Emulation of each frame (RunMainBoardFrame()):
 *  - VBlank (40 lines): CReal3D::BeginVBlank(), then IR7 (GP sync) at the
 *    end of VBlank.
 *  - Active display (384 lines): the ping-pong bit flips at line
 *    PING_PONG_FLIP_LINE (238, the value that most Model 3 games put in a
 *    tile generator register), and IR0 (field sync) occurs at line
 *    FIELD_SYNC_LINE (383).
 *  - Then the renderer draws the scene that is in memory (RenderFrame()).
 * The Pro-1000 time of the ping-pong flip is not known. The update buffers
 * use the update memory configuration, which the SCSI ROM sets at boot (see
 * Update Memory Configuration).
 *
 * 2D layer: the Pro-1000 has no tile generator. Its only 2D layer is the
 * solid color of the SCROLL register (0x62000000). CPro1000Render2D draws it
 * as CRender2D draws the Model 3 tile layers: the background before the 3D
 * image, the foreground after it. The register value goes to the renderer in
 * SyncGPUs(). As in the proof of concept (pro1000_aspi), there is no layer if
 * the viewport select bits (25-24) are 0, and a foreground covers the
 * complete 3D image. No known program uses a foreground, thus that part is a
 * guess. The meaning of the viewport select bits is not known. The layer
 * covers the complete viewable area.
 *
 * Textures:
 *  - Texture port (REND_ROM_UPDATE, 0x90000000): the firmware and the SDK
 *    demos use it. The data is the word address of a texture block in
 *    polygon memory (Model 3 Step 1.0 texture port, with polygon memory as
 *    VROM).
 *  - Texture FIFO (REND_TEX_UPDATE, REND_TEX_CONTINUE, REND_TRANS_UPDATE,
 *    0x94000000): the firmware address table gives it, but no firmware code
 *    uses it. A host can write to it with packets. The SDK demos were not
 *    seen to use it.
 *  - Texture memory: 8 MB (4M texels), as on Model 3. The SDK gives an
 *    optional 32 MB (16M texels), and the firmware reports "Extended
 *    Texture Memory = 1". The extended texture memory is not emulated.
 *
 *
 * CPU Clock and Time Base
 * -----------------------
 * The firmware measures time with the PowerPC time base. It divides time
 * base ticks by 0x208E (8334) to get milliseconds: at 0x3328 (the "field
 * time" message in _start) and in the wait for the SCSI controller (0x45C4C).
 * Thus the firmware expects a time base of 8.33 MHz. The 603 time base
 * increments once every four bus clocks, so the bus is 33.3 MHz.
 *
 * The emulated CPU has a 33 MHz bus and a 2X multiplier. The core clock is
 * 66 MHz, as on Model 3 Step 1.0. The real core clock is not known; the
 * firmware constants give only the bus clock.
 *
 * Earlier, the bus was 66 MHz (1X multiplier), as on Model 3 Step 1.0. Then
 * the time base was 16.5 MHz, all times that the firmware measured were two
 * times too long ("field time = 34 milliseconds" instead of 17), and
 * firmware timeouts expired in half of the correct time.
 *
 *
 * Other Firmware Landmarks
 * ------------------------
 * 0x47D6C  printf
 * 0x8750   Disable external interrupts (clears MSR[EE]), returns old MSR
 * 0x876C   Restore MSR[EE]
 * 0x9400   Read time base (lower)
 *
 *
 * Open Questions
 * --------------
 *  - Field sync timing (IR0). The nominal rate is 60 Hz (or 59.94/50 Hz for
 *    interlaced modes) per the SDK documentation, but emulation still uses
 *    the Model 3 rate of 57.52 Hz, and the firmware measures 17 ms per field.
 *    (Before the time base correction, it measured 34 ms; see "CPU Clock and
 *    Time Base".) The logic sync memory (LSYNC_BASE) probably sequences
 *    these signals.
 *  - Device 12 (Real3D) register layout and the culling/polygon memory map.
 *  - Models. The PRO-1200 and PRO-1400 boot, but rendering is that of the
 *    PRO-1100 (see Models). To revisit once rendering works.
 *  - Where scan chains should be built. Currently CPro1000 builds its chain
 *    while Model 3's CJTAG builds its own. Since the Pro-1000 single and dual
 *    chains are the Model 3 Step 1.x and 2.x chains, consider having CReal3D
 *    build the chain (its ASICs are the Real3D chipset) from a board
 *    configuration supplied by the system, preserving the Model 3 JTAG2 save
 *    state layout.
 *  - What the system type (see Models) changes, and what determines the
 *    extended texture memory setting reported by show_config_data.
 *  - Contents of the SCSI ROM (see SCSI ROM). It is not part of the SDK
 *    flash update (p1_p4.bin, p2.bin), so it was presumably programmed at
 *    the factory. Supermodel makes a synthetic ROM. It is not known how the
 *    real ROM supplies the viewport of the power-on logo.
 *  - The ping-pong memory: where the second ping-pong memory is, which
 *    memory each window (0x8C000000, 0x8E000000) and the renderer use, and
 *    what swaps them (see Update Memory Configuration and "TODO: Physical
 *    Ping-Pong Memory" in Real3D.cpp).
 *  - RIF_MODE (0x70000000) is not emulated as a register, so the mode word
 *    from the INIT ROM (0x1401) does not stick.
 *  - REND_ROM_UPDATE (0x90000000): the Model 3 Step 1.0 texture port, which
 *    reads textures from polygon memory instead of VROM (as in the proof of
 *    concept ASPI DLL and the SDK demos).
 *  - What p2.bin is.
 *  - The auxiliary processor/interrupt controller (MAIN_RST_AUX, AUX_*).
 *  - MAIN_RD_INTA_R0/R1 (0x78000000) semantics.
 *
 *
 * Endianness
 * ----------
 * As with CModel3, PowerPC RAM and ROM regions are byte reversed so that
 * aligned words can be accessed without conversion on little endian hosts.
 */

#include "Pro1000.h"
#include "SyntheticSCSIROM.h"

#include <new>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "Supermodel.h"
#include "ROMSet.h"
#include "OSD/Video.h"
#include "Graphics/Pro1000Render2D.h"
#include "Util/ByteSwap.h"

constexpr static uint32_t RAM_SIZE          = 0x400000;   // 4MB (DRAM_TOP)
constexpr static uint32_t FLASH_SIZE        = 0x800000;   // 8MB window

// Flash is mapped in an 8MB window (as on Model 3) although only the top 1MB
// (CPU_ROM_BASE-CPU_ROM_TOP) is populated
constexpr static uint32_t FLASH_BASE_ADDR   = 0xFF800000; // start of flash window in PowerPC address space
constexpr static uint32_t CPU_ROM_BASE      = 0xFFF00000; // boot stub and loop code image (reset vector is at +0x100)
constexpr static uint32_t CPU_ROM_OFFSET    = CPU_ROM_BASE - FLASH_BASE_ADDR;
constexpr static uint32_t INIT_ROM_BASE     = 0xFFF80000; // INIT ROM image (LOGIC_SYNC_INIT)
constexpr static uint32_t INIT_ROM_OFFSET   = INIT_ROM_BASE - FLASH_BASE_ADDR;

// SCSI ROM: flash on the SCSI controller's expansion ROM interface, mapped by
// the firmware through PCI device 11's expansion ROM BAR
constexpr static uint32_t SCSI_ROM_BASE     = 0xC0000000;
constexpr static uint32_t SCSI_ROM_SIZE     = 0x80000;    // 512KB (SCSI_ROM_TOP = 0xC0080000)
constexpr static uint32_t SCROLL_REGISTER_ADDR = 0x62000000;  // SCROLL: 2D layer color

constexpr static uint32_t MEM_POOL_SIZE     = RAM_SIZE + FLASH_SIZE + SCSI_ROM_SIZE;
constexpr static uint32_t RAM_OFFSET        = 0;
constexpr static uint32_t FLASH_OFFSET      = RAM_OFFSET + RAM_SIZE;
constexpr static uint32_t SCSI_ROM_OFFSET   = FLASH_OFFSET + FLASH_SIZE;

constexpr static unsigned PPC_FREQUENCY_MHZ = 66;         // core clock (33 MHz bus x 2), see "CPU Clock and Time Base"

// On Model 3, the ping-pong flip line is programmed into a tile generator
// register. There is no tile generator here, so use the value most Model 3
// games program.
constexpr static unsigned PING_PONG_FLIP_LINE = 238;

// Field sync is signaled at the start of the last active line, which is when
// Model 3 asserts its VBlank interrupt (IRQ 0x02)
constexpr static unsigned FIELD_SYNC_LINE = 383;

// The SCSI controller's SCRIPTS processor runs alongside the PowerPC: it is
// given a slice of instructions each line. SCRIPTS waiting on the SCSI bus end
// their slice early, so this only bounds runaway loops.
constexpr static unsigned SCSI_INSTRUCTIONS_PER_LINE = 100;

// UART registers are 8 bytes apart and occupy the least significant byte of
// each big endian 32-bit word
constexpr static uint32_t UART_BASE_ADDR    = 0x64000000;
constexpr static uint32_t UART_END_ADDR     = UART_BASE_ADDR + 8 * 8;

static inline unsigned UARTRegister(UINT32 addr)
{
  return (addr >> 3) & 7;
}

/*
 * Byte-wide peripherals are connected to D7:0 and decode address bits 3 and
 * up, so register n of a device is accessed as the least significant byte of
 * the big endian 32-bit word at base+n*8. Firmware uses both byte accesses
 * (at base+n*8+3) and word accesses (at base+n*8).
 */
static inline bool DecodeByteRegister(UINT32 addr, UINT32 base, unsigned numRegs, unsigned lane, unsigned *reg)
{
  if (addr < base || addr >= base + numRegs * 8 || (addr & 7) != lane)
    return false;
  *reg = (addr - base) >> 3;
  return true;
}

constexpr static uint32_t PIC_BASE_ADDR     = 0x54000000; // 8259 (2 registers: A0)
constexpr static uint32_t INTA_ADDR         = 0x52000000; // any write performs one 8259 INTA pulse
constexpr static uint32_t JTAG_BASE_ADDR    = 0x68000000; // SCANPSC100F (8 registers: A2:0)

// 8259 input lines (IR mask bits)
constexpr static uint16_t PIC_LINE_FIELD_SYNC = 1 << 0;  // handler counts fields
constexpr static uint16_t PIC_LINE_SCSI     = 1 << 3;     // handler is ncrintr
constexpr static uint16_t PIC_LINE_JTAG     = 1 << 4;     // handler is jtag_int
constexpr static uint16_t PIC_LINE_UART     = 1 << 5;     // handler at 0x6530 (uart.c)
constexpr static uint16_t PIC_LINE_GP_SYNC  = 1 << 7;     // handler counts geometry processor frames
constexpr static uint16_t PIC_LINE_NONE     = 0;          // not connected

// SCSI controller is PCI device 11 (an NCR 53C825 on the real hardware;
// emulated using the 53C810). Its registers are located wherever firmware
// maps its memory BAR (0xD0000000 has been observed). A base address of 0
// means it has not been mapped yet.
constexpr static unsigned SCSI_PCI_DEVICE   = 11;

bool CPro1000::IsSCSIAddress(UINT32 addr) const
{
  UINT8 base = SCSI.GetBaseAddress();
  return base != 0 && (addr >> 24) == base && (addr & 0x00FFFF00) == 0;
}


/******************************************************************************
 Address Space Access Handlers
******************************************************************************/

// REND_ROM_UPDATE is the Model 3 VROM texture port. The Pro-1000 has no VROM
// that we know of, but the firmware references the port, so all accesses are
// logged.
constexpr static uint32_t REND_ROM_UPDATE   = 0x90000000;

static inline bool IsVROMPortAddress(UINT32 addr)
{
  return (addr >> 24) == (REND_ROM_UPDATE >> 24);
}

static void LogVROMPortAccess(const char *access, UINT32 addr)
{
  InfoLog("Pro-1000: %s REND_ROM_UPDATE (VROM port) @ %08X (PC=%08X, LR=%08X)", access, addr, ppc_get_pc(), ppc_get_lr());
}

static void LogVROMPortAccess(const char *access, UINT32 addr, UINT32 data)
{
  InfoLog("Pro-1000: %s REND_ROM_UPDATE (VROM port) %08X=%08X (PC=%08X, LR=%08X)", access, addr, data, ppc_get_pc(), ppc_get_lr());
}

UINT8 CPro1000::Read8(UINT32 addr)
{
  // RAM (most frequently accessed)
  if (addr < RAM_SIZE)
    return ram[addr ^ 3];

  if (IsVROMPortAddress(addr))
    LogVROMPortAccess("read8", addr);

  // SCSI
  if (IsSCSIAddress(addr))
    return SCSI.ReadRegister(addr & 0xFF);

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 3, &reg))
    return PIC.Read(reg);
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 3, &reg))
    return JTAG.Read(reg);

  // Other
  switch ((addr >> 24))
  {
  // Flash
  case 0xFF:
    if (addr >= FLASH_BASE_ADDR)
      return flash[(addr & 0x7FFFFF) ^ 3];
    break;

  // SCSI ROM
  case 0xC0:
    if (addr < SCSI_ROM_BASE + SCSI_ROM_SIZE)
      return scsiROM[(addr - SCSI_ROM_BASE) ^ 3];
    break;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 3)
      return UART.Read(UARTRegister(addr));
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread8 : %08X\n", ppc_get_pc(), addr);
  return 0xFF;
}

UINT16 CPro1000::Read16(UINT32 addr)
{
  UINT16 data;

  if ((addr & 1))
  {
    data =  Read8(addr + 0) << 8;
    data |= Read8(addr + 1);
    return data;
  }

  // RAM (most frequently accessed)
  if (addr < RAM_SIZE)
    return *(UINT16 *) &ram[addr ^ 2];

  if (IsVROMPortAddress(addr))
    LogVROMPortAccess("read16", addr);

  // SCSI
  if (IsSCSIAddress(addr))
  {
    data =  SCSI.ReadRegister((addr + 0) & 0xFF) << 8;
    data |= SCSI.ReadRegister((addr + 1) & 0xFF);
    return data;
  }

  // Other
  switch ((addr >> 24))
  {
  // Flash
  case 0xFF:
    if (addr >= FLASH_BASE_ADDR)
      return *(UINT16 *) &flash[(addr & 0x7FFFFF) ^ 2];
    break;

  // SCSI ROM
  case 0xC0:
    if (addr < SCSI_ROM_BASE + SCSI_ROM_SIZE)
      return *(UINT16 *) &scsiROM[(addr - SCSI_ROM_BASE) ^ 2];
    break;

  case 0xF0:
    // MPC105 CONFIG_DATA (F0C00CF8)
    if (((addr >> 16) & 0xFF) == 0xC0)
      return PCIBridge.ReadPCIConfigData(16, addr & 3);
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread16: %08X\n", ppc_get_pc(), addr);
  return 0xFFFF;
}

UINT32 CPro1000::Read32(UINT32 addr)
{
  UINT32 data;

  if ((addr & 3))
  {
    data =  Read16(addr + 0) << 16;
    data |= Read16(addr + 2);
    return data;
  }

  // RAM (most frequently accessed)
  if (addr < RAM_SIZE)
    return *(UINT32 *) &ram[addr];

  if (IsVROMPortAddress(addr))
    LogVROMPortAccess("read32", addr);

  // SCSI
  if (IsSCSIAddress(addr))
  {
    data =  (SCSI.ReadRegister((addr + 0) & 0xFF) << 24);
    data |= (SCSI.ReadRegister((addr + 1) & 0xFF) << 16);
    data |= (SCSI.ReadRegister((addr + 2) & 0xFF) << 8);
    data |= (SCSI.ReadRegister((addr + 3) & 0xFF) << 0);
    return data;
  }

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 0, &reg))
    return 0xFFFFFF00 | PIC.Read(reg);
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 0, &reg))
    return 0xFFFFFF00 | JTAG.Read(reg);

  // Other
  switch ((addr >> 24))
  {
  // Flash
  case 0xFF:
    if (addr >= FLASH_BASE_ADDR)
      return *(UINT32 *) &flash[(addr & 0x7FFFFF)];
    break;

  // SCSI ROM
  case 0xC0:
    if (addr < SCSI_ROM_BASE + SCSI_ROM_SIZE)
      return *(UINT32 *) &scsiROM[(addr - SCSI_ROM_BASE)];
    break;

  // Real3D registers
  case 0x84:
    data = GPU.ReadRegister(addr & 0x3F);
    return FLIPENDIAN32(data);

  // SCROLL register
  case 0x62:
    if (addr == SCROLL_REGISTER_ADDR)
      return m_scrollRegister;
    break;

  case 0xF0:
    // MPC105 CONFIG_DATA (F0C00CF8)
    if (((addr >> 16) & 0xFF) == 0xC0)
      return PCIBridge.ReadPCIConfigData(32, 0);
    break;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 0)
      return 0xFFFFFF00 | UART.Read(UARTRegister(addr));
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread32: %08X\n", ppc_get_pc(), addr);
  return 0xFFFFFFFF;
}

UINT64 CPro1000::Read64(UINT32 addr)
{
  UINT64 data;

  data = Read32(addr + 0);
  data <<= 32;
  data |= Read32(addr + 4);
  return data;
}

void CPro1000::Write8(UINT32 addr, UINT8 data)
{
  // RAM (most frequently accessed)
  if (addr < RAM_SIZE)
  {
    ram[addr ^ 3] = data;
    return;
  }

  if (IsVROMPortAddress(addr))
    LogVROMPortAccess("write8", addr, data);

  // SCSI
  if (IsSCSIAddress(addr))
  {
    SCSI.WriteRegister(addr & 0xFF, data);
    return;
  }

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 3, &reg))
  {
    PIC.Write(reg, data);
    return;
  }
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 3, &reg))
  {
    JTAG.Write(reg, data);
    return;
  }

  // 8259 interrupt acknowledge
  if ((addr >> 24) == (INTA_ADDR >> 24))
  {
    PIC.Acknowledge();
    return;
  }

  // Other
  switch ((addr >> 24))
  {
  // MPC105
  case 0xF8:
    PCIBridge.WriteRegister(addr & 0xFF, data);
    return;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 3)
    {
      UART.Write(UARTRegister(addr), data);
      return;
    }
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\twrite8 : %08X=%02X\n", ppc_get_pc(), addr, data);
}

void CPro1000::Write16(UINT32 addr, UINT16 data)
{
  if ((addr & 1))
  {
    Write8(addr + 0, data >> 8);
    Write8(addr + 1, data & 0xFF);
    return;
  }

  // RAM (most frequently accessed)
  if (addr < RAM_SIZE)
  {
    *(UINT16 *) &ram[addr ^ 2] = data;
    return;
  }

  if (IsVROMPortAddress(addr))
    LogVROMPortAccess("write16", addr, data);

  // SCSI
  if (IsSCSIAddress(addr))
  {
    SCSI.WriteRegister((addr + 0) & 0xFF, data >> 8);
    SCSI.WriteRegister((addr + 1) & 0xFF, data & 0xFF);
    return;
  }

  // Other
  switch ((addr >> 24))
  {
  case 0xF0:
    // MPC105 CONFIG_DATA (F0C00CF8)
    if (((addr >> 16) & 0xFF) == 0xC0)
    {
      PCIBridge.WritePCIConfigData(16, addr & 2, data);
      return;
    }
    break;

  // MPC105
  case 0xF8:
    // Write in big endian order, like a real PowerPC
    PCIBridge.WriteRegister((addr & 0xFF) + 0, data >> 8);
    PCIBridge.WriteRegister((addr & 0xFF) + 1, data & 0xFF);
    return;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\twrite16: %08X=%04X\n", ppc_get_pc(), addr, data);
}

void CPro1000::Write32(UINT32 addr, UINT32 data)
{
  if ((addr & 3))
  {
    Write16(addr + 0, data >> 16);
    Write16(addr + 2, data);
    return;
  }

  // RAM (most frequently accessed)
  if (addr < RAM_SIZE)
  {
    *(UINT32 *) &ram[addr] = data;
    return;
  }

  if (IsVROMPortAddress(addr))
    LogVROMPortAccess("write32", addr, data);

  // SCSI
  if (IsSCSIAddress(addr))
  {
    SCSI.WriteRegister((addr & 0xFF) + 0, (data >> 24) & 0xFF);
    SCSI.WriteRegister((addr & 0xFF) + 1, (data >> 16) & 0xFF);
    SCSI.WriteRegister((addr & 0xFF) + 2, (data >> 8) & 0xFF);
    SCSI.WriteRegister((addr & 0xFF) + 3, data & 0xFF);
    return;
  }

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 0, &reg))
  {
    PIC.Write(reg, data & 0xFF);
    return;
  }
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 0, &reg))
  {
    JTAG.Write(reg, data & 0xFF);
    return;
  }

  // 8259 interrupt acknowledge
  if ((addr >> 24) == (INTA_ADDR >> 24))
  {
    PIC.Acknowledge();
    return;
  }

  // Other
  switch ((addr >> 24))
  {
  // Real3D trigger
  case 0x88:  // 88000000
    GPU.Flush();
    return;

  // SCROLL register (2D layer color, see CPro1000Render2D)
  case 0x62:
    if (addr == SCROLL_REGISTER_ADDR)
    {
      m_scrollRegister = data;
      return;
    }
    break;

  // Real3D low culling RAM
  case 0x8C:  // 8C000000-8C400000
    GPU.WriteLowCullingRAM(addr & 0x3FFFFF, FLIPENDIAN32(data));
    return;

  // Real3D high culling RAM
  case 0x8E:  // 8E000000-8E100000
    GPU.WriteHighCullingRAM(addr & 0xFFFFF, FLIPENDIAN32(data));
    return;

  // Real3D texture port (REND_ROM_UPDATE). The data is the word address of
  // a texture in polygon RAM (which replaces VROM on the Pro-1000).
  case 0x90:  // 90000000-90??????
    GPU.WriteTexturePort(addr & 0xFF, FLIPENDIAN32(data));
    return;

  // Real3D texture FIFO
  case 0x94:  // 94000000-94100000
    GPU.WriteTextureFIFO(FLIPENDIAN32(data));
    return;

  // Real3D polygon RAM: 64MB, which includes the area that is VROM on Model 3
  case 0x98:  // 98000000-9BFFFFFF
  case 0x99:
  case 0x9A:
  case 0x9B:
    GPU.WritePolygonRAM(addr - 0x98000000, FLIPENDIAN32(data));
    return;

  // Real3D configuration registers
  case 0x9C:  // 9Cxxxxxx
    GPU.WriteConfigurationRegister(addr, FLIPENDIAN32(data));
    return;

  case 0xF0:
    switch ((addr >> 16) & 0xFF)
    {
    // MPC105 CONFIG_ADDR (F0800CF8)
    case 0x80:
      PCIBridge.WritePCIConfigAddress(data);
      return;

    // MPC105 CONFIG_DATA (F0C00CF8)
    case 0xC0:
      if (addr >= 0xF0C00CF8 && addr < 0xF0C00D00)
      {
        PCIBridge.WritePCIConfigData(32, 0, data);
        return;
      }
      break;

    default:
      break;
    }
    break;

  // MPC105
  case 0xF8:  // F8FFF000-F8FFF0FF
    // Write in big endian order, like a real PowerPC
    PCIBridge.WriteRegister((addr & 0xFF) + 0, (data >> 24) & 0xFF);
    PCIBridge.WriteRegister((addr & 0xFF) + 1, (data >> 16) & 0xFF);
    PCIBridge.WriteRegister((addr & 0xFF) + 2, (data >> 8) & 0xFF);
    PCIBridge.WriteRegister((addr & 0xFF) + 3, data & 0xFF);
    return;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 0)
    {
      UART.Write(UARTRegister(addr), data & 0xFF);
      return;
    }
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\twrite32: %08X=%08X\n", ppc_get_pc(), addr, data);
}

void CPro1000::Write64(UINT32 addr, UINT64 data)
{
  Write32(addr + 0, (UINT32) (data >> 32));
  Write32(addr + 4, (UINT32) data);
}


/******************************************************************************
 Emulation and Interface Functions
******************************************************************************/

void CPro1000::SaveState(CBlockFile *SaveState)
{
  puts("Warning: Save states are not supported on the Real3D Pro-1000.");
  InfoLog("Save states are not supported on the Real3D Pro-1000.");
}

void CPro1000::LoadState(CBlockFile *SaveState)
{
  puts("Warning: Save states are not supported on the Real3D Pro-1000.");
  InfoLog("Save states are not supported on the Real3D Pro-1000.");
}

void CPro1000::SaveNVRAM(CBlockFile *NVRAM)
{
  // No NVRAM
}

void CPro1000::LoadNVRAM(CBlockFile *NVRAM)
{
  // No NVRAM
}

void CPro1000::ClearNVRAM(void)
{
  // No NVRAM
}

void CPro1000::RunFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // See if currently running multi-threaded
  if (m_multiThreaded)
  {
    // If so, check all threads are up and running
    if (!StartThreads())
      goto ThreadError;

    // Wake PPC main board thread (if multi-threading GPU) so it can process a frame
    if (m_gpuMultiThreaded && !ppcBrdThreadSync->Post())
      goto ThreadError;

    // If not multi-threading GPU, then run PPC main board for a frame and sync GPUs now in this thread
    if (!m_gpuMultiThreaded)
    {
      RunMainBoardFrame();
      SyncGPUs();
    }

    // Render frame
    RenderFrame();

    // Enter notify wait critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Wait for PPC main board thread to finish its work (if it is running and hasn't finished already)
    while (m_gpuMultiThreaded && !ppcBrdThreadDone)
    {
      if (!notifySync->Wait(notifyLock))
        goto ThreadError;
    }
    ppcBrdThreadDone = false;

    // Leave notify wait critical section
    if (!notifyLock->Unlock())
      goto ThreadError;

    // If multi-threading GPU, then sync GPUs last while PPC main board thread is waiting
    if (m_gpuMultiThreaded)
      SyncGPUs();
  }
  else
  {
    // If not multi-threaded, then just process and render a single frame in this thread
    RunMainBoardFrame();
    SyncGPUs();
    RenderFrame();
  }

  timings.frameTicks = CThread::GetTicks() - start;
  timings.frameId++;
  return;

ThreadError:
  ErrorLog("Threading error in CPro1000::RunFrame: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
}

void CPro1000::RunMainBoardFrame(void)
{
  UINT32 start = CThread::GetTicks();

  /*
   * Display timings are assumed to be the same as Model 3: 57.524160 Hz with
   * 424 lines total, 384 display and 40 blanking/vsync.
   */
  unsigned mhz = m_config["PowerPCFrequency"].ValueAsDefault<unsigned>(0);
  unsigned ppcCycles    = (mhz ? mhz : PPC_FREQUENCY_MHZ) * 1000000;
  unsigned frameCycles  = (unsigned)((float)ppcCycles / 57.524160f);
  unsigned lineCycles   = frameCycles / 424;
  unsigned vBlankCycles = lineCycles * 40;

  // Scale PPC timer ratio according to speed at which the PowerPC is being
  // emulated so that the observed running frequency of the PPC timer
  // registers is more or less correct
  ppc_set_timer_ratio(ppc_get_bus_freq_multipler() * 2 * ppcCycles / ppc_get_cycles_per_sec());

  // VBlank
  if (gpusReady)
  {
    GPU.BeginVBlank();
    for (unsigned i = 0; i < vBlankCycles / lineCycles; i++)
    {
      UART.Tick();
      SCSI.Run(SCSI_INSTRUCTIONS_PER_LINE);
      PIC.Tick();
      ppc_execute(lineCycles);
    }

    // Geometry processor sync at end of VBlank, which is when Model 3 asserts
    // its GP done interrupt (IRQ 0x08). Deasserted after first active line.
    PIC.Assert(PIC_LINE_GP_SYNC);

    GPU.EndVBlank();
  }

  // Run the PowerPC for the active display part of the frame
  for (unsigned i = 0; i < 384; i++)
  {
    if (i == PING_PONG_FLIP_LINE)
      GPU.FlipPingPongBit();
    if (i == FIELD_SYNC_LINE)
      PIC.Assert(PIC_LINE_FIELD_SYNC);
    UART.Tick();
    SCSI.Run(SCSI_INSTRUCTIONS_PER_LINE);
    PIC.Tick();
    ppc_execute(lineCycles);
    if (i == 0)
      PIC.Deassert(PIC_LINE_GP_SYNC);     // one line long pulse
    if (i == FIELD_SYNC_LINE)
      PIC.Deassert(PIC_LINE_FIELD_SYNC);  // one line long pulse
  }

  timings.ppcTicks = CThread::GetTicks() - start;
}

void CPro1000::SyncGPUs(void)
{
  UINT32 start = CThread::GetTicks();

  timings.syncSize = GPU.SyncSnapshots();
  if (m_render2D)
    m_render2D->SetScrollRegister(m_scrollRegister);
  gpusReady = true;

  timings.syncTicks = CThread::GetTicks() - start;
}

void CPro1000::RenderFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // Call OSD video callbacks
  if (BeginFrameVideo() && gpusReady)
  {
    m_render2D->BeginFrame();
    GPU.BeginFrame();
    m_render2D->PreRenderFrame();
    m_render2D->RenderFrameBottom();
    GPU.RenderFrame();
    m_render2D->RenderFrameTop();
    GPU.EndFrame();
    m_render2D->EndFrame();
    m_superAA->Draw();
  }

  EndFrameVideo();

  timings.renderTicks = CThread::GetTicks() - start;
}

bool CPro1000::StartThreads(void)
{
  if (startedThreads)
    return true;

  // Create synchronization objects
  if (m_gpuMultiThreaded)
  {
    ppcBrdThreadSync = CThread::CreateSemaphore(0);
    if (ppcBrdThreadSync == NULL)
      goto ThreadError;
  }
  notifyLock = CThread::CreateMutex();
  if (notifyLock == NULL)
    goto ThreadError;
  notifySync = CThread::CreateCondVar();
  if (notifySync == NULL)
    goto ThreadError;

  // Reset thread flags
  pauseThreads = false;
  stopThreads = false;

  // Create PPC main board thread, if multi-threading GPU
  if (m_gpuMultiThreaded)
  {
    ppcBrdThread = CThread::CreateThread("MainBoard", StartMainBoardThread, this);
    if (ppcBrdThread == NULL)
      goto ThreadError;
  }

  startedThreads = true;
  return true;

ThreadError:
  ErrorLog("Unable to create threads and/or synchronization objects: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  DeleteThreadObjects();
  m_multiThreaded = false;
  return false;
}

bool CPro1000::PauseThreads(void)
{
  if (!startedThreads)
    return true;

  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let threads know that they should pause and wait for all of them to do so
  pauseThreads = true;
  while (ppcBrdThreadRunning)
  {
    if (!notifySync->Wait(notifyLock))
      goto ThreadError;
  }

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  return true;

ThreadError:
  ErrorLog("Threading error in CPro1000::PauseThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

bool CPro1000::ResumeThreads(void)
{
  if (!startedThreads)
    return true;

  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let all threads know that they can continue running
  pauseThreads = false;

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  return true;

ThreadError:
  ErrorLog("Threading error in CPro1000::ResumeThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

bool CPro1000::StopThreads(void)
{
  if (!startedThreads)
    return true;

  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let threads know that they should pause and wait for all of them to do so
  pauseThreads = true;
  while (ppcBrdThreadRunning)
  {
    if (!notifySync->Wait(notifyLock))
      goto ThreadError;
  }

  // Now let threads know that they should exit
  stopThreads = true;

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;

  // Resume thread and wait for it to exit
  if (ppcBrdThread != NULL)
  {
    if (ppcBrdThreadSync->Post())
      ppcBrdThread->Wait();
  }

  // Delete all thread and synchronization objects
  DeleteThreadObjects();
  startedThreads = false;
  return true;

ThreadError:
  ErrorLog("Threading error in CPro1000::StopThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

void CPro1000::DeleteThreadObjects(void)
{
  if (ppcBrdThread != NULL)
  {
    delete ppcBrdThread;
    ppcBrdThread = NULL;
  }
  if (ppcBrdThreadSync != NULL)
  {
    delete ppcBrdThreadSync;
    ppcBrdThreadSync = NULL;
  }
  if (notifyLock != NULL)
  {
    delete notifyLock;
    notifyLock = NULL;
  }
  if (notifySync != NULL)
  {
    delete notifySync;
    notifySync = NULL;
  }
}

void CPro1000::DumpTimings(void)
{
  printf("PPC:%3ums%c render:%3ums%c sync:%4uK%c%3ums%c frame:%3ums%c\n",
    timings.ppcTicks, (timings.ppcTicks > timings.renderTicks ? '!' : ','),
    timings.renderTicks, (timings.renderTicks > timings.ppcTicks ? '!' : ','),
    timings.syncSize / 1024, (timings.syncSize / 1024 > 128 ? '!' : ','),
    timings.syncTicks, (timings.syncTicks > 1 ? '!' : ','),
    timings.frameTicks, (timings.frameTicks > 16 ? '!' : ' '));
}

int CPro1000::StartMainBoardThread(void *data)
{
  // Call method on CPro1000 to run PPC main board thread
  CPro1000 *pro1000 = (CPro1000 *) data;
  return pro1000->RunMainBoardThread();
}

int CPro1000::RunMainBoardThread(void)
{
  for (;;)
  {
    bool wait = true;
    bool exit = false;
    while (wait && !exit)
    {
      // Wait on PPC main board thread semaphore
      if (!ppcBrdThreadSync->Wait())
        goto ThreadError;

      // Enter notify critical section
      if (!notifyLock->Lock())
        goto ThreadError;

      // Check threads are not being stopped or paused
      if (stopThreads)
        exit = true;
      else if (!pauseThreads)
      {
        wait = false;
        ppcBrdThreadRunning = true;
      }

      // Leave notify critical section
      if (!notifyLock->Unlock())
        goto ThreadError;
    }
    if (exit)
      return 0;

    // Process a single frame for PPC main board
    RunMainBoardFrame();

    // Enter notify critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Let other threads know processing has finished
    ppcBrdThreadRunning = false;
    ppcBrdThreadDone = true;
    if (!notifySync->SignalAll())
      goto ThreadError;

    // Leave notify critical section
    if (!notifyLock->Unlock())
      goto ThreadError;
  }

ThreadError:
  ErrorLog("Threading error in RunMainBoardThread: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return 1;
}

void CPro1000::Reset(void)
{
  // Clear memory
  memset(ram, 0, RAM_SIZE);

  // Reset all devices
  ppc_reset();
  PIC.Reset();
  JTAG.Reset();
  m_scanChain.Reset();
  PCIBridge.Reset();
  PCIBus.Reset();
  SCSI.Reset();
  SCSIHostAdapter.Reset();
  GPU.Reset();
  UART.Reset();
  m_scrollRegister = 0;

  gpusReady = false;

  memset(&timings, 0, sizeof(timings));

  DebugLog("Pro-1000 reset\n");
}


/******************************************************************************
 Initialization, Shutdown, and ROM Management
******************************************************************************/

const Game &CPro1000::GetGame() const
{
  return m_game;
}

void CPro1000::AttachRenderers(IRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA)
{
  // No tile generator. The 2D layer is the scroll layer (CPro1000Render2D).
  m_render2D = dynamic_cast<CPro1000Render2D *>(Render2DPtr);
  Render3DPtr->SetStepping(m_game.stepping);

  // Set the native display coordinate range for Pro-1000 sync file 1 (VGA
  // 640x480). The API sync table gives j_max=636, i_max=480 for this mode.
  // These values are the coordinate range that the host program uses to set
  // viewport positions and sizes.
  //
  // TODO: detect sync file changes at runtime and call SetScreenSize() again.
  // The host program sends a record with cntrl=0x17 and the sync file number
  // (1-8) in the update buffer. The firmware processes this record and writes
  // new timing values to the video sync memory (0x58000000) and logic sync
  // memory (0x60000000). Intercept these writes, or parse the SCSI WRITE data
  // before the firmware receives it, to read the new j_max and i_max values
  // from the sync table and call SetScreenSize() with those values.
  Render3DPtr->SetScreenSize(636.0f, 480.0f);

  GPU.AttachRenderer(Render3DPtr);
  m_superAA = superAA;
}

void CPro1000::AttachInputs(CInputs *InputsPtr)
{
  // No game inputs
}

void CPro1000::AttachOutputs(COutputs *OutputsPtr)
{
  // No outputs
}

void CPro1000::AttachConsoleInput(IConsoleInput *input)
{
  // Console input is received by the serial port
  UART.AttachInput(input);
}

void CPro1000::AttachNetServer(std::shared_ptr<CNetServer> server)
{
  m_server = std::move(server);
}

/*
 * Builds the JTAG scan chain of the model. The firmware's configuration
 * detection (0x17EB0) reads the chain's ID codes and expects one of three
 * layouts (see JTAG):
 *
 *   PRO-1100  single pixel board  Jupiter, Mercury, Venus, then one group
 *   PRO-1200  dual pixel board    Jupiter, Mercury, Venus, then two groups
 *   PRO-1400  high res            Jupiter, Mercury, Venus, then four groups
 *
 * A group is Earth, 3D-RAM, Mars, Mars, 3D-RAM, 3D-RAM, 3D-RAM. The single
 * and dual layouts are the Model 3 Step 1.x and 2.x chains. The high res
 * layout must have the high res ID codes for Jupiter (0x0D7F1057) and Earth
 * (0x0D7F0057). ID code version fields are ignored by the firmware but are
 * set to 0, as printed in its expected ID table. Only the first ASIC of each
 * type forwards its mode word to the Real3D object (as on Model 3).
 */
void CPro1000::BuildScanChain(Game::Stepping stepping)
{
  auto addASIC = [this](CASIC::Name name, uint32_t idCode) -> CJTAGDevice *
  {
    m_asics.emplace_back(std::make_unique<CASIC>(GPU, name));
    m_asics.back()->SetIDCode(idCode);
    return m_asics.back().get();
  };
  auto add3DRAM = [this]() -> CJTAGDevice *
  {
    m_3drams.emplace_back(std::make_unique<C3DRAM>());
    return m_3drams.back().get();
  };

  bool highRes = stepping == Game::Stepping::PRO1000_1400;
  uint32_t jupiterID = highRes ? 0x0D7F1057 : 0x016C7057;
  uint32_t earthID = highRes ? 0x0D7F0057 : 0x016C5057;
  std::vector<CJTAGDevice *> devices
  {
    addASIC(CASIC::Name::Jupiter, jupiterID),
    addASIC(CASIC::Name::Mercury, 0x016C3057),
    addASIC(CASIC::Name::Venus,   0x016C4057)
  };
  unsigned groups = stepping == Game::Stepping::PRO1000_1100 ? 1 : (stepping == Game::Stepping::PRO1000_1200 ? 2 : 4);
  for (unsigned group = 0; group < groups; group++)
  {
    bool first = group == 0;
    devices.push_back(addASIC(first ? CASIC::Name::Earth : CASIC::Name::Dummy, earthID));
    devices.push_back(add3DRAM());
    devices.push_back(addASIC(first ? CASIC::Name::Mars : CASIC::Name::Dummy, 0x016C6057));
    devices.push_back(addASIC(CASIC::Name::Dummy, 0x016C6057));
    devices.push_back(add3DRAM());
    devices.push_back(add3DRAM());
    devices.push_back(add3DRAM());
  }
  m_scanChain = CJTAGScanChain(devices);
  JTAG.AttachScanChain(&m_scanChain);
}

// Pro-1000 initialization: memory, devices, the model, and ROMs
Result CPro1000::Init(const Game &game, const ROMSet &rom_set)
{
  constexpr float memSizeMB = (float)MEM_POOL_SIZE / (float)0x100000;

  // Polygon memory (see "Models"). GameLoader has checked the stepping.
  unsigned polygonMemoryMB = game.polygon_memory ? game.polygon_memory : 8;
  if (polygonMemoryMB != 8 && polygonMemoryMB != 16 && polygonMemoryMB != 24 && polygonMemoryMB != 32)
    return ErrorLog("Polygon memory of %u MB for '%s' is not possible. It must be 8, 16, 24, or 32 MB.", polygonMemoryMB, game.name.c_str());

  // Allocate all memory for ROMs and PPC RAM
  memoryPool = new(std::nothrow) UINT8[MEM_POOL_SIZE];
  if (NULL == memoryPool)
    return ErrorLog("Insufficient memory for Pro-1000 object (needs %1.1f MB).", memSizeMB);
  memset(memoryPool, 0, MEM_POOL_SIZE);

  // Set up pointers
  ram = &memoryPool[RAM_OFFSET];
  flash = &memoryPool[FLASH_OFFSET];
  scsiROM = &memoryPool[SCSI_ROM_OFFSET];

  // Initialize devices (PowerPC initialized after ROMs loaded)
  PCIBridge.Init();
  PCIBus.Init();
  SCSI.Init(this, &PIC, PIC_LINE_SCSI);
  SCSI.AttachInitiator(&SCSIHostAdapter);

  // Host computer programs connect to the SCSI host adapter through the
  // bridge
  if (m_server)
    m_scsiTCPBridge = std::make_unique<CSCSITCPBridge>(SCSIHostAdapter, m_server);
  if (Result::OKAY != GPU.Init(nullptr, this, &PIC, PIC_LINE_NONE, Real3DConfig::Pro1000(polygonMemoryMB * 0x100000))) // no VROM; Real3D DMA interrupt line unknown
    return Result::FAIL;
  UART.AttachInterruptOutput([this](bool level)
  {
    if (level)
      PIC.Assert(PIC_LINE_UART);
    else
      PIC.Deassert(PIC_LINE_UART);
  });
  JTAG.AttachInterruptOutput([this](bool level)
  {
    if (level)
      PIC.Assert(PIC_LINE_JTAG);
    else
      PIC.Deassert(PIC_LINE_JTAG);
  });

  PCIBridge.AttachPCIBus(&PCIBus);
  PCIBus.AttachDevice(13, &GPU);
  PCIBus.AttachDevice(SCSI_PCI_DEVICE, &SCSI);
  BuildScanChain(game.stepping);

  DebugLog("Initialized Pro-1000 (allocated %1.1f MB)\n", memSizeMB);

  // Unprogrammed flash reads back as all 1's
  memset(flash, 0xFF, FLASH_SIZE);

  // CPU ROM: boot stub and loop code
  ROM cpuROM = rom_set.get_rom("cpu_rom");
  if (cpuROM.size == 0)
    return ErrorLog("No CPU ROM defined for '%s'.", game.name.c_str());
  if (cpuROM.size > INIT_ROM_BASE - CPU_ROM_BASE)
    return ErrorLog("CPU ROM for '%s' is too large (%zu bytes). It must fit within 0x%08X-0x%08X.", game.name.c_str(), cpuROM.size, CPU_ROM_BASE, INIT_ROM_BASE - 1);
  cpuROM.CopyTo(&flash[CPU_ROM_OFFSET], cpuROM.size);

  // INIT ROM: power-up video timing, mode words, and renderer initialization
  // data stored in flash above the loop code
  ROM initROM = rom_set.get_rom("init_rom");
  if (initROM.size == 0)
    return ErrorLog("No INIT ROM defined for '%s'.", game.name.c_str());
  if (initROM.size > FLASH_SIZE - INIT_ROM_OFFSET)
    return ErrorLog("INIT ROM for '%s' is too large (%zu bytes). It must fit within 0x%08X-0xFFFFFFFF.", game.name.c_str(), initROM.size, INIT_ROM_BASE);
  initROM.CopyTo(&flash[INIT_ROM_OFFSET], initROM.size);

  // SCSI ROM: use image if provided, otherwise synthesize a minimal one
  memset(scsiROM, 0xFF, SCSI_ROM_SIZE);
  ROM scsiROMImage = rom_set.get_rom("scsi_rom");
  if (scsiROMImage.size)
  {
    if (scsiROMImage.size > SCSI_ROM_SIZE)
      return ErrorLog("SCSI ROM for '%s' is too large (%zu bytes). It must fit within 0x%08X-0x%08X.", game.name.c_str(), scsiROMImage.size, SCSI_ROM_BASE, SCSI_ROM_BASE + SCSI_ROM_SIZE - 1);
    scsiROMImage.CopyTo(scsiROM, scsiROMImage.size);
  }
  else
  {
    // The synthetic ROM also contains the update memory configuration. At
    // boot, rend_init sends it to the Real3D configuration registers. Without
    // it, the registers stay zero and the update buffers do not operate.
    BuildSyntheticSCSIROM(scsiROM, SCSI_ROM_SIZE);
  }

  // Convert PowerPC ROMs to little endian words
  Util::FlipEndian32(flash, FLASH_SIZE);
  Util::FlipEndian32(scsiROM, SCSI_ROM_SIZE);

  // Configure CPU and PCI bridge. The bus is 33 MHz because the firmware
  // expects a time base of 8.33 MHz (see "CPU Clock and Time Base"). The 2X
  // multiplier keeps the core at 66 MHz, as on Model 3 Step 1.0.
  PPC_CONFIG ppc_config;
  ppc_config.pvr = PPC_MODEL_603R;
  ppc_config.bus_frequency = BUS_FREQUENCY_33MHZ;
  ppc_config.bus_frequency_multiplier = 0x20;   // 2X multiplier
  PCIBridge.SetModel(0x105);                    // MPC105

  // Initialize CPU
  ppc_init(&ppc_config);
  ppc_attach_bus(this);
  PPCFetchRegions[0].start = 0;
  PPCFetchRegions[0].end = RAM_SIZE - 1;
  PPCFetchRegions[0].ptr = (UINT32 *) ram;
  PPCFetchRegions[1].start = FLASH_BASE_ADDR;
  PPCFetchRegions[1].end = 0xFFFFFFFF;
  PPCFetchRegions[1].ptr = (UINT32 *) flash;
  PPCFetchRegions[2].start = 0;
  PPCFetchRegions[2].end = 0;
  PPCFetchRegions[2].ptr = NULL;
  ppc_set_fetch(PPCFetchRegions);

  // Print game information
  if (!game.version.empty())
    std::cout << "    Title:          " << game.title << " (" << game.version << ")" << std::endl;
  else
    std::cout << "    Title:          " << game.title << std::endl;
  std::cout << "    ROM Set:        " << game.name << std::endl;
  std::cout << "    Developer:      " << game.manufacturer << std::endl;
  std::cout << "    Year:           " << game.year << std::endl;
  const char *pixelProcessors = game.stepping == Game::Stepping::PRO1000_1100 ? "1 pixel processor" : (game.stepping == Game::Stepping::PRO1000_1200 ? "2 pixel processors" : "4 pixel processors");
  std::cout << "    Hardware:       " << Game::SteppingName(game.stepping) << " (" << pixelProcessors << "), " << polygonMemoryMB << " MB polygon memory" << std::endl;
  std::cout << std::endl;

  m_game = game;
  return Result::OKAY;
}

CPro1000::CPro1000(Util::Config::Node &config)
  : m_config(config),
    m_multiThreaded(config["MultiThreaded"].ValueAs<bool>()),
    m_gpuMultiThreaded(config["GPUMultiThreaded"].ValueAs<bool>()),
    memoryPool(NULL),
    ram(NULL),
    flash(NULL),
    scsiROM(NULL),
    gpusReady(false),
    startedThreads(false),
    pauseThreads(false),
    stopThreads(false),
    ppcBrdThread(NULL),
    ppcBrdThreadRunning(false),
    ppcBrdThreadDone(false),
    ppcBrdThreadSync(NULL),
    notifyLock(NULL),
    notifySync(NULL),
    GPU(config),
    m_superAA(nullptr)
{
  memset(PPCFetchRegions, 0, sizeof(PPCFetchRegions));
  memset(&timings, 0, sizeof(timings));
  DebugLog("Built Pro-1000\n");
}

// Dumps a memory region to a file for debugging purposes
static void Dump(const char *name, const char *file, uint8_t *buf, size_t size)
{
  FILE *fp = fopen(file, "wb");
  if (NULL != fp)
  {
    Util::FlipEndian32(buf, size);
    fwrite(buf, sizeof(UINT8), size, fp);
    fclose(fp);
    printf("Wrote %s to '%s'\n", name, file);
  }
  else
    ErrorLog("Unable to open '%s' for writing.", file);
}

CPro1000::~CPro1000(void)
{
  // Dump memory if requested
  if (m_config["DumpMemory"].ValueAsDefault<bool>(false) && memoryPool != NULL)
  {
    Dump("RAM", "ram.bin", ram, RAM_SIZE);
    Dump("flash", "flash.bin", flash, FLASH_SIZE);
  }

  // Stop all threads
  StopThreads();

  // Free memory
  if (memoryPool != NULL)
  {
    delete [] memoryPool;
    memoryPool = NULL;
  }

  ram = NULL;
  flash = NULL;
  scsiROM = NULL;

  DebugLog("Destroyed Pro-1000\n");
}
