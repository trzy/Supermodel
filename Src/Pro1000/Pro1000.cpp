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
 *    of the SCSI ROM. The SDK's DB_Demos/Orlando contains 3dlogo.cmem and
 *    3dlogo.pmem.
 *  - The real-time clock count returned to the host is in 30 ns hardware
 *    clocks.
 *  - Demo databases (DB_Demos) are distributed precompiled as .cmem (culling
 *    memory), .pmem (polygon memory), and .tmap (texture) files.
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
 *  9. get_hw_config, rend_mw_init, then rend_init (0x51A8), which uploads
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
 *   +0x00        Magic, 0xCAFEBABE (checked by rend_init and 0xA680)
 *   +0x1C,+0x20  Size, offset of a block uploaded to polygon memory
 *                (REND_POLY_BASE) by 0x615C, preceded by a header word
 *                written to 0x98000000; +0x48 is also read
 *   +0x24,+0x28  Offset, size in bytes of a block uploaded to culling memory
 *                (REND_CULL_BASE) in four parts by rend_init
 *   +0x2C,+0x30  Offset, size of a block uploaded to polygon memory in four
 *                parts by 0x12AF0
 *   +0x34        Offset of a 12-entry table (used by 0xA680)
 *   +0x3C        3 words written to REND_UPDATE_START (0x9C000000, the Model
 *                3 configuration registers) by rend_init
 *
 * Block data is copied word for word with no byte swapping (0x944C), so it is
 * stored in the renderer's little endian byte order. Each upload goes through
 * 0x10E3C (write words to a renderer address) and 0x63DC, and is followed by a
 * write to REND_UPDATE_COMPL (0x88000000) and a wait for GP sync (0x577C).
 * After uploading, 0x615C writes one word to REND_ROM_UPDATE (0x90000000).
 *
 * The SCSI ROM is not part of the SDK flash update and its contents are
 * unknown. All that is known is where the blocks are uploaded: culling memory,
 * polygon memory, and the configuration registers. They could be a default
 * scene, tables the renderer needs in order to operate, default data
 * structures, or test data. The SDK documentation suggests it is the
 * default database, a spinning Real 3D logo displayed at power-on.
 *
 * Emulation: the ROM is mapped read only at SCSI_ROM_BASE regardless of the
 * expansion ROM BAR (the firmware always programs 0xC0000000). Writes (flash
 * programming) are not emulated. If the game defines a "scsi_rom" region, it
 * is loaded as a byte image in PCI address order. Otherwise a synthetic image
 * is created (CPro1000::BuildSyntheticSCSIROM()): magic, zeroed 256-byte
 * directory (all blocks empty, configuration words 0), and 0xFF elsewhere.
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
 *                                        Bit 26: foreground/background, bits
 *                                        25-24: viewport select, bits 23-0:
 *                                        RGB color (from the SC monitor
 *                                        command's formatted dump). INIT ROM
 *                                        value is 0x040CF0F8. Probably a solid
 *                                        color layer in the output stage.
 * 64000000-38        UART_*              16550 UART (RDTD_BUFFER, INT_ENABLE,
 *                                        INT_ID/FIFO_CNTRL, LINE_CNTRL,
 *                                        MODEM_CNTRL, LINE_STATUS,
 *                                        MODEM_STATUS, SCRATCH, DIV_LATCH0/1).
 *                                        [Emulated]
 * 68000000-38        JTAG_*              SCANPSC100F (TDO, TDI, TMS0, TMS1,
 *                                        CNT0, MODE0-2). [Emulated]
 * 70000000           RIF_MODE            RIF mode word. Bit 13 checked at boot,
 *                                        bit 14 toggled by boot stub, bits
 *                                        23:16 probably DIP switches, bit 7
 *                                        cleared around GP sync waits
 *                                        (0x577C). Field names from the
 *                                        RIF_MODE monitor command (bit
 *                                        positions not yet mapped): BIST
 *                                        enabled, flash ROM write enabled,
 *                                        renderer init disabled, unused
 *                                        switch, SCSI ID (4 bits), align
 *                                        syncs, BIST LED, main CPU flag, four
 *                                        external sync logic resistor
 *                                        selects, kill video syncs, overload
 *                                        force/no report/mode (2 bits)/enable,
 *                                        select external video reference,
 *                                        sync transition select, RIF on-line.
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
 * Expected ID codes (table at 0x61200): Jupiter 0x016C7057 or 0x0D7F1057,
 * Mercury 0x016C3057, Venus 0x016C4057, Earth 0x016C5057 or 0x0D7F0057, Mars
 * 0x016C6057. These are the Model 3 ID codes with version field 0.
 *
 * The emulated chain (CPro1000::BuildScanChain()) is the single pixel board
 * configuration, using the Model 3 CASIC and C3DRAM device models. With it,
 * the firmware prints "SINGLE PIXEL BOARD DETECTED". Early attempts used only
 * the 15 ASICs from the firmware's name table (0x611AC) and omitted the
 * 3D-RAMs; every missing 1-bit device shifted the ID codes out of position and
 * all three configuration checks failed.
 *
 * Other configurations could be detected simply by building the corresponding
 * chain (dual is the Model 3 Step 2.x chain). Actually emulating them also
 * depends on what the firmware configures afterwards (mode words for the
 * additional Earth/Mars ASICs, memory sizes, high res video timing) and on
 * renderer support. Dual should be similar to Model 3 Step 2.x; high res goes
 * beyond anything on Model 3. show_config_data reports a "Pre-Production
 * System" with this chain, possibly derived from ID code version fields.
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
 *    the Model 3 rate of 57.52 Hz. The firmware measures 34 ms per field,
 *    so the time base rate may also be off. The logic sync memory
 *    (LSYNC_BASE) probably sequences these signals.
 *  - Device 12 (Real3D) register layout and the culling/polygon memory map.
 *  - Board configurations. Only the single pixel board is emulated. Support
 *    single, dual, and high res configurations, probably selected by reusing
 *    the Games.xml stepping field (currently a placeholder "1.0" for the
 *    Pro-1000). To revisit once rendering works.
 *  - Where scan chains should be built. Currently CPro1000 builds its chain
 *    while Model 3's CJTAG builds its own. Since the Pro-1000 single and dual
 *    chains are the Model 3 Step 1.x and 2.x chains, consider having CReal3D
 *    build the chain (its ASICs are the Real3D chipset) from a board
 *    configuration supplied by the system, preserving the Model 3 JTAG2 save
 *    state layout.
 *  - What determines "Pre-Production" vs. "Production" system and the
 *    extended texture memory setting reported by show_config_data.
 *  - Contents of the SCSI ROM at SCSI_ROM_BASE (magic 0xCAFEBABE at offset 0,
 *    offset at 0x24), checked by rend_init. This is the SCSI controller's
 *    expansion ROM, separate from the INIT ROM in CPU flash, and is not part
 *    of the SDK flash update (p1_p4.bin, p2.bin), so it was presumably
 *    programmed at the factory. Without it, renderer initialization is
 *    skipped.
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

#include <new>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "Supermodel.h"
#include "ROMSet.h"
#include "OSD/Video.h"
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
constexpr static uint32_t SCSI_ROM_HEADER_SIZE = 0x100;   // directory area zeroed in synthetic image

constexpr static uint32_t MEM_POOL_SIZE     = RAM_SIZE + FLASH_SIZE + SCSI_ROM_SIZE;
constexpr static uint32_t RAM_OFFSET        = 0;
constexpr static uint32_t FLASH_OFFSET      = RAM_OFFSET + RAM_SIZE;
constexpr static uint32_t SCSI_ROM_OFFSET   = FLASH_OFFSET + FLASH_SIZE;

constexpr static int      STEPPING          = 0x10;       // treat hardware as Model 3 Step 1.0
constexpr static unsigned PPC_FREQUENCY_MHZ = 66;         // Step 1.0 PowerPC frequency

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
  gpusReady = true;

  timings.syncTicks = CThread::GetTicks() - start;
}

