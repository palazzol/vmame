//============================================================
//
//  window.h - Win32 window handling
//
//  Copyright (c) 1996-2006, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//============================================================

#ifndef __WIN_WINDOW__
#define __WIN_WINDOW__

#include "blit.h"


//============================================================
//  PARAMETERS
//============================================================

#ifndef MESS
#define HAS_WINDOW_MENU			FALSE
#else
#define HAS_WINDOW_MENU			TRUE
#endif



//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct _win_effect_data win_effect_data;

struct _win_effect_data
{
	const char *name;
	int effect;
	int min_xscale;
	int min_yscale;
	int max_xscale;
	int max_yscale;
};



//============================================================
//  GLOBAL VARIABLES
//============================================================

// command line config
extern int			win_window_mode;
extern int			win_wait_vsync;
extern int			win_triple_buffer;
extern int			win_use_ddraw;
extern int			win_use_d3d;
extern int			win_dd_hw_stretch;
extern int			win_force_int_stretch;
extern int			win_gfx_width;
extern int			win_gfx_height;
extern int			win_gfx_depth;
extern int			win_gfx_zoom;
extern int			win_old_scanlines;
extern int			win_switch_res;
extern int			win_switch_bpp;
extern int			win_start_maximized;
extern int			win_keep_aspect;
extern int			win_gfx_refresh;
extern int			win_match_refresh;
extern int			win_sync_refresh;
extern float		win_gfx_gamma;
extern int			win_blit_effect;
extern float		win_screen_aspect;
extern int			win_force_rdtsc;
extern int			win_priority;

// windows
extern HWND			win_video_window;
extern HWND			win_debug_window;

// video bounds
extern double		win_aspect_ratio_adjust;
extern int 			win_default_constraints;

// visible bounds
extern RECT			win_visible_rect;
extern int			win_visible_width;
extern int			win_visible_height;

// 16bpp color conversion
extern int			win_color16_rsrc_shift;
extern int			win_color16_gsrc_shift;
extern int			win_color16_bsrc_shift;
extern int			win_color16_rdst_shift;
extern int			win_color16_gdst_shift;
extern int			win_color16_bdst_shift;

// 32bpp color conversion
extern int			win_color32_rdst_shift;
extern int			win_color32_gdst_shift;
extern int			win_color32_bdst_shift;

// raw mouse support
extern int			win_use_raw_mouse;



//============================================================
//  DEFINES
//============================================================

// win_constrain_to_aspect_ratio() constraints parameter
#define CONSTRAIN_INTEGER_WIDTH 1
#define CONSTRAIN_INTEGER_HEIGHT 2

// win_constrain_to_aspect_ratio() coordinate_system parameter
// desktop coordinates--when not in any full screen mode
#define COORDINATES_DESKTOP 1
// display coordinates, when each display is 0,0 at the top left
#define COORDINATES_DISPLAY 2


// win_force_int_stretch values
#define FORCE_INT_STRECT_NONE 0
#define FORCE_INT_STRECT_FULL 1
#define FORCE_INT_STRECT_AUTO 2
#define FORCE_INT_STRECT_HOR 3
#define FORCE_INT_STRECT_VER 4



//============================================================
//  PROTOTYPES
//============================================================

int win_init_window(void);
int win_create_window(int width, int height, int depth, int attributes, double aspect);
void win_destroy_window(void);
void win_update_cursor_state(void);
void win_toggle_maximize(int force_maximize);
void win_toggle_full_screen(void);
void win_adjust_window(void);

void win_constrain_to_aspect_ratio(RECT *rect, int adjustment, int constraints, int coordinate_system);
void win_adjust_window_for_visible(int min_x, int max_x, int min_y, int max_y);

void win_update_video_window(mame_bitmap *bitmap, const rectangle *bounds, void *vector_dirty_pixels);

void win_set_palette_entry(int _index, UINT8 red, UINT8 green, UINT8 blue);

int win_process_events(int ingame);
void win_process_events_periodic(void);

UINT32 *win_prepare_palette(win_blit_params *params);

int win_lookup_effect(const char *arg);
int win_determine_effect(const win_blit_params *params);
void win_compute_multipliers(const RECT *rect, int *xmult, int *ymult);

BOOL win_raw_mouse_update(HANDLE in_device_handle);

LRESULT CALLBACK win_video_window_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam);

#if HAS_WINDOW_MENU
int win_create_menu(HMENU *menus);
#endif

#ifndef NEW_DEBUGGER
int debugwin_init_windows(void);
void debugwin_update_windows(mame_bitmap *bitmap, const rgb_t *palette);
void debugwin_show(int type);
void debugwin_set_focus(int focus);
#endif


//============================================================
//  win_color16
//============================================================

INLINE UINT16 win_color16(UINT8 r, UINT8 g, UINT8 b)
{
	return ((r >> win_color16_rsrc_shift) << win_color16_rdst_shift) |
		   ((g >> win_color16_gsrc_shift) << win_color16_gdst_shift) |
		   ((b >> win_color16_bsrc_shift) << win_color16_bdst_shift);
}

INLINE UINT8 win_red16(UINT16 color)
{
	int val = (color >> win_color16_rdst_shift) << win_color16_rsrc_shift;
	return val | (val >> (8 - win_color16_rsrc_shift));
}

INLINE UINT8 win_green16(UINT16 color)
{
	int val = (color >> win_color16_gdst_shift) << win_color16_gsrc_shift;
	return val | (val >> (8 - win_color16_gsrc_shift));
}

INLINE UINT8 win_blue16(UINT16 color)
{
	int val = (color >> win_color16_bdst_shift) << win_color16_bsrc_shift;
	return val | (val >> (8 - win_color16_bsrc_shift));
}



//============================================================
//  win_color32
//============================================================

INLINE UINT32 win_color32(UINT8 r, UINT8 g, UINT8 b)
{
	return (r << win_color32_rdst_shift) |
		   (g << win_color32_gdst_shift) |
		   (b << win_color32_bdst_shift);
}

INLINE UINT8 win_red32(UINT32 color)
{
	return color >> win_color32_rdst_shift;
}

INLINE UINT8 win_green32(UINT32 color)
{
	return color >> win_color32_gdst_shift;
}

INLINE UINT8 win_blue32(UINT32 color)
{
	return color >> win_color32_bdst_shift;
}



//============================================================
//  win_has_menu
//============================================================

INLINE BOOL win_has_menu(void)
{
#if HAS_WINDOW_MENU
	return GetMenu(win_video_window) ? TRUE : FALSE;
#else
	return FALSE;
#endif
}



#endif
