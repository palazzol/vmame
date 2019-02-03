//============================================================
//
//  fileio.c - Win32 file access functions
//
//  Copyright (c) 1996-2006, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//============================================================

// standard windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctype.h>
#include <tchar.h>

// MAME headers
#include "osdepend.h"
#include "driver.h"
#include "unzip.h"
#include "rc.h"

#ifdef MESS
#include "image.h"
#endif

/* Quick fix to allow compilation with win32api 2.4 */
#undef INVALID_FILE_ATTRIBUTES
#undef INVALID_SET_FILE_POINTER

/* Older versions of Platform SDK don't define these */
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

#define VERBOSE				0

#define MAX_OPEN_FILES		16
#define FILE_BUFFER_SIZE	256

#ifdef UNICODE
#define appendstring(dest,src)	wsprintf((dest) + wcslen(dest), TEXT("%S"), (src))
#else
#define appendstring(dest,src)	strcat((dest), (src))
#endif // UNICODE


//============================================================
//  EXTERNALS
//============================================================

extern char *rompath_extra;

// from cheat.c
extern char *cheatfile;



//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct _pathdata pathdata;

struct _pathdata
{
	const char *rawpath;
	const char **path;
	int pathcount;
};

struct _osd_file
{
	HANDLE		handle;
	UINT64		filepos;
	UINT64		end;
	UINT64		offset;
	UINT64		bufferbase;
	DWORD		bufferbytes;
	UINT8		buffer[FILE_BUFFER_SIZE];
};

static pathdata pathlist[FILETYPE_end];
static osd_file openfile[MAX_OPEN_FILES];



//============================================================
//  FILE PATH OPTIONS
//============================================================

