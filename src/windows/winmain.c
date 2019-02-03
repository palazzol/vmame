//============================================================
//
//  winmain.c - Win32 main program
//
//  Copyright (c) 1996-2006, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//============================================================

// standard windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>
#include <mmsystem.h>
#include <shellapi.h>

// standard includes
#include <time.h>
#include <ctype.h>

// MAME headers
#include "driver.h"
#include "window.h"
#include "input.h"
#include "config.h"
#include "video.h"


#define ENABLE_PROFILER		0


//============================================================
//  TYPE DEFINITIONS
//============================================================

#define MAX_SYMBOLS		65536

typedef struct _map_entry map_entry;

struct _map_entry
{
	UINT32 start;
	UINT32 end;
	UINT32 hits;
	char *name;
};



//============================================================
//  GLOBAL VARIABLES
//============================================================

int verbose;

// this line prevents globbing on the command line
int _CRT_glob = 0;



//============================================================
//  LOCAL VARIABLES
//============================================================

static char mapfile_name[MAX_PATH];
static LPTOP_LEVEL_EXCEPTION_FILTER pass_thru_filter;

#if ENABLE_PROFILER
static map_entry symbol_map[MAX_SYMBOLS];
static int map_entries;
static HANDLE profiler_thread;
static DWORD profiler_thread_id;
static volatile UINT8 profiler_thread_exit;
#endif

#ifndef MESS
static const char helpfile[] = "docs\\windows.txt";
#else
static const char helpfile[] = "mess.chm";
#endif



//============================================================
//  PROTOTYPES
//============================================================

static int check_for_double_click_start(int argc);
static void osd_exit(void);
static LONG CALLBACK exception_filter(struct _EXCEPTION_POINTERS *info);
static const char *lookup_symbol(UINT32 address);
static int get_code_base_size(UINT32 *base, UINT32 *size);

#if ENABLE_PROFILER
static void parse_map_file(void);
static void free_symbol_map(void);
static void output_symbol_list(FILE *f);
static void increment_bucket(UINT32 addr);
static void start_profiler(void);
static void stop_profiler(void);
#endif



//============================================================
//  main
//============================================================

#ifdef WINUI
#define main main_
#endif

int main(int argc, char **argv)
{
	int game_index;
	char *ext;
	int res = 0;
	extern void free_pathlists(void);

	// set up exception handling
	pass_thru_filter = SetUnhandledExceptionFilter(exception_filter);

#ifndef WINUI
	// check for double-clicky starts
	if (check_for_double_click_start(argc) != 0)
		return 1;
#endif

	// parse the map file, if present
	strcpy(mapfile_name, argv[0]);
	ext = strchr(mapfile_name, '.');
	if (ext)
		strcpy(ext, ".map");
	else
		strcat(mapfile_name, ".map");
#if ENABLE_PROFILER
	parse_map_file();
#endif

	// parse config and cmdline options
	game_index = cli_frontend_init(argc, argv);

	// remember the initial LED states and init keyboard handle
	start_led();

	// have we decided on a game?
	if (game_index != -1)
	{
		TIMECAPS caps;
		MMRESULT result;

		// crank up the multimedia timer resolution to its max
		// this gives the system much finer timeslices
		result = timeGetDevCaps(&caps, sizeof(caps));
		if (result == TIMERR_NOERROR)
			timeBeginPeriod(caps.wPeriodMin);

#if ENABLE_PROFILER
		start_profiler();
#endif

		// run the game
		res = run_game(game_index);

#if ENABLE_PROFILER
		stop_profiler();
#endif

		// restore the timer resolution
		if (result == TIMERR_NOERROR)
			timeEndPeriod(caps.wPeriodMin);
	}

	// restore the original LED state and close keyboard handle
	stop_led();
	win_process_events(0);

	// close errorlog, input and playback
	cli_frontend_exit();

#if ENABLE_PROFILER
	output_symbol_list(stderr);
	free_symbol_map();
#endif
	free_pathlists();

#ifdef MALLOC_DEBUG
	{
		void check_unfreed_mem(void);
		check_unfreed_mem();
	}
#endif

	return res;
}



//============================================================
//  output_oslog
//============================================================

static void output_oslog(const char *buffer)
{
	extern int win_erroroslog;
	if (win_erroroslog)
		OutputDebugString(buffer);
}



//============================================================
//  osd_init
//============================================================

