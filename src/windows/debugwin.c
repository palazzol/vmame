//============================================================
//
//  debugwin.c - Win32 debug window handling
//
//  Copyright (c) 1996-2006, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//============================================================

// standard windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#ifdef _MSC_VER
#include <zmouse.h>
#endif

// MAME headers
#include "driver.h"

#ifdef NEW_DEBUGGER
#include "debug/debugvw.h"
#include "debug/debugcon.h"
#include "debug/debugcpu.h"
#endif

// MAMEOS headers
#include "debugwin.h"
#include "window.h"
#include "video.h"
#include "config.h"



//============================================================
//  PARAMETERS
//============================================================

#define MAX_VIEWS				4
#define EDGE_WIDTH				3
#define MAX_EDIT_STRING			256
#define HISTORY_LENGTH			20
#define MAX_OTHER_WND			4

// debugger window styles
#define DEBUG_WINDOW_STYLE		(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) & (~WS_MINIMIZEBOX & ~WS_MAXIMIZEBOX)
#define DEBUG_WINDOW_STYLE_EX	0

// debugger view styles
#define DEBUG_VIEW_STYLE		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN
#define DEBUG_VIEW_STYLE_EX		0

// edit box styles
#define EDIT_BOX_STYLE			WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL
#define EDIT_BOX_STYLE_EX		0

// combo box styles
#define COMBO_BOX_STYLE			WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST
#define COMBO_BOX_STYLE_EX		0

// horizontal scroll bar styles
#define HSCROLL_STYLE			WS_CHILD | WS_VISIBLE | SBS_HORZ
#define HSCROLL_STYLE_EX		0

// vertical scroll bar styles
#define VSCROLL_STYLE			WS_CHILD | WS_VISIBLE | SBS_VERT
#define VSCROLL_STYLE_EX		0


enum
{
	ID_NEW_MEMORY_WND = 1,
	ID_NEW_DISASM_WND,
	ID_NEW_LOG_WND,
	ID_RUN,
	ID_RUN_AND_HIDE,
	ID_RUN_VBLANK,
	ID_RUN_IRQ,
	ID_NEXT_CPU,
	ID_STEP,
	ID_STEP_OVER,
	ID_STEP_OUT,
	ID_HARD_RESET,
	ID_SOFT_RESET,
	ID_EXIT,

	ID_1_BYTE_CHUNKS,
	ID_2_BYTE_CHUNKS,
	ID_4_BYTE_CHUNKS,
	ID_REVERSE_VIEW,
	ID_INCREASE_MEM_WIDTH,
	ID_DECREASE_MEM_WIDTH,

	ID_SHOW_RAW,
	ID_SHOW_ENCRYPTED,
	ID_SHOW_COMMENTS
};



//============================================================
//  TYPES
//============================================================

typedef struct _debugview_info debugview_info;
typedef struct _debugwin_info debugwin_info;


struct _debugview_info
{
	debugwin_info *			owner;
	debug_view *			view;
	HWND					wnd;
	HWND					hscroll;
	HWND					vscroll;
	UINT8					is_textbuf;
};


struct _debugwin_info
{
	debugwin_info *			next;
	HWND					wnd;
	HWND					focuswnd;
	WNDPROC					handler;

	UINT32					minwidth, maxwidth;
	UINT32					minheight, maxheight;
	void					(*recompute_children)(debugwin_info *);

	int						(*handle_command)(debugwin_info *, WPARAM, LPARAM);
	int						(*handle_key)(debugwin_info *, WPARAM, LPARAM);
	UINT16					ignore_char_lparam;

	debugview_info			view[MAX_VIEWS];

	HWND					editwnd;
	char					edit_defstr[256];
	void					(*process_string)(debugwin_info *, const char *);
	WNDPROC 				original_editproc;
	char					history[HISTORY_LENGTH][MAX_EDIT_STRING];
	int						history_count;
	int						last_history;

	HWND					otherwnd[MAX_OTHER_WND];
};


typedef struct _memorycombo_item memorycombo_item;
struct _memorycombo_item
{
	memorycombo_item *		next;
	char					name[256];
	UINT8					cpunum;
	UINT8					spacenum;
	void *					base;
	UINT32					length;
	UINT8					offset_xor;
	UINT8					little_endian;
	UINT8					prefsize;
};


//============================================================
//  GLOBAL VARIABLES
//============================================================



//============================================================
//  LOCAL VARIABLES
//============================================================

static debugwin_info *window_list;
static debugwin_info *main_console;

static memorycombo_item *memorycombo;

static UINT8 debugger_active_countdown;
static UINT8 waiting_for_debugger;

static HFONT debug_font;
static UINT32 debug_font_height;
static UINT32 debug_font_width;
static UINT32 debug_font_ascent;

static UINT32 hscroll_height;
static UINT32 vscroll_width;

static int temporarily_fake_that_we_are_not_visible;



//============================================================
//  PROTOTYPES
//============================================================

static debugwin_info *debug_window_create(LPCTSTR title, WNDPROC handler);
static void debug_window_free(debugwin_info *info);
static LRESULT CALLBACK debug_window_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam);

static void debug_view_draw_contents(debugview_info *view, HDC dc);
static debugview_info *debug_view_find(debug_view *view);
static LRESULT CALLBACK debug_view_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam);
static void debug_view_update(debug_view *view);
static int debug_view_create(debugwin_info *info, int which, int type);

static LRESULT CALLBACK debug_edit_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam);

static void generic_create_window(int type);
static void generic_recompute_children(debugwin_info *info);

static void memory_create_window(void);
static void memory_recompute_children(debugwin_info *info);
static void memory_process_string(debugwin_info *info, const char *string);
static void memory_update_checkmarks(debugwin_info *info);
static int memory_handle_command(debugwin_info *info, WPARAM wparam, LPARAM lparam);
static int memory_handle_key(debugwin_info *info, WPARAM wparam, LPARAM lparam);

static void disasm_create_window(void);
static void disasm_recompute_children(debugwin_info *info);
static void disasm_process_string(debugwin_info *info, const char *string);
static void disasm_update_checkmarks(debugwin_info *info);
static int disasm_handle_command(debugwin_info *info, WPARAM wparam, LPARAM lparam);
static int disasm_handle_key(debugwin_info *info, WPARAM wparam, LPARAM lparam);
static void disasm_update_caption(HWND wnd);

static void console_create_window(void);
static void console_recompute_children(debugwin_info *info);
static void console_process_string(debugwin_info *info, const char *string);
static void console_set_cpunum(int cpunum);

static HMENU create_standard_menubar(void);
static int global_handle_command(debugwin_info *info, WPARAM wparam, LPARAM lparam);
static int global_handle_key(debugwin_info *info, WPARAM wparam, LPARAM lparam);
static void smart_set_window_bounds(HWND wnd, HWND parent, RECT *bounds);
static void smart_show_window(HWND wnd, BOOL show);
static void smart_show_all(BOOL show);



//============================================================
//  osd_wait_for_debugger
//============================================================

void osd_wait_for_debugger(void)
{
	MSG message;

	// create a console window
	if (!main_console)
		console_create_window();

	// update the views in the console to reflect the current CPU
	if (main_console)
		console_set_cpunum(cpu_getactivecpu());

	// make sure the debug windows are visible
	waiting_for_debugger = TRUE;
	smart_show_all(TRUE);

	// get and process messages
	GetMessage(&message, NULL, 0, 0);

	switch (message.message)
	{
		// special case for quit
		case WM_QUIT:
			exit(0);
			break;

		// check for F10 -- we need to capture that ourselves
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			if (message.wParam == VK_F10)
				SendMessage(GetFocus(), (message.message == WM_SYSKEYDOWN) ? WM_KEYDOWN : WM_KEYUP, message.wParam, message.lParam);
			break;

		// process everything else
		default:
			TranslateMessage(&message);
			DispatchMessage(&message);
			break;
	}

	// mark the debugger as active
	debugger_active_countdown = 2;
	waiting_for_debugger = FALSE;
}



//============================================================
//  debugwin_init_windows
//============================================================

int debugwin_init_windows(void)
{
	static int class_registered;

	// register the window classes
	if (!class_registered)
	{
		WNDCLASS wc = { 0 };

		// initialize the description of the window class
		wc.lpszClassName 	= TEXT("MAMEDebugWindow");
		wc.hInstance 		= GetModuleHandle(NULL);
		wc.lpfnWndProc		= debug_window_proc;
		wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wc.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
		wc.lpszMenuName		= NULL;
		wc.hbrBackground	= NULL;
		wc.style			= 0;
		wc.cbClsExtra		= 0;
		wc.cbWndExtra		= 0;

		// register the class; fail if we can't
		if (!RegisterClass(&wc))
			return 1;

		// initialize the description of the view class
		wc.lpszClassName 	= TEXT("MAMEDebugView");
		wc.lpfnWndProc		= debug_view_proc;

		// register the class; fail if we can't
		if (!RegisterClass(&wc))
			return 1;

		class_registered = TRUE;
	}

	// create the font
	if (!debug_font)
	{
		// create a temporary DC
		HDC temp_dc = GetDC(NULL);
		TEXTMETRIC metrics;
		HGDIOBJ old_font;

		if (temp_dc != NULL)
		{
			// create a standard Lucida Console 8 font
			debug_font = CreateFont(-MulDiv(8, GetDeviceCaps(temp_dc, LOGPIXELSY), 72), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
						ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, TEXT("Lucida Console"));
			if (!debug_font)
				return 1;

			// get the metrics
			old_font = SelectObject(temp_dc, debug_font);
			if (GetTextMetrics(temp_dc, &metrics))
			{
				debug_font_width = metrics.tmMaxCharWidth;
				debug_font_height = metrics.tmHeight;
				debug_font_ascent = metrics.tmAscent + metrics.tmExternalLeading;
			}
			SelectObject(temp_dc, old_font);
			ReleaseDC(NULL, temp_dc);
		}
	}

	// get other metrics
	hscroll_height = GetSystemMetrics(SM_CYHSCROLL);
	vscroll_width = GetSystemMetrics(SM_CXVSCROLL);

	return 0;
}



//============================================================
//  debugwin_destroy_windows
//============================================================

void debugwin_destroy_windows(void)
{
	// loop over windows and free them
	while (window_list)
		DestroyWindow(window_list->wnd);

	// free the combobox info
	while (memorycombo)
	{
		void *temp = memorycombo;
		memorycombo = memorycombo->next;
		free(temp);
	}

	main_console = NULL;
}



//============================================================
//  debugwin_show
//============================================================

void debugwin_show(int type)
{
	debugwin_info *info;

	// loop over windows and show/hide them
	for (info = window_list; info; info = info->next)
		ShowWindow(info->wnd, type);
}



//============================================================
//  debugwin_update_during_game
//============================================================

