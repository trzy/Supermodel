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
 * bridge.h
 *
 * MPC105 and MPC106 PCI bridge/memory controller header.
 */

#ifndef INCLUDED_BRIDGE_H
#define INCLUDED_BRIDGE_H

extern UINT8    bridge_read_8(UINT32);
extern UINT16   bridge_read_16(UINT32);
extern UINT32   bridge_read_32(UINT32);
extern void     bridge_write_8(UINT32, UINT8);
extern void     bridge_write_16(UINT32, UINT16);
extern void     bridge_write_32(UINT32, UINT32);

extern UINT8    bridge_read_config_data_8(UINT32);
extern UINT16   bridge_read_config_data_16(UINT32);
extern UINT32   bridge_read_config_data_32(UINT32);
extern void     bridge_write_config_addr_32(UINT32, UINT32);
extern void     bridge_write_config_data_8(UINT32, UINT8);
extern void     bridge_write_config_data_16(UINT32, UINT16);
extern void     bridge_write_config_data_32(UINT32, UINT32);

extern void     bridge_save_state(FILE *);
extern void     bridge_load_state(FILE *);

extern void     bridge_reset(INT);
extern void     bridge_init(UINT32 (*)(UINT32));

#endif  // INCLUDED_BRIDGE_H