void CPro1000::RenderFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // Call OSD video callbacks
  if (BeginFrameVideo() && gpusReady)
  {
    GPU.BeginFrame();
    GPU.RenderFrame();
    GPU.EndFrame();
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

Result CPro1000::LoadGame(const Game &game, const ROMSet &rom_set)
{
  m_game = Game();

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
    BuildSyntheticSCSIROM();

  // Convert PowerPC ROMs to little endian words
  Util::FlipEndian32(flash, FLASH_SIZE);
  Util::FlipEndian32(scsiROM, SCSI_ROM_SIZE);

  // Configure CPU and PCI bridge as Step 1.0
  PPC_CONFIG ppc_config;
  ppc_config.pvr = PPC_MODEL_603R;              // 66 MHz
  ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
  ppc_config.bus_frequency_multiplier = 0x10;   // 1X multiplier
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

  // Initialize Real3D
  GPU.SetStepping(STEPPING);

  // Print game information
  if (!game.version.empty())
    std::cout << "    Title:          " << game.title << " (" << game.version << ")" << std::endl;
  else
    std::cout << "    Title:          " << game.title << std::endl;
  std::cout << "    ROM Set:        " << game.name << std::endl;
  std::cout << "    Developer:      " << game.manufacturer << std::endl;
  std::cout << "    Year:           " << game.year << std::endl;
  std::cout << "    Platform:       Real3D Pro-1000" << std::endl;
  std::cout << std::endl;

  m_game = game;
  return Result::OKAY;
}

void CPro1000::AttachRenderers(CRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA)
{
  // No tile generator, so 2D renderer is unused
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

/*
 * The real SCSI ROM contents are unknown (it was likely used to hold an initial
 * scene, perhaps a Real3D logo). This creates the minimum needed for rend_init
 * to proceed: the magic word and a zeroed directory, so that every block has
 * offset 0 and size 0 and the three configuration register words are 0 (the
 * Real3D's reset values). The rest of the ROM is erased (0xFF).The image is in
 * PCI byte order (little endian words) as the real ROM would be.
 */
void CPro1000::BuildSyntheticSCSIROM(void)
{
  memset(scsiROM, 0, SCSI_ROM_HEADER_SIZE);
  const uint32_t magic = 0xCAFEBABE;
  scsiROM[0] = uint8_t(magic >> 0);
  scsiROM[1] = uint8_t(magic >> 8);
  scsiROM[2] = uint8_t(magic >> 16);
  scsiROM[3] = uint8_t(magic >> 24);
  InfoLog("Pro-1000: Using synthetic SCSI ROM (magic and empty directory only).");
}

/*
 * Single pixel board configuration. The firmware's configuration detection
 * (0x17EB0) reads the chain's ID codes and expects exactly the Model 3 Step
 * 1.x chain layout: Jupiter, Mercury, Venus, Earth, 3D-RAM, Mars, Mars,
 * 3D-RAM x3 (196 bits in IDCODE/BYPASS). ID code version fields are ignored by
 * the firmware but are set to 0, as printed in its expected ID table. Only the
 * first ASIC of each type forwards its mode word to the Real3D object (as on
 * Model 3).
 */
void CPro1000::BuildScanChain(void)
{
  m_asics.clear();
  auto addASIC = [this](CASIC::Name name, uint32_t idCode) -> CJTAGDevice *
  {
    m_asics.emplace_back(std::make_unique<CASIC>(GPU, name));
    m_asics.back()->SetIDCode(idCode);
    return m_asics.back().get();
  };

  std::vector<CJTAGDevice *> devices
  {
    addASIC(CASIC::Name::Jupiter, 0x016C7057),
    addASIC(CASIC::Name::Mercury, 0x016C3057),
    addASIC(CASIC::Name::Venus,   0x016C4057),
    addASIC(CASIC::Name::Earth,   0x016C5057),
    &m_3dram[0],
    addASIC(CASIC::Name::Mars,    0x016C6057),
    addASIC(CASIC::Name::Dummy,   0x016C6057),
    &m_3dram[1],
    &m_3dram[2],
    &m_3dram[3]
  };
  m_scanChain = CJTAGScanChain(devices);
  JTAG.AttachScanChain(&m_scanChain);
}

Result CPro1000::Init(void)
{
  constexpr float memSizeMB = (float)MEM_POOL_SIZE / (float)0x100000;

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
  // bridge. If the bridge cannot start, emulation continues without it.
  m_scsiTCPBridge = std::make_unique<CSCSITCPBridge>(SCSIHostAdapter, SCSITCPBridgeProtocol::DEFAULT_PORT);
  m_scsiTCPBridge->Start();
  if (Result::OKAY != GPU.Init(nullptr, this, &PIC, PIC_LINE_NONE, Real3DConfig::Pro1000())) // no VROM; Real3D DMA interrupt line unknown
    return Result::FAIL;
  BuildScanChain();
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

  DebugLog("Initialized Pro-1000 (allocated %1.1f MB)\n", memSizeMB);

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
