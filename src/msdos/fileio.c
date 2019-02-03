//============================================================
//
//  fileio.c - MSDOS file access functions
//
//============================================================

// MSDOS headers
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <dos.h>
#include <dirent.h>

// MAME headers
#include "driver.h"
#include "unzip.h"
#include "rc.h"

#ifdef MESS
#include "image.h"
#endif

#define VERBOSE				0

#define MAX_OPEN_FILES		16
#define FILE_BUFFER_SIZE	256

#define INVALID_HANDLE_VALUE ( -1 )

//============================================================
//  EXTERNALS
//============================================================

extern char *rompath_extra;

// from datafile.c
extern const char *history_filename;
extern const char *mameinfo_filename;

// from cheat.c
extern char *cheatfile;



//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef int HANDLE;
typedef int DWORD;

struct pathdata
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

static struct pathdata pathlist[FILETYPE_end];
static osd_file openfile[MAX_OPEN_FILES];




//============================================================
//  GLOBAL VARIABLES
//============================================================

int m_b_initialised = 0;

//============================================================
//  FILE PATH OPTIONS
//============================================================

struct rc_option fileio_opts[] =
{
	// name, shortname, type, dest, deflt, min, max, func, help
	{ "MSDOS path and directory options", NULL, rc_seperator, NULL, NULL, 0, 0, NULL, NULL },
#ifndef MESS
	{ "rompath", "rp", rc_string, &pathlist[FILETYPE_ROM].rawpath, "roms", 0, 0, NULL, "path to romsets" },
#else
	{ "biospath", "bp", rc_string, &pathlist[FILETYPE_ROM].rawpath, "bios", 0, 0, NULL, "path to BIOS sets" },
	{ "softwarepath", "swp", rc_string, &pathlist[FILETYPE_IMAGE].rawpath, "software", 0, 0, NULL, "path to software" },
	{ "hash_directory", "hash", rc_string, &pathlist[FILETYPE_HASH].rawpath, "hash", 0, 0, NULL, "path to hash files" },
#endif
	{ "samplepath", "sp", rc_string, &pathlist[FILETYPE_SAMPLE].rawpath, "samples", 0, 0, NULL, "path to samplesets" },
#ifdef MSDOS
	{ "inipath", NULL, rc_string, &pathlist[FILETYPE_INI].rawpath, ".;ini", 0, 0, NULL, "path to ini files" },
#else
	{ "inipath", NULL, rc_string, &pathlist[FILETYPE_INI].rawpath, "$HOME/.mame;.;ini", 0, 0, NULL, "path to ini files" },
#endif
	{ "cfg_directory", NULL, rc_string, &pathlist[FILETYPE_CONFIG].rawpath, "cfg", 0, 0, NULL, "directory to save configurations" },
	{ "nvram_directory", NULL, rc_string, &pathlist[FILETYPE_NVRAM].rawpath, "nvram", 0, 0, NULL, "directory to save nvram contents" },
	{ "memcard_directory", NULL, rc_string, &pathlist[FILETYPE_MEMCARD].rawpath, "memcard", 0, 0, NULL, "directory to save memory card contents" },
	{ "input_directory", NULL, rc_string, &pathlist[FILETYPE_INPUTLOG].rawpath, "inp", 0, 0, NULL, "directory to save input device logs" },
	{ "hiscore_directory", NULL, rc_string, &pathlist[FILETYPE_HIGHSCORE].rawpath, "hi", 0, 0, NULL, "directory to save hiscores" },
	{ "state_directory", NULL, rc_string, &pathlist[FILETYPE_STATE].rawpath, "sta", 0, 0, NULL, "directory to save states" },
	{ "artwork_directory", NULL, rc_string, &pathlist[FILETYPE_ARTWORK].rawpath, "artwork", 0, 0, NULL, "directory for Artwork (Overlays etc.)" },
	{ "snapshot_directory", NULL, rc_string, &pathlist[FILETYPE_SCREENSHOT].rawpath, "snap", 0, 0, NULL, "directory for screenshots (.png format)" },
	{ "diff_directory", NULL, rc_string, &pathlist[FILETYPE_IMAGE_DIFF].rawpath, "diff", 0, 0, NULL, "directory for hard drive image difference files" },
	{ "ctrlr_directory", NULL, rc_string, &pathlist[FILETYPE_CTRLR].rawpath, "ctrlr", 0, 0, NULL, "directory to save controller definitions" },
	{ "cheat_file", NULL, rc_string, &cheatfile, "cheat.dat", 0, 0, NULL, "cheat filename" },
#ifdef MESS
	{ "sysinfo_file", NULL, rc_string, &history_filename, "sysinfo.dat", 0, 0, NULL, NULL },
	{ "messinfo_file", NULL, rc_string, &mameinfo_filename, "messinfo.dat", 0, 0, NULL, NULL },
#else
	{ "history_file", NULL, rc_string, &history_filename, "history.dat", 0, 0, NULL, NULL },
	{ "mameinfo_file", NULL, rc_string, &mameinfo_filename, "mameinfo.dat", 0, 0, NULL, NULL },
#endif

#ifdef MMSND
	{ "MMSND directory options", NULL, rc_seperator, NULL, NULL, 0, 0, NULL, NULL },
	{ "waveout", NULL, rc_string, &wavebasename, "waveout", 0, 0, NULL, "wave out path" },
#endif