int osd_init(void)
{
	extern int win_init_input(void);
	extern void win_blit_init(void);
	extern int win_erroroslog;
	int result;

	win_blit_init();

	result = win_init_window();
	if (result == 0)
		result = win_init_input();

	add_pause_callback(win_pause);
	add_exit_callback(osd_exit);

	if (win_erroroslog)
		add_logerror_callback(output_oslog);

	return result;
}



//============================================================
//  osd_exit
//============================================================

static void osd_exit(void)
{
	extern void win_shutdown_input(void);

	win_shutdown_input();
	osd_set_leds(0);
}



//============================================================
//  osd_alloc_executable
//============================================================

void *osd_alloc_executable(size_t size)
{
	return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}



//============================================================
//  osd_free_executable
//============================================================

void osd_free_executable(void *ptr)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}



//============================================================
//  osd_is_bad_read_ptr
//============================================================

int osd_is_bad_read_ptr(const void *ptr, size_t size)
{
	return IsBadReadPtr(ptr, size);
}



//============================================================
//  check_for_double_click_start
//============================================================

static int check_for_double_click_start(int argc)
{
	STARTUPINFO startup_info = { sizeof(STARTUPINFO) };

	// determine our startup information
	GetStartupInfo(&startup_info);

	// try to determine if MAME was simply double-clicked
	if (argc <= 1 && startup_info.dwFlags && !(startup_info.dwFlags & STARTF_USESTDHANDLES))
	{
		char message_text[1024] = "";
		int button;

#ifndef MESS
		sprintf(message_text, APPLONGNAME " v%s - Multiple Arcade Machine Emulator\n"
							  "Copyright (C) 1997-2006 by Nicola Salmoria and the MAME Team\n"
							  "\n"
							  APPLONGNAME " is a console application, you should launch it from a command prompt.\n"
							  "\n"
							  "Usage:\tMAME gamename [options]\n"
							  "\n"
							  "\tMAME -showusage\t\tfor a brief list of options\n"
							  "\tMAME -showconfig\t\tfor a list of configuration options\n"
							  "\tMAME -createconfig\tto create a mame.ini\n"
							  "\n"
							  "Please consult the documentation for more information.\n"
							  "\n"
							  "Would you like to open the documentation now?"
							  , build_version);
#else
		sprintf(message_text, APPLONGNAME " is a console application, you should launch it from a command prompt.\n"
							  "\n"
							  "Please consult the documentation for more information.\n"
							  "\n"
							  "Would you like to open the documentation now?");
#endif

		// pop up a messagebox with some information
		button = MessageBox(NULL, message_text, APPLONGNAME " usage information...", MB_YESNO | MB_ICONASTERISK);

		if (button == IDYES)
		{
			// check if windows.txt exists
			FILE *fp = fopen(helpfile, "r");
			if (fp)
			{
				fclose(fp);

				// if so, open it with the default application
				ShellExecute(NULL, "open", helpfile, NULL, NULL, SW_SHOWNORMAL);
			}
			else
			{
				// if not, inform the user
				MessageBox(NULL, "Couldn't find the documentation.", "Error...", MB_OK | MB_ICONERROR);
			}
		}
		return 1;
	}
	return 0;
}


//============================================================
//  exception_filter
//============================================================