void debugwin_update_during_game(void)
{
	int execution_state = debug_get_execution_state();

	// if we're running live, do some checks
	if (execution_state != EXECUTION_STATE_STOPPED)
	{
		// see if the interrupt key is pressed and break if it is
		temporarily_fake_that_we_are_not_visible = TRUE;
		if (input_ui_pressed(IPT_UI_DEBUG_BREAK))
		{
			debugwin_info *info;
			HWND focuswnd = GetFocus();

			debug_halt_on_next_instruction();
			debug_console_printf("User-initiated break\n");

			// if we were focused on some window's edit box, reset it to default
			for (info = window_list; info; info = info->next)
				if (focuswnd == info->editwnd)
				{
					SendMessage(focuswnd, WM_SETTEXT, (WPARAM)0, (LPARAM)info->edit_defstr);
					SendMessage(focuswnd, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
				}
		}
		temporarily_fake_that_we_are_not_visible = FALSE;
	}
}



//============================================================
//  debugwin_is_debugger_visible
//============================================================

int debugwin_is_debugger_visible(void)
{
	debugwin_info *info;

	// a bit of hackiness to allow us to check key sequences even if we are visible
	if (temporarily_fake_that_we_are_not_visible)
		return 0;

	// if any one of our windows is visible, return true
	for (info = window_list; info; info = info->next)
		if (IsWindowVisible(info->wnd))
			return 1;
	return 0;
}



//============================================================
//  debug_window_create
//============================================================

static debugwin_info *debug_window_create(LPCTSTR title, WNDPROC handler)
{
	debugwin_info *info = NULL;
	RECT work_bounds;

	// allocate memory
	info = malloc(sizeof(*info));
	if (!info)
		return NULL;
	memset(info, 0, sizeof(*info));

	// create the window
	info->handler = handler;
	info->wnd = CreateWindowEx(DEBUG_WINDOW_STYLE_EX, TEXT("MAMEDebugWindow"), title, DEBUG_WINDOW_STYLE,
			0, 0, 100, 100, win_video_window, create_standard_menubar(), GetModuleHandle(NULL), info);
	if (!info->wnd)
		goto cleanup;

	// fill in some defaults
	SystemParametersInfo(SPI_GETWORKAREA, 0, &work_bounds, 0);
	info->minwidth = 200;
	info->minheight = 200;
	info->maxwidth = work_bounds.right - work_bounds.left;
	info->maxheight = work_bounds.bottom - work_bounds.top;

	// set the default handlers
	info->handle_command = global_handle_command;
	info->handle_key = global_handle_key;
	strcpy(info->edit_defstr, "");

	// hook us in
	info->next = window_list;
	window_list = info;
	return info;

cleanup:
	if (info->wnd)
		DestroyWindow(info->wnd);
	free(info);
	return NULL;
}



//============================================================
//  debug_window_free
//============================================================

static void debug_window_free(debugwin_info *info)
{
	debugwin_info *prev, *curr;
	int viewnum;

	// first unlink us from the list
	for (curr = window_list, prev = NULL; curr; prev = curr, curr = curr->next)
		if (curr == info)
		{
			if (prev)
				prev->next = curr->next;
			else
				window_list = curr->next;
			break;
		}

	// free any views
	for (viewnum = 0; viewnum < MAX_VIEWS; viewnum++)
		if (info->view[viewnum].view)
		{
			debug_view_free(info->view[viewnum].view);
			info->view[viewnum].view = NULL;
		}

	// free our memory
	free(info);
}



//============================================================
//  debug_window_draw_contents
//============================================================

static void debug_window_draw_contents(debugwin_info *info, HDC dc)
{
	RECT bounds, parent;
	int curview, curwnd;

	// fill the background with light gray
	GetClientRect(info->wnd, &parent);
	FillRect(dc, &parent, GetStockObject(LTGRAY_BRUSH));

	// get the parent bounds in screen coords
	ClientToScreen(info->wnd, &((POINT *)&parent)[0]);
	ClientToScreen(info->wnd, &((POINT *)&parent)[1]);

	// draw edges around all views
	for (curview = 0; curview < MAX_VIEWS; curview++)
		if (info->view[curview].wnd)
		{
			GetWindowRect(info->view[curview].wnd, &bounds);
			bounds.top -= parent.top;
			bounds.bottom -= parent.top;
			bounds.left -= parent.left;
			bounds.right -= parent.left;
			InflateRect(&bounds, EDGE_WIDTH, EDGE_WIDTH);
			DrawEdge(dc, &bounds, EDGE_SUNKEN, BF_RECT);
		}

	// draw edges around all children
	if (info->editwnd)
	{
		GetWindowRect(info->editwnd, &bounds);
		bounds.top -= parent.top;
		bounds.bottom -= parent.top;
		bounds.left -= parent.left;
		bounds.right -= parent.left;
		InflateRect(&bounds, EDGE_WIDTH, EDGE_WIDTH);
		DrawEdge(dc, &bounds, EDGE_SUNKEN, BF_RECT);
	}

	for (curwnd = 0; curwnd < MAX_OTHER_WND; curwnd++)
		if (info->otherwnd[curwnd])
		{
			GetWindowRect(info->otherwnd[curwnd], &bounds);
			bounds.top -= parent.top;
			bounds.bottom -= parent.top;
			bounds.left -= parent.left;
			bounds.right -= parent.left;
			InflateRect(&bounds, EDGE_WIDTH, EDGE_WIDTH);
			DrawEdge(dc, &bounds, EDGE_SUNKEN, BF_RECT);
		}
}



//============================================================
//  debug_window_proc
//============================================================

static LRESULT CALLBACK debug_window_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	debugwin_info *info = (debugwin_info *)(UINT32)GetWindowLongPtr(wnd, GWLP_USERDATA);

	// handle a few messages
	switch (message)
	{
		// set the info pointer
		case WM_CREATE:
		{
			CREATESTRUCT *createinfo = (CREATESTRUCT *)lparam;
			info = (debugwin_info *)createinfo->lpCreateParams;
			SetWindowLongPtr(wnd, GWLP_USERDATA, (UINT32)createinfo->lpCreateParams);
			if (info->handler)
				SetWindowLongPtr(wnd, GWLP_WNDPROC, (UINT32)info->handler);
			break;
		}

		// paint: draw bezels as necessary
		case WM_PAINT:
		{
			PAINTSTRUCT pstruct;
			HDC dc = BeginPaint(wnd, &pstruct);
			debug_window_draw_contents(info, dc);
			EndPaint(wnd, &pstruct);
			break;
		}

		// keydown: handle debugger keys
		case WM_KEYDOWN:
		{
			if ((*info->handle_key)(info, wparam, lparam))
				info->ignore_char_lparam = lparam >> 16;
			break;
		}

		// char: ignore chars associated with keys we've handled
		case WM_CHAR:
		{
			if (info->ignore_char_lparam != (lparam >> 16))
				return DefWindowProc(wnd, message, wparam, lparam);
			else
				info->ignore_char_lparam = 0;
			break;
		}

		// activate: set the focus
		case WM_ACTIVATE:
		{
			if (wparam != WA_INACTIVE && info->focuswnd != NULL)
				SetFocus(info->focuswnd);
			break;
		}

		// get min/max info: set the minimum window size
		case WM_GETMINMAXINFO:
		{
			MINMAXINFO *minmax = (MINMAXINFO *)lparam;
			if (info)
			{
				minmax->ptMinTrackSize.x = info->minwidth;
				minmax->ptMinTrackSize.y = info->minheight;
				minmax->ptMaxSize.x = minmax->ptMaxTrackSize.x = info->maxwidth;
				minmax->ptMaxSize.y = minmax->ptMaxTrackSize.y = info->maxheight;
			}
			break;
		}

		// sizing: recompute child window locations
		case WM_SIZING:
		{
			if (info->recompute_children)
				(*info->recompute_children)(info);
			InvalidateRect(wnd, NULL, FALSE);
			break;
		}

		// mouse wheel: forward to the first view
		case WM_MOUSEWHEEL:
		{
			int delta = (INT16)HIWORD(wparam) / WHEEL_DELTA;
			int viewnum = 0;
			POINT point;
			HWND child;

			// figure out which view we are hovering over
			GetCursorPos(&point);
			ScreenToClient(info->wnd, &point);
			child = ChildWindowFromPoint(info->wnd, point);
			if (child)
			{
				for (viewnum = 0; viewnum < MAX_VIEWS; viewnum++)
					if (info->view[viewnum].wnd == child)
						break;
				if (viewnum == MAX_VIEWS)
					break;
			}

			// send the appropriate message to this view's scrollbar
			if (info->view[viewnum].wnd && info->view[viewnum].vscroll)
			{
				int message = SB_LINELEFT;
				if (delta < 0)
				{
					message = SB_LINERIGHT;
					delta = -delta;
				}
				while (delta > 0)
				{
					SendMessage(info->view[viewnum].wnd, WM_VSCROLL, message, (LPARAM)info->view[viewnum].vscroll);
					delta--;
				}
			}
			break;
		}

		// command: handle a comment
		case WM_COMMAND:
			if (!(*info->handle_command)(info, wparam, lparam))
				return DefWindowProc(wnd, message, wparam, lparam);
			break;

		// close: close the window if it's not the main console
		case WM_CLOSE:
			if (main_console && main_console->wnd == wnd)
			{
				smart_show_all(FALSE);
				debug_cpu_go(~0);
			}
			else
				DestroyWindow(wnd);
			break;

		// destroy: close down the window
		case WM_NCDESTROY:
			debug_window_free(info);
			break;

		// everything else: defaults
		default:
			return DefWindowProc(wnd, message, wparam, lparam);
	}

	return 0;
}



//============================================================
//  debug_view_create
//============================================================

static int debug_view_create(debugwin_info *info, int which, int type)
{
	debugview_info *view = &info->view[which];
	void *callback = (void *)debug_view_update;

	// set the owner
	view->owner = info;

	// create the child view
	view->wnd = CreateWindowEx(DEBUG_VIEW_STYLE_EX, TEXT("MAMEDebugView"), NULL, DEBUG_VIEW_STYLE,
			0, 0, 100, 100, info->wnd, NULL, GetModuleHandle(NULL), view);
	if (!view->wnd)
		goto cleanup;

	// create the scroll bars
	view->hscroll = CreateWindowEx(HSCROLL_STYLE_EX, TEXT("SCROLLBAR"), NULL, HSCROLL_STYLE,
			0, 0, 100, 100, view->wnd, NULL, GetModuleHandle(NULL), view);
	view->vscroll = CreateWindowEx(VSCROLL_STYLE_EX, TEXT("SCROLLBAR"), NULL, VSCROLL_STYLE,
			0, 0, 100, 100, view->wnd, NULL, GetModuleHandle(NULL), view);
	if (!view->hscroll || !view->vscroll)
		goto cleanup;

	// create the debug view
	view->view = debug_view_alloc(type);
	if (!view->view)
		goto cleanup;

	// set the update handler
	debug_view_set_property_fct(view->view, DVP_UPDATE_CALLBACK, callback);
	return 1;

cleanup:
	if (view->view)
		debug_view_free(view->view);
	if (view->hscroll)
		DestroyWindow(view->hscroll);
	if (view->vscroll)
		DestroyWindow(view->vscroll);
	if (view->wnd)
		DestroyWindow(view->wnd);
	return 0;
}



//============================================================
//  debug_view_set_bounds
//============================================================

static void debug_view_set_bounds(debugview_info *info, HWND parent, const RECT *newbounds)
{
	RECT bounds = *newbounds;

	// account for the edges and set the bounds
	if (info->wnd)
		smart_set_window_bounds(info->wnd, parent, &bounds);

	// update
	debug_view_update(info->view);
}



//============================================================
//  debug_view_draw_contents
//============================================================

