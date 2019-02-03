/***************************************************************************

    osd_tool.c

    OS-dependent code interface for tools

    Copyright (c) 1996-2006, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#include <stdio.h>

#include "osd_tool.h"

/*-------------------------------------------------
    is_physical_drive - clue to Win32 code that
    we're reading a physical drive directly
-------------------------------------------------*/

int osd_is_physical_drive(const char *file)
{
	return 0;
}



/*-------------------------------------------------
    osd_get_physical_drive_geometry - retrieves
    geometry for physical drives
-------------------------------------------------*/

int osd_get_physical_drive_geometry(const char *filename, UINT32 *cylinders, UINT32 *heads, UINT32 *sectors, UINT32 *bps)
{
	return 0;
}



/*-------------------------------------------------
    osd_get_file_size - returns the 64-bit file size
    for a file
-------------------------------------------------*/

UINT64 osd_get_file_size(const char *file)
{
	UINT64 filesize;
	FILE *f = fopen( file, "rb" );
	fseek( f, 0, SEEK_END );
	filesize = ftell( f );
	fclose( f );
	return filesize;
}



osd_tool_file *osd_tool_fopen(const char *filename, const char *mode)
{
	return (osd_tool_file *) fopen( filename, mode );
}



void osd_tool_fclose(osd_tool_file *file)
{
	fclose( (FILE *)file );
}



UINT32 osd_tool_fread(osd_tool_file *file, UINT64 offset, UINT32 count, void *buffer)
{
	fseek( (FILE *) file, offset, SEEK_SET );
	return fread( buffer, 1, count, (FILE *) file );
}



UINT32 osd_tool_fwrite(osd_tool_file *file, UINT64 offset, UINT32 count, const void *buffer)
{
	fseek( (FILE*) file, offset, SEEK_SET );
	return fwrite( buffer, 1, count, (FILE *) file );
}



UINT64 osd_tool_flength(osd_tool_file *file)
{
	fseek( (FILE *) file, 0, SEEK_END );
	return ftell( (FILE *) file );
}