	{ NULL,	NULL, rc_end, NULL, NULL, 0, 0,	NULL, NULL }
};



//============================================================
//  is_pathsep
//============================================================

INLINE int is_pathsep(char c)
{
	return (c == '/' || c == '\\' || c == ':');
}



//============================================================
//  find_reverse_path_sep
//============================================================

static char *find_reverse_path_sep(char *name)
{
	char *p = name + strlen(name) - 1;
	while (p >= name && !is_pathsep(*p))
		p--;
	return (p >= name) ? p : NULL;
}



//============================================================
//  create_path
//============================================================

static void create_path(char *path, int has_filename)
{
	char *sep = find_reverse_path_sep(path);
	struct stat s;

	/* if there's still a separator, and it's not the root, nuke it and recurse */
	if (sep && sep > path && !is_pathsep(sep[-1]))
	{
		*sep = 0;
		create_path(path, 0);
		*sep = '\\';
	}

	/* if we have a filename, we're done */
	if (has_filename)
		return;

	/* if the path already exists, we're done */
	if( stat( path, &s ) == 0 )
		return;

	/* create the path */
	mkdir( path, S_IWUSR );
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
	if (!result)
		goto out_of_memory;

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

out_of_memory:
	fatalerror("Out of memory in variable expansion!\n");
}



//============================================================
//  expand_pathlist
//============================================================

static void expand_pathlist(struct pathdata *list)
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
		if (!list->path)
			goto out_of_memory;

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
	return;

out_of_memory:
	fatalerror("Out of memory!\n");
}



//============================================================
//  get_path_for_filetype
//============================================================