static void debug_view_draw_contents(debugview_info *view, HDC windc)
{
	debug_view_char *viewdata;
	HGDIOBJ oldfont, oldbitmap;
	UINT32 visrows, viscols;
	COLORREF oldfgcolor;
	UINT32 col, row;
	HBITMAP bitmap;
	int oldbkmode;
	RECT client;
	HDC dc;

	// get the client rect
	GetClientRect(view->wnd, &client);

	// create a compatible DC and an offscreen bitmap
	dc = CreateCompatibleDC(windc);
	if (!dc)
		return;
	bitmap = CreateCompatibleBitmap(windc, client.right, client.bottom);
	if (!bitmap)
	{
		DeleteDC(dc);
		return;
	}
	oldbitmap = SelectObject(dc, bitmap);

	// first get the visible size from the view and a pointer to the data
	visrows = debug_view_get_property_UINT32(view->view, DVP_VISIBLE_ROWS);
	viscols = debug_view_get_property_UINT32(view->view, DVP_VISIBLE_COLS);
	viewdata = debug_view_get_property_ptr(view->view, DVP_VIEW_DATA);

	// set the font
	oldfont = SelectObject(dc, debug_font);
	oldfgcolor = GetTextColor(dc);
	oldbkmode = GetBkMode(dc);
	SetBkMode(dc, TRANSPARENT);

	// iterate over rows and columns
	for (row = 0; row < visrows; row++)
	{
		int iter;

		// loop twice; once to fill the background and once to draw the text
		for (iter = 0; iter < 2; iter++)
		{
			COLORREF fgcolor = RGB(0x00,0x00,0x00);
			COLORREF bgcolor = RGB(0xff,0xff,0xff);
			HBRUSH bgbrush = NULL;
			int last_attrib = -1;
			TCHAR buffer[256];
			int count = 0;
			RECT bounds;

			// initialize the text bounds
			bounds.left = bounds.right = 0;
			bounds.top = row * debug_font_height;
			bounds.bottom = bounds.top + debug_font_height;

			// start with a brush on iteration #0
			if (iter == 0)
				bgbrush = CreateSolidBrush(bgcolor);

			// iterate over columns
			for (col = 0; col < viscols; col++)
			{
				// if the attribute changed, adjust the colors
				if (viewdata[col].attrib != last_attrib)
				{
					COLORREF oldbg = bgcolor;

					// reset to standard colors
					fgcolor = RGB(0x00,0x00,0x00);
					bgcolor = RGB(0xff,0xff,0xff);

					// pick new fg/bg colors
					if (viewdata[col].attrib & DCA_ANCILLARY) bgcolor = RGB(0xe0,0xe0,0xe0);
					if (viewdata[col].attrib & DCA_SELECTED) bgcolor = RGB(0xff,0x80,0x80);
					if (viewdata[col].attrib & DCA_CURRENT) bgcolor = RGB(0xff,0xff,0x00);
					if (viewdata[col].attrib & DCA_CHANGED) fgcolor = RGB(0xff,0x00,0x00);
					if (viewdata[col].attrib & DCA_INVALID) fgcolor = RGB(0x00,0x00,0xff);
					if (viewdata[col].attrib & DCA_DISABLED) fgcolor = RGB((GetRValue(fgcolor) + GetRValue(bgcolor)) / 2, (GetGValue(fgcolor) + GetGValue(bgcolor)) / 2, (GetBValue(fgcolor) + GetBValue(bgcolor)) / 2);
					if (viewdata[col].attrib & DCA_COMMENT) fgcolor = RGB(0x00,0x80,0x00);

					// flush any pending drawing
					if (count > 0)
					{
						bounds.right = bounds.left + count * debug_font_width;
						if (iter == 0)
							FillRect(dc, &bounds, bgbrush);
						else
							ExtTextOut(dc, bounds.left, bounds.top, 0, NULL, buffer, count, NULL);
						bounds.left = bounds.right;
						count = 0;
					}

					// set the new colors
					if (iter == 0 && oldbg != bgcolor)
					{
						DeleteObject(bgbrush);
						bgbrush = CreateSolidBrush(bgcolor);
					}
					else if (iter == 1)
						SetTextColor(dc, fgcolor);
					last_attrib = viewdata[col].attrib;
				}

				// add this character to the buffer
				buffer[count++] = viewdata[col].byte;
			}

			// flush any remaining stuff
			if (count > 0)
			{
				bounds.right = bounds.left + count * debug_font_width;
				if (iter == 0)
					FillRect(dc, &bounds, bgbrush);
				else
					ExtTextOut(dc, bounds.left, bounds.top, 0, NULL, buffer, count, NULL);
			}

			// erase to the end of the line
			if (iter == 0)
			{
				bounds.left = bounds.right;
				bounds.right = client.right;
				FillRect(dc, &bounds, bgbrush);
				DeleteObject(bgbrush);
			}
		}

		// advance viewdata
		viewdata += viscols;
	}

	// erase anything beyond the bottom with white
	GetClientRect(view->wnd, &client);
	client.top = visrows * debug_font_height;
	FillRect(dc, &client, (HBRUSH)GetStockObject(WHITE_BRUSH));

	// reset the font
	SetBkMode(dc, oldbkmode);
	SetTextColor(dc, oldfgcolor);
	SelectObject(dc, oldfont);

	// blit the final results
	BitBlt(windc, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);

	// undo the offscreen stuff
	SelectObject(dc, oldbitmap);
	DeleteObject(bitmap);
	DeleteDC(dc);
}



//============================================================
//  debug_view_update
//============================================================

static void debug_view_update(debug_view *view)
{
	debugview_info *info = debug_view_find(view);

	// if we have a view window, process it
	if (info && info->view)
	{
		RECT bounds, vscroll_bounds, hscroll_bounds;
		int show_vscroll, show_hscroll;
		UINT32 visible_rows, visible_cols;
		UINT32 total_rows, total_cols;
		UINT32 top_row, left_col;
		SCROLLINFO scrollinfo;

		// get the view window bounds
		GetClientRect(info->wnd, &bounds);
		visible_rows = (bounds.bottom - bounds.top) / debug_font_height;
		visible_cols = (bounds.right - bounds.left) / debug_font_width;

		// get the updated total rows/cols and left row/col
		total_rows = debug_view_get_property_UINT32(view, DVP_TOTAL_ROWS);
		total_cols = debug_view_get_property_UINT32(view, DVP_TOTAL_COLS);
		top_row = debug_view_get_property_UINT32(view, DVP_TOP_ROW);
		left_col = debug_view_get_property_UINT32(view, DVP_LEFT_COL);

		// determine if we need to show the scrollbars
		show_vscroll = show_hscroll = 0;
		if (total_rows > visible_rows)
		{
			bounds.right -= vscroll_width;
			visible_cols = (bounds.right - bounds.left) / debug_font_width;
			show_vscroll = TRUE;
		}
		if (total_cols > visible_cols)
		{
			bounds.bottom -= hscroll_height;
			visible_rows = (bounds.bottom - bounds.top) / debug_font_height;
			show_hscroll = TRUE;
		}
		if (!show_vscroll && total_rows > visible_rows)
		{
			bounds.right -= vscroll_width;
			visible_cols = (bounds.right - bounds.left) / debug_font_width;
			show_vscroll = TRUE;
		}

		// compute the bounds of the scrollbars
		GetClientRect(info->wnd, &vscroll_bounds);
		vscroll_bounds.left = vscroll_bounds.right - vscroll_width;
		if (show_hscroll)
			vscroll_bounds.bottom -= hscroll_height;

		GetClientRect(info->wnd, &hscroll_bounds);
		hscroll_bounds.top = hscroll_bounds.bottom - hscroll_height;
		if (show_vscroll)
			hscroll_bounds.right -= vscroll_width;

		// if we hid the scrollbars, make sure we reset the top/left corners
		if (top_row + visible_rows > total_rows)
			top_row = (total_rows > visible_rows) ? (total_rows - visible_rows) : 0;
		if (left_col + visible_cols > total_cols)
			left_col = (total_cols > visible_cols) ? (total_cols - visible_cols) : 0;

		// fill out the scroll info struct for the vertical scrollbar
		scrollinfo.cbSize = sizeof(scrollinfo);
		scrollinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		scrollinfo.nMin = 0;
		scrollinfo.nMax = total_rows - 1;
		scrollinfo.nPage = visible_rows;
		scrollinfo.nPos = top_row;
		SetScrollInfo(info->vscroll, SB_CTL, &scrollinfo, TRUE);

		// fill out the scroll info struct for the horizontal scrollbar
		scrollinfo.cbSize = sizeof(scrollinfo);
		scrollinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		scrollinfo.nMin = 0;
		scrollinfo.nMax = total_cols - 1;
		scrollinfo.nPage = visible_cols;
		scrollinfo.nPos = left_col;
		SetScrollInfo(info->hscroll, SB_CTL, &scrollinfo, TRUE);

		// update window info
		visible_rows++;
		visible_cols++;
		debug_view_set_property_UINT32(view, DVP_VISIBLE_ROWS, visible_rows);
		debug_view_set_property_UINT32(view, DVP_VISIBLE_COLS, visible_cols);
		debug_view_set_property_UINT32(view, DVP_TOP_ROW, top_row);
		debug_view_set_property_UINT32(view, DVP_LEFT_COL, left_col);

		// invalidate the bounds
		InvalidateRect(info->wnd, NULL, FALSE);

		// adjust the bounds of the scrollbars and show/hide them
		if (info->vscroll)
		{
			if (show_vscroll)
				smart_set_window_bounds(info->vscroll, info->wnd, &vscroll_bounds);
			smart_show_window(info->vscroll, show_vscroll);
		}
		if (info->hscroll)
		{
			if (show_hscroll)
				smart_set_window_bounds(info->hscroll, info->wnd, &hscroll_bounds);
			smart_show_window(info->hscroll, show_hscroll);
		}
	}
}



//============================================================
//  debug_view_find
//============================================================

static debugview_info *debug_view_find(debug_view *view)
{
	debugwin_info *info;
	int curview;

	// loop over windows and find the view
	for (info = window_list; info; info = info->next)
		for (curview = 0; curview < MAX_VIEWS; curview++)
			if (info->view[curview].view == view)
				return &info->view[curview];
	return NULL;
}



//============================================================
//  debug_view_process_scroll
//============================================================

static UINT32 debug_view_process_scroll(debugview_info *info, WORD type, HWND wnd)
{
	SCROLLINFO scrollinfo;
	INT32 maxval;
	INT32 result;

	// get the current info
	scrollinfo.cbSize = sizeof(scrollinfo);
	scrollinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
	GetScrollInfo(wnd, SB_CTL, &scrollinfo);

	// by default we stay put
	result = scrollinfo.nPos;

	// determine the maximum value
	maxval = (scrollinfo.nMax > scrollinfo.nPage) ? (scrollinfo.nMax - scrollinfo.nPage + 1) : 0;

	// handle the message
	switch (type)
	{
		case SB_THUMBTRACK:
			result = scrollinfo.nTrackPos;
			break;

		case SB_LEFT:
			result = 0;
			break;

		case SB_RIGHT:
			result = maxval;
			break;

		case SB_LINELEFT:
			result -= 1;
			break;

		case SB_LINERIGHT:
			result += 1;
			break;

		case SB_PAGELEFT:
			result -= scrollinfo.nPage - 1;
			break;

		case SB_PAGERIGHT:
			result += scrollinfo.nPage - 1;
			break;
	}

	// generic rangecheck
	if (result < 0)
		result = 0;
	if (result > maxval)
		result = maxval;

	// set the new position
	scrollinfo.fMask = SIF_POS;
	scrollinfo.nPos = result;
	SetScrollInfo(wnd, SB_CTL, &scrollinfo, TRUE);

	// note if we are at the bottom
	if (wnd == info->vscroll && info->is_textbuf && result >= maxval - 1)
		return (UINT32)-1;

	return (UINT32)result;
}



//============================================================
//  debug_view_prev_view
//============================================================