struct rc_option fileio_opts[] =
{
	// name, shortname, type, dest, deflt, min, max, func, help
	{ "Windows path and directory options", NULL, rc_seperator, NULL, NULL, 0, 0, NULL, NULL },
#ifndef MESS
	{ "rompath", "rp", rc_string, (char *)&pathlist[FILETYPE_ROM].rawpath, "roms", 0, 0, NULL, "path to romsets" },
#else
	{ "biospath", "bp", rc_string, (char *)&pathlist[FILETYPE_ROM].rawpath, "bios", 0, 0, NULL, "path to BIOS sets" },
	{ "softwarepath", "swp", rc_string, (char *)&pathlist[FILETYPE_IMAGE].rawpath, "software", 0, 0, NULL, "path to software" },
	{ "hash_directory", "hash", rc_string, (char *)&pathlist[FILETYPE_HASH].rawpath, "hash", 0, 0, NULL, "path to hash files" },
#endif
	{ "samplepath", "sp", rc_string, (char *)&pathlist[FILETYPE_SAMPLE].rawpath, "samples", 0, 0, NULL, "path to samplesets" },
#ifdef __WIN32__
	{ "inipath", NULL, rc_string, (char *)&pathlist[FILETYPE_INI].rawpath, ".;ini", 0, 0, NULL, "path to ini files" },
#else
	{ "inipath", NULL, rc_string, (char *)&pathlist[FILETYPE_INI].rawpath, "$HOME/.mame;.;ini", 0, 0, NULL, "path to ini files" },
#endif
	{ "cfg_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_CONFIG].rawpath, "cfg", 0, 0, NULL, "directory to save configurations" },
	{ "nvram_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_NVRAM].rawpath, "nvram", 0, 0, NULL, "directory to save nvram contents" },
	{ "memcard_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_MEMCARD].rawpath, "memcard", 0, 0, NULL, "directory to save memory card contents" },
	{ "input_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_INPUTLOG].rawpath, "inp", 0, 0, NULL, "directory to save input device logs" },
	{ "hiscore_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_HIGHSCORE].rawpath, "hi", 0, 0, NULL, "directory to save hiscores" },
	{ "state_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_STATE].rawpath, "sta", 0, 0, NULL, "directory to save states" },
	{ "artwork_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_ARTWORK].rawpath, "artwork", 0, 0, NULL, "directory for Artwork (Overlays etc.)" },
	{ "snapshot_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_SCREENSHOT].rawpath, "snap", 0, 0, NULL, "directory for screenshots (.png format)" },
	{ "diff_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_IMAGE_DIFF].rawpath, "diff", 0, 0, NULL, "directory for hard drive image difference files" },
	{ "ctrlr_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_CTRLR].rawpath, "ctrlr", 0, 0, NULL, "directory to save controller definitions" },
	{ "comment_directory", NULL, rc_string, (char *)&pathlist[FILETYPE_COMMENT].rawpath, "comments", 0, 0, NULL, "directory to save comment files" },
	{ "cheat_file", NULL, rc_string, &cheatfile, "cheat.dat", 0, 0, NULL, "cheat filename" },
	{ NULL,	NULL, rc_end, NULL, NULL, 0, 0,	NULL, NULL }
};



//============================================================
//  is_pathsep
//============================================================

INLINE int is_pathsep(TCHAR c)
{
	return (c == '/' || c == '\\' || c == ':');
}



//============================================================
//  find_reverse_path_sep
//============================================================

static TCHAR *find_reverse_path_sep(TCHAR *name)
{
	TCHAR *p = name + _tcslen(name) - 1;
	while (p >= name && !is_pathsep(*p))
		p--;
	return (p >= name) ? p : NULL;
}



//============================================================
//  create_path
//============================================================

static int create_path(TCHAR *path, int has_filename)
{
	TCHAR *sep = find_reverse_path_sep(path);
	DWORD attributes;

	/* if there's still a separator, and it's not the root, nuke it and recurse */
	if (sep && sep > path && !is_pathsep(sep[-1]))
	{
		*sep = 0;
		if (!create_path(path, FALSE))
			return 0;
		*sep = '\\';
	}

	/* if we have a filename, we're done */
	if (has_filename)
		return 1;

	/* if the path already exists, we're done */
	attributes = GetFileAttributes(path);
	if (attributes != INVALID_FILE_ATTRIBUTES)
		return 1;

	/* create the path */
	return CreateDirectory(path, NULL);
}



//============================================================
//  is_variablechar
//============================================================

INLINE int is_variablechar(char c)
{
	return (isalnum(c) || c == '_' || c == '-');
}



//============================================================
//  parse_variable
//============================================================

static const char *parse_variable(const char **start, const char *end)
{
	const char *src = *start, *var;
	char variable[1024];
	char *dest = variable;

	/* copy until we hit the end or until we hit a non-variable character */
	for (src = *start; src < end && is_variablechar(*src); src++)
		*dest++ = *src;

	// an empty variable means "$" and should not be expanded
	if(src == *start)
		return("$");

	/* NULL terminate and return a pointer to the end */
	*dest = 0;
	*start = src;

	/* return the actual variable value */
	var = getenv(variable);
	return (var) ? var : "";
}



//============================================================
//  copy_and_expand_variables
//============================================================

static char *copy_and_expand_variables(const char *path, int len)
{
	char *dst, *result;
	const char *src;
	int length = 0;

	/* first determine the length of the expanded string */
	for (src = path; src < path + len; )
		if (*src++ == '$')
			length += strlen(parse_variable(&src, path + len));
		else
			length++;

	/* allocate a string of the appropriate length */
	result = malloc(length + 1);
	assert_always(result != NULL, "Out of memory in variable expansion!");

	/* now actually generate the string */
	for (src = path, dst = result; src < path + len; )
	{
		char c = *src++;
		if (c == '$')
			dst += sprintf(dst, "%s", parse_variable(&src, path + len));
		else
			*dst++ = c;
	}

	/* NULL terminate and return */
	*dst = 0;
	return result;
}



//============================================================
//  expand_pathlist
//============================================================

static void expand_pathlist(pathdata *list)
{
	const char *rawpath = (list->rawpath) ? list->rawpath : "";
	const char *token;

#if VERBOSE
	printf("Expanding: %s\n", rawpath);
#endif

	// free any existing paths
	if (list->pathcount != 0)
	{
		int pathindex;

		for (pathindex = 0; pathindex < list->pathcount; pathindex++)
			free((void *)list->path[pathindex]);
		free((void *)list->path);
	}

	// by default, start with an empty list
	list->path = NULL;
	list->pathcount = 0;

	// look for separators
	token = strchr(rawpath, ';');
	if (!token)
		token = rawpath + strlen(rawpath);

	// loop until done
	while (1)
	{
		// allocate space for the new pointer
		list->path = realloc((void *)list->path, (list->pathcount + 1) * sizeof(char *));
		assert_always(list->path != NULL, "Out of memory!");

		// copy the path in
		list->path[list->pathcount++] = copy_and_expand_variables(rawpath, token - rawpath);
#if VERBOSE
		printf("  %s\n", list->path[list->pathcount - 1]);
#endif

		// if this was the end, break
		if (*token == 0)
			break;
		rawpath = token + 1;

		// find the next separator
		token = strchr(rawpath, ';');
		if (!token)
			token = rawpath + strlen(rawpath);
	}

	// when finished, reset the path info, so that future INI parsing will
	// cause us to get called again
	free((void *)list->rawpath);
	list->rawpath = NULL;
}



//============================================================
//  get_path_for_filetype
//============================================================

void free_pathlists(void)
{
	int i;

	for (i = 0;i < FILETYPE_end;i++)
	{
		pathdata *list = &pathlist[i];

		// free any existing paths
		if (list->pathcount != 0)
		{
			int pathindex;

			for (pathindex = 0; pathindex < list->pathcount; pathindex++)
				free((void *)list->path[pathindex]);
			free((void *)list->path);
		}

		// by default, start with an empty list
		list->path = NULL;
		list->pathcount = 0;
	}
}



//============================================================
//  get_path_for_filetype
//============================================================

static const char *get_path_for_filetype(int filetype, int pathindex, DWORD *count)
{
	pathdata *list;

	// handle aliasing of some paths
	switch (filetype)
	{
#ifndef MESS
		case FILETYPE_IMAGE:
			list = &pathlist[FILETYPE_ROM];
			break;
#endif

		default:
			list = &pathlist[filetype];
			break;
	}

	// if we don't have expanded paths, expand them now
	if (list->pathcount == 0 || list->rawpath)
	{
		// special hack for ROMs
		if (list == &pathlist[FILETYPE_ROM] && rompath_extra)
		{
			// this may leak a little memory, but it's a hack anyway! :-P
			const char *rawpath = (list->rawpath) ? list->rawpath : "";
			char *newpath = malloc(strlen(rompath_extra) + strlen(rawpath) + 2);
			sprintf(newpath, "%s;%s", rompath_extra, rawpath);
			free((void *)list->rawpath);
			list->rawpath = newpath;
		}

		// decompose the path
		expand_pathlist(list);
	}

	// set the count
	if (count)
		*count = list->pathcount;

	// return a valid path always
	return (pathindex < list->pathcount) ? list->path[pathindex] : "";
}



//============================================================
//  compose_path
//============================================================

static void compose_path(TCHAR *output, int pathtype, int pathindex, const char *filename)
{
	const char *basepath = get_path_for_filetype(pathtype, pathindex, NULL);
	TCHAR *p;

#ifdef MESS
	if (osd_is_absolute_path(filename))
		basepath = NULL;
#endif

	/* compose the full path */
	*output = 0;
	if (basepath)
		appendstring(output, basepath);
	if (*output && !is_pathsep(output[_tcslen(output) - 1]))
		appendstring(output, "\\");
	appendstring(output, filename);

	/* convert forward slashes to backslashes */
	for (p = output; *p; p++)
		if (*p == '/')
			*p = '\\';
}



//============================================================
//  get_last_fileerror
//============================================================

static osd_file_error get_last_fileerror(void)
{
	osd_file_error error;

	switch(GetLastError())
	{
		case ERROR_SUCCESS:
			error = FILEERR_SUCCESS;
			break;

		case ERROR_OUTOFMEMORY:
			error = FILEERR_OUT_OF_MEMORY;
			break;

		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			error = FILEERR_NOT_FOUND;
			break;

		case ERROR_ACCESS_DENIED:
			error = FILEERR_ACCESS_DENIED;
			break;

		case ERROR_SHARING_VIOLATION:
			error = FILEERR_ALREADY_OPEN;
			break;

		default:
			error = FILEERR_FAILURE;
			break;
	}
	return error;
}



//============================================================
//  osd_get_path_count
//============================================================

int osd_get_path_count(int pathtype)
{
	DWORD count;

	/* get the count and return it */
	get_path_for_filetype(pathtype, 0, &count);
	return count;
}



//============================================================
//  osd_get_path_info
//============================================================

int osd_get_path_info(int pathtype, int pathindex, const char *filename)
{
	TCHAR fullpath[1024];
	DWORD attributes;

	/* compose the full path */
	compose_path(fullpath, pathtype, pathindex, filename);

	/* get the file attributes */
	attributes = GetFileAttributes(fullpath);
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return PATH_NOT_FOUND;
	else if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		return PATH_IS_DIRECTORY;
	else
		return PATH_IS_FILE;
}



//============================================================
//  osd_fopen
//============================================================

osd_file *osd_fopen(int pathtype, int pathindex, const char *filename, const char *mode, osd_file_error *error)
{
	osd_file_error err = 0;
	DWORD disposition = 0, access = 0, sharemode = 0, flags = 0;
	TCHAR fullpath[1024];
	DWORD upperPos = 0;
	osd_file *file;
	int i;
	const TCHAR *s;
	TCHAR temp_dir[1024];
	TCHAR temp_file[MAX_PATH];

	temp_file[0] = '\0';

	/* find an empty file handle */
	for (i = 0; i < MAX_OPEN_FILES; i++)
		if (openfile[i].handle == NULL || openfile[i].handle == INVALID_HANDLE_VALUE)
			break;
	if (i == MAX_OPEN_FILES)
	{
		err = FILEERR_TOO_MANY_FILES;
		goto error;
	}

	/* zap the file record */
	file = &openfile[i];
	memset(file, 0, sizeof(*file));

	/* convert the mode into disposition and access */
	if (strchr(mode, 'r'))
		disposition = OPEN_EXISTING, access = GENERIC_READ, sharemode = FILE_SHARE_READ;
	if (strchr(mode, 'w'))
		disposition = CREATE_ALWAYS, access = GENERIC_WRITE, sharemode = 0;
	if (strchr(mode, '+'))
		access = GENERIC_READ | GENERIC_WRITE;

	/* compose the full path */
	compose_path(fullpath, pathtype, pathindex, filename);

	/* if 'g' is specified, we 'ghost' our changes; in other words any changes
     * made to the file last only as long as the file is open.  Under the hood
     * this is implemented by using a temporary file */
	if (strchr(mode, 'g'))
	{
		GetTempPath(sizeof(temp_dir) / sizeof(temp_dir[0]), temp_dir);
		GetTempFileName(temp_dir, TEXT("tmp"), 0, temp_file);

		if (!CopyFile(fullpath, temp_file, FALSE))
			goto error;

		s = temp_file;
		flags |= FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;
	}
	else
	{
		s = fullpath;
	}

	/* attempt to open the file */
	file->handle = CreateFile(s, access, sharemode, NULL, disposition, 0, NULL);
	if (file->handle == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();

		/* if it's read-only, or if the path exists, then that's final */
		if (!(access & GENERIC_WRITE) || error != ERROR_PATH_NOT_FOUND)
			goto error;

		/* create the path and try again */
		create_path(fullpath, TRUE);
		file->handle = CreateFile(fullpath, access, sharemode, NULL, disposition, flags, NULL);

		/* if that doesn't work, we give up */
		if (file->handle == INVALID_HANDLE_VALUE)
			goto error;
	}

	/* get the file size */
	file->end = GetFileSize(file->handle, &upperPos);
	file->end |= (UINT64)upperPos << 32;
	*error = FILEERR_SUCCESS;
	return file;

error:
	if (!err)
		err = get_last_fileerror();
	*error = err;
	if (temp_file[0])
		DeleteFile(temp_file);
	return NULL;
}


//============================================================
//  osd_fseek
//============================================================

int osd_fseek(osd_file *file, INT64 offset, int whence)
{
	/* convert the whence into method */
	switch (whence)
	{
		default:
		case SEEK_SET:	file->offset = offset;				break;
		case SEEK_CUR:	file->offset += offset;				break;
		case SEEK_END:	file->offset = file->end + offset;	break;
	}
	return 0;
}



//============================================================
//  osd_ftell
//============================================================

UINT64 osd_ftell(osd_file *file)
{
	return file->offset;
}



//============================================================
//  osd_feof
//============================================================

int osd_feof(osd_file *file)
{
	return (file->offset >= file->end);
}



//============================================================
//  osd_fread
//============================================================

UINT32 osd_fread(osd_file *file, void *buffer, UINT32 length)
{
	UINT32 bytes_left = length;
	int bytes_to_copy;
	DWORD result;

	// handle data from within the buffer
	if (file->offset >= file->bufferbase && file->offset < file->bufferbase + file->bufferbytes)
	{
		// copy as much as we can
		bytes_to_copy = file->bufferbase + file->bufferbytes - file->offset;
		if (bytes_to_copy > length)
			bytes_to_copy = length;
		memcpy(buffer, &file->buffer[file->offset - file->bufferbase], bytes_to_copy);

		// account for it
		bytes_left -= bytes_to_copy;
		file->offset += bytes_to_copy;
		buffer = (UINT8 *)buffer + bytes_to_copy;

		// if that's it, we're done
		if (bytes_left == 0)
			return length;
	}

	// attempt to seek to the current location if we're not there already
	if (file->offset != file->filepos)
	{
		LONG upperPos = file->offset >> 32;
		result = SetFilePointer(file->handle, (UINT32)file->offset, &upperPos, FILE_BEGIN);
		if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		{
			file->filepos = ~0;
			return length - bytes_left;
		}
		file->filepos = file->offset;
	}

	// if we have a small read remaining, do it to the buffer and copy out the results
	if (length < FILE_BUFFER_SIZE/2)
	{
		// read as much of the buffer as we can
		file->bufferbase = file->offset;
		file->bufferbytes = 0;
		ReadFile(file->handle, file->buffer, FILE_BUFFER_SIZE, &file->bufferbytes, NULL);
		file->filepos += file->bufferbytes;

		// copy it out
		bytes_to_copy = bytes_left;
		if (bytes_to_copy > file->bufferbytes)
			bytes_to_copy = file->bufferbytes;
		memcpy(buffer, file->buffer, bytes_to_copy);

		// adjust pointers and return
		file->offset += bytes_to_copy;
		bytes_left -= bytes_to_copy;
		return length - bytes_left;
	}

	// otherwise, just read directly to the buffer
	else
	{
		// do the read
		ReadFile(file->handle, buffer, bytes_left, &result, NULL);
		file->filepos += result;

		// adjust the pointers and return
		file->offset += result;
		bytes_left -= result;
		return length - bytes_left;
	}
}



//============================================================
//  osd_fwrite
//============================================================

UINT32 osd_fwrite(osd_file *file, const void *buffer, UINT32 length)
{
	LONG upperPos;
	DWORD result;

	// invalidate any buffered data
	file->bufferbytes = 0;

	// attempt to seek to the current location
	upperPos = file->offset >> 32;
	result = SetFilePointer(file->handle, (UINT32)file->offset, &upperPos, FILE_BEGIN);
	if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		return 0;

    // do the write
    WriteFile(file->handle, buffer, length, &result, NULL);
    file->filepos = file->offset + result;

	// adjust the pointers
	file->offset += result;
	if (file->offset > file->end)
		file->end = file->offset;
	return result;
}



//============================================================
//  osd_fclose
//============================================================

void osd_fclose(osd_file *file)
{
	// close the handle and clear it out
	if (file->handle)
		CloseHandle(file->handle);
	file->handle = NULL;
}



//============================================================
//  osd_create_directory
//============================================================

int osd_create_directory(int pathtype, int pathindex, const char *dirname)
{
	TCHAR fullpath[1024];

	/* compose the full path */
	compose_path(fullpath, pathtype, pathindex, dirname);
	return create_path(fullpath, FALSE);
}



//============================================================
//  osd_display_loading_rom_message
//============================================================

// called while loading ROMs. It is called a last time with name == 0 to signal
// that the ROM loading process is finished.
// return non-zero to abort loading
#ifndef WINUI
int osd_display_loading_rom_message(const char *name,rom_load_data *romdata)
{
	if (name)
		fprintf(stdout, "loading %-32s\r", name);
	else
		fprintf(stdout, "                                        \r");
	fflush(stdout);

	return 0;
}

#endif


#ifdef WINUI
//============================================================
//  set_pathlist
//============================================================

void set_pathlist(int file_type, const char *new_rawpath)
{
	pathdata *list = &pathlist[file_type];

	// free any existing paths
	if (list->pathcount != 0)
	{
		int pathindex;

		for (pathindex = 0; pathindex < list->pathcount; pathindex++)
			free((void *)list->path[pathindex]);
		free(list->path);
	}

	// by default, start with an empty list
	list->path = NULL;
	list->pathcount = 0;

	free((void *)list->rawpath);
	list->rawpath = new_rawpath;

}
#endif