static const char *get_path_for_filetype(int filetype, int pathindex, DWORD *count)
{
	struct pathdata *list;

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

static void compose_path(char *output, int pathtype, int pathindex, const char *filename)
{
	const char *basepath = get_path_for_filetype(pathtype, pathindex, NULL);
	char *p;

#ifdef MESS
	if (osd_is_absolute_path(filename))
		basepath = NULL;
#endif

	/* compose the full path */
	*output = 0;
	if (basepath)
		strcat(output, basepath);
	if (*output && !is_pathsep(output[strlen(output) - 1]))
		strcat(output, "\\");
	strcat(output, filename);

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

	switch( errno )
	{
		case 0:
			error = FILEERR_SUCCESS;
			break;

		case ENOMEM:
			error = FILEERR_OUT_OF_MEMORY;
			break;

		case ENOENT:
			error = FILEERR_NOT_FOUND;
			break;

		case EACCES:
			error = FILEERR_ACCESS_DENIED;
			break;

		case EBUSY:
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
	char fullpath[1024];
	struct stat s;

	/* compose the full path */
	compose_path(fullpath, pathtype, pathindex, filename);

	/* get the file attributes */
	if( stat( fullpath, &s ) != 0 )
		return PATH_NOT_FOUND;
	else if( S_ISDIR( s.st_mode ) )
		return PATH_IS_DIRECTORY;
	else
		return PATH_IS_FILE;
}



//============================================================
//  osd_fopen
//============================================================

osd_file *osd_fopen(int pathtype, int pathindex, const char *filename, const char *mode, osd_file_error *error)
{
	DWORD disposition = 0, attrib = 0;
	char fullpath[1024];
	osd_file *file;
	off_t size;
	int i;

	/* initialise the file table the first time */
	if( !m_b_initialised )
	{
		for (i = 0; i < MAX_OPEN_FILES; i++)
		{
			openfile[i].handle = INVALID_HANDLE_VALUE;
		}
		m_b_initialised = 1;
	}
	/* find an empty file handle */
	for (i = 0; i < MAX_OPEN_FILES; i++)
	{
		if ( openfile[i].handle == INVALID_HANDLE_VALUE )
		{
			break;
		}
	}
	if (i == MAX_OPEN_FILES)
	{
		goto error;
	}

	/* zap the file record */
	file = &openfile[i];
	memset(file, 0, sizeof(*file));

	/* convert the mode into disposition and attrib */
	if (strchr(mode, 'r'))
		disposition = 0, attrib = O_RDONLY;
	if (strchr(mode, 'w'))
		disposition = O_CREAT | O_TRUNC, attrib = O_WRONLY;
	if (strchr(mode, '+'))
		attrib = O_RDWR;

	/* compose the full path */
	compose_path(fullpath, pathtype, pathindex, filename);

	/* attempt to open the file */
	file->handle = open( fullpath, attrib | disposition | O_BINARY, S_IRUSR | S_IWUSR  );
	if( file->handle == INVALID_HANDLE_VALUE )
	{
		/* if it's read-only, or if the path exists, then that's final */
		if( ( disposition & O_CREAT ) == 0 || errno != ENOENT )
			goto error;

		/* create the path and try again */
		create_path(fullpath, 1);
		file->handle = open( fullpath, attrib | disposition | O_BINARY, S_IRUSR | S_IWUSR  );

		/* if that doesn't work, we give up */
		if( file->handle == INVALID_HANDLE_VALUE )
			goto error;
	}
	/* get the file size */
	size = lseek( file->handle, 0, SEEK_END );
	if( size == -1 )
	{
		osd_fclose( file );
		goto error;
	}
	file->end = size;
	file->filepos = ~0;
	*error = FILEERR_SUCCESS;
	return file;

error:
	*error = get_last_fileerror();
//	if (temp_file[0])
//		DeleteFile(temp_file);
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
		if( file->offset > INT_MAX || lseek( file->handle, (offs_t)file->offset, SEEK_SET ) == -1 )
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
		file->bufferbytes = read( file->handle, file->buffer, FILE_BUFFER_SIZE );
		if( file->bufferbytes < 0 )
		{
			file->bufferbytes = 0;
		}
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
		result = read( file->handle, buffer, bytes_left );
		if( result < 0 )
		{
			result = 0;
		}
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
	DWORD result;

	// invalidate any buffered data
	file->bufferbytes = 0;

	// attempt to seek to the current location
	if( file->offset > INT_MAX || lseek( file->handle, (offs_t)file->offset, SEEK_SET ) == -1 )
	{
		return 0;
	}

	// do the write
	result = write( file->handle, buffer, length );
	if( result < 0 )
	{
		result = 0;
	}
	file->filepos += result;

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
	if( file->handle != INVALID_HANDLE_VALUE )
	{
		close( file->handle );
		file->handle = INVALID_HANDLE_VALUE;
	}
}

//============================================================
//  osd_create_directory
//============================================================

int osd_create_directory(int pathtype, int pathindex, const char *s_dirname)
{
	char fullpath[1024];

	/* compose the full path */
	compose_path(fullpath, pathtype, pathindex, s_dirname);

	return mkdir( fullpath, S_IWUSR );
}

#ifdef MESS

//============================================================
//  osd_dirname
//============================================================

char *osd_dirname(const char *filename)
{
	char *s_dirname;
	char *c;

	// NULL begets NULL
	if (!filename)
		return NULL;

	// allocate space for it
	s_dirname = malloc(strlen(filename) + 1);
	if (!s_dirname)
	{
		logerror("error: malloc failed in osd_dirname\n");
		return NULL;
	}

	// copy in the name
	strcpy(s_dirname, filename);

	// search backward for a slash or a colon
	for (c = s_dirname + strlen(s_dirname) - 1; c >= s_dirname; c--)
		if (*c == '\\' || *c == '/' || *c == ':')
		{
			// found it: NULL terminate and return
			*(c + 1) = 0;
			return s_dirname;
		}

	// otherwise, return an empty string
	s_dirname[0] = 0;
	return s_dirname;
}

//============================================================
//  osd_basename
//============================================================

char *osd_basename(char *filename)
{
	char *c;

	// NULL begets NULL
	if (!filename)
		return NULL;

	// start at the end and return when we hit a slash or colon
	for (c = filename + strlen(filename) - 1; c >= filename; c--)
		if (*c == '\\' || *c == '/' || *c == ':')
			return c + 1;

	// otherwise, return the whole thing
	return filename;
}

//============================================================
//  osd_path_separator
//============================================================

const char *osd_path_separator(void)
{
	return "\\";
}

//============================================================
//  osd_is_path_separator
//============================================================

int osd_is_path_separator(char ch)
{
	return (ch == '\\') || (ch == '/');
}

//============================================================
//  osd_is_absolute_path
//============================================================
int osd_is_absolute_path(const char *path)
{
	int result;

	if (osd_is_path_separator(path[0]))
		result = 1;
#ifndef UNDER_CE
	else if (isalpha(path[0]))
		result = (path[1] == ':');
#endif
	else
		result = 0;
	return result;
}

static int num_devices = 0;
static char dos_devices[32*4];
static char dos_device[2];
static char dos_filemask[260];

static int fnmatch(const char *f1, const char *f2)
{
	while (*f1 && *f2)
	{
		if (*f1 == '*')
		{
			/* asterisk is not the last character? */
			if (f1[1])
			{
				/* skip until first occurance of the character after the asterisk */
				while (*f2 && toupper(f1[1]) != toupper(*f2))
					f2++;
				/* skip repetitions of the character after the asterisk */
				while (*f2 && toupper(f1[1]) == toupper(f2[1]))
					f2++;
			}
			else
			{
				/* skip until end of string */
				while (*f2)
					f2++;
			}
		}
		else
		if (*f1 == '?')
		{
			/* skip one character */
			f2++;
		}
		else
		{
			/* mismatch? */
			if (toupper(*f1) != toupper(*f2))
				return 0;
			/* skip one character */
			f2++;
		}
		/* skip mask */
		f1++;
	}
	/* no match if anything is left */
	if (*f1 || *f2)
		return 0;
	return 1;
}

int osd_num_devices(void)
{
	if (num_devices == 0)
	{
		union REGS r;
		int dev, previous_dev;
		r.h.ah = 0x19;	/* get current drive */
		intdos(&r,&r);
		previous_dev = r.h.al;	/* save current drive */
		for (dev = 0; dev < 26; dev++)
		{
			r.h.ah = 0x0e;		/* select drive */
			r.h.dl = dev;		/* DL */
			intdos(&r,&r);
			r.h.ah = 0x19;		/* get current drive */
			intdos(&r,&r);
			if (r.h.al == dev)	/* successful? */
			{
				dos_devices[num_devices*4+0] = 'A' + dev;
				dos_devices[num_devices*4+1] = ':';
				dos_devices[num_devices*4+2] = '\\';
				dos_devices[num_devices*4+3] = '\0';
				num_devices++;
			}
		}
		r.h.ah = 0x0e;		/* select previous drive again */
		r.h.dl = previous_dev;
		intdos(&r,&r);
	}
	return num_devices;
}

const char *osd_get_device_name(int idx)
{
	if (idx < num_devices)
		return &dos_devices[idx*4];
	return "";
}

void osd_change_device(const char *device)
{
	char chdir_device[4];

	dos_device[0] = device[0];
	dos_device[1] = '\0';

	chdir_device[0] = device[0];
	chdir_device[1] = ':';
	chdir_device[2] = '\\';
	chdir_device[3] = '\0';

	chdir(chdir_device);
}

static char startup_dir[260]; /* Max Windows Path? */


/* Go back to the startup dir on exit */
void return_to_startup_dir(void)
{
	chdir(startup_dir);
}

void osd_change_directory(const char *directory)
{
	if (!startup_dir[0])
	{
		getcwd(startup_dir,sizeof(startup_dir));
		atexit(return_to_startup_dir);
	}

	chdir(directory);
}

static char dos_cwd[260];

const char *osd_get_cwd(void)
{
	char *s_cwd;

	getcwd(dos_cwd, 260);

	s_cwd = dos_cwd;
	while( *( s_cwd ) != 0 )
	{
		if( *( s_cwd ) == '/' )
		{
			*( s_cwd ) = '\\';
		}
		s_cwd++;
	}

	if( s_cwd != dos_cwd && *( s_cwd - 1 ) != '\\' )
	{
		*( s_cwd++ ) = '\\';
		*( s_cwd++ ) = 0;
	}

	return dos_cwd;
}

void *osd_dir_open(const char *mess_dirname, const char *filemask)
{
	DIR *dir;

	strcpy(dos_filemask, filemask);

	dir = opendir(".");

	return dir;
}

int osd_dir_get_entry(void *dir, char *name, int namelength, int *is_dir)
{
	int len;
	struct dirent *d;

	name[0] = '\0';
	*is_dir = 0;

	if (!dir)
		return 0;

	d = readdir(dir);
	while (d)
	{
		struct stat st;

		strncpy(name, d->d_name, namelength-1);
		name[namelength-1]='\0';

		len = strlen(name);

		if( stat(d->d_name, &st) == 0 )
			*is_dir = S_ISDIR(st.st_mode);

		if (*is_dir)
			return len;
		else
		if (fnmatch(dos_filemask, d->d_name))
			return len;
		else
		{
			/* no match, zap the name and type again */
			name[0] = '\0';
			*is_dir = 0;
		}
		d = readdir(dir);
	}
	return 0;
}

void osd_dir_close(void *dir)
{
	if (dir)
		closedir(dir);
}

#endif


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
	struct pathdata *list = &pathlist[file_type];

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
