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
 * scsi.h
 *
 * NCR 53C810 SCSI controller header.
 */

#ifndef INCLUDED_SCSI_H
#define INCLUDED_SCSI_H

/*
 * Functions
 */

extern UINT8    scsi_read_8(UINT32);
extern UINT32   scsi_read_32(UINT32);

extern void     scsi_write_8(UINT32, UINT8);
extern void     scsi_write_16(UINT32, UINT16);
extern void     scsi_write_32(UINT32, UINT32);

extern void     scsi_save_state(FILE *);
extern void     scsi_load_state(FILE *);

extern void     scsi_run(INT);
extern void     scsi_reset(void);

extern void     scsi_init(UINT8 (*)(UINT32), UINT16 (*)(UINT32),
                          UINT32 (*)(UINT32), void (*)(UINT32, UINT8),
                          void (*)(UINT32, UINT16),
                          void (*)(UINT32, UINT32));
extern void     scsi_shutdown(void);

#endif  // INCLUDED_SCSI_H