static LONG CALLBACK exception_filter(struct _EXCEPTION_POINTERS *info)
{
	static const struct
	{
		DWORD code;
		const char *string;
	} exception_table[] =
	{
		{ EXCEPTION_ACCESS_VIOLATION,		"ACCESS VIOLATION" },
		{ EXCEPTION_DATATYPE_MISALIGNMENT,	"DATATYPE MISALIGNMENT" },
		{ EXCEPTION_BREAKPOINT, 			"BREAKPOINT" },
		{ EXCEPTION_SINGLE_STEP,			"SINGLE STEP" },
		{ EXCEPTION_ARRAY_BOUNDS_EXCEEDED,	"ARRAY BOUNDS EXCEEDED" },
		{ EXCEPTION_FLT_DENORMAL_OPERAND,	"FLOAT DENORMAL OPERAND" },
		{ EXCEPTION_FLT_DIVIDE_BY_ZERO,		"FLOAT DIVIDE BY ZERO" },
		{ EXCEPTION_FLT_INEXACT_RESULT,		"FLOAT INEXACT RESULT" },
		{ EXCEPTION_FLT_INVALID_OPERATION,	"FLOAT INVALID OPERATION" },
		{ EXCEPTION_FLT_OVERFLOW,			"FLOAT OVERFLOW" },
		{ EXCEPTION_FLT_STACK_CHECK,		"FLOAT STACK CHECK" },
		{ EXCEPTION_FLT_UNDERFLOW,			"FLOAT UNDERFLOW" },
		{ EXCEPTION_INT_DIVIDE_BY_ZERO,		"INTEGER DIVIDE BY ZERO" },
		{ EXCEPTION_INT_OVERFLOW, 			"INTEGER OVERFLOW" },
		{ EXCEPTION_PRIV_INSTRUCTION, 		"PRIVILEGED INSTRUCTION" },
		{ EXCEPTION_IN_PAGE_ERROR, 			"IN PAGE ERROR" },
		{ EXCEPTION_ILLEGAL_INSTRUCTION, 	"ILLEGAL INSTRUCTION" },
		{ EXCEPTION_NONCONTINUABLE_EXCEPTION,"NONCONTINUABLE EXCEPTION" },
		{ EXCEPTION_STACK_OVERFLOW, 		"STACK OVERFLOW" },
		{ EXCEPTION_INVALID_DISPOSITION, 	"INVALID DISPOSITION" },
		{ EXCEPTION_GUARD_PAGE, 			"GUARD PAGE VIOLATION" },
		{ EXCEPTION_INVALID_HANDLE, 		"INVALID HANDLE" },
		{ 0,								"UNKNOWN EXCEPTION" }
	};
	static int already_hit = 0;
	UINT32 code_start, code_size;
	int i;

	// if we're hitting this recursively, just exit
	if (already_hit)
		return EXCEPTION_EXECUTE_HANDLER;
	already_hit = 1;

	// find our man
	for (i = 0; exception_table[i].code != 0; i++)
		if (info->ExceptionRecord->ExceptionCode == exception_table[i].code)
			break;

	// print the exception type and address
	fprintf(stderr, "\n-----------------------------------------------------\n");
	fprintf(stderr, "Exception at EIP=%08X%s: %s\n", (UINT32)info->ExceptionRecord->ExceptionAddress,
			lookup_symbol((UINT32)info->ExceptionRecord->ExceptionAddress), exception_table[i].string);

	// for access violations, print more info
	if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
		fprintf(stderr, "While attempting to %s memory at %08X\n",
				info->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
				(UINT32)info->ExceptionRecord->ExceptionInformation[1]);

	// print the state of the CPU
	fprintf(stderr, "-----------------------------------------------------\n");
#ifdef PTR64
	fprintf(stderr, "RAX=%p RBX=%p RCX=%p RDX=%p\n",
			(void *)info->ContextRecord->Rax,
			(void *)info->ContextRecord->Rbx,
			(void *)info->ContextRecord->Rcx,
			(void *)info->ContextRecord->Rdx);
	fprintf(stderr, "RSI=%p RDI=%p RBP=%p RSP=%p\n",
			(void *)info->ContextRecord->Rsi,
			(void *)info->ContextRecord->Rdi,
			(void *)info->ContextRecord->Rbp,
			(void *)info->ContextRecord->Rsp);
	fprintf(stderr, " R8=%p  R9=%p R10=%p R11=%p\n",
			(void *)info->ContextRecord->R8,
			(void *)info->ContextRecord->R9,
			(void *)info->ContextRecord->R10,
			(void *)info->ContextRecord->R11);
	fprintf(stderr, "R12=%p R13=%p R14=%p R15=%p\n",
			(void *)info->ContextRecord->R12,
			(void *)info->ContextRecord->R13,
			(void *)info->ContextRecord->R14,
			(void *)info->ContextRecord->R15);
#else
	fprintf(stderr, "EAX=%p EBX=%p ECX=%p EDX=%p\n",
			(void *)info->ContextRecord->Eax,
			(void *)info->ContextRecord->Ebx,
			(void *)info->ContextRecord->Ecx,
			(void *)info->ContextRecord->Edx);
	fprintf(stderr, "ESI=%p EDI=%p EBP=%p ESP=%p\n",
			(void *)info->ContextRecord->Esi,
			(void *)info->ContextRecord->Edi,
			(void *)info->ContextRecord->Ebp,
			(void *)info->ContextRecord->Esp);
#endif

#ifndef PTR64
	// crawl the stack for a while
	if (get_code_base_size(&code_start, &code_size))
	{
		char prev_symbol[1024], curr_symbol[1024];
		UINT32 last_call = (UINT32)info->ExceptionRecord->ExceptionAddress;
		UINT32 esp_start = info->ContextRecord->Esp;
		UINT32 esp_end = (esp_start | 0xffff) + 1;
		UINT32 esp;

		// reprint the actual exception address
		fprintf(stderr, "-----------------------------------------------------\n");
		fprintf(stderr, "Stack crawl:\n");
		fprintf(stderr, "exception-> %08X%s\n", last_call, strcpy(prev_symbol, lookup_symbol(last_call)));

		// crawl the stack until we hit the next 64k boundary
		for (esp = esp_start; esp < esp_end; esp += 4)
		{
			UINT32 stack_val = *(UINT32 *)esp;

			// if the value on the stack points within the code block, check it out
			if (stack_val >= code_start && stack_val < code_start + code_size)
			{
				UINT8 *return_addr = (UINT8 *)stack_val;
				UINT32 call_target = 0;

				// make sure the code that we think got us here is actually a CALL instruction
				if (return_addr[-5] == 0xe8)
					call_target = stack_val - 5 + *(INT32 *)&return_addr[-4];
				if ((return_addr[-2] == 0xff && (return_addr[-1] & 0x38) == 0x10) ||
					(return_addr[-3] == 0xff && (return_addr[-2] & 0x38) == 0x10) ||
					(return_addr[-4] == 0xff && (return_addr[-3] & 0x38) == 0x10) ||
					(return_addr[-5] == 0xff && (return_addr[-4] & 0x38) == 0x10) ||
					(return_addr[-6] == 0xff && (return_addr[-5] & 0x38) == 0x10) ||
					(return_addr[-7] == 0xff && (return_addr[-6] & 0x38) == 0x10))
					call_target = 1;

				// make sure it points somewhere a little before the last call
				if (call_target == 1 || (call_target < last_call && call_target >= last_call - 0x1000))
				{
					char *stop_compare = strchr(prev_symbol, '+');

					// don't print duplicate hits in the same routine
					strcpy(curr_symbol, lookup_symbol(stack_val));
					if (stop_compare == NULL || strncmp(curr_symbol, prev_symbol, stop_compare - prev_symbol))
					{
						strcpy(prev_symbol, curr_symbol);
						fprintf(stderr, "  %08X: %08X%s\n", esp, stack_val, curr_symbol);
						last_call = stack_val;
					}
				}
			}
		}
	}
#endif

	logerror("shutting down after exception\n");

	cli_frontend_exit();

	// exit
	return EXCEPTION_EXECUTE_HANDLER;
}