static void debug_view_prev_view(debugwin_info *info, debugview_info *curview)
{
	int curindex = 1;
	int numviews;

	// count the number of views
	for (numviews = 0; numviews < MAX_VIEWS; numviews++)
		if (info->view[numviews].wnd == NULL)
			break;

	// if we have a curview, find out its index
	if (curview)
		curindex = curview - &info->view[0];

	// loop until we find someone to take focus
	while (1)
	{
		// advance to the previous index
		curindex--;
		if (curindex < -1)
			curindex = numviews - 1;

		// negative numbers mean the focuswnd
		if (curindex < 0 && info->focuswnd != NULL)
		{
			SetFocus(info->focuswnd);
			break;
		}

		// positive numbers mean a view
		else if (curindex >= 0 && info->view[curindex].wnd != NULL && debug_view_get_property_UINT32(info->view[curindex].view, DVP_SUPPORTS_CURSOR))
		{
			SetFocus(info->view[curindex].wnd);
			break;
		}
	}
}



//============================================================
//  debug_view_next_view
//============================================================

static void debug_view_next_view(debugwin_info *info, debugview_info *curview)
{
	int curindex = -1;
	int numviews;

	// count the number of views
	for (numviews = 0; numviews < MAX_VIEWS; numviews++)
		if (info->view[numviews].wnd == NULL)
			break;

	// if we have a curview, find out its index
	if (curview)
		curindex = curview - &info->view[0];

	// loop until we find someone to take focus
	while (1)
	{
		// advance to the previous index
		curindex++;
		if (curindex >= numviews)
			curindex = -1;

		// negative numbers mean the focuswnd
		if (curindex < 0 && info->focuswnd != NULL)
		{
			SetFocus(info->focuswnd);
			break;
		}

		// positive numbers mean a view
		else if (curindex >= 0 && info->view[curindex].wnd != NULL && debug_view_get_property_UINT32(info->view[curindex].view, DVP_SUPPORTS_CURSOR))
		{
			SetFocus(info->view[curindex].wnd);
			break;
		}
	}
}



//============================================================
//  debug_view_proc
//============================================================

static LRESULT CALLBACK debug_view_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	debugview_info *info = (debugview_info *)(UINT32)GetWindowLongPtr(wnd, GWLP_USERDATA);

	// handle a few messages
	switch (message)
	{
		// set the info pointer
		case WM_CREATE:
		{
			CREATESTRUCT *createinfo = (CREATESTRUCT *)lparam;
			SetWindowLongPtr(wnd, GWLP_USERDATA, (UINT32)createinfo->lpCreateParams);
			break;
		}

		// paint: redraw the last bitmap
		case WM_PAINT:
		{
			PAINTSTRUCT pstruct;
			HDC dc = BeginPaint(wnd, &pstruct);
			debug_view_draw_contents(info, dc);
			EndPaint(wnd, &pstruct);
			break;
		}

		// keydown: handle debugger keys
		case WM_KEYDOWN:
		{
			if ((*info->owner->handle_key)(info->owner, wparam, lparam))
				info->owner->ignore_char_lparam = lparam >> 16;
			else
			{
				switch (wparam)
				{
					case VK_UP:
						debug_view_set_property_UINT32(info->view, DVP_CHARACTER, DCH_UP);
						info->owner->ignore_char_lparam = lparam >> 16;
						break;
					case VK_DOWN:
						debug_view_set_property_UINT32(info->view, DVP_CHARACTER, DCH_DOWN);
						info->owner->ignore_char_lparam = lparam >> 16;
						break;
					case VK_LEFT:
						debug_view_set_property_UINT32(info->view, DVP_CHARACTER, DCH_LEFT);
						info->owner->ignore_char_lparam = lparam >> 16;
						break;
					case VK_RIGHT:
						debug_view_set_property_UINT32(info->view, DVP_CHARACTER, DCH_RIGHT);
						info->owner->ignore_char_lparam = lparam >> 16;
						break;
					case VK_ESCAPE:
						if (info->owner->focuswnd != NULL)
							SetFocus(info->owner->focuswnd);
						info->owner->ignore_char_lparam = lparam >> 16;
						break;
					case VK_TAB:
						if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
							debug_view_prev_view(info->owner, info);
						else
							debug_view_next_view(info->owner, info);
						info->owner->ignore_char_lparam = lparam >> 16;
						break;
				}
			}
			break;
		}

		// char: ignore chars associated with keys we've handled
		case WM_CHAR:
		{
			if (info->owner->ignore_char_lparam != (lparam >> 16))
			{
				if (wparam >= 32 && wparam < 127 && debug_view_get_property_UINT32(info->view, DVP_SUPPORTS_CURSOR))
					debug_view_set_property_UINT32(info->view, DVP_CHARACTER, wparam);
				else
					return DefWindowProc(wnd, message, wparam, lparam);
			}
			else
				info->owner->ignore_char_lparam = 0;
			break;
		}

		// gaining focus
		case WM_SETFOCUS:
		{
			if (debug_view_get_property_UINT32(info->view, DVP_SUPPORTS_CURSOR))
				debug_view_set_property_UINT32(info->view, DVP_CURSOR_VISIBLE, 1);
			break;
		}

		// losing focus
		case WM_KILLFOCUS:
		{
			if (debug_view_get_property_UINT32(info->view, DVP_SUPPORTS_CURSOR))
				debug_view_set_property_UINT32(info->view, DVP_CURSOR_VISIBLE, 0);
			break;
		}

		// mouse click
		case WM_LBUTTONDOWN:
		{
			if (debug_view_get_property_UINT32(info->view, DVP_SUPPORTS_CURSOR))
			{
				int x = GET_X_LPARAM(lparam) / debug_font_width;
				int y = GET_Y_LPARAM(lparam) / debug_font_height;
				debug_view_begin_update(info->view);
				debug_view_set_property_UINT32(info->view, DVP_CURSOR_ROW, debug_view_get_property_UINT32(info->view, DVP_TOP_ROW) + y);
				debug_view_set_property_UINT32(info->view, DVP_CURSOR_COL, debug_view_get_property_UINT32(info->view, DVP_LEFT_COL) + x);
				debug_view_end_update(info->view);
				SetFocus(wnd);
			}
 			break;
		}

		// hscroll
		case WM_HSCROLL:
		{
			UINT32 left_col = debug_view_process_scroll(info, LOWORD(wparam), (HWND)lparam);
			debug_view_set_property_UINT32(info->view, DVP_LEFT_COL, left_col);
			break;
		}

		// vscroll
		case WM_VSCROLL:
		{
			UINT32 top_row = debug_view_process_scroll(info, LOWORD(wparam), (HWND)lparam);
			if (info->is_textbuf)
				debug_view_set_property_UINT32(info->view, DVP_TEXTBUF_LINE_LOCK, top_row);
			else
				debug_view_set_property_UINT32(info->view, DVP_TOP_ROW, top_row);
			break;
		}

		// everything else: defaults
		default:
			return DefWindowProc(wnd, message, wparam, lparam);
	}

	return 0;
}



//============================================================
//  debug_edit_proc
//============================================================

static LRESULT CALLBACK debug_edit_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	debugwin_info *info = (debugwin_info *)(UINT32)GetWindowLongPtr(wnd, GWLP_USERDATA);
	TCHAR buffer[MAX_EDIT_STRING];

	// handle a few messages
	switch (message)
	{
		// key down: handle navigation in the attached view
		case WM_KEYDOWN:
			switch (wparam)
			{
				case VK_UP:
					if (info->last_history < info->history_count - 1)
						info->last_history++;
					else
						info->last_history = 0;
					SendMessage(wnd, WM_SETTEXT, (WPARAM)0, (LPARAM)&info->history[info->last_history][0]);
					SendMessage(wnd, EM_SETSEL, (WPARAM)MAX_EDIT_STRING, (LPARAM)MAX_EDIT_STRING);
					break;

				case VK_DOWN:
					if (info->last_history > 0)
						info->last_history--;
					else if (info->history_count > 0)
						info->last_history = info->history_count - 1;
					else
						info->last_history = 0;
					SendMessage(wnd, WM_SETTEXT, (WPARAM)0, (LPARAM)&info->history[info->last_history][0]);
					SendMessage(wnd, EM_SETSEL, (WPARAM)MAX_EDIT_STRING, (LPARAM)MAX_EDIT_STRING);
					break;

				case VK_PRIOR:
					if (info->view[0].wnd && info->view[0].vscroll)
						SendMessage(info->view[0].wnd, WM_VSCROLL, SB_PAGELEFT, (LPARAM)info->view[0].vscroll);
					break;

				case VK_NEXT:
					if (info->view[0].wnd && info->view[0].vscroll)
						SendMessage(info->view[0].wnd, WM_VSCROLL, SB_PAGERIGHT, (LPARAM)info->view[0].vscroll);
					break;

				case VK_TAB:
					if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
						debug_view_prev_view(info, NULL);
					else
						debug_view_next_view(info, NULL);
					info->ignore_char_lparam = lparam >> 16;
					break;

				default:
					if (!(*info->handle_key)(info, wparam, lparam))
						return CallWindowProc(info->original_editproc, wnd, message, wparam, lparam);
					else
						info->ignore_char_lparam = lparam >> 16;
					break;
			}
			break;

		// char: handle the return key
		case WM_CHAR:

			// ignore chars associated with keys we've handled
			if (info->ignore_char_lparam != (lparam >> 16))
			{
				switch (wparam)
				{
					case 13:
					{
						// fetch the text
						SendMessage(wnd, WM_GETTEXT, (WPARAM)sizeof(buffer), (LPARAM)buffer);

						// add to the history if it's not a repeat of the last one
						if (buffer[0] != 0 && strcmp(buffer, &info->history[0][0]))
						{
							memmove(&info->history[1][0], &info->history[0][0], (HISTORY_LENGTH - 1) * MAX_EDIT_STRING);
							strcpy(&info->history[0][0], buffer);
							if (info->history_count < HISTORY_LENGTH)
								info->history_count++;
						}
						info->last_history = info->history_count - 1;

						// process
						if (info->process_string)
							(*info->process_string)(info, buffer);
						break;
					}

					case 27:
					{
						// fetch the text
						SendMessage(wnd, WM_GETTEXT, (WPARAM)sizeof(buffer), (LPARAM)buffer);

						// if it's not empty, clear the text
						if (strlen(buffer) > 0)
						{
							info->ignore_char_lparam = lparam >> 16;
							SendMessage(wnd, WM_SETTEXT, (WPARAM)0, (LPARAM)info->edit_defstr);
						}
						break;
					}

					default:
						return CallWindowProc(info->original_editproc, wnd, message, wparam, lparam);
				}
			}
			else
				info->ignore_char_lparam = 0;
			break;

		// everything else: defaults
		default:
			return CallWindowProc(info->original_editproc, wnd, message, wparam, lparam);
	}

	return 0;
}



//============================================================
//  generic_create_window
//============================================================

static void generic_create_window(int type)
{
	debugwin_info *info;
	TCHAR title[256];

	// create the window
	sprintf(title, "Debug: %s [%s]", Machine->gamedrv->description, Machine->gamedrv->name);
	info = debug_window_create(title, NULL);
	if (!info || !debug_view_create(info, 0, type))
		return;

	// set the child function
	info->recompute_children = generic_recompute_children;

	// recompute the children once to get the maxwidth
	generic_recompute_children(info);

	// position the window and recompute children again
	SetWindowPos(info->wnd, HWND_TOP, 100, 100, info->maxwidth, 200, SWP_SHOWWINDOW);
	generic_recompute_children(info);
}



//============================================================
//  generic_recompute_children
//============================================================

