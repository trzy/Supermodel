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
 * file.c
 *
 * File access and ZIP file support
 *
 */

#include "model3.h"
#include "file.h"
#include "unzip/unzip.h"	/* ZLIB */

static unzFile		zip_file;

/* zip_get_file_size
 *
 * Gets the uncompressed size of a file in the ZIP file
 * Returns the file size or 0 if the file is not found (or the file size is 0)
 */

static UINT32 zip_get_file_size(const char* file)
{
	unz_file_info	file_info;

	if( unzLocateFile(zip_file, file, 2) != UNZ_OK )
		return 0;

	if( unzGetCurrentFileInfo(zip_file, &file_info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK )
		return 0;

	return file_info.uncompressed_size;
}

/* zip_read_file
 *
 * Reads the specified file to a buffer
 * Returns TRUE if successful otherwise FALSE
 */

static BOOL zip_read_file(const char* file, UINT8* buffer, UINT32 size)
{
	if( unzLocateFile(zip_file, file, 2) != UNZ_OK )
		return FALSE;

	if( unzOpenCurrentFile(zip_file) != UNZ_OK )
		return FALSE;

	if( unzReadCurrentFile(zip_file, buffer, size) < 0 ) {
		unzCloseCurrentFile(zip_file);
		return FALSE;
	}

	unzCloseCurrentFile(zip_file);
	return TRUE;
}

/* zip_find_name_for_crc
 *
 * Finds a matching file name for a crc
 * Returns TRUE if crc was found. The file name is copied to the supplied string pointer.
 *
 */

static BOOL zip_find_name_for_crc(UINT32 crc, char* file)
{
	unz_file_info file_info;
	char filename[4096];

	if( unzGoToFirstFile(zip_file) != UNZ_OK )
		return FALSE;

	do
	{
		if( unzGetCurrentFileInfo(zip_file, &file_info, filename, sizeof(filename), NULL, 0, NULL, 0) != UNZ_OK )
			return FALSE;

		if( file_info.crc == crc ) {
			strcpy( file, filename );
			return TRUE;
		}

	} while( unzGoToNextFile(zip_file) == UNZ_OK );

	return FALSE;
}

/* zip_open
 * 
 * Opens a zip file. Must be called before any of the above functions !
 */

static BOOL zip_open(const char* file)
{
	zip_file = unzOpen(file);
	if( zip_file == NULL )
		return FALSE;

	return TRUE;
}

/* zip_close
 *
 * Closes the zip file
 */

static void zip_close(void)
{
	unzClose(zip_file);
}



/* File access functions */

static BOOL zip_directory = FALSE;
static char directory_path[4096];

/* TODO: set_directory and set_directory_zip could be merged together */

/* set_directory
 *
 * Sets the current directory
 *
 */

BOOL set_directory(char* path, ...)
{
	char string[4096];
	va_list l;

	va_start(l, path);
	vsprintf(string, path, l);
	va_end(l);

	zip_directory = FALSE;
	strcpy( directory_path, string );
	return TRUE;
}

/* set_directory_zip
 *
 * Sets a zip file as current directory
 *
 */

BOOL set_directory_zip(char* file, ...)
{
	char string[4096];
	va_list l;

	va_start(l, file);
	vsprintf(string, file, l);
	va_end(l);

	/* Close the old zip file */
	zip_close();
	
	if( !zip_open(string) )
		return FALSE;

	zip_directory = TRUE;
	strcpy( directory_path, string );
	return TRUE;
}

/* get_file_size
 *
 * Returns the file size
 *
 */

size_t get_file_size(const char* file)
{
	FILE* f;
	size_t length = 0;
	char path[4096];

	if( zip_directory ) {
		length = zip_get_file_size(file);
	} 
	else {
		sprintf( path, "%s/%s", directory_path, file );
		f = fopen( path, "rb" );
		if( f == NULL )
			return 0;

		fseek( f, 0, SEEK_END );
		length = ftell(f);
		fclose(f);
	}

	return length;
}

/* read_file
 *
 * Reads the content of the file to a buffer
 *
 */

BOOL read_file(const char* file, UINT8* buffer, UINT32 size)
{
	FILE* f;
	char path[4096];

	if( zip_directory ) {
		return zip_read_file( file, buffer, size );
	}
	else {
		/* Build a full path to file */
		strcpy( path, directory_path );
		strcat( path, "/" );
		strcat( path, file );

		f = fopen( path, "rb" );
		if( f == NULL )
			return FALSE;

		fseek( f, 0, SEEK_SET );
		if( fread(buffer, sizeof(UINT8), size, f) != (size_t)size ) {
			fclose(f);
			return FALSE;
		}

		fclose(f);
	}
	return TRUE;
}

/* write_file
 *
 * Writes the contents of a buffer to a file
 *
 */

BOOL write_file(const char* file, UINT8* buffer, UINT32 size)
{
	FILE* f;
	char path[4096];

	/* Whoops. No saving to a zip file :) */
	if( zip_directory )
		return FALSE;

	/* Build a full path to file */
	strcpy( path, directory_path );
	strcat( path, "/" );
	strcat( path, file );

	f = fopen( path, "wb" );

	fseek( f, 0, SEEK_SET );
	if( fwrite(buffer, sizeof(UINT8), size, f) != (size_t)size ) {
		fclose(f);
		return FALSE;
	}

	fclose(f);

	return TRUE;
}


/* get_file_size_crc
 *
 * Returns the file size of a file specified by crc
 *
 */

size_t get_file_size_crc(UINT32 crc)
{
	char file[4096];

	/* only zip files supported */
	if( !zip_directory )
		return 0;

	if( !zip_find_name_for_crc(crc, file) )
		return 0;

	return zip_get_file_size(file);
}

/* read_file_crc
 *
 * Reads the contents of a file specified by crc
 *
 */

BOOL read_file_crc(UINT32 crc, UINT8* buffer, UINT32 size)
{
	char file[4096];

	/* only zip files supported */
	if( !zip_directory )
		return FALSE;

	if( !zip_find_name_for_crc(crc, file) )
		return FALSE;

	return zip_read_file(file, buffer, size);
}



/* open_file
 *
 * Opens a file
 *
 */

FILE* open_file(UINT32 flags, char* file, ...)
{
	char string[4096];
	char path[4096];
	char mode[3];
	char* m = mode;
	va_list l;

	/* zip files are not supported this way */
	if( zip_directory )
		return 0;

	va_start(l, file);
	vsprintf(string, file, l);
	va_end(l);

	/* Build full path */
	sprintf( path, "%s/%s", directory_path, string );

	/* Set file access mode */
	if( flags & FILE_READ )
		m += sprintf( m, "r" );
	else
		m += sprintf( m, "w" );

	if( flags & FILE_BINARY )
		m += sprintf( m, "b" );
	else
		m += sprintf( m, "t" );

	m[1] = '\0';

	return fopen( path, m );
}

/* close_file
 *
 * Closes the file
 *
 */

void close_file(FILE* file)
{
	fclose(file);
}

/* read_from_file
 *
 * Reads n bytes from file to a buffer
 *
 */

BOOL read_from_file(FILE* file, UINT8* buffer, UINT32 size)
{
	if( file == NULL )
		return FALSE;

	if( fread(buffer, sizeof(UINT8), size, file) != (size_t)size )
		return FALSE;

	return TRUE;
}

/* write_to_file
 *
 * Writes n bytes to file
 *
 */

BOOL write_to_file(FILE* file, UINT8* buffer, UINT32 size)
{
	if( file == NULL )
		return FALSE;

	if( fwrite(buffer, sizeof(UINT8), size, file) != (size_t)size )
		return FALSE;

	return TRUE;
}