//============================================================
//  lookup_symbol
//============================================================

static const char *lookup_symbol(UINT32 address)
{
	static char buffer[1024];
	FILE *	map = fopen(mapfile_name, "r");
	char	symbol[1024], best_symbol[1024];
	UINT32	addr, best_addr = 0;
	char	line[1024];

	// if no file, return nothing
	if (map == NULL)
		return "";

	// reset the bests
	*best_symbol = 0;
	best_addr = 0;

	// parse the file, looking for map entries
	while (fgets(line, sizeof(line) - 1, map))
		if (!strncmp(line, "                0x", 18))
			if (sscanf(line, "                0x%08x %s", &addr, symbol) == 2)
				if (addr <= address && addr > best_addr)
				{
					best_addr = addr;
					strcpy(best_symbol, symbol);
				}

	// create the final result
	if (address - best_addr > 0x10000)
		return "";
	sprintf(buffer, " (%s+0x%04x)", best_symbol, address - best_addr);
	return buffer;
}



//============================================================
//  get_code_base_size
//============================================================

static int get_code_base_size(UINT32 *base, UINT32 *size)
{
	FILE *	map = fopen(mapfile_name, "r");
	char	line[1024];

	// if no file, return nothing
	if (map == NULL)
		return 0;

	// parse the file, looking for .text entry
	while (fgets(line, sizeof(line) - 1, map))
		if (!strncmp(line, ".text           0x", 18))
			if (sscanf(line, ".text           0x%08x 0x%x", base, size) == 2)
				return 1;

	return 0;
}



//============================================================
//  compare_base
//  compare_hits -- qsort callbacks to sort on
//============================================================

static int CLIB_DECL compare_start(const void *item1, const void *item2)
{
	return ((const map_entry *)item1)->start - ((const map_entry *)item2)->start;
}


#if ENABLE_PROFILER
static int compare_hits(const void *item1, const void *item2)
{
	return ((const map_entry *)item2)->hits - ((const map_entry *)item1)->hits;
}


//============================================================
//  parse_map_file
//============================================================