static void generic_recompute_children(debugwin_info *info)
{
	RECT parent;
	RECT bounds;
	UINT32 width;

	// get the view width
	width = debug_view_get_property_UINT32(info->view[0].view, DVP_TOTAL_COLS);

	// compute a client rect
	bounds.top = bounds.left = 0;
	bounds.right = width * debug_font_width + vscroll_width + 2 * EDGE_WIDTH;
	bounds.bottom = 200;
	AdjustWindowRectEx(&bounds, DEBUG_WINDOW_STYLE, FALSE, DEBUG_WINDOW_STYLE_EX);

	// clamp the min/max size
	info->maxwidth = bounds.right - bounds.left;

	// get the parent's dimensions
	GetClientRect(info->wnd, &parent);

	// view gets the remaining space
	InflateRect(&parent, -EDGE_WIDTH, -EDGE_WIDTH);
	debug_view_set_bounds(&info->view[0], info->wnd, &parent);
}



//============================================================
//  log_create_window
//============================================================

static void log_create_window(void)
{
	debugwin_info *info;
	TCHAR title[256];
	UINT32 width;
	RECT bounds;

	// create the window
	sprintf(title, "Errorlog: %s [%s]", Machine->gamedrv->description, Machine->gamedrv->name);
	info = debug_window_create(title, NULL);
	if (!info || !debug_view_create(info, 0, DVT_LOG))
		return;
	info->view->is_textbuf = TRUE;

	// set the child function
	info->recompute_children = generic_recompute_children;

	// get the view width
	width = debug_view_get_property_UINT32(info->view[0].view, DVP_TOTAL_COLS);

	// compute a client rect
	bounds.top = bounds.left = 0;
	bounds.right = width * debug_font_width + vscroll_width + 2 * EDGE_WIDTH;
	bounds.bottom = 200;
	AdjustWindowRectEx(&bounds, DEBUG_WINDOW_STYLE, FALSE, DEBUG_WINDOW_STYLE_EX);

	// clamp the min/max size
	info->maxwidth = bounds.right - bounds.left;

	// position the window at the bottom-right
	SetWindowPos(info->wnd, HWND_TOP,
				100, 100,
				bounds.right - bounds.left, bounds.bottom - bounds.top,
				SWP_SHOWWINDOW);

	// recompute the children and set focus on the edit box
	generic_recompute_children(info);
}



//============================================================
//  memory_determine_combo_items
//============================================================

static void memory_determine_combo_items(void)
{
	memorycombo_item **tail = &memorycombo;
	UINT32 cpunum, spacenum;
	int rgnnum, itemnum;

	// first add all the CPUs' address spaces
	for (cpunum = 0; cpunum < MAX_CPU; cpunum++)
	{
		const debug_cpu_info *cpuinfo = debug_get_cpu_info(cpunum);
		if (cpuinfo->valid)
			for (spacenum = 0; spacenum < ADDRESS_SPACES; spacenum++)
				if (cpuinfo->space[spacenum].databytes)
				{
					memorycombo_item *ci = malloc_or_die(sizeof(*ci));
					memset(ci, 0, sizeof(*ci));
					ci->cpunum = cpunum;
					ci->spacenum = spacenum;
					ci->prefsize = MIN(cpuinfo->space[spacenum].databytes, 4);
					sprintf(ci->name, "CPU #%d (%s) %s memory", cpunum, cpunum_name(cpunum), address_space_names[spacenum]);
					*tail = ci;
					tail = &ci->next;
				}
	}

	// then add all the memory regions
	for (rgnnum = 0; rgnnum < MAX_MEMORY_REGIONS; rgnnum++)
	{
		UINT8 *base = memory_region(rgnnum);
		UINT32 type = memory_region_type(rgnnum);
		if (base != NULL && type > REGION_INVALID && (type - REGION_INVALID) < ARRAY_LENGTH(memory_region_names))
		{
			memorycombo_item *ci = malloc_or_die(sizeof(*ci));
			UINT32 flags = memory_region_flags(rgnnum);
			UINT8 width, little_endian;
			memset(ci, 0, sizeof(*ci));
			ci->base = base;
			ci->length = memory_region_length(rgnnum);
			width = 1 << (flags & ROMREGION_WIDTHMASK);
			little_endian = ((flags & ROMREGION_ENDIANMASK) == ROMREGION_LE);
			if (type >= REGION_CPU1 && type <= REGION_CPU8)
			{
				const debug_cpu_info *cpuinfo = debug_get_cpu_info(type - REGION_CPU1);
				if (cpuinfo)
				{
					width = cpuinfo->space[ADDRESS_SPACE_PROGRAM].databytes;
					little_endian = (cpuinfo->endianness == CPU_IS_LE);
				}
			}
			ci->prefsize = MIN(width, 4);
			ci->offset_xor = width - 1;
			ci->little_endian = little_endian;
			strcpy(ci->name, memory_region_names[type - REGION_INVALID]);
			*tail = ci;
			tail = &ci->next;
		}
	}

	// finally add all global array symbols
	for (itemnum = 0; itemnum < 10000; itemnum++)
	{
		UINT32 valsize, valcount;
		const char *name;
		void *base;

		/* stop when we run out of items */
		name = state_save_get_indexed_item(itemnum, &base, &valsize, &valcount);
		if (name == NULL)
			break;

		/* if this is a single-entry global, add it */
		if (valcount > 1 && strstr(name, "/globals/"))
		{
			memorycombo_item *ci = malloc_or_die(sizeof(*ci));
			memset(ci, 0, sizeof(*ci));
			ci->base = base;
			ci->length = valcount * valsize;
			ci->prefsize = MIN(valsize, 4);
			ci->little_endian = TRUE;
			strcpy(ci->name, strrchr(name, '/') + 1);
			*tail = ci;
			tail = &ci->next;
		}
	}
}


//============================================================
//  memory_update_selection
//============================================================

static void memory_update_selection(debugwin_info *info, memorycombo_item *ci)
{
	debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_CPUNUM, ci->cpunum);
	debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_SPACENUM, ci->spacenum);
	debug_view_set_property_ptr(info->view[0].view, DVP_MEM_RAW_BASE, ci->base);
	debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_RAW_LENGTH, ci->length);
	debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_RAW_OFFSET_XOR, ci->offset_xor);
	debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_RAW_LITTLE_ENDIAN, ci->little_endian);
	debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_BYTES_PER_CHUNK, ci->prefsize);
	SetWindowText(info->wnd, ci->name);
}


//============================================================
//  memory_create_window
//============================================================

