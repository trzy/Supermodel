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
 * osd_common/disasm.h
 *
 * PowerPC disassembler header.
 */

#ifndef INCLUDED_OSD_COMMON_DISASM_H
#define INCLUDED_OSD_COMMON_DISASM_H

#include "drppc.h"

extern BOOL DisassemblePowerPC(UINT32, UINT32, CHAR *, CHAR *, BOOL);

#endif  // INCLUDED_OSD_COMMON_DISASM_H
