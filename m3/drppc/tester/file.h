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

#ifndef INCLUDED_FILE_H
#define INCLUDED_FILE_H

#include "types.h"

#define FILE_READ		0x1
#define FILE_WRITE		0x2
#define FILE_BINARY		0x4
#define FILE_TEXT		0x8

/* Functions */

FILE* open_file(UINT32, char*, ...);
void close_file(FILE*);
BOOL read_from_file(FILE*, UINT8*, UINT32);
BOOL write_to_file(FILE*, UINT8*, UINT32);

size_t get_file_size(const char*);
BOOL read_file(const char*, UINT8*, UINT32);
BOOL write_file(const char*, UINT8*, UINT32);

size_t get_file_size_crc(UINT32);
BOOL read_file_crc(UINT32, UINT8*, UINT32);

BOOL set_directory(char*, ...);
BOOL set_directory_zip(char*, ...);

#endif /* INCLUDED_FILE_H */