static void memory_create_window(void)
{
	int curcpu = cpu_getactivecpu(), cursel = 0;
	memorycombo_item *ci, *selci = NULL;
	debugwin_info *info;
	HMENU optionsmenu;

	// create the window
	info = debug_window_create("Memory", NULL);
	if (!info || !debug_view_create(info, 0, DVT_MEMORY))
		return;

	// set the handlers
	info->handle_command = memory_handle_command;
	info->handle_key = memory_handle_key;

	// create the options menu
	optionsmenu = CreatePopupMenu();
	AppendMenu(optionsmenu, MF_ENABLED, ID_1_BYTE_CHUNKS, "1-byte chunks\tCtrl+1");
	AppendMenu(optionsmenu, MF_ENABLED, ID_2_BYTE_CHUNKS, "2-byte chunks\tCtrl+2");
	AppendMenu(optionsmenu, MF_ENABLED, ID_4_BYTE_CHUNKS, "4-byte chunks\tCtrl+4");
	AppendMenu(optionsmenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(optionsmenu, MF_ENABLED, ID_REVERSE_VIEW, "Reverse View\tCtrl+R");
	AppendMenu(optionsmenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(optionsmenu, MF_ENABLED, ID_INCREASE_MEM_WIDTH, "Increase bytes per line\tCtrl+P");
	AppendMenu(optionsmenu, MF_ENABLED, ID_DECREASE_MEM_WIDTH, "Decrease bytes per line\tCtrl+O");
	AppendMenu(GetMenu(info->wnd), MF_ENABLED | MF_POPUP, (UINT_PTR)optionsmenu, "Options");
	memory_update_checkmarks(info);

	// set up the view to track the initial expression
	debug_view_begin_update(info->view[0].view);
	debug_view_set_property_string(info->view[0].view, DVP_MEM_EXPRESSION, "0");
	strcpy(info->edit_defstr, "0");
	debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_TRACK_LIVE, 1);
	debug_view_end_update(info->view[0].view);

	// create an edit box and override its key handling
	info->editwnd = CreateWindowEx(EDIT_BOX_STYLE_EX, TEXT("EDIT"), NULL, EDIT_BOX_STYLE,
			0, 0, 100, 100, info->wnd, NULL, GetModuleHandle(NULL), NULL);
	info->original_editproc = (void *)(UINT32)GetWindowLongPtr(info->editwnd, GWLP_WNDPROC);
	SetWindowLongPtr(info->editwnd, GWLP_USERDATA, (UINT32)info);
	SetWindowLongPtr(info->editwnd, GWLP_WNDPROC, (UINT32)debug_edit_proc);
	SendMessage(info->editwnd, WM_SETFONT, (WPARAM)debug_font, (LPARAM)FALSE);
	SendMessage(info->editwnd, WM_SETTEXT, (WPARAM)0, (LPARAM)"0");
	SendMessage(info->editwnd, EM_LIMITTEXT, (WPARAM)MAX_EDIT_STRING, (LPARAM)0);
	SendMessage(info->editwnd, EM_SETSEL, (WPARAM)0, (LPARAM)-1);

	// create a combo box
	info->otherwnd[0] = CreateWindowEx(COMBO_BOX_STYLE_EX, TEXT("COMBOBOX"), NULL, COMBO_BOX_STYLE,
			0, 0, 100, 100, info->wnd, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(info->otherwnd[0], GWLP_USERDATA, (UINT32)info);
	SendMessage(info->otherwnd[0], WM_SETFONT, (WPARAM)debug_font, (LPARAM)FALSE);

	// populate the combobox
	if (!memorycombo)
		memory_determine_combo_items();
	for (ci = memorycombo; ci; ci = ci->next)
	{
		int item = SendMessage(info->otherwnd[0], CB_ADDSTRING, 0, (LPARAM)ci->name);
		if (ci->base == NULL && ci->cpunum == curcpu && ci->spacenum == ADDRESS_SPACE_PROGRAM)
		{
			cursel = item;
			selci = ci;
		}
	}
	SendMessage(info->otherwnd[0], CB_SETCURSEL, cursel, 0);

	// set the child functions
	info->recompute_children = memory_recompute_children;
	info->process_string = memory_process_string;

	// set the CPUnum and spacenum properties
	if (selci == NULL)
		selci = memorycombo;
	memory_update_selection(info, selci);

	// recompute the children once to get the maxwidth
	memory_recompute_children(info);

	// position the window and recompute children again
	SetWindowPos(info->wnd, HWND_TOP, 100, 100, info->maxwidth, 200, SWP_SHOWWINDOW);
	memory_recompute_children(info);

	// mark the edit box as the default focus and set it
	info->focuswnd = info->editwnd;
	SetFocus(info->editwnd);
}



//============================================================
//  memory_recompute_children
//============================================================

static void memory_recompute_children(debugwin_info *info)
{
	RECT parent, memrect, editrect, comborect;
	RECT bounds;
	UINT32 width;

	// get the view width
	width = debug_view_get_property_UINT32(info->view[0].view, DVP_TOTAL_COLS);

	// compute a client rect
	bounds.top = bounds.left = 0;
	bounds.right = width * debug_font_width + vscroll_width + 2 * EDGE_WIDTH;
	bounds.bottom = 200;
	AdjustWindowRectEx(&bounds, DEBUG_WINDOW_STYLE, FALSE, DEBUG_WINDOW_STYLE_EX);

	// clamp the min/max size
	info->maxwidth = bounds.right - bounds.left;

	// get the parent's dimensions
	GetClientRect(info->wnd, &parent);

	// edit box gets half of the width
	editrect.top = parent.top + EDGE_WIDTH;
	editrect.bottom = editrect.top + debug_font_height + 4;
	editrect.left = parent.left + EDGE_WIDTH;
	editrect.right = parent.left + (parent.right - parent.left) / 2 - EDGE_WIDTH;

	// combo box gets the other half of the width
	comborect.top = editrect.top;
	comborect.bottom = editrect.bottom;
	comborect.left = editrect.right + 2 * EDGE_WIDTH;
	comborect.right = parent.right - EDGE_WIDTH;

	// memory view gets the rest
	memrect.top = editrect.bottom + 2 * EDGE_WIDTH;
	memrect.bottom = parent.bottom - EDGE_WIDTH;
	memrect.left = parent.left + EDGE_WIDTH;
	memrect.right = parent.right - EDGE_WIDTH;

	// set the bounds of things
	debug_view_set_bounds(&info->view[0], info->wnd, &memrect);
	smart_set_window_bounds(info->editwnd, info->wnd, &editrect);
	smart_set_window_bounds(info->otherwnd[0], info->wnd, &comborect);
}



//============================================================
//  memory_process_string
//============================================================

static void memory_process_string(debugwin_info *info, const char *string)
{
	// set the string to the memory view
	debug_view_set_property_string(info->view[0].view, DVP_MEM_EXPRESSION, string);

	// select everything in the edit text box
	SendMessage(info->editwnd, EM_SETSEL, (WPARAM)0, (LPARAM)-1);

	// update the default string to match
	strncpy(info->edit_defstr, string, sizeof(info->edit_defstr) - 1);
}



//============================================================
//  memory_update_checkmarks
//============================================================

static void memory_update_checkmarks(debugwin_info *info)
{
	CheckMenuItem(GetMenu(info->wnd), ID_1_BYTE_CHUNKS, MF_BYCOMMAND | (debug_view_get_property_UINT32(info->view[0].view, DVP_MEM_BYTES_PER_CHUNK) == 1? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(GetMenu(info->wnd), ID_2_BYTE_CHUNKS, MF_BYCOMMAND | (debug_view_get_property_UINT32(info->view[0].view, DVP_MEM_BYTES_PER_CHUNK) == 2 ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(GetMenu(info->wnd), ID_4_BYTE_CHUNKS, MF_BYCOMMAND | (debug_view_get_property_UINT32(info->view[0].view, DVP_MEM_BYTES_PER_CHUNK) == 4 ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(GetMenu(info->wnd), ID_REVERSE_VIEW, MF_BYCOMMAND | (debug_view_get_property_UINT32(info->view[0].view, DVP_MEM_REVERSE_VIEW) ? MF_CHECKED : MF_UNCHECKED));
}



//============================================================
//  memory_handle_command
//============================================================

static int memory_handle_command(debugwin_info *info, WPARAM wparam, LPARAM lparam)
{
	switch (HIWORD(wparam))
	{
		// combo box selection changed
		case CBN_SELCHANGE:
		{
			int sel = SendMessage((HWND)lparam, CB_GETCURSEL, 0, 0);
			if (sel != CB_ERR)
			{
				// find the matching entry
				memorycombo_item *ci;
				for (ci = memorycombo; ci; ci = ci->next)
					if (sel-- == 0)
					{
						debug_view_begin_update(info->view[0].view);
						memory_update_selection(info, ci);
						debug_view_end_update(info->view[0].view);
					}

				// reset the focus
				SetFocus(info->focuswnd);
				return 1;
			}
			break;
		}

		// menu selections
		case 0:
			switch (LOWORD(wparam))
			{
				case ID_1_BYTE_CHUNKS:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_BYTES_PER_CHUNK, 1);
					debug_view_end_update(info->view[0].view);
					memory_update_checkmarks(info);
					return 1;

				case ID_2_BYTE_CHUNKS:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_BYTES_PER_CHUNK, 2);
					debug_view_end_update(info->view[0].view);
					memory_update_checkmarks(info);
					return 1;

				case ID_4_BYTE_CHUNKS:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_BYTES_PER_CHUNK, 4);
					debug_view_end_update(info->view[0].view);
					memory_update_checkmarks(info);
					return 1;

				case ID_REVERSE_VIEW:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_REVERSE_VIEW, !debug_view_get_property_UINT32(info->view[0].view, DVP_MEM_REVERSE_VIEW));
					debug_view_end_update(info->view[0].view);
					memory_update_checkmarks(info);
					return 1;

				case ID_INCREASE_MEM_WIDTH:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_WIDTH, debug_view_get_property_UINT32(info->view[0].view, DVP_MEM_WIDTH) + 1);
					debug_view_end_update(info->view[0].view);
					return 1;

				case ID_DECREASE_MEM_WIDTH:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_MEM_WIDTH, debug_view_get_property_UINT32(info->view[0].view, DVP_MEM_WIDTH) - 1);
					debug_view_end_update(info->view[0].view);
					return 1;
			}
			break;
	}
	return global_handle_command(info, wparam, lparam);
}



//============================================================
//  memory_handle_key
//============================================================

static int memory_handle_key(debugwin_info *info, WPARAM wparam, LPARAM lparam)
{
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
	{
		switch (wparam)
		{
			case '1':
				SendMessage(info->wnd, WM_COMMAND, ID_1_BYTE_CHUNKS, 0);
				return 1;

			case '2':
				SendMessage(info->wnd, WM_COMMAND, ID_2_BYTE_CHUNKS, 0);
				return 1;

			case '4':
				SendMessage(info->wnd, WM_COMMAND, ID_4_BYTE_CHUNKS, 0);
				return 1;

			case 'R':
				SendMessage(info->wnd, WM_COMMAND, ID_REVERSE_VIEW, 0);
				return 1;

			case 'P':
				SendMessage(info->wnd, WM_COMMAND, ID_INCREASE_MEM_WIDTH, 0);
				return 1;

			case 'O':
				SendMessage(info->wnd, WM_COMMAND, ID_DECREASE_MEM_WIDTH, 0);
				return 1;
		}
	}
	return global_handle_key(info, wparam, lparam);
}



//============================================================
//  disasm_create_window
//============================================================

static void disasm_create_window(void)
{
	int curcpu = cpu_getactivecpu(), cursel = 0;
	debugwin_info *info;
	HMENU optionsmenu;
	UINT32 cpunum;

	// create the window
	info = debug_window_create("Disassembly", NULL);
	if (!info || !debug_view_create(info, 0, DVT_DISASSEMBLY))
		return;

	// create the options menu
	optionsmenu = CreatePopupMenu();
	AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_RAW, "Raw opcodes\tCtrl+R");
	AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_ENCRYPTED, "Encrypted opcodes\tCtrl+E");
	AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_COMMENTS, "Comments\tCtrl+C");
	AppendMenu(GetMenu(info->wnd), MF_ENABLED | MF_POPUP, (UINT_PTR)optionsmenu, "Options");
	disasm_update_checkmarks(info);

	// set the handlers
	info->handle_command = disasm_handle_command;
	info->handle_key = disasm_handle_key;

	// set up the view to track the initial expression
	debug_view_begin_update(info->view[0].view);
	debug_view_set_property_string(info->view[0].view, DVP_DASM_EXPRESSION, "pc");
	strcpy(info->edit_defstr, "pc");
	debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_TRACK_LIVE, 1);
	debug_view_end_update(info->view[0].view);

	// create an edit box and override its key handling
	info->editwnd = CreateWindowEx(EDIT_BOX_STYLE_EX, TEXT("EDIT"), NULL, EDIT_BOX_STYLE,
			0, 0, 100, 100, info->wnd, NULL, GetModuleHandle(NULL), NULL);
	info->original_editproc = (void *)(UINT32)GetWindowLongPtr(info->editwnd, GWLP_WNDPROC);
	SetWindowLongPtr(info->editwnd, GWLP_USERDATA, (UINT32)info);
	SetWindowLongPtr(info->editwnd, GWLP_WNDPROC, (UINT32)debug_edit_proc);
	SendMessage(info->editwnd, WM_SETFONT, (WPARAM)debug_font, (LPARAM)FALSE);
	SendMessage(info->editwnd, WM_SETTEXT, (WPARAM)0, (LPARAM)"pc");
	SendMessage(info->editwnd, EM_LIMITTEXT, (WPARAM)MAX_EDIT_STRING, (LPARAM)0);
	SendMessage(info->editwnd, EM_SETSEL, (WPARAM)0, (LPARAM)-1);

	// create a combo box
	info->otherwnd[0] = CreateWindowEx(COMBO_BOX_STYLE_EX, TEXT("COMBOBOX"), NULL, COMBO_BOX_STYLE,
			0, 0, 100, 100, info->wnd, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(info->otherwnd[0], GWLP_USERDATA, (UINT32)info);
	SendMessage(info->otherwnd[0], WM_SETFONT, (WPARAM)debug_font, (LPARAM)FALSE);

	// populate the combobox
	for (cpunum = 0; cpunum < MAX_CPU; cpunum++)
	{
		const debug_cpu_info *cpuinfo = debug_get_cpu_info(cpunum);
		if (cpuinfo->valid)
			if (cpuinfo->space[ADDRESS_SPACE_PROGRAM].databytes)
			{
				char name[100];
				int item;
				sprintf(name, "CPU #%d (%s)", cpunum, cpunum_name(cpunum));
				item = SendMessage(info->otherwnd[0], CB_ADDSTRING, 0, (LPARAM)name);
				if (cpunum == curcpu)
					cursel = item;
			}
	}
	SendMessage(info->otherwnd[0], CB_SETCURSEL, cursel, 0);

	// set the child functions
	info->recompute_children = disasm_recompute_children;
	info->process_string = disasm_process_string;

	// set the CPUnum and spacenum properties
	debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_CPUNUM, (curcpu == -1) ? 0 : curcpu);

	// set the caption
	disasm_update_caption(info->wnd);

	// recompute the children once to get the maxwidth
	disasm_recompute_children(info);

	// position the window and recompute children again
	SetWindowPos(info->wnd, HWND_TOP, 100, 100, info->maxwidth, 200, SWP_SHOWWINDOW);
	disasm_recompute_children(info);

	// mark the edit box as the default focus and set it
	info->focuswnd = info->editwnd;
	SetFocus(info->editwnd);
}



//============================================================
//  disasm_recompute_children
//============================================================

