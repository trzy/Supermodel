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
 * dma.h
 *
 * Step 2.0+ DMA device header.
 */

#ifndef INCLUDED_DMA_H
#define INCLUDED_DMA_H

extern void     dma_init(UINT32 (*)(UINT32), void (*)(UINT32, UINT32));
extern void     dma_shutdown(void);
extern void     dma_reset(void);

extern void     dma_save_state(FILE *);
extern void     dma_load_state(FILE *);

extern UINT8    dma_read_8(UINT32 a);
extern UINT32   dma_read_32(UINT32 a);
extern void     dma_write_8(UINT32 a, UINT8 d);
extern void     dma_write_32(UINT32 a, UINT32 d);

#endif  // INCLUDED_DMA_H