static void parse_map_file(void)
{
	int got_text = 0;
	char line[1024];
	FILE *map;
	int i;

	// open the map file
	map = fopen(mapfile_name, "r");
	if (!map)
		return;

	// parse out the various symbols into map entries
	map_entries = 0;
	while (fgets(line, sizeof(line) - 1, map))
	{
		/* look for the code boundaries */
		if (!got_text && !strncmp(line, ".text           0x", 18))
		{
			UINT32 base, size;
			if (sscanf(line, ".text           0x%08x 0x%x", &base, &size) == 2)
			{
				symbol_map[map_entries].start = base;
				symbol_map[map_entries].name = strcpy(malloc(strlen("Code start") + 1), "Code start");
				map_entries++;
				symbol_map[map_entries].start = base + size;
				symbol_map[map_entries].name = strcpy(malloc(strlen("Other") + 1), "Other");
				map_entries++;
				got_text = 1;
			}
		}

		/* look for symbols */
		else if (!strncmp(line, "                0x", 18))
		{
			char symbol[1024];
			UINT32 addr;
			if (sscanf(line, "                0x%08x %s", &addr, symbol) == 2)
			{
				symbol_map[map_entries].start = addr;
				symbol_map[map_entries].name = strcpy(malloc(strlen(symbol) + 1), symbol);
				map_entries++;
			}
		}
	}

	/* add a symbol for end-of-memory */
	symbol_map[map_entries].start = ~0;
	symbol_map[map_entries].name = strcpy(malloc(strlen("<end>") + 1), "<end>");

	/* close the file */
	fclose(map);

	/* sort by address */
	qsort(symbol_map, map_entries, sizeof(symbol_map[0]), compare_start);

	/* fill in the end of each bucket */
	for (i = 0; i < map_entries; i++)
		symbol_map[i].end = symbol_map[i+1].start ? (symbol_map[i+1].start - 1) : 0;
}


//============================================================
//  free_symbol_map
//============================================================

static void free_symbol_map(void)
{
	int i;

	for (i = 0; i <= map_entries; i++)
	{
		free(symbol_map[i].name);
		symbol_map[i].name = NULL;
	}
}


//============================================================
//  output_symbol_list
//============================================================

static void output_symbol_list(FILE *f)
{
	map_entry *entry;
	int i;

	/* sort by hits */
	qsort(symbol_map, map_entries, sizeof(symbol_map[0]), compare_hits);

	for (i = 0, entry = symbol_map; i < map_entries; i++, entry++)
		if (entry->hits > 0)
			fprintf(f, "%10d  %08X-%08X  %s\n", entry->hits, entry->start, entry->end, entry->name);
}



//============================================================
//  increment_bucket
//============================================================

static void increment_bucket(UINT32 addr)
{
	int i;

	for (i = 0; i < map_entries; i++)
		if (addr <= symbol_map[i].end)
		{
			symbol_map[i].hits++;
			return;
		}
}



//============================================================
//  profiler_thread
//============================================================

static DWORD WINAPI profiler_thread_entry(LPVOID lpParameter)
{
	HANDLE mainThread = (HANDLE)lpParameter;
	CONTEXT context;

	/* loop until done */
	memset(&context, 0, sizeof(context));
	while (!profiler_thread_exit)
	{
		/* pause the main thread and get its context */
		SuspendThread(mainThread);
		context.ContextFlags = CONTEXT_FULL;
		GetThreadContext(mainThread, &context);
		ResumeThread(mainThread);

		/* add to the bucket */
		increment_bucket(context.Eip);

		/* sleep */
		Sleep(1);
	}

	return 0;
}



//============================================================
//  start_profiler
//============================================================

static void start_profiler(void)
{
	HANDLE currentThread;
	BOOL result;

	/* do the dance to get a handle to ourself */
	result = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &currentThread,
			THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, 0);
	assert_always(result, "Failed to get thread handle for main thread");

	profiler_thread_exit = 0;

	/* start the thread */
	profiler_thread = CreateThread(NULL, 0, profiler_thread_entry, (LPVOID)currentThread, 0, &profiler_thread_id);
	assert_always(profiler_thread, "Failed to create profiler thread\n");

	/* max out the priority */
	SetThreadPriority(profiler_thread, THREAD_PRIORITY_TIME_CRITICAL);
}



//============================================================
//  stop_profiler
//============================================================

static void stop_profiler(void)
{
	profiler_thread_exit = 1;
	WaitForSingleObject(profiler_thread, 2000);
}
#endif