static void disasm_recompute_children(debugwin_info *info)
{
	RECT parent, dasmrect, editrect, comborect;
	RECT bounds;
	UINT32 width;

	// get the view width
	width = debug_view_get_property_UINT32(info->view[0].view, DVP_TOTAL_COLS);

	// compute a client rect
	bounds.top = bounds.left = 0;
	bounds.right = width * debug_font_width + vscroll_width + 2 * EDGE_WIDTH;
	bounds.bottom = 200;
	AdjustWindowRectEx(&bounds, DEBUG_WINDOW_STYLE, FALSE, DEBUG_WINDOW_STYLE_EX);

	// clamp the min/max size
	info->maxwidth = bounds.right - bounds.left;

	// get the parent's dimensions
	GetClientRect(info->wnd, &parent);

	// edit box gets half of the width
	editrect.top = parent.top + EDGE_WIDTH;
	editrect.bottom = editrect.top + debug_font_height + 4;
	editrect.left = parent.left + EDGE_WIDTH;
	editrect.right = parent.left + (parent.right - parent.left) / 2 - EDGE_WIDTH;

	// combo box gets the other half of the width
	comborect.top = editrect.top;
	comborect.bottom = editrect.bottom;
	comborect.left = editrect.right + 2 * EDGE_WIDTH;
	comborect.right = parent.right - EDGE_WIDTH;

	// disasm view gets the rest
	dasmrect.top = editrect.bottom + 2 * EDGE_WIDTH;
	dasmrect.bottom = parent.bottom - EDGE_WIDTH;
	dasmrect.left = parent.left + EDGE_WIDTH;
	dasmrect.right = parent.right - EDGE_WIDTH;

	// set the bounds of things
	debug_view_set_bounds(&info->view[0], info->wnd, &dasmrect);
	smart_set_window_bounds(info->editwnd, info->wnd, &editrect);
	smart_set_window_bounds(info->otherwnd[0], info->wnd, &comborect);
}



//============================================================
//  disasm_process_string
//============================================================

static void disasm_process_string(debugwin_info *info, const char *string)
{
	// set the string to the disasm view
	debug_view_set_property_string(info->view[0].view, DVP_DASM_EXPRESSION, string);

	// select everything in the edit text box
	SendMessage(info->editwnd, EM_SETSEL, (WPARAM)0, (LPARAM)-1);

	// update the default string to match
	strncpy(info->edit_defstr, string, sizeof(info->edit_defstr) - 1);
}



//============================================================
//  disasm_update_checkmarks
//============================================================

static void disasm_update_checkmarks(debugwin_info *info)
{
	int rightcol = debug_view_get_property_UINT32(info->view[0].view, DVP_DASM_RIGHT_COLUMN);
	CheckMenuItem(GetMenu(info->wnd), ID_SHOW_RAW, MF_BYCOMMAND | (rightcol == DVP_DASM_RIGHTCOL_RAW ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(GetMenu(info->wnd), ID_SHOW_ENCRYPTED, MF_BYCOMMAND | (rightcol == DVP_DASM_RIGHTCOL_ENCRYPTED ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(GetMenu(info->wnd), ID_SHOW_COMMENTS, MF_BYCOMMAND | (rightcol == DVP_DASM_RIGHTCOL_COMMENTS ? MF_CHECKED : MF_UNCHECKED));
}



//============================================================
//  disasm_handle_command
//============================================================

static int disasm_handle_command(debugwin_info *info, WPARAM wparam, LPARAM lparam)
{
	switch (HIWORD(wparam))
	{
		// combo box selection changed
		case CBN_SELCHANGE:
		{
			int sel = SendMessage((HWND)lparam, CB_GETCURSEL, 0, 0);
			if (sel != CB_ERR)
			{
				// find the matching entry
				UINT32 cpunum;
				for (cpunum = 0; cpunum < MAX_CPU; cpunum++)
				{
					const debug_cpu_info *cpuinfo = debug_get_cpu_info(cpunum);
					if (cpuinfo->valid)
						if (cpuinfo->space[ADDRESS_SPACE_PROGRAM].databytes)
							if (sel-- == 0)
							{
								debug_view_begin_update(info->view[0].view);
								debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_CPUNUM, cpunum);
								debug_view_end_update(info->view[0].view);
								disasm_update_caption(info->wnd);
							}
				}

				// reset the focus
				SetFocus(info->focuswnd);
				return 1;
			}
			break;
		}

		// menu selections
		case 0:
			switch (LOWORD(wparam))
			{
				case ID_SHOW_RAW:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_RIGHT_COLUMN, DVP_DASM_RIGHTCOL_RAW);
					debug_view_end_update(info->view[0].view);
					disasm_update_checkmarks(info);
					(*info->recompute_children)(info);
					return 1;

				case ID_SHOW_ENCRYPTED:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_RIGHT_COLUMN, DVP_DASM_RIGHTCOL_ENCRYPTED);
					debug_view_end_update(info->view[0].view);
					disasm_update_checkmarks(info);
					(*info->recompute_children)(info);
					return 1;

				case ID_SHOW_COMMENTS:
					debug_view_begin_update(info->view[0].view);
					debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_RIGHT_COLUMN, DVP_DASM_RIGHTCOL_COMMENTS);
					debug_view_end_update(info->view[0].view);
					disasm_update_checkmarks(info);
					(*info->recompute_children)(info);
					return 1;
			}
			break;
	}
	return global_handle_command(info, wparam, lparam);
}



//============================================================
//  disasm_handle_key
//============================================================

static int disasm_handle_key(debugwin_info *info, WPARAM wparam, LPARAM lparam)
{
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
	{
		switch (wparam)
		{
			case 'R':
				SendMessage(info->wnd, WM_COMMAND, ID_SHOW_RAW, 0);
				return 1;

			case 'E':
				SendMessage(info->wnd, WM_COMMAND, ID_SHOW_ENCRYPTED, 0);
				return 1;

			case 'C':
				SendMessage(info->wnd, WM_COMMAND, ID_SHOW_COMMENTS, 0);
				return 1;
		}
	}
	return global_handle_key(info, wparam, lparam);
}



//============================================================
//  disasm_update_caption
//============================================================

static void disasm_update_caption(HWND wnd)
{
	debugwin_info *info = (debugwin_info *)(UINT32)GetWindowLongPtr(wnd, GWLP_USERDATA);
	char title[100];
	UINT32 cpunum;

	// get the properties
	cpunum = debug_view_get_property_UINT32(info->view[0].view, DVP_DASM_CPUNUM);

	// then update the caption
	sprintf(title, "Disassembly: %s (%d)", cpunum_name(cpunum), cpunum);
	SetWindowText(wnd, title);
}



//============================================================
//  console_create_window
//============================================================

void console_create_window(void)
{
	debugwin_info *info;
	int bestwidth, bestheight;
	RECT bounds, work_bounds;
	HMENU optionsmenu;
	UINT32 cpunum;

	// create the window
	info = debug_window_create("Debug", NULL);
	if (!info)
		return;
	main_console = info;
	console_set_cpunum(0);

	// create the views
	if (!debug_view_create(info, 0, DVT_DISASSEMBLY))
		goto cleanup;
	if (!debug_view_create(info, 1, DVT_REGISTERS))
		goto cleanup;
	if (!debug_view_create(info, 2, DVT_CONSOLE))
		goto cleanup;

	// create the options menu
	optionsmenu = CreatePopupMenu();
	AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_RAW, "Raw opcodes\tCtrl+R");
	AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_ENCRYPTED, "Encrypted opcodes\tCtrl+E");
	AppendMenu(optionsmenu, MF_ENABLED, ID_SHOW_COMMENTS, "Comments\tCtrl+C");
	AppendMenu(GetMenu(info->wnd), MF_ENABLED | MF_POPUP, (UINT_PTR)optionsmenu, "Options");
	disasm_update_checkmarks(info);

	// set the handlers
	info->handle_command = disasm_handle_command;
	info->handle_key = disasm_handle_key;

	// lock us to the bottom of the console by default
	info->view[2].is_textbuf = TRUE;

	// set up the disassembly view to track the current pc
	debug_view_begin_update(info->view[0].view);
	debug_view_set_property_string(info->view[0].view, DVP_DASM_EXPRESSION, "pc");
	debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_TRACK_LIVE, 1);
	debug_view_end_update(info->view[0].view);

	// create an edit box and override its key handling
	info->editwnd = CreateWindowEx(EDIT_BOX_STYLE_EX, TEXT("EDIT"), NULL, EDIT_BOX_STYLE,
			0, 0, 100, 100, info->wnd, NULL, GetModuleHandle(NULL), NULL);
	info->original_editproc = (void *)(UINT32)GetWindowLongPtr(info->editwnd, GWLP_WNDPROC);
	SetWindowLongPtr(info->editwnd, GWLP_USERDATA, (UINT32)info);
	SetWindowLongPtr(info->editwnd, GWLP_WNDPROC, (UINT32)debug_edit_proc);
	SendMessage(info->editwnd, WM_SETFONT, (WPARAM)debug_font, (LPARAM)FALSE);
	SendMessage(info->editwnd, EM_LIMITTEXT, (WPARAM)MAX_EDIT_STRING, (LPARAM)0);

	// set the child functions
	info->recompute_children = console_recompute_children;
	info->process_string = console_process_string;

	// loop over all CPUs and compute the sizes
	info->minwidth = 0;
	info->maxwidth = 0;
	for (cpunum = MAX_CPU - 1; (INT32)cpunum >= 0; cpunum--)
		if (Machine->drv->cpu[cpunum].cpu_type != CPU_DUMMY)
		{
			UINT32 regchars, dischars, conchars;
			UINT32 minwidth, maxwidth;

			// point all views to the new CPU number
			debug_view_set_property_UINT32(info->view[0].view, DVP_DASM_CPUNUM, cpunum);
			debug_view_set_property_UINT32(info->view[1].view, DVP_REGS_CPUNUM, cpunum);

			// get the total width of all three children
			dischars = debug_view_get_property_UINT32(info->view[0].view, DVP_TOTAL_COLS);
			regchars = debug_view_get_property_UINT32(info->view[1].view, DVP_TOTAL_COLS);
			conchars = debug_view_get_property_UINT32(info->view[2].view, DVP_TOTAL_COLS);

			// compute the preferred width
			minwidth = EDGE_WIDTH + regchars * debug_font_width + vscroll_width + 2 * EDGE_WIDTH + 100 + EDGE_WIDTH;
			maxwidth = EDGE_WIDTH + regchars * debug_font_width + vscroll_width + 2 * EDGE_WIDTH + ((dischars > conchars) ? dischars : conchars) * debug_font_width + vscroll_width + EDGE_WIDTH;
			if (minwidth > info->minwidth)
				info->minwidth = minwidth;
			if (maxwidth > info->maxwidth)
				info->maxwidth = maxwidth;
		}

	// get the work bounds
	SystemParametersInfo(SPI_GETWORKAREA, 0, &work_bounds, 0);

	// adjust the min/max sizes for the window style
	bounds.top = bounds.left = 0;
	bounds.right = bounds.bottom = info->minwidth;
	AdjustWindowRectEx(&bounds, DEBUG_WINDOW_STYLE, FALSE, DEBUG_WINDOW_STYLE_EX);
	info->minwidth = bounds.right - bounds.left;

	bounds.top = bounds.left = 0;
	bounds.right = bounds.bottom = info->maxwidth;
	AdjustWindowRectEx(&bounds, DEBUG_WINDOW_STYLE, FALSE, DEBUG_WINDOW_STYLE_EX);
	info->maxwidth = bounds.right - bounds.left;

	// position the window at the bottom-right
	bestwidth = (info->maxwidth < (work_bounds.right - work_bounds.left)) ? info->maxwidth : (work_bounds.right - work_bounds.left);
	bestheight = (500 < (work_bounds.bottom - work_bounds.top)) ? 500 : (work_bounds.bottom - work_bounds.top);
	SetWindowPos(info->wnd, HWND_TOP,
				work_bounds.right - bestwidth, work_bounds.bottom - bestheight,
				bestwidth, bestheight,
				SWP_SHOWWINDOW);

	// recompute the children
	console_recompute_children(info);

	// mark the edit box as the default focus and set it
	info->focuswnd = info->editwnd;
	SetFocus(info->editwnd);
	return;

cleanup:
	if (info->view[2].view)
		debug_view_free(info->view[2].view);
	if (info->view[1].view)
		debug_view_free(info->view[1].view);
	if (info->view[0].view)
		debug_view_free(info->view[0].view);
}



//============================================================
//  console_recompute_children
//============================================================

static void console_recompute_children(debugwin_info *info)
{
	RECT parent, regrect, disrect, conrect, editrect;
	UINT32 regchars, dischars, conchars;

	// get the parent's dimensions
	GetClientRect(info->wnd, &parent);

	// get the total width of all three children
	dischars = debug_view_get_property_UINT32(info->view[0].view, DVP_TOTAL_COLS);
	regchars = debug_view_get_property_UINT32(info->view[1].view, DVP_TOTAL_COLS);
	conchars = debug_view_get_property_UINT32(info->view[2].view, DVP_TOTAL_COLS);

	// registers always get their desired width, and span the entire height
	regrect.top = parent.top + EDGE_WIDTH;
	regrect.bottom = parent.bottom - EDGE_WIDTH;
	regrect.left = parent.left + EDGE_WIDTH;
	regrect.right = regrect.left + debug_font_width * regchars + vscroll_width;

	// edit box goes at the bottom of the remaining area
	editrect.bottom = parent.bottom - EDGE_WIDTH;
	editrect.top = editrect.bottom - debug_font_height - 4;
	editrect.left = regrect.right + EDGE_WIDTH * 2;
	editrect.right = parent.right - EDGE_WIDTH;

	// console and disassembly split the difference
	disrect.top = parent.top + EDGE_WIDTH;
	disrect.bottom = (editrect.top - parent.top) / 2 - EDGE_WIDTH;
	disrect.left = regrect.right + EDGE_WIDTH * 2;
	disrect.right = parent.right - EDGE_WIDTH;

	conrect.top = disrect.bottom + EDGE_WIDTH * 2;
	conrect.bottom = editrect.top - EDGE_WIDTH;
	conrect.left = regrect.right + EDGE_WIDTH * 2;
	conrect.right = parent.right - EDGE_WIDTH;

	// set the bounds of things
	debug_view_set_bounds(&info->view[0], info->wnd, &disrect);
	debug_view_set_bounds(&info->view[1], info->wnd, &regrect);
	debug_view_set_bounds(&info->view[2], info->wnd, &conrect);
	smart_set_window_bounds(info->editwnd, info->wnd, &editrect);
}



//============================================================
//  console_process_string
//============================================================

static void console_process_string(debugwin_info *info, const char *string)
{
	TCHAR buffer = 0;

	// an empty string is a single step
	if (string[0] == 0)
		debug_cpu_single_step(1);

	// otherwise, just process the command
	else
		debug_console_execute_command(string, 1);

	// clear the edit text box
	SendMessage(info->editwnd, WM_SETTEXT, 0, (LPARAM)&buffer);
}



//============================================================
//  console_set_cpunum
//============================================================

static void console_set_cpunum(int cpunum)
{
	TCHAR title[256], curtitle[256];

	// first set all the views to the new cpu number
	if (main_console->view[0].view)
		debug_view_set_property_UINT32(main_console->view[0].view, DVP_DASM_CPUNUM, cpunum);
	if (main_console->view[1].view)
		debug_view_set_property_UINT32(main_console->view[1].view, DVP_REGS_CPUNUM, cpunum);

	// then update the caption
	sprintf(title, "Debug: %s - CPU %d (%s)", Machine->gamedrv->name, cpu_getactivecpu(), activecpu_name());
	GetWindowText(main_console->wnd, curtitle, sizeof(curtitle) / sizeof(curtitle[0]));
	if (strcmp(title, curtitle))
		SetWindowText(main_console->wnd, title);
}



//============================================================
//  create_standard_menubar
//============================================================

static HMENU create_standard_menubar(void)
{
	HMENU menubar, debugmenu;

	// create the debug menu
	debugmenu = CreatePopupMenu();
	if (!debugmenu)
		return NULL;
	AppendMenu(debugmenu, MF_ENABLED, ID_NEW_MEMORY_WND, "New Memory Window\tCtrl+M");
	AppendMenu(debugmenu, MF_ENABLED, ID_NEW_DISASM_WND, "New Disassembly Window\tCtrl+D");
	AppendMenu(debugmenu, MF_ENABLED, ID_NEW_LOG_WND, "New Error Log Window\tCtrl+L");
	AppendMenu(debugmenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(debugmenu, MF_ENABLED, ID_RUN, "Run\tF5");
	AppendMenu(debugmenu, MF_ENABLED, ID_RUN_AND_HIDE, "Run and Hide Debugger\tF12");
	AppendMenu(debugmenu, MF_ENABLED, ID_NEXT_CPU, "Run to Next CPU\tF6");
	AppendMenu(debugmenu, MF_ENABLED, ID_RUN_IRQ, "Run until Next Interrupt on This CPU\tF7");
	AppendMenu(debugmenu, MF_ENABLED, ID_RUN_VBLANK, "Run until Next VBLANK\tF8");
	AppendMenu(debugmenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(debugmenu, MF_ENABLED, ID_STEP, "Step Into\tF11");
	AppendMenu(debugmenu, MF_ENABLED, ID_STEP_OVER, "Step Over\tF10");
	AppendMenu(debugmenu, MF_ENABLED, ID_STEP_OUT, "Step Out\tShift+F11");
	AppendMenu(debugmenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(debugmenu, MF_ENABLED, ID_SOFT_RESET, "Soft Reset\tF3");
	AppendMenu(debugmenu, MF_ENABLED, ID_HARD_RESET, "Hard Reset\tShift+F3");
	AppendMenu(debugmenu, MF_ENABLED, ID_EXIT, "Exit");

	// create the menu bar
	menubar = CreateMenu();
	if (!menubar)
	{
		DestroyMenu(debugmenu);
		return NULL;
	}
	AppendMenu(menubar, MF_ENABLED | MF_POPUP, (UINT_PTR)debugmenu, "Debug");

	return menubar;
}



//============================================================
//  global_handle_command
//============================================================

static int global_handle_command(debugwin_info *info, WPARAM wparam, LPARAM lparam)
{
	if (HIWORD(wparam) == 0)
		switch (LOWORD(wparam))
		{
			case ID_NEW_MEMORY_WND:
				memory_create_window();
				return 1;

			case ID_NEW_DISASM_WND:
				disasm_create_window();
				return 1;

			case ID_NEW_LOG_WND:
				log_create_window();
				return 1;

			case ID_RUN_AND_HIDE:
				smart_show_all(FALSE);
			case ID_RUN:
				debug_cpu_go(~0);
				return 1;

			case ID_NEXT_CPU:
				debug_cpu_next_cpu();
				return 1;

			case ID_RUN_VBLANK:
				debug_cpu_go_vblank();
				return 1;

			case ID_RUN_IRQ:
				debug_cpu_go_interrupt(-1);
				return 1;

			case ID_STEP:
				debug_cpu_single_step(1);
				return 1;

			case ID_STEP_OVER:
				debug_cpu_single_step_over(1);
				return 1;

			case ID_STEP_OUT:
				debug_cpu_single_step_out();
				return 1;

			case ID_HARD_RESET:
				mame_schedule_hard_reset();
				return 1;

			case ID_SOFT_RESET:
				mame_schedule_soft_reset();
				debug_cpu_go(~0);
				return 1;

			case ID_EXIT:
				mame_schedule_exit();
				return 1;
		}

	return 0;
}



//============================================================
//  global_handle_key
//============================================================

static int global_handle_key(debugwin_info *info, WPARAM wparam, LPARAM lparam)
{
	int ignoreme;

	/* ignore any keys that are received while the debug key is down */
	ignoreme = input_ui_pressed(IPT_UI_DEBUG_BREAK);
	if (ignoreme)
		return 1;

	switch (wparam)
	{
		case VK_F3:
			if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
				SendMessage(info->wnd, WM_COMMAND, ID_HARD_RESET, 0);
			else
				SendMessage(info->wnd, WM_COMMAND, ID_SOFT_RESET, 0);
			return 1;

		case VK_F4:
			if (GetAsyncKeyState(VK_MENU) & 0x8000)
			{
				SendMessage(info->wnd, WM_COMMAND, ID_EXIT, 0);
				return 1;
			}
			break;

		case VK_F5:
			SendMessage(info->wnd, WM_COMMAND, ID_RUN, 0);
			return 1;

		case VK_F6:
			SendMessage(info->wnd, WM_COMMAND, ID_NEXT_CPU, 0);
			return 1;

		case VK_F7:
			SendMessage(info->wnd, WM_COMMAND, ID_RUN_IRQ, 0);
			return 1;

		case VK_F8:
			SendMessage(info->wnd, WM_COMMAND, ID_RUN_VBLANK, 0);
			return 1;

		case VK_F10:
			SendMessage(info->wnd, WM_COMMAND, ID_STEP_OVER, 0);
			return 1;

		case VK_F11:
			if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
				SendMessage(info->wnd, WM_COMMAND, ID_STEP_OUT, 0);
			else
				SendMessage(info->wnd, WM_COMMAND, ID_STEP, 0);
			return 1;

		case VK_F12:
			SendMessage(info->wnd, WM_COMMAND, ID_RUN_AND_HIDE, 0);
			return 1;

		case 'M':
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
			{
				SendMessage(info->wnd, WM_COMMAND, ID_NEW_MEMORY_WND, 0);
				return 1;
			}
			break;

		case 'D':
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
			{
				SendMessage(info->wnd, WM_COMMAND, ID_NEW_DISASM_WND, 0);
				return 1;
			}
			break;

		case 'L':
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
			{
				SendMessage(info->wnd, WM_COMMAND, ID_NEW_LOG_WND, 0);
				return 1;
			}
			break;
	}

	return 0;
}



//============================================================
//  smart_set_window_bounds
//============================================================

static void smart_set_window_bounds(HWND wnd, HWND parent, RECT *bounds)
{
	RECT curbounds;
	int flags = 0;

	// first get the current bounds, relative to the parent
	GetWindowRect(wnd, &curbounds);
	if (parent != NULL)
	{
		RECT parentbounds;
		GetWindowRect(parent, &parentbounds);
		curbounds.top -= parentbounds.top;
		curbounds.bottom -= parentbounds.top;
		curbounds.left -= parentbounds.left;
		curbounds.right -= parentbounds.left;
	}

	// if the position matches, don't change it
	if (curbounds.top == bounds->top && curbounds.left == bounds->left)
		flags |= SWP_NOMOVE;
	if ((curbounds.bottom - curbounds.top) == (bounds->bottom - bounds->top) &&
		(curbounds.right - curbounds.left) == (bounds->right - bounds->left))
		flags |= SWP_NOSIZE;

	// if we need to, reposition the window
	if (flags != (SWP_NOMOVE | SWP_NOSIZE))
		SetWindowPos(wnd, NULL,
					bounds->left, bounds->top,
					bounds->right - bounds->left, bounds->bottom - bounds->top,
					SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | flags);
}



//============================================================
//  smart_show_window
//============================================================

static void smart_show_window(HWND wnd, BOOL show)
{
	BOOL visible = IsWindowVisible(wnd);
	if ((visible && !show) || (!visible && show))
		ShowWindow(wnd, show ? SW_SHOW : SW_HIDE);
}



//============================================================
//  smart_show_all
//============================================================

static void smart_show_all(BOOL show)
{
	debugwin_info *info;
	if (!show)
		SetForegroundWindow(win_video_window);
	for (info = window_list; info; info = info->next)
		smart_show_window(info->wnd, show);
}
