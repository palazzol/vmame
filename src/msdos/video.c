#define DIRECT15_USE_LOOKUP ( 1 )

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <go32.h>
#include <dpmi.h>
#include <libc/dosio.h>
#include <sys/farptr.h>

#include "driver.h"
#include "profiler.h"
#include "vidhrdw/vector.h"

#include "mamalleg.h"
#include <pc.h>
#include <sys/farptr.h>
#include <go32.h>
#include "TwkUser.h"
#include <math.h>
#include "vgafreq.h"
/*extra functions for 15.75KHz modes */
#include "gen15khz.h"
#include "blit.h"
#include "osdepend.h"

// from sound.c
extern void sound_update_refresh_rate(float newrate);

/* from zvg/zvgintrf.c, for zvg board */
extern int zvg_enabled;

/* function to make scanline mode */
Register *make_nondoubled_mode(Register *inreg,int entries);
Register *make_scanline_mode(Register *inreg,int entries);

/*15.75KHz SVGA driver (req. for 15.75KHz Arcade Monitor Modes)*/
SVGA15KHZDRIVER *SVGA15KHzdriver;

static int warming_up;

/* tweak values for centering tweaked modes */
int center_x;
int center_y;

double screen_aspect = (4.0 / 3.0);
int keep_aspect = 1;

/* define these here so allegro doesn't need them configured */

BEGIN_GFX_DRIVER_LIST
	GFX_DRIVER_VGA
	GFX_DRIVER_VESA3
	GFX_DRIVER_VESA2L
	GFX_DRIVER_VESA2B
	GFX_DRIVER_VESA1
END_GFX_DRIVER_LIST

BEGIN_COLOR_DEPTH_LIST
	COLOR_DEPTH_8
	COLOR_DEPTH_15
	COLOR_DEPTH_16
	COLOR_DEPTH_24
	COLOR_DEPTH_32
END_COLOR_DEPTH_LIST

void center_mode(Register *pReg);

/* in msdos/sound.c */
int msdos_update_audio( int throttle );

/* in msdos/input.c */
void poll_joysticks(void);

int video_flipx;
int video_flipy;
int video_swapxy;

static void bitblit_dummy( mame_bitmap *bitmap, int sx, int sy, int sw, int sh, int dx, int dy );
void ( *bitblit )( mame_bitmap *bitmap, int sx, int sy, int sw, int sh, int dx, int dy ) = bitblit_dummy;

extern const char *g_s_resolution;
int gfx_depth;
static int bitmap_depth;
static int video_depth,video_attributes;
static int video_width;
static int video_height;
static float video_fps;
static double bitmap_aspect_ratio;

// 256 color palette reduction
#define BLACK_PEN ( 0 )
#define WHITE_PEN ( 1 )
#define RESERVED_PENS ( 2 )
#define TOTAL_PENS ( 256 )
#define VERBOSE ( 0 )
#define PEDANTIC ( 0 )
static int pen_usage_count[ TOTAL_PENS ];
static RGB shrinked_palette[ TOTAL_PENS ];
static unsigned int rgb6_to_pen[ 64 ][ 64 ][ 64 ];
static unsigned char remap_color[ 65536 ];

static int palette_bit;
#define MAX_PALETTE_BITS ( 5 )

static int *scale_palette_r;
static int *scale_palette_g;
static int *scale_palette_b;

static int scale_palette[ 8 ][ 256 ];

static struct
{
	int r;
	int g;
	int b;
} palette_bits[] =
{
	/*	r	g	b */
	{	6,	6,	6	},
	{	5,	5,	5	},
	{	4,	4,	4	},
	{	3,	3,	3	},
	{	3,	3,	2	},
};

int frameskip,autoframeskip;
#define FRAMESKIP_LEVELS 12

static const int skiptable[FRAMESKIP_LEVELS][FRAMESKIP_LEVELS] =
{
	{ 0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 0,0,0,0,0,0,0,0,0,0,0,1 },
	{ 0,0,0,0,0,1,0,0,0,0,0,1 },
	{ 0,0,0,1,0,0,0,1,0,0,0,1 },
	{ 0,0,1,0,0,1,0,0,1,0,0,1 },
	{ 0,1,0,0,1,0,1,0,0,1,0,1 },
	{ 0,1,0,1,0,1,0,1,0,1,0,1 },
	{ 0,1,0,1,1,0,1,0,1,1,0,1 },
	{ 0,1,1,0,1,1,0,1,1,0,1,1 },
	{ 0,1,1,1,0,1,1,1,0,1,1,1 },
	{ 0,1,1,1,1,1,0,1,1,1,1,1 },
	{ 0,1,1,1,1,1,1,1,1,1,1,1 }
};

static const int waittable[FRAMESKIP_LEVELS][FRAMESKIP_LEVELS] =
{
	{ 1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 2,1,1,1,1,1,1,1,1,1,1,0 },
	{ 2,1,1,1,1,0,2,1,1,1,1,0 },
	{ 2,1,1,0,2,1,1,0,2,1,1,0 },
	{ 2,1,0,2,1,0,2,1,0,2,1,0 },
	{ 2,0,2,1,0,2,0,2,1,0,2,0 },
	{ 2,0,2,0,2,0,2,0,2,0,2,0 },
	{ 2,0,2,0,0,3,0,2,0,0,3,0 },
	{ 3,0,0,3,0,0,3,0,0,3,0,0 },
	{ 4,0,0,0,4,0,0,0,4,0,0,0 },
	{ 6,0,0,0,0,0,6,0,0,0,0,0 },
	{12,0,0,0,0,0,0,0,0,0,0,0 }
};

/* type of monitor output- */
/* Standard PC, NTSC, PAL or Arcade */
int monitor_type;

int use_keyboard_leds;
int vgafreq;
int always_synced;
int video_sync;
int wait_vsync;
int triple_buffer;
float vsync_frame_rate;
int skiplines;
int skipcolumns;
int gfx_skiplines;
int gfx_skipcolumns;
int vscanlines;
int hscanlines;
int stretch;
int use_mmx;
int mmxlfb;
int use_tweaked;
int use_vesa;
char *resolution;
char *mode_desc;
int gfx_mode;
int gfx_width;
int gfx_height;
static int vis_min_x,vis_max_x,vis_min_y,vis_max_y;
static int xdivide;
static int ydivide;
/* indicates unchained video mode (req. for 15.75KHz Arcade Monitor Modes)*/
int unchained;
/* flags for lowscanrate modes */
int scanrate15KHz;

static int viswidth;
static int visheight;
static int skiplinesmax;
static int skipcolumnsmax;
static int skiplinesmin;
static int skipcolumnsmin;
static int show_debugger,debugger_focus_changed;

static Register *reg = 0;       /* for VGA modes */
static int reglen = 0;  /* for VGA modes */
static int videofreq;   /* for VGA modes */

int gfx_xoffset;
int gfx_yoffset;
int gfx_display_lines;
int gfx_display_columns;
static unsigned long cmultiply;
static int xmultiply,ymultiply;
int throttle;       /* toggled by F10 */

static int gone_to_gfx_mode;
static int frameskip_counter;
static int frameskipadjust;
static int frames_displayed;
static cycles_t start_time,end_time;    /* to calculate fps average on exit */
#define FRAMES_TO_SKIP 20       /* skip the first few frames from the FPS calculation */
							/* to avoid counting the copyright and info screens */

unsigned char tw224x288_h, tw224x288_v;
unsigned char tw240x256_h, tw240x256_v;
unsigned char tw256x240_h, tw256x240_v;
unsigned char tw256x256_h, tw256x256_v;
unsigned char tw256x256_hor_h, tw256x256_hor_v;
unsigned char tw288x224_h, tw288x224_v;
unsigned char tw240x320_h, tw240x320_v;
unsigned char tw320x240_h, tw320x240_v;
unsigned char tw336x240_h, tw336x240_v;
unsigned char tw384x224_h, tw384x224_v;
unsigned char tw384x240_h, tw384x240_v;
unsigned char tw384x256_h, tw384x256_v;


struct vga_tweak { int x, y; Register *reg; int reglen; int syncvgafreq; int unchained; int vertical_mode; int xdivide; int ydivide; };
struct vga_tweak vga_tweaked[] = {
	{ 240, 256, scr240x256, sizeof(scr240x256)/sizeof(Register),  1, 0, 1, 1, 1 },
	{ 256, 240, scr256x240, sizeof(scr256x240)/sizeof(Register),  0, 0, 0, 1, 1 },
	{ 256, 256, scr256x256, sizeof(scr256x256)/sizeof(Register),  1, 0, 1, 1, 1 },
	{ 256, 256, scr256x256hor, sizeof(scr256x256hor)/sizeof(Register),  0, 0, 0, 1, 1 },
	{ 224, 288, scr224x288, sizeof(scr224x288)/sizeof(Register),  1, 0, 1, 1, 1 },
	{ 288, 224, scr288x224, sizeof(scr288x224)/sizeof(Register),  0, 0, 0, 1, 1 },
	{ 240, 320, scr240x320, sizeof(scr240x320)/sizeof(Register),  1, 1, 1, 1, 1 },
	{ 320, 240, scr320x240, sizeof(scr320x240)/sizeof(Register),  0, 1, 0, 1, 1 },
	{ 336, 240, scr336x240, sizeof(scr336x240)/sizeof(Register),  0, 1, 0, 1, 1 },
	{ 384, 224, scr384x224, sizeof(scr384x224)/sizeof(Register),  1, 1, 0, 1, 1 },
	{ 384, 240, scr384x240, sizeof(scr384x240)/sizeof(Register),  1, 1, 0, 1, 1 },
	{ 384, 256, scr384x256, sizeof(scr384x256)/sizeof(Register),  1, 1, 0, 1, 1 },
/* 'half x/y' tweaked VGA modes, used to fake hires when vesa disabled */
	{ 512, 240, scr256x240, sizeof(scr256x240)/sizeof(Register),  0, 0, 0, 2, 1 },
	{ 512, 480, scr256x240, sizeof(scr256x240)/sizeof(Register),  0, 0, 0, 2, 2 },
	{ 640, 480, scr320x240, sizeof(scr320x240)/sizeof(Register),  0, 1, 0, 2, 2 },
	{ 640, 480, scr320x240, sizeof(scr320x240)/sizeof(Register),  0, 1, 1, 2, 2 },
	{ 0, 0 }
};
struct mode_adjust  {int x, y; unsigned char *hadjust; unsigned char *vadjust; int vertical_mode; };

/* horizontal and vertical total tweak values for above modes */
struct mode_adjust  pc_adjust[] = {
	{ 240, 256, &tw240x256_h, &tw240x256_v, 1 },
	{ 256, 240, &tw256x240_h, &tw256x240_v, 0 },
	{ 256, 256, &tw256x256_hor_h, &tw256x256_hor_v, 0 },
	{ 256, 256, &tw256x256_h, &tw256x256_v, 1 },
	{ 224, 288, &tw224x288_h, &tw224x288_v, 1 },
	{ 288, 224, &tw288x224_h, &tw288x224_v, 0 },
	{ 240, 320, &tw240x320_h, &tw240x320_v, 1 },
	{ 320, 240, &tw320x240_h, &tw320x240_v, 0 },
	{ 336, 240, &tw336x240_h, &tw336x240_v, 0 },
	{ 384, 224, &tw384x224_h, &tw384x224_v, 0 },
	{ 384, 240, &tw384x240_h, &tw384x240_v, 0 },
	{ 384, 256, &tw384x256_h, &tw384x256_v, 0 },
	{ 512, 240, &tw256x240_h, &tw256x240_v, 0 },
	{ 512, 480, &tw256x240_h, &tw256x240_v, 0 },
	{ 640, 480, &tw320x240_h, &tw320x240_v, 0 },
	{ 640, 480, &tw320x240_h, &tw320x240_v, 1 },
	{ 0, 0 }
};

/* Tweak values for arcade/ntsc/pal modes */
unsigned char tw224x288arc_h, tw224x288arc_v, tw288x224arc_h, tw288x224arc_v;
unsigned char tw256x240arc_h, tw256x240arc_v, tw256x256arc_h, tw256x256arc_v;
unsigned char tw320x240arc_h, tw320x240arc_v, tw320x256arc_h, tw320x256arc_v;
unsigned char tw352x240arc_h, tw352x240arc_v, tw352x256arc_h, tw352x256arc_v;
unsigned char tw368x224arc_h, tw368x224arc_v;
unsigned char tw368x240arc_h, tw368x240arc_v, tw368x256arc_h, tw368x256arc_v;
unsigned char tw512x224arc_h, tw512x224arc_v, tw512x256arc_h, tw512x256arc_v;
unsigned char tw512x448arc_h, tw512x448arc_v, tw512x512arc_h, tw512x512arc_v;
unsigned char tw640x480arc_h, tw640x480arc_v;

/* 15.75KHz Modes */
struct vga_15KHz_tweak
{
	int x, y;
	Register *reg;
	int reglen;
	int syncvgafreq;
	int vesa;
	int ntsc;
	int xdivide;
	int ydivide;
	int matchx;
	int vertical_mode;
};

struct vga_15KHz_tweak arcade_tweaked[] = {
	{ 224, 288, scr224x288_15KHz, sizeof(scr224x288_15KHz)/sizeof(Register), 0, 0, 0, 1, 1, 224, 1 },
	{ 256, 240, scr256x240_15KHz, sizeof(scr256x240_15KHz)/sizeof(Register), 0, 0, 1, 1, 1, 256, 0 },
	{ 256, 256, scr256x256_15KHz, sizeof(scr256x256_15KHz)/sizeof(Register), 0, 0, 0, 1, 1, 256, 0 },
	{ 288, 224, scr288x224_15KHz, sizeof(scr288x224_15KHz)/sizeof(Register), 0, 0, 1, 1, 1, 288, 0 },
	{ 320, 240, scr320x240_15KHz, sizeof(scr320x240_15KHz)/sizeof(Register), 1, 0, 1, 1, 1, 320, 0 },
	{ 320, 256, scr320x256_15KHz, sizeof(scr320x256_15KHz)/sizeof(Register), 1, 0, 0, 1, 1, 320, 0 },
	{ 352, 240, scr352x240_15KHz, sizeof(scr352x240_15KHz)/sizeof(Register), 1, 0, 1, 1, 1, 352, 0 },
	{ 352, 256, scr352x256_15KHz, sizeof(scr352x256_15KHz)/sizeof(Register), 1, 0, 0, 1, 1, 352, 0 },
/* force 384 games to match to 368 modes - the standard VGA clock speeds mean we can't go as wide as 384 */
	{ 368, 224, scr368x224_15KHz, sizeof(scr368x224_15KHz)/sizeof(Register), 1, 0, 1, 1, 1, 384, 0 },
/* all VGA modes from now on are too big for triple buffering */
	{ 368, 240, scr368x240_15KHz, sizeof(scr368x240_15KHz)/sizeof(Register), 1, 0, 1, 1, 1, 384, 0 },
	{ 368, 256, scr368x256_15KHz, sizeof(scr368x256_15KHz)/sizeof(Register), 1, 0, 0, 1, 1, 384, 0 },
/* double monitor modes */
	{ 512, 224, scr512x224_15KHz, sizeof(scr512x224_15KHz)/sizeof(Register), 0, 0, 1, 1, 1, 512, 0 },
	{ 512, 256, scr512x256_15KHz, sizeof(scr512x256_15KHz)/sizeof(Register), 0, 0, 0, 1, 1, 512, 0 },
/* 'half y' tweaked VGA modes, used to fake hires when vesa disabled */
	{ 512, 448, scr512x224_15KHz, sizeof(scr512x224_15KHz)/sizeof(Register), 0, 0, 1, 1, 2, 512, 0 },
	{ 512, 512, scr512x256_15KHz, sizeof(scr512x256_15KHz)/sizeof(Register), 0, 0, 0, 1, 2, 512, 0 },
/* SVGA Mode (VGA register array not used) */
	{ 640, 480, NULL            , 0                                        , 0, 1, 1, 1, 1, 640, 0 },
/* 'half x/y' tweaked VGA modes, used to fake hires when vesa disabled */
	{ 640, 480, scr320x240_15KHz, sizeof(scr320x240_15KHz)/sizeof(Register), 1, 0, 1, 2, 2, 640, 0 },
	{ 640, 512, scr320x256_15KHz, sizeof(scr320x256_15KHz)/sizeof(Register), 1, 0, 0, 2, 2, 640, 0 },
	{ 0, 0 }
};

/* horizontal and vertical total tweak values for above modes */
struct mode_adjust  arcade_adjust[] = {
	{ 224, 288, &tw224x288arc_h, &tw224x288arc_v, 1 },
	{ 256, 240, &tw256x240arc_h, &tw256x240arc_v, 0 },
	{ 256, 256, &tw256x256arc_h, &tw256x256arc_v, 0 },
	{ 288, 224, &tw288x224arc_h, &tw288x224arc_v, 0 },
	{ 320, 240, &tw320x240arc_h, &tw320x240arc_v, 0 },
	{ 320, 256, &tw320x256arc_h, &tw320x256arc_v, 0 },
	{ 352, 240, &tw352x240arc_h, &tw352x240arc_v, 0 },
	{ 352, 256, &tw352x256arc_h, &tw352x256arc_v, 0 },
	{ 368, 224, &tw368x224arc_h, &tw368x224arc_v, 0 },
	{ 368, 240, &tw368x240arc_h, &tw368x240arc_v, 0 },
	{ 368, 256, &tw368x256arc_h, &tw368x256arc_v, 0 },
	{ 512, 224, &tw512x224arc_h, &tw512x224arc_v, 0 },
	{ 512, 256, &tw512x256arc_h, &tw512x256arc_v, 0 },
//	{ 640, 480, &tw640x480arc_h, &tw640x480arc_v, 0 }, /* vesa */
	{ 512, 448, &tw512x224arc_h, &tw512x224arc_v, 0 },
	{ 512, 512, &tw512x256arc_h, &tw512x256arc_v, 0 },
	{ 640, 480, &tw320x240arc_h, &tw320x240arc_v, 0 },
	{ 640, 512, &tw320x256arc_h, &tw320x256arc_v, 0 },
	{ 0, 0 }
};

/* setup register array to be unchained */
void unchain_vga(Register *pReg)
{
/* setup registers for an unchained mode */
	pReg[UNDERLINE_LOC_INDEX].value = 0x00;
	pReg[MODE_CONTROL_INDEX].value = 0xe3;
	pReg[MEMORY_MODE_INDEX].value = 0x06;
/* flag the fact it's unchained */
	unchained = 1;
}

//============================================================
//	match_resolution
//============================================================

static unsigned char match_resolution( int width, int height, int depth )
{
	int n_f;
	int p_n_f[ 3 ];
	const char *p_ch_s;

	if( ( gfx_width != 0 && width != gfx_width ) ||
		( gfx_height != 0 && height != gfx_height ) ||
		( gfx_depth != 0 && depth != gfx_depth ) )
	{
		return 0;
	}

	p_ch_s = g_s_resolution;

	while( *( p_ch_s ) != 0 )
	{
		n_f = 0;
		for( ;; )
		{
			if( n_f < 3 )
			{
				p_n_f[ n_f++ ] = atoi( p_ch_s );
			}
			while( *( p_ch_s ) != 0 &&
				*( p_ch_s ) != 'x' &&
				*( p_ch_s ) != 'X' &&
				*( p_ch_s ) != ';' )
			{
				p_ch_s++;
			}
			if( *( p_ch_s ) == 0 )
			{
				break;
			}
			if( *( p_ch_s ) == ';' )
			{
				p_ch_s++;
				break;
			}
			p_ch_s++;
		}
		while( n_f < 3 )
		{
			p_n_f[ n_f++ ] = 0;
		}
		if( ( p_n_f[ 0 ] == 0 || p_n_f[ 0 ] == width ) &&
			( p_n_f[ 1 ] == 0 || p_n_f[ 1 ] == height ) &&
			( p_n_f[ 2 ] == 0 || p_n_f[ 2 ] == depth ) )
		{
			return 1;
		}
	}
	return 0;
}

//============================================================
//	compute_mode_score
//============================================================

static unsigned int compute_mode_score( int width, int height, int depth )
{
	static const unsigned int depth_matrix[ 4 ][ 4 ] =
	{
//	mode: 8bpp 16bpp 24bpp 32bpp	// target:
		{ 4,   1,    2,    3 },	// 8bpp (doesn't exist)
		{ 1,   4,    2,    3 },	// 16bpp
		{ 1,   2,    4,    3 },	// 24bpp (doesn't exist)
		{ 1,   2,    3,    4 }	// 32bpp
	};

	unsigned int size_score, depth_score, stretch_score, final_score;
	int target_width, target_height, target_depth;
	int xm,ym;

	/* modes that don't match requirements have a score of zero */
	if( !match_resolution( width, height, depth ) )
	{
		return 0;
	}

	target_width = video_width;
	target_height = video_height;

	if( keep_aspect )
	{
		if( bitmap_aspect_ratio < screen_aspect )
		{
			target_width *= ( ( ( ( target_width * height * screen_aspect ) / target_height ) / width ) / bitmap_aspect_ratio );
		}
		else if( bitmap_aspect_ratio > screen_aspect )
		{
			target_height *= ( ( ( ( target_height * width * bitmap_aspect_ratio ) / target_width ) / height ) / screen_aspect );
		}
	}

	xm = 1;
	ym = 1;

	stretch_score = 1;

	if( ( video_attributes & VIDEO_TYPE_VECTOR ) == 0 )
	{
		if( ( video_attributes & VIDEO_PIXEL_ASPECT_RATIO_MASK ) == VIDEO_PIXEL_ASPECT_RATIO_1_2 )
		{
			if( video_swapxy )
			{
				xm *= 2;
			}
			else
			{
				ym *= 2;
			}
		}
		else if( ( video_attributes & VIDEO_PIXEL_ASPECT_RATIO_MASK ) == VIDEO_PIXEL_ASPECT_RATIO_2_1 )
		{
			if( video_swapxy )
			{
				ym *= 2;
			}
			else
			{
				xm *= 2;
			}
		}

		if( stretch && ( xm == 1 || ym == 1 ) )
		{
			if( target_width * xm * 2 <= width && target_height * ym * 2 <= height )
			{
				xm *= 2;
				ym *= 2;
				stretch_score = vscanlines | hscanlines;
			}
			else
			{
				stretch_score = !( vscanlines | hscanlines );
			}
		}
	}

	target_width *= xm;
	target_height *= ym;

	if( ( video_attributes & VIDEO_RGB_DIRECT ) == 0 && ( video_attributes & VIDEO_NEEDS_6BITS_PER_GUN ) != 0 )
	{
		/* try for a 32bit mode as the palette is > 15bpp. */
		target_depth = 32;
	}
	else
	{
		target_depth = bitmap_depth;
	}

	// compute initial score based on difference between target and current
	size_score = ( ( target_width * 100 ) / width ) + ( ( target_height * 100 ) / height );
	if( target_width > width || target_height > height )
	{
		size_score = 1000 / size_score;
	}

	// next compute depth score
	depth_score = depth_matrix[ ( target_depth + 7 ) / 8 - 1 ][ ( depth + 7 ) / 8 - 1 ] - 1;

	// weight size highest, followed by depth and refresh
	final_score = ( ( ( size_score * 2 ) + stretch_score ) * 4 ) + depth_score;
	return final_score;
}

struct __attribute__ ((packed))
{
	unsigned char  VbeSignature[ 4 ];
	unsigned short VbeVersion;
	unsigned long  OemStringPtr;
	unsigned char  Capabilities[ 4 ];
	unsigned long  VideoModePtr; 
	unsigned short TotalMemory; 
	unsigned short OemSoftwareRev; 
	unsigned long  OemVendorNamePtr; 
	unsigned long  OemProductNamePtr;
	unsigned long  OemProductRevPtr;
	unsigned char  Reserved[222];
	unsigned char  OemData[256];
} VbeInfoBlock;

struct __attribute__ ((packed)) MODE_INFO
{
	unsigned short ModeAttributes;
	unsigned char  WinAAttributes;
	unsigned char  WinBAttributes;
	unsigned short WinGranularity;
	unsigned short WinSize; 
	unsigned short WinASegment; 
	unsigned short WinBSegment; 
	unsigned long  WinFuncPtr; 
	unsigned short BytesPerScanLine; 
	unsigned short XResolution; 
	unsigned short YResolution; 
	unsigned char  XCharSize; 
	unsigned char  YCharSize; 
	unsigned char  NumberOfPlanes; 
	unsigned char  BitsPerPixel; 
	unsigned char  NumberOfBanks; 
	unsigned char  MemoryModel; 
	unsigned char  BankSize; 
	unsigned char  NumberOfImagePages;
	unsigned char  Reserved_page; 
	unsigned char  RedMaskSize; 
	unsigned char  RedMaskPos; 
	unsigned char  GreenMaskSize; 
	unsigned char  GreenMaskPos;
	unsigned char  BlueMaskSize; 
	unsigned char  BlueMaskPos; 
	unsigned char  ReservedMaskSize; 
	unsigned char  ReservedMaskPos; 
	unsigned char  DirectColorModeInfo;

	/* VBE 2.0 extensions */
	unsigned long  PhysBasePtr; 
	unsigned long  OffScreenMemOffset; 
	unsigned short OffScreenMemSize; 

	/* VBE 3.0 extensions */
	unsigned short LinBytesPerScanLine;
	unsigned char  BnkNumberOfPages;
	unsigned char  LinNumberOfPages;
	unsigned char  LinRedMaskSize;
	unsigned char  LinRedFieldPos;
	unsigned char  LinGreenMaskSize;
	unsigned char  LinGreenFieldPos;
	unsigned char  LinBlueMaskSize;
	unsigned char  LinBlueFieldPos;
	unsigned char  LinRsvdMaskSize;
	unsigned char  LinRsvdFieldPos;
	unsigned long  MaxPixelClock;

	unsigned char  Reserved[190]; 
} mode_info;

struct MODE_SCORE
{
	unsigned short n_mode;
	unsigned int n_score;
	unsigned int n_width;
	unsigned int n_height;
	unsigned int n_depth;
};

#define RM_TO_LINEAR(addr)    ((((addr) & 0xFFFF0000) >> 12) + ((addr) & 0xFFFF))

/*
 * This function tries to find the best display mode.
 */
static int select_display_mode(int width,int height,int depth,int colors,int attributes)
{
	int i;
	struct mode_adjust *adjust_array;
	int display_pages;
	int display_page_offset;
	int found;

	display_pages = 1;
	display_page_offset = 0;
	unchained = 0;
	xdivide = 1;
	ydivide = 1;
	found = 0;
	videofreq = vgafreq;
	SVGA15KHzdriver = NULL;

	/* see if it's a low scanrate mode */
	switch (monitor_type)
	{
	case MONITOR_TYPE_NTSC:
	case MONITOR_TYPE_PAL:
	case MONITOR_TYPE_ARCADE:
		scanrate15KHz = 1;
		break;
	default:
		scanrate15KHz = 0;
		break;
	}

	if( gfx_mode == GFX_VGA )
	{
		use_vesa = 0;
	}
	else
	{
		use_vesa = 1;
	}

	if( !use_vesa && !use_tweaked )
	{
		printf( "\nYou must enable tweaked or vesa modes\n" );
		return 0;
	}

	gfx_width = 0;
	gfx_height = 0;

	if( gfx_depth > 8 || ( gfx_depth != 8 && ( colors > 256 || ( attributes & VIDEO_RGB_DIRECT ) != 0 ) ) )
	{
		if( use_vesa && monitor_type != MONITOR_TYPE_PAL )
		{
			logerror("Game needs %d-bit colors. Using VESA\n",depth);
			use_tweaked = 0;
			/* only one 15.75KHz VESA mode, so force that */
			if( scanrate15KHz )
			{
				gfx_width = 640;
				gfx_height = 480;
			}
		}
		else
		{
			logerror("Game needs %d-bit colors. Reducing palette\n",depth);
			gfx_depth = 8;
		}
	}

	/* Check for special 15.75KHz mode (req. for 15.75KHz Arcade Modes) */
	if( scanrate15KHz )
	{
		switch (monitor_type)
		{
		case MONITOR_TYPE_NTSC:
			logerror("Using special NTSC video mode.\n");
			break;
		case MONITOR_TYPE_PAL:
			logerror("Using special PAL video mode.\n");
			break;
		case MONITOR_TYPE_ARCADE:
			logerror("Using special arcade monitor mode.\n");
			break;
		}
		vscanlines = 0;
		hscanlines = 0;
		/* if no width/height specified, pick one from our tweaked list */
		if (gfx_width == 0 && gfx_height == 0)
		{
			for (i=0; arcade_tweaked[i].x != 0; i++)
			{
				/* find height/width fit */
				/* only allow tweaked modes if explicitly selected */
				/* only allow VESA modes if explicitly selected */
				/* only allow PAL / NTSC modes if explicitly selected */
				/* arcade modes cover 50-60Hz) */
				if( ( ( use_tweaked && !arcade_tweaked[ i ].vesa ) || ( use_vesa && arcade_tweaked[ i ].vesa ) ) &&
					(monitor_type == MONITOR_TYPE_ARCADE || /* handles all 15.75KHz modes */
					(arcade_tweaked[i].ntsc && monitor_type == MONITOR_TYPE_NTSC) ||  /* NTSC only */
					(!arcade_tweaked[i].ntsc && monitor_type == MONITOR_TYPE_PAL)) &&  /* PAL ONLY */
					width  <= arcade_tweaked[i].matchx &&
					height <= arcade_tweaked[i].y &&
					( arcade_tweaked[ i ].ydivide == 1 || !use_vesa ) &&
					match_resolution( width, height, 8 ) )
				{
					gfx_width  = arcade_tweaked[i].x;
					gfx_height = arcade_tweaked[i].y;
					break;
				}
			}

			/* we didn't find a tweaked 15.75KHz mode to fit */
			if (gfx_width == 0 && gfx_height == 0)
			{
				/* pick a default resolution for the monitor type */
				/* something with the right refresh rate + an aspect ratio which can handle vectors */
				switch (monitor_type)
				{
					case MONITOR_TYPE_NTSC:
					case MONITOR_TYPE_ARCADE:
						if( match_resolution( 640, 480, 8 ) )
						{
							gfx_width = 640;
							gfx_height = 480;
						}
						break;
					case MONITOR_TYPE_PAL:
						if( match_resolution( 640, 512, 8 ) )
						{
							gfx_width = 640;
							gfx_height = 512;
						}
						break;
				}
			}
		}

		if( gfx_width != 0 && gfx_height != 0 )
		{
			/* pick the mode from our 15.75KHz tweaked modes */
			for (i=0; ((arcade_tweaked[i].x != 0) && !found); i++)
			{
				if (gfx_width  == arcade_tweaked[i].x &&
					gfx_height == arcade_tweaked[i].y &&
					( ( arcade_tweaked[ i ].vesa && use_vesa ) || ( !arcade_tweaked[ i ].vesa && use_tweaked ) ) &&
					( arcade_tweaked[ i ].ydivide == 1 || !use_vesa ) )
				{
					/* check for a NTSC or PAL mode with no arcade flag */
					if (monitor_type != MONITOR_TYPE_ARCADE)
					{
						if (arcade_tweaked[i].ntsc && monitor_type != MONITOR_TYPE_NTSC)
						{
							printf("\n %dx%d 15.75KHz mode only available if -monitor set to 'arcade' or 'ntsc' \n", gfx_width, gfx_height);
							return 0;
						}
						if (!arcade_tweaked[i].ntsc && monitor_type != MONITOR_TYPE_PAL)
						{
							printf("\n %dx%d 15.75KHz mode only available if -monitor set to 'arcade' or 'pal' \n", gfx_width, gfx_height);
							return 0;
						}
					}

					reg = arcade_tweaked[i].reg;
					reglen = arcade_tweaked[i].reglen;
					if( arcade_tweaked[i].vesa )
					{
						unchained = 0;
					}
					else
					{
						/* all 15.75KHz VGA modes are unchained */
						unchained = 1;
						gfx_mode = GFX_VGA;
					}
					xdivide = arcade_tweaked[ i ].xdivide;
					ydivide = arcade_tweaked[ i ].ydivide;
					logerror("15.75KHz mode (%dx%d) vesa:%d\n",
						gfx_width, gfx_height, gfx_mode != GFX_VGA);
					/* always use the freq from the structure */
					videofreq = arcade_tweaked[i].syncvgafreq;
					if( arcade_tweaked[ i ].vertical_mode )
					{
						screen_aspect = 1 / screen_aspect;
					}
					found = 1;
				}
			}
		}
		/* no 15.75KHz matches */
		if (!found)
		{
			printf ("\nNo %dx%d 15.75KHz mode available.\n", width, height);
			return 0;
		}
	}
	else if( use_tweaked )
	{
		/* If using tweaked modes, check if there exists one to fit
		the screen in, otherwise use VESA */
		for (i=0; vga_tweaked[i].x != 0; i++)
		{
			if (width <= vga_tweaked[i].x &&
				height <= vga_tweaked[i].y)
			{
				/*check for 57Hz modes which would fit into a 60Hz mode*/
				if (width <= 256 && height <= 256 &&
					video_sync && (int)video_fps == 57)
				{
					gfx_width = 256;
					gfx_height = 256;
					break;
				}

				/* check for correct horizontal/vertical modes */
				if( ( ( !vga_tweaked[ i ].vertical_mode && !video_swapxy ) ||
					( vga_tweaked[ i ].vertical_mode && video_swapxy ) ) &&
					( vga_tweaked[ i ].xdivide == 1 || !use_vesa ) &&
					match_resolution( vga_tweaked[i].x, vga_tweaked[i].y, 8 ) )
				{
					/* leave the loop on match */
					gfx_width  = vga_tweaked[i].x;
					gfx_height = vga_tweaked[i].y;
					break;
				}
			}
		}
		if (gfx_width == 0 && gfx_height == 0)
		{
			if( use_vesa )
			{
				/* If we didn't find a tweaked VGA mode, use VESA */
				logerror("Did not find a tweaked VGA mode. Trying VESA.\n");
				use_tweaked = 0;
			}
			else
			{
				/* pick the largest possible tweaked mode. */
				if( match_resolution( 640, 480, 8 ) )
				{
					gfx_width = 640;
					gfx_height = 480;
				}
			}
		}

		if( use_tweaked )
		{
			/* find the matching tweaked mode */
			for (i=0; (vga_tweaked[i].x != 0); i++)
			{
				if( gfx_width  == vga_tweaked[i].x &&
					gfx_height == vga_tweaked[i].y &&
					( vga_tweaked[ i ].xdivide == 1 || !use_vesa ) &&
					( ( !vga_tweaked[ i ].vertical_mode && !video_swapxy ) || ( vga_tweaked[ i ].vertical_mode && video_swapxy ) ) )
				{
					reg = vga_tweaked[i].reg;
					reglen = vga_tweaked[i].reglen;
					if( videofreq == -1 )
					{
						videofreq = vga_tweaked[i].syncvgafreq;
					}
					found = 1;
					gfx_mode = GFX_VGA;
					unchained = vga_tweaked[i].unchained;
					xdivide = vga_tweaked[ i ].xdivide;
					ydivide = vga_tweaked[ i ].ydivide;
					if( vga_tweaked[ i ].vertical_mode )
					{
						screen_aspect = 1 / screen_aspect;
					}
					break;
				}
			}

			if( found )
			{
				if (videofreq < 0)
				{
					videofreq = 0;
				}
				else if (videofreq > 3)
				{
					videofreq = 3;
				}
			}
		}
	}

	if( gfx_mode != GFX_VGA )
	{
		int err;
		int mode;
		unsigned int last_score;
		unsigned int best_score;
		__dpmi_regs _dpmi_reg;
		unsigned long  mode_ptr;
		unsigned int n_mode;
		unsigned int n_modes;
		struct MODE_SCORE *p_mode_score;

		found = 0;

		if( scanrate15KHz )
		{
			/* find a VESA driver for 15KHz modes */
			getSVGA15KHzdriver( &SVGA15KHzdriver );
			/* check that we found a driver */
			if( SVGA15KHzdriver == NULL )
			{
				logerror( "\nUnable to find 15.75KHz SVGA driver for %dx%d\n", gfx_width, gfx_height );
				return 0;
			}
		}

		memset( &VbeInfoBlock, 0, sizeof( VbeInfoBlock ) );
		memcpy( VbeInfoBlock.VbeSignature, "VBE2", 4 );
		dosmemput( &VbeInfoBlock, sizeof( VbeInfoBlock ), __tb );

		_dpmi_reg.x.ax = 0x4f00;
		_dpmi_reg.x.di = __tb_offset;
		_dpmi_reg.x.es = __tb_segment;
		__dpmi_int( 0x10, &_dpmi_reg );
		if( _dpmi_reg.h.ah != 0 )
		{
			printf( "VBE not found\n" );
			return 0;
		}

		dosmemget( __tb, sizeof( VbeInfoBlock ), &VbeInfoBlock );
		if( memcmp( VbeInfoBlock.VbeSignature, "VESA", 4 ) != 0 )
		{
			printf( "Invalid VBE signature\n" );
			return 0;
		}

		n_modes = 0;
		_farsetsel( _dos_ds );
		mode_ptr = RM_TO_LINEAR( VbeInfoBlock.VideoModePtr );
		while( _farnspeekw( mode_ptr ) != 0xffff )
		{
			n_modes++;
			mode_ptr += 2;
		}

		p_mode_score = malloc( sizeof( struct MODE_SCORE ) * n_modes );
		if( p_mode_score == NULL )
		{
			printf( "Out of memory\n" );
			return 0;
		}

		_farsetsel( _dos_ds );
		mode_ptr = RM_TO_LINEAR( VbeInfoBlock.VideoModePtr );
		for( n_mode = 0; n_mode < n_modes; n_mode++ )
		{
			p_mode_score[ n_mode ].n_mode = _farnspeekw( mode_ptr );
			mode_ptr += 2;
		}

		for( n_mode = 0; n_mode < n_modes; n_mode++ )
		{
			memset( &mode_info, 0, sizeof( mode_info ) );
			dosmemput( &mode_info, sizeof( mode_info ), __tb );

			_dpmi_reg.x.ax = 0x4f01;
			_dpmi_reg.x.di = __tb_offset;
			_dpmi_reg.x.es = __tb_segment;
			_dpmi_reg.x.cx = p_mode_score[ n_mode ].n_mode;
			__dpmi_int( 0x10, &_dpmi_reg );
			if( _dpmi_reg.h.ah == 0 )
			{
				dosmemget( __tb, sizeof( mode_info ), &mode_info );
				p_mode_score[ n_mode ].n_width = mode_info.XResolution;
				p_mode_score[ n_mode ].n_height = mode_info.YResolution;
				p_mode_score[ n_mode ].n_depth = mode_info.BitsPerPixel;
				if( ( mode_info.ModeAttributes & 0x19 ) == 0x19 )
				{
					p_mode_score[ n_mode ].n_score = compute_mode_score( mode_info.XResolution, mode_info.YResolution, mode_info.BitsPerPixel );
				}
				else
				{
					p_mode_score[ n_mode ].n_score = 0;
				}
			}
		}

		last_score = 0xffffffff;

		do
		{
			best_score = 0;

			for( n_mode = 0; n_mode < n_modes; n_mode++ )
			{
				if( p_mode_score[ n_mode ].n_score > best_score && p_mode_score[ n_mode ].n_score < last_score )
				{
					// if so, remember it
					best_score = p_mode_score[ n_mode ].n_score;
				}
			}

			if( best_score == 0 )
			{
				break;
			}

			for( n_mode = 0; n_mode < n_modes; n_mode++ )
			{
				if( best_score == p_mode_score[ n_mode ].n_score )
				{
					gfx_width = p_mode_score[ n_mode ].n_width;
					gfx_height = p_mode_score[ n_mode ].n_height;
					video_depth = p_mode_score[ n_mode ].n_depth;

					mode = gfx_mode;

					for( ;; )
					{
						logerror("Trying ");
						if( mode == GFX_VESA1 )
						{
							logerror( "VESA1" );
						}
						else if( mode == GFX_VESA2B )
						{
							logerror( "VESA2B" );
						}
						else if( mode == GFX_VESA2L )
						{
							logerror( "VESA2L" );
						}
						else if( mode == GFX_VESA3 )
						{
							logerror( "VESA3" );
						}
						logerror( "  %dx%d, %d bit\n", gfx_width, gfx_height, video_depth );

						set_color_depth( video_depth );

						/* allocate a wide enough virtual screen if possible */
						/* we round the width (in dwords) to be an even multiple 256 - that */
						/* way, during page flipping only one byte of the video RAM */
						/* address changes, therefore preventing flickering. */
						err = 1;
						/* don't ask for a larger screen if triplebuffer not requested - could */
						/* cause problems in some cases. */
						display_page_offset = 0;
						display_pages = 1;
						if( triple_buffer && !scanrate15KHz )
						{
							display_pages = 3;
							if( video_depth == 8 )
							{
								display_page_offset = ( gfx_width + 0x3ff ) & ~0x3ff;
							}
							else if( video_depth == 15 || video_depth == 16 )
							{
								display_page_offset = ( gfx_width + 0x1ff ) & ~0x1ff;
							}
							else if( video_depth == 24 || video_depth == 32 )
							{
								display_page_offset = ( gfx_width + 0x0ff ) & ~0x0ff;
							}
							err = set_gfx_mode( mode, gfx_width, gfx_height, display_pages * display_page_offset, 0 );
						}
						if( err )
						{
							/* if we're using a SVGA 15KHz driver - tell Allegro the virtual screen width */
							if( scanrate15KHz )
							{
								err = set_gfx_mode( mode, gfx_width, gfx_height, SVGA15KHzdriver->getlogicalwidth( gfx_width ), 0 );
							}
							else
							{
								err = set_gfx_mode( mode, gfx_width, gfx_height, 0, 0 );
							}
						}
						if( err == 0 )
						{
							found = 1;
							/* replace gfx_mode with found mode */
							gfx_mode = mode;
							logerror( "Found matching %s mode\n", gfx_driver->desc );
							/* disable triple buffering if the screen is not large enough */
							logerror( "Virtual screen size %dx%d\n", VIRTUAL_W, VIRTUAL_H );
							if( triple_buffer && VIRTUAL_W < display_pages * display_page_offset )
							{
								display_pages = 1;
								display_page_offset = 0;
								triple_buffer = 0;
								logerror( "Triple buffer disabled\n" );
							}
							break;
						}

						logerror( "%s\n", allegro_error );

						/* try VESA modes in VESA3-VESA2L-VESA2B-VESA1 order */
						if( mode == GFX_VESA3 )
						{
							mode = GFX_VESA2L;
						}
						else if( mode == GFX_VESA2L )
						{
							mode = GFX_VESA2B;
						}
						else if( mode == GFX_VESA2B )
						{
							mode = GFX_VESA1;
						}
						else if( mode == GFX_VESA1 )
						{
							break;
						}
					}
					if( found )
					{
						break;
					}
				}
			}
			last_score = best_score;
		} while( !found );

		if( !found )
		{
			if( last_score != 0xffffffff )
			{
				set_gfx_mode (GFX_TEXT,0,0,0,0);
			}

			printf( "\nNo %d-bit %dx%d VESA mode available.\n",
					depth, width, height );
			printf( "\nPossible causes:\n"
					"1) Your video card does not support VESA modes at all. Almost all\n"
					"   video cards support VESA modes natively these days, so you probably\n"
					"   have an older card which needs some driver loaded first.\n"
					"   In case you can't find such a driver in the software that came with\n"
					"   your video card, Scitech Display Doctor or (for S3 cards) S3VBE\n"
					"   are good alternatives.\n"
					"2) Your VESA implementation does not support this resolution. For example,\n"
					"   '-r 320x240', '-r 400x300' and '-r 512x384' are only supported by a few\n"
					"   implementations.\n"
					"3) Your video card doesn't support this resolution at this color depth.\n"
					"   For example, 1024x768 in 16 bit colors requires 2MB video memory.\n"
					"   You can either force an 8 bit video mode ('-depth 8') or use a lower\n"
					"   resolution ('-r 640x480', '-r 800x600').\n");
			return 0;
		}
		free( p_mode_score );
		p_mode_score = NULL;
	}
	else
	{
		unsigned long address;
		int vga_page_size;

		if( !found )
		{
			if( video_swapxy )
			{
				printf ("\nNo vertical %dx%d tweaked mode available.\n",
						width,height);
			}
			else
			{
				printf ("\nNo horizontal %dx%d tweaked mode available.\n",
						width,height);
			}
			return 0;
		}

		/* set the VGA clock */
		if (video_sync || always_synced || wait_vsync)
			reg[0].value = (reg[0].value & 0xf3) | (videofreq << 2);

		/* center the mode */
		center_mode (reg);

		/* set the horizontal and vertical total */
		if( scanrate15KHz )
			/* 15.75KHz modes */
			adjust_array = arcade_adjust;
		else
			/* PC monitor modes */
			adjust_array = pc_adjust;

		for (i=0; adjust_array[i].x != 0; i++)
		{
			if ((gfx_width == adjust_array[i].x) && (gfx_height == adjust_array[i].y))
			{
				/* check for 'special vertical' modes */
				if((!adjust_array[i].vertical_mode && !video_swapxy) ||
					(adjust_array[i].vertical_mode && video_swapxy))
				{
					reg[H_TOTAL_INDEX].value = *adjust_array[i].hadjust;
					reg[V_TOTAL_INDEX].value = *adjust_array[i].vadjust;
					break;
				}
			}
		}

		/* adjust pc tweaked modes */
		if( !scanrate15KHz )
		{
			if( ydivide == 2 )
			{
				reg = make_nondoubled_mode( reg, reglen );
			}
			else if( hscanlines && ( video_attributes & VIDEO_TYPE_VECTOR ) == 0 )
			{
				reg = make_scanline_mode( reg, reglen );
				hscanlines = 0;
			}
		}

		vga_page_size = ( gfx_width / xdivide ) * ( gfx_height / ydivide );

		/* default page sizes for non-triple buffering */
		if( unchained )
		{
			if( ( vga_page_size * 2 ) <= 0x40000 )
			{
				display_pages = 2;
			}
			else
			{
				display_pages = 1;
			}
			display_page_offset = 0x08000;
		}
		else
		{
			display_pages = 1;
			display_page_offset = 0;
		}

		/* VGA triple buffering */
		if(triple_buffer)
		{
			/* see if it'll fit */
			if ((vga_page_size * 3) > 0x40000)
			{
				/* too big */
				logerror("tweaked mode %dx%d is too large to triple buffer\n",gfx_width,gfx_height);
				logerror("Triple buffer disabled\n");
				triple_buffer = 0;
			}
			else
			{
				/* it fits, so set up the 3 pages */
				display_pages = 3;
				display_page_offset = vga_page_size / 4;
				if( !unchained )
				{
					logerror("using unchained VGA for triple buffering\n");
					/* and make sure the mode's unchained */
					unchain_vga (reg);
				}
			}
		}

		/* big hack: open a mode 13h screen using Allegro, then load the custom screen */
		/* definition over it. */
		if (set_gfx_mode(GFX_VGA,320,200,0,0) != 0)
			return 0;

		logerror("Generated Tweak Values :-\n");
		for (i=0; i<reglen; i++)
		{
			logerror("{ 0x%02x, 0x%02x, 0x%02x},",reg[i].port,reg[i].index,reg[i].value);
			if (!((i+1)%3))
				logerror("\n");
		}

		/* tweak the mode */
		outRegArray(reg,reglen);

		/* check for unchained mode,  if unchained clear all pages */
		if (unchained)
		{
			/* clear all 4 bit planes */
			outportw (0x3c4, (0x02 | (0x0f << 0x08)));
		}
		for (address = 0xa0000; address < 0xb0000; address += 4)
		{
			_farpokel(screen->seg, address, 0x00000000);
		}
		video_depth = 8;
	}

	/* if triple buffering is enabled, turn off vsync */
	if (triple_buffer)
	{
		wait_vsync = 0;
		video_sync = 0;
	}

	blit_set_buffers( display_pages, display_page_offset );
	logerror( "Display buffers: %d offset: 0x%x\n", display_pages, display_page_offset );

	gone_to_gfx_mode = 1;

	vsync_frame_rate = video_fps;

	if (video_sync)
	{
		cycles_t a,b;
		float rate;
		cycles_t cps = osd_cycles_per_second();

		/* wait some time to let everything stabilize */
		for (i = 0;i < 60;i++)
		{
			vsync();
			a = osd_cycles();
		}

		/* small delay for really really fast machines */
		for (i = 0;i < 100000;i++) ;

		vsync();
		b = osd_cycles();

		rate = (float)cps/(b-a);

		logerror("target frame rate = %3.2ffps, video frame rate = %3.2fHz\n",video_fps,rate);

		/* don't allow more than 8% difference between target and actual frame rate */
		while (rate > video_fps * 108 / 100)
			rate /= 2;

		if (rate < video_fps * 92 / 100)
		{
			osd_close_display();
			logerror("-vsync option cannot be used with this display mode:\n"
						"video refresh frequency = %3.2fHz, target frame rate = %3.2ffps\n",
						(float)cps/(b-a),video_fps);
			return 0;
		}

		logerror("adjusted video frame rate = %3.2fHz\n",rate);
			vsync_frame_rate = rate;

		if (Machine->sample_rate)
		{
			Machine->sample_rate = Machine->sample_rate * video_fps / rate;
			logerror("sample rate adjusted to match video freq: %d\n",Machine->sample_rate);
		}
	}

	return 1;
}


//============================================================
//	build_rgb_to_pen
//============================================================

static void build_rgb_to_pen( void )
{
	int i;
	int r;
	int g;
	int b;

	for( r = 0; r < 64; r++ )
	{
		for( g = 0; g < 64; g++ )
		{
			for( b = 0; b < 64; b++ )
			{
				rgb6_to_pen[ r ][ g ][ b ] = TOTAL_PENS;
			}
		}
	}
	rgb6_to_pen[ 0 ][ 0 ][ 0 ] = BLACK_PEN;
	rgb6_to_pen[ scale_palette_r[ 255 ] ][ scale_palette_g[ 255 ] ][ scale_palette_b[ 255 ] ] = WHITE_PEN;

	for( i = RESERVED_PENS; i < TOTAL_PENS; i++ )
	{
		if( pen_usage_count[ i ] > 0 )
		{
			r = shrinked_palette[ i ].r;
			g = shrinked_palette[ i ].g;
			b = shrinked_palette[ i ].b;

			if( rgb6_to_pen[ r ][ g ][ b ] == TOTAL_PENS )
			{
				int j;
				int max;

				rgb6_to_pen[ r ][ g ][ b ] = i;
				max = pen_usage_count[ i ];

				/* to reduce flickering during remaps, find the pen used by most colors */
				for( j = i + 1; j < TOTAL_PENS; j++ )
				{
					if( pen_usage_count[ j ] > max &&
							r == shrinked_palette[ j ].r &&
							g == shrinked_palette[ j ].g &&
							b == shrinked_palette[ j ].b )
					{
						rgb6_to_pen[ r ][ g ][ b ] = j;
						max = pen_usage_count[ j ];
					}
				}
			}
		}
	}
}


//============================================================
//	compress_palette
//============================================================

static int compress_palette( mame_display *display )
{
	int i;
	int j;
	int r;
	int saved;
	int g;
	int b;
	int updated;

	build_rgb_to_pen();

	saved = 0;
	updated = 0;

	for( i = 0; i < display->game_palette_entries; i++ )
	{
		/* merge pens of the same color */
		rgb_t rgbvalue = display->game_palette[ i ];
		r = scale_palette_r[ RGB_RED( rgbvalue ) ];
		g = scale_palette_g[ RGB_GREEN( rgbvalue ) ];
		b = scale_palette_b[ RGB_BLUE( rgbvalue ) ];

		j = rgb6_to_pen[ r ][ g ][ b ];

		if( j != TOTAL_PENS && ( blit_lookup_low[ i ] & 0xff ) != j )
		{
			pen_usage_count[ ( blit_lookup_low[ i ] & 0xff ) ]--;
			if( pen_usage_count[ ( blit_lookup_low[ i ] & 0xff ) ] == 0 )
			{
				saved++;
				updated = 1;
			}
			blit_lookup_low[ i ] = j * cmultiply;
			pen_usage_count[ ( blit_lookup_low[ i ] & 0xff ) ]++;
			if( remap_color[ i ] )
			{
				remap_color[ i ] = 0;
				updated = 1;
			}
		}
	}

#if VERBOSE
	logerror( "Compressed the palette, saving %d pens\n", saved );
#endif

	return updated;
}


//============================================================
//	update_palette
//============================================================

#define PALETTE_DIRTY( i ) ( display->game_palette_dirty[ ( i ) / 32 ] & ( 1 << ( ( i ) % 32 ) ) )
#define PALETTE_DIRTY_CLEAR( i ) display->game_palette_dirty[ ( i ) / 32 ] &= ~( 1 << ( ( i ) % 32 ) )

static void update_palette(mame_display *display)
{
	int i;
	int j;

	if( video_depth == 8 )
	{
		int need;

		need = 0;

		for( i = 0; i < display->game_palette_entries; i++ )
		{
			if( PALETTE_DIRTY ( i ) )
			{
				int pen = ( blit_lookup_low[ i ] & 0xff );
				if( pen_usage_count[ pen ] == 1 )
				{
					// extract the RGB values
					rgb_t rgbvalue = display->game_palette[ i ];

					PALETTE_DIRTY_CLEAR( i );

					shrinked_palette[ pen ].r = scale_palette_r[ RGB_RED( rgbvalue ) ];
					shrinked_palette[ pen ].g = scale_palette_g[ RGB_GREEN( rgbvalue ) ];
					shrinked_palette[ pen ].b = scale_palette_b[ RGB_BLUE( rgbvalue ) ];
					set_color( pen, &shrinked_palette[ pen ] );
				}
				else
				{
					if( pen < RESERVED_PENS )
					{
						/* the color uses a reserved pen, the only thing we can do is remap it */
						for( j = i; j < display->game_palette_entries; j++ )
						{
							if( ( blit_lookup_low[ j ] & 0xff ) == pen && PALETTE_DIRTY( j ) )
							{
								PALETTE_DIRTY_CLEAR( j );

								remap_color[ j ] = 1;
							}
						}
					}
					else
					{
						// extract the RGB values
						rgb_t rgbvalue = display->game_palette[ i ];
						int r = scale_palette_r[ RGB_RED( rgbvalue ) ];
						int g = scale_palette_g[ RGB_GREEN( rgbvalue ) ];
						int b = scale_palette_b[ RGB_BLUE( rgbvalue ) ];

						/* the pen is shared with other colors, let's see if all of them */
						/* have been changed to the same value */
						for( j = 0; j < display->game_palette_entries; j++ )
						{
							if( ( blit_lookup_low[ j ] & 0xff ) == pen )
							{
								rgb_t rgbvalue2 = display->game_palette[ j ];
								int r2 = scale_palette_r[ RGB_RED( rgbvalue2 ) ];
								int g2 = scale_palette_g[ RGB_GREEN( rgbvalue2 ) ];
								int b2 = scale_palette_b[ RGB_BLUE( rgbvalue2 ) ];
								if ( !PALETTE_DIRTY( j ) || r != r2 || g != g2 || b != b2 )
								{
									break;
								}
							}
						}

						if( j == display->game_palette_entries )
						{
							/* all colors sharing this pen still are the same, so we */
							/* just change the palette. */
							shrinked_palette[ pen ].r = r;
							shrinked_palette[ pen ].g = g;
							shrinked_palette[ pen ].b = b;
							set_color( pen, &shrinked_palette[ pen ] );

							for( j = i; j < display->game_palette_entries; j++ )
							{
								if( ( blit_lookup_low[ j ] & 0xff ) == pen && PALETTE_DIRTY( j ) )
								{
									PALETTE_DIRTY_CLEAR( j );
								}
							}
						}
						else
						{
							/* the colors sharing this pen now are different, we'll */
							/* have to remap them. */
							for( j = i; j < display->game_palette_entries; j++ )
							{
								if( ( blit_lookup_low[ j ] & 0xff ) == pen && PALETTE_DIRTY( j ) )
								{
									PALETTE_DIRTY_CLEAR( j );
									remap_color[ j ] = 1;
								}
							}
						}
					}
				}
			}
			if( remap_color[ i ] )
			{
				need++;
			}
		}

		if( need > 0 )
		{
			int reuse_pens;
			int ran_out;
			int avail;
			int first_free_pen;

			avail = 0;
			for( i = 0; i < TOTAL_PENS; i++ )
			{
				if( pen_usage_count[ i ] == 0 )
				{
					avail++;
				}
			}

			reuse_pens = 0;
			if( need > avail )
			{
#if VERBOSE
				logerror( "Need %d new pens; %d available. I'll reuse some pens.\n", need, avail );
#endif
				reuse_pens = 1;
				build_rgb_to_pen();
			}

			for( ;; )
			{
				ran_out = 0;
				for( i = 0; i < display->game_palette_entries; i++ )
				{
					if( remap_color[ i ] )
					{
						rgb_t rgbvalue = display->game_palette[ i ];
						int r = scale_palette_r[ RGB_RED( rgbvalue ) ];
						int g = scale_palette_g[ RGB_GREEN( rgbvalue ) ];
						int b = scale_palette_b[ RGB_BLUE( rgbvalue ) ];

						pen_usage_count[ ( blit_lookup_low[ i ] & 0xff ) ]--;

						if( reuse_pens )
						{
							int pen = rgb6_to_pen[ r ][ g ][ b ];
							if( pen != TOTAL_PENS )
							{
								blit_lookup_low[ i ] = pen * cmultiply;
								remap_color[ i ] = 0;
							}
						}

						/* if we still haven't found a pen, choose a new one */
						if( remap_color[ i ] )
						{
							/* if possible, reuse the last associated pen */
							if( pen_usage_count[ ( blit_lookup_low[ i ] & 0xff ) ] == 0 )
							{
								logerror( "reuse last associated pen %d %d\n", i, blit_lookup_low[ i ] & 0xff );
								remap_color[ i ] = 0;
							}
							else
							{
								/* allocate a new pen */
								first_free_pen = RESERVED_PENS;
								while( first_free_pen < TOTAL_PENS && pen_usage_count[ first_free_pen ] > 0 )
								{
									first_free_pen++;
								}

								if( first_free_pen < TOTAL_PENS )
								{
									blit_lookup_low[ i ] = first_free_pen * cmultiply;
									remap_color[ i ] = 0;
								}
								else if( reuse_pens )
								{
									/* if the pen doesn't match any of the
										palette entries then just change it */
									int pen = ( blit_lookup_low[ i ] & 0xff );
									if( pen >= RESERVED_PENS )
									{
										int rr = shrinked_palette[ pen ].r;
										int gg = shrinked_palette[ pen ].g;
										int bb = shrinked_palette[ pen ].b;
										for( j = 0; j < display->game_palette_entries; j++ )
										{
											if( ( blit_lookup_low[ j ] & 0xff ) == pen )
											{
												rgb_t rgbvalue2 = display->game_palette[ j ];
												int r2 = scale_palette_r[ RGB_RED( rgbvalue2 ) ];
												int g2 = scale_palette_g[ RGB_GREEN( rgbvalue2 ) ];
												int b2 = scale_palette_b[ RGB_BLUE( rgbvalue2 ) ];
												if ( rr == r2 && gg == g2 && bb == b2 )
												{
													break;
												}
											}
										}
										if( j == display->game_palette_entries )
										{
											logerror( "reuse current pen %d %d\n", i, blit_lookup_low[ i ] & 0xff );
											remap_color[ i ] = 0;
										}
									}
								}
							}
							if( !remap_color[ i ] )
							{
								int pen = ( blit_lookup_low[ i ] & 0xff );
								int rr = shrinked_palette[ pen ].r;
								int gg = shrinked_palette[ pen ].g;
								int bb = shrinked_palette[ pen ].b;
								if( rgb6_to_pen[ rr ][ gg ][ bb ] == pen )
								{
									rgb6_to_pen[ rr ][ gg ][ bb ] = TOTAL_PENS;
								}

								shrinked_palette[ pen ].r = r;
								shrinked_palette[ pen ].g = g;
								shrinked_palette[ pen ].b = b;
								set_color( pen, &shrinked_palette[ pen ] );

								if( rgb6_to_pen[ r ][ g ][ b ] == TOTAL_PENS)
								{
									rgb6_to_pen[ r ][ g ][ b ] = pen;
								}
							}
							else
							{
								ran_out++;
							}
						}
						pen_usage_count[ ( blit_lookup_low[ i ] & 0xff ) ]++;
					}
				}
				if( ran_out == 0 )
				{
					break;
				}

				if( !reuse_pens )
				{
					/* Ran out of pens! Let's see what we can do. */
					/* from now on, try to reuse already allocated pens */
					reuse_pens = 1;
				}
				else if( !compress_palette( display ) )
				{
					/* no free pens, reduce color accuracy */
					if( palette_bit < MAX_PALETTE_BITS - 1 )
					{
						palette_bit++;
						scale_palette_r = scale_palette[ palette_bits[ palette_bit ].r ];
						scale_palette_g = scale_palette[ palette_bits[ palette_bit ].g ];
						scale_palette_b = scale_palette[ palette_bits[ palette_bit ].b ];

						for( j = 0; j < TOTAL_PENS; j++ )
						{
							shrinked_palette[ j ].r = scale_palette_r[ ( ( shrinked_palette[ j ].r * 255 ) + 31 ) / 63 ];
							shrinked_palette[ j ].g = scale_palette_g[ ( ( shrinked_palette[ j ].g * 255 ) + 31 ) / 63 ];
							shrinked_palette[ j ].b = scale_palette_b[ ( ( shrinked_palette[ j ].b * 255 ) + 31 ) / 63 ];
							set_color( j, &shrinked_palette[ j ] );
						}
						compress_palette( display );
#ifdef MAME_DEBUG
						{
							ui_popup( "need %d colors switching to r%dg%db%d", ran_out,
								palette_bits[ palette_bit ].r,
								palette_bits[ palette_bit ].g,
								palette_bits[ palette_bit ].b );
						}
#endif
						logerror( "switching to r%dg%db%d\n", palette_bits[ palette_bit ].r,
							palette_bits[ palette_bit ].g, palette_bits[ palette_bit ].b );
					}
					else
					{
						/* this should not happen */
						ui_popup( "Error: Palette overflow -%d", ran_out );

						logerror( "Error: no way to shrink the palette to 256 colors, left out %d colors.\n", ran_out );
#if VERBOSE
						logerror( "color list:\n");
						for( i = 0; i < display->game_palette_entries; i++ )
						{
							rgb_t rgbvalue = display->game_palette[ i ];
							int r = scale_palette_r[ RGB_RED( rgbvalue ) ];
							int g = scale_palette_g[ RGB_GREEN( rgbvalue ) ];
							int b = scale_palette_b[ RGB_BLUE( rgbvalue ) ];
							logerror( "%02x %02x %02x\n", r, g, b );
						}
#endif
						break;
					}
				}
			}

#if PEDANTIC
			{
				int pen;

				/* invalidate unused pens to make bugs in color allocation evident. */
				for( pen = 0; pen < TOTAL_PENS; pen++ )
				{
					if( pen_usage_count[ pen ] == 0 )
					{
						shrinked_palette[ pen ].r = scale_palette_r[ rand() & 0xff ];
						shrinked_palette[ pen ].g = scale_palette_g[ rand() & 0xff ];
						shrinked_palette[ pen ].b = scale_palette_b[ rand() & 0xff ];
						set_color( pen, &shrinked_palette[ pen ] );
					}
				}
			}
#endif
		}
	}
	else
	{
		// loop over dirty colors in batches of 32
		for (i = 0; i < display->game_palette_entries; i += 32)
		{
			UINT32 dirtyflags = display->game_palette_dirty[i / 32];
			if (dirtyflags)
			{
				display->game_palette_dirty[i / 32] = 0;
				
				// loop over all 32 bits and update dirty entries
				for (j = 0; j < 32; j++, dirtyflags >>= 1)
				{
					if (dirtyflags & 1)
					{
						// extract the RGB values
						rgb_t rgbvalue = display->game_palette[i + j];
						int r = RGB_RED(rgbvalue);
						int g = RGB_GREEN(rgbvalue);
						int b = RGB_BLUE(rgbvalue);

						blit_lookup_low[ i + j ] = makecol( r, g, b ) * cmultiply;
					}
				}
			}
		}
	}
}

//============================================================
//	init_palette
//============================================================

static void init_palette( int colors )
{
	int i;
	int r;
	int g;
	int b;

	if( video_depth == 8 )
	{
		if( colors < 32768 )
		{
			/* r6g6b6 */
			palette_bit = 0;
		}
		else
		{
			/* r3g3b2 */
			palette_bit = MAX_PALETTE_BITS - 1;
		}
		scale_palette_r = scale_palette[ palette_bits[ palette_bit ].r ];
		scale_palette_g = scale_palette[ palette_bits[ palette_bit ].g ];
		scale_palette_b = scale_palette[ palette_bits[ palette_bit ].b ];

		for( i = 0; i < 256; i++ )
		{
			scale_palette[ 7 ][ i ] = 0;
			scale_palette[ 6 ][ i ] = ( ( ( i >> 2 ) * 63 ) + 31 ) / 63;
			scale_palette[ 5 ][ i ] = ( ( ( i >> 3 ) * 63 ) + 15 ) / 31;
			scale_palette[ 4 ][ i ] = ( ( ( i >> 4 ) * 63 ) + 7 ) / 15;
			scale_palette[ 3 ][ i ] = ( ( ( i >> 5 ) * 63 ) + 3 ) / 7;
			scale_palette[ 2 ][ i ] = ( ( ( i >> 6 ) * 63 ) + 1 ) / 3;
			scale_palette[ 1 ][ i ] = 0;
			scale_palette[ 0 ][ i ] = 0;
		}

		if( video_attributes & VIDEO_RGB_DIRECT )
		{
			if( bitmap_depth == 32 )
			{
				/* initialise 8-8-8 to 3-3-2 reduction table */
				for( i = 0; i < 65536; i++ )
				{
					r = ( ( ( i & 255 ) * 7 ) + 127 ) / 255;
					blit_lookup_high[ i ] = ( r << 5 ) * cmultiply;

					g = ( ( ( ( i >> 8 ) & 255 ) * 7 ) + 127 ) / 255;
					b = ( ( ( i & 255 ) * 3 ) + 127 ) / 255;
					blit_lookup_low[ i ] = ( ( g << 2 ) | b ) * cmultiply;
				}
			}
			else if( bitmap_depth == 15 )
			{
				/* initialise 5-5-5 to 3-3-2 reduction table */
				for( i = 0; i < 65536; i++ )
				{
					r = ( ( ( ( i >> 10 ) & 31 ) * 7 ) + 15 ) / 31;
					g = ( ( ( ( i >> 5 ) & 31 ) * 7 ) + 15 ) / 31;
					b = ( ( ( i & 31 ) * 3 ) + 15 ) / 31;
					blit_lookup_low[ i ] = ( ( r << 5 ) | ( g << 2 ) | b ) * cmultiply;
				}
			}
			/* initialize the palette to a fixed 3-3-2 mapping */
			for( r = 0; r < 8; r++ )
			{
				for( g = 0; g < 8; g++ )
				{
					for( b = 0; b < 4; b++ )
					{
						int idx = ( r << 5 ) | ( g << 2 ) | b;
						int rr = ( ( r * 255 ) + 3 ) / 7;
						int gg = ( ( g * 255 ) + 3 ) / 7;
						int bb = ( ( b * 255 ) + 1 ) / 3;

						shrinked_palette[ idx ].r = scale_palette_r[ rr ];
						shrinked_palette[ idx ].g = scale_palette_g[ gg ];
						shrinked_palette[ idx ].b = scale_palette_b[ bb ];
						set_color( idx, &shrinked_palette[ idx ] );
					}
				}
			}
		}
		else
		{
			/* initialise palette lookup table */
			for( i = 0; i < 256; i++ )
			{
				pen_usage_count[ i ] = 0;
				shrinked_palette[ i ].r = scale_palette_r[ 0 ];
				shrinked_palette[ i ].g = scale_palette_g[ 0 ];
				shrinked_palette[ i ].b = scale_palette_b[ 0 ];
				set_color( i, &shrinked_palette[ i ] );
			}

			shrinked_palette[ WHITE_PEN ].r = scale_palette_r[ 255 ];
			shrinked_palette[ WHITE_PEN ].g = scale_palette_g[ 255 ];
			shrinked_palette[ WHITE_PEN ].b = scale_palette_b[ 255 ];
			set_color( WHITE_PEN, &shrinked_palette[ WHITE_PEN ] );

			for( i = 0; i < 65536; i++ )
			{
				blit_lookup_low[ i ] = BLACK_PEN * cmultiply;
			}
			pen_usage_count[ BLACK_PEN ] = 65536;
		}
	}
	else
	{
		if( bitmap_depth == 32 )
		{
			if( video_depth == 15 || video_depth == 16 )
			{
				/* initialise 8-8-8 to 5-5-5/5-6-5 reduction table */
				for( i = 0; i < 65536; i++ )
				{
					blit_lookup_low[ i ] = makecol( 0, ( i >> 8 ) & 0xff, i & 0xff ) * cmultiply;
					blit_lookup_high[ i ] = makecol( i & 0xff, 0, 0 ) * cmultiply;
				}
			}
		}
		else
		{
			/* initialize the palette to a fixed 5-5-5 mapping */
			for (r = 0; r < 32; r++)
			{
				for (g = 0; g < 32; g++)
				{
					for (b = 0; b < 32; b++)
					{
						int idx = (r << 10) | (g << 5) | b;
						int rr = ( ( r * 255 ) + 15 ) / 31;
						int gg = ( ( g * 255 ) + 15 ) / 31;
						int bb = ( ( b * 255 ) + 15 ) / 31;
						blit_lookup_low[ idx ] = makecol( rr, gg, bb ) * cmultiply;
					}
				}
			}
		}
	}
}

//============================================================
//	update_palette_debugger
//============================================================

static void update_palette_debugger(mame_display *display)
{
	int i;

	for (i = 0; i < display->debug_palette_entries; i ++)
	{
		// extract the RGB values
		rgb_t rgbvalue = display->debug_palette[i];
		int r = RGB_RED(rgbvalue);
		int g = RGB_GREEN(rgbvalue);
		int b = RGB_BLUE(rgbvalue);

		if( video_depth == 8 )
		{
			if( i < 256 )
			{
				RGB adjusted_palette;
				adjusted_palette.r = r >> 2;
				adjusted_palette.g = g >> 2;
				adjusted_palette.b = b >> 2;

				set_color( i, &adjusted_palette );
				blit_lookup_low[ i ] = i * cmultiply;
			}
		}
		else
		{
			blit_lookup_low[ i ] = makecol( r, g, b ) * cmultiply;
		}
	}
}

/* center image inside the display based on the visual area */
static void update_visible_area(mame_display *display)
{
	int align;
	int act_width;

/* if it's a SVGA arcade monitor mode, get the memory width of the mode */
/* this could be double the width of the actual mode set */
	if( gfx_mode != GFX_VGA && scanrate15KHz )
	{
		act_width = SVGA15KHzdriver->getlogicalwidth (gfx_width);
	}
	else
	{
		act_width = gfx_width;
	}

	if (show_debugger)
	{
		vis_min_x = 0;
		vis_max_x = display->debug_bitmap->width-1;
		vis_min_y = 0;
		vis_max_y = display->debug_bitmap->height-1;
	}
	else
	{
		if( video_swapxy )
		{
			vis_min_x = display->game_visible_area.min_y;
			vis_max_x = display->game_visible_area.max_y;
			vis_min_y = display->game_visible_area.min_x;
			vis_max_y = display->game_visible_area.max_x;
		}
		else
		{
			vis_min_x = display->game_visible_area.min_x;
			vis_max_x = display->game_visible_area.max_x;
			vis_min_y = display->game_visible_area.min_y;
			vis_max_y = display->game_visible_area.max_y;
		}
	}

	logerror("set visible area %d-%d %d-%d\n",vis_min_x,vis_max_x,vis_min_y,vis_max_y);

	viswidth  = vis_max_x - vis_min_x + 1;
	visheight = vis_max_y - vis_min_y + 1;

	if( viswidth <= 1 || visheight <= 1 )
	{
		return;
	}

	if (show_debugger)
	{
		xmultiply = ymultiply = 1;
	}
	else
	{
		/* setup xmultiply to handle SVGA driver's (possible) double width */
		xmultiply = act_width / gfx_width;
		ymultiply = 1;

		if( ( video_attributes & VIDEO_TYPE_VECTOR ) == 0 )
		{
			if( stretch )
			{
				int xm;
				int ym;

				if( keep_aspect )
				{
					int reqwidth;
					int reqheight;
					double adjusted_ratio;

					adjusted_ratio = ( ( (double)gfx_width / (double)gfx_height ) / screen_aspect ) * bitmap_aspect_ratio;

					reqwidth = ( gfx_width / viswidth ) * viswidth;
					reqheight = (int)((double)reqwidth / adjusted_ratio);

					// scale up if too small
					if (reqwidth < viswidth)
					{
						reqwidth = viswidth;
						reqheight = (int)((double)reqwidth / adjusted_ratio);
					}
					if (reqheight < visheight)
					{
						reqheight = visheight;
						reqwidth = (int)((double)reqheight * adjusted_ratio);
					}

					// scale down if too big
					if (reqwidth > gfx_width)
					{
						reqwidth = ( gfx_width / viswidth ) * viswidth;
						reqheight = (int)((double)reqwidth / adjusted_ratio);
					}
					if (reqheight > gfx_height)
					{
						reqheight = ( gfx_height / visheight ) * visheight;
						reqwidth = (int)((double)reqheight * adjusted_ratio);
					}

#define VISDIV ( 2 )
					/* round to 33% of visible size, clamp at screen size */
					reqwidth = ( reqwidth * VISDIV ) + viswidth;
					if( reqwidth > gfx_width * VISDIV )
					{
						reqwidth = gfx_width * VISDIV;
					}
					reqheight = ( reqheight * VISDIV ) + visheight;
					if( reqheight > gfx_height * VISDIV )
					{
						reqheight = gfx_height * VISDIV;
					}

					xm = ( reqwidth / viswidth ) / VISDIV;
					ym = ( reqheight / visheight ) / VISDIV;
				}
				else
				{
					xm = ( gfx_width / viswidth );
					ym = ( gfx_height / visheight );
				}
				if( xm < 1 )
				{
					xm = 1;
				}
				if( ym < 1 )
				{
					ym = 1;
				}

				xmultiply *= xm;
				ymultiply *= ym;
			}
			else
			{
				if ((video_attributes & VIDEO_PIXEL_ASPECT_RATIO_MASK) == VIDEO_PIXEL_ASPECT_RATIO_1_2)
				{
					if (video_swapxy)
					{
						xmultiply *= 2;
					}
					else
					{
						ymultiply *= 2;
					}
				}
				else if ((video_attributes & VIDEO_PIXEL_ASPECT_RATIO_MASK) == VIDEO_PIXEL_ASPECT_RATIO_2_1)
				{
					if (video_swapxy)
					{
						ymultiply *= 2;
					}
					else
					{
						xmultiply *= 2;
					}
				}
			}
		}

		if (xmultiply > MAX_X_MULTIPLY) xmultiply = MAX_X_MULTIPLY;
		if (ymultiply > MAX_Y_MULTIPLY) ymultiply = MAX_Y_MULTIPLY;
	}

	gfx_display_lines = visheight;
	gfx_display_columns = viswidth;

	if (show_debugger)
	{
		gfx_xoffset = 0;
		gfx_yoffset = 0;
		skiplines = 0;
		skiplinesmin = 0;
		skiplinesmax = 0;
		skipcolumns = 0;
		skipcolumnsmin = 0;
		skipcolumnsmax = 0;
	}
	else
	{
		gfx_xoffset = (act_width - viswidth * xmultiply) / 2;
		if (gfx_display_columns > act_width / xmultiply)
			gfx_display_columns = act_width / xmultiply;

		gfx_yoffset = (gfx_height - visheight * ymultiply) / 2;
			if (gfx_display_lines > gfx_height / ymultiply)
				gfx_display_lines = gfx_height / ymultiply;

		skiplinesmin = vis_min_y;
		skiplinesmax = visheight - gfx_display_lines + vis_min_y;
		skipcolumnsmin = vis_min_x;
		skipcolumnsmax = viswidth - gfx_display_columns + vis_min_x;

		/* the skipcolumns from mame.cfg/cmdline is relative to the visible area */
		if( video_flipx )
		{
			skipcolumns = skipcolumnsmax;
			if( gfx_skipcolumns >= 0 )
			{
				skipcolumns -= gfx_skipcolumns;
			}
		}
		else
		{
			skipcolumns = skipcolumnsmin;
			if( gfx_skipcolumns >= 0 )
			{
				skipcolumns += gfx_skipcolumns;
			}
		}
		if( video_flipy )
		{
			skiplines   = skiplinesmax;
			if( gfx_skiplines >= 0 )
			{
				skiplines -= gfx_skiplines;
			}
		}
		else
		{
			skiplines   = skiplinesmin;
			if( gfx_skiplines >= 0 )
			{
				skiplines += gfx_skiplines;
			}
		}

		/* Just in case the visual area doesn't fit */
		if (gfx_xoffset < 0)
		{
			if( gfx_skipcolumns < 0 )
			{
				if( video_flipx )
				{
					skipcolumns += gfx_xoffset;
				}
				else
				{
					skipcolumns -= gfx_xoffset;
				}
			}
			gfx_xoffset = 0;
		}
		if (gfx_yoffset < 0)
		{
			if( gfx_skiplines < 0 )
			{
				if( video_flipy )
				{
					skiplines   += gfx_yoffset;
				}
				else
				{
					skiplines   -= gfx_yoffset;
				}
			}
			gfx_yoffset = 0;
		}

		/* align left hand side */
		if( unchained )
		{
			align = 8;
		}
		else if( video_depth == 8 )
		{
			align = 4;
		}
		else if( video_depth == 15 || video_depth == 16 )
		{
			align = 2;
		}
		else if( video_depth == 24 )
		{
			align = 12;
		}
		else
		{
			align = 1;
		}

		gfx_xoffset -= gfx_xoffset % ( align * xdivide );

		/* Failsafe against silly parameters */
		if (skiplines < skiplinesmin)
			skiplines = skiplinesmin;
		if (skipcolumns < skipcolumnsmin)
			skipcolumns = skipcolumnsmin;
		if (skiplines > skiplinesmax)
			skiplines = skiplinesmax;
		if (skipcolumns > skipcolumnsmax)
			skipcolumns = skipcolumnsmax;

		logerror("gfx_width = %d gfx_height = %d\n"
				"gfx_xoffset = %d gfx_yoffset = %d\n"
				"xmin %d ymin %d xmax %d ymax %d\n"
				"skiplines %d skipcolumns %d\n"
				"gfx_skiplines %d gfx_skipcolumns %d\n"
				"gfx_display_lines %d gfx_display_columns %d\n"
				"xmultiply %d ymultiply %d\n"
				"xdivide %d ydivide %d unchained %d\n"
				"skiplinesmin %d skiplinesmax %d\n"
				"skipcolumnsmin %d skipcolumnsmax %d\n"
				"video_flipx %d video_flipy %d video_swapxy %d\n",
				gfx_width, gfx_height,
				gfx_xoffset, gfx_yoffset,
				vis_min_x, vis_min_y, vis_max_x, vis_max_y,
				skiplines, skipcolumns,
				gfx_skiplines, gfx_skipcolumns,
				gfx_display_lines, gfx_display_columns,
				xmultiply, ymultiply,
				xdivide, ydivide, unchained,
				skiplinesmin, skiplinesmax,
				skipcolumnsmin, skipcolumnsmax,
				video_flipx, video_flipy, video_swapxy );

		if( video_swapxy )
		{
			ui_set_visible_area(skiplines, skipcolumns, skiplines+gfx_display_lines-1, skipcolumns+gfx_display_columns-1);
		}
		else
		{
			ui_set_visible_area(skipcolumns, skiplines, skipcolumns+gfx_display_columns-1, skiplines+gfx_display_lines-1);
		}
	}

	if (gfx_mode == GFX_VGA)
	{
		/* check for unchained modes */
		if (unchained)
		{
			bitblit = bitblit_unchained_vga;
		}
		else
		{
			bitblit = bitblit_vga;
		}
	}
	else
	{
		if (use_mmx == -1) /* mmx=auto: can new mmx blitters be applied? */
		{
			/* impossible cases follow */
			if (!cpu_mmx)
				mmxlfb = 0;
			else if ((gfx_mode != GFX_VESA2L) && (gfx_mode != GFX_VESA3))
				mmxlfb = 0;
			else
				mmxlfb = 1;
		}
		else /* use forced mmx= setting from mame.cfg at own risk!!! */
			mmxlfb = use_mmx;

		/* Check for SVGA 15.75KHz mode (req. for 15.75KHz Arcade Monitor Modes)
		need to do this here, as the double params will be set up correctly */
		if( scanrate15KHz )
		{
			int dbl;
			if( ymultiply == 2 )
			{
				/* if we're doubling, we might as well have scanlines */
				/* the 15.75KHz driver is going to drop every other line anyway -
					so we can avoid drawing them and save some time */
				hscanlines=1;
				dbl = 1;
			}
			else
			{
				dbl = 0;
			}
			logerror("Using %s 15.75KHz SVGA driver\n", SVGA15KHzdriver->name);
			if (!SVGA15KHzdriver->setSVGA15KHzmode (dbl, gfx_width, gfx_height))
			{
				/*and try to set the mode */
				logerror("\nUnable to set SVGA 15.75KHz mode %dx%d (driver: %s)\n", gfx_width, gfx_height, SVGA15KHzdriver->name);
			}
		}
		bitblit = bitblit_vesa;
	}
	if( show_debugger )
	{
		cmultiply = blit_setup( gfx_width, gfx_height, display->debug_bitmap->depth, video_depth,
			0, 1, 1, xdivide, ydivide,
			0, 0, 0, 0, 0 );
	}
	else
	{
		cmultiply = blit_setup( gfx_width, gfx_height, bitmap_depth, video_depth,
			video_attributes, xmultiply, ymultiply, xdivide, ydivide,
			vscanlines, hscanlines, video_flipx, video_flipy, video_swapxy );
	}
}



//============================================================
//	osd_create_display
//============================================================

int osd_create_display(const osd_create_params *params, UINT32 *rgb_components)
{
	mame_display dummy_display;

	if( video_swapxy )
	{
		video_width = params->height;
		video_height = params->width;
		bitmap_aspect_ratio = (double)params->aspect_y / (double)params->aspect_x;
	}
	else
	{
		video_width = params->width;
		video_height = params->height;
		bitmap_aspect_ratio = (double)params->aspect_x / (double)params->aspect_y;
	}

	bitmap_depth = params->depth;
	video_fps = params->fps;
	video_attributes = params->video_attributes;

	// clamp the frameskip value to within range
	if (frameskip < 0)
		frameskip = 0;
	if (frameskip >= FRAMESKIP_LEVELS)
		frameskip = FRAMESKIP_LEVELS - 1;

	show_debugger = 0;
	gone_to_gfx_mode = 0;

	logerror("width %d, height %d depth %d colors %d\n",video_width,video_height,bitmap_depth,params->colors);

	if (!select_display_mode(video_width,video_height,bitmap_depth,params->colors,video_attributes) )
	{
		return 1;
	}

	cmultiply = 0x00000001;
	init_palette( params->colors );

	// fill in the resulting RGB components
	if (rgb_components)
	{
		if ( bitmap_depth == 32 )
		{
			if( video_depth == 24 || video_depth == 32 )
			{
				rgb_components[0] = makecol(0xff, 0x00, 0x00);
				rgb_components[1] = makecol(0x00, 0xff, 0x00);
				rgb_components[2] = makecol(0x00, 0x00, 0xff);
			}
			else
			{
				rgb_components[0] = 0xff0000;
				rgb_components[1] = 0x00ff00;
				rgb_components[2] = 0x0000ff;
			}
		}
		else
		{
			if( video_depth == 15 || video_depth == 16 )
			{
#if defined( DIRECT15_USE_LOOKUP )
				video_attributes &= ~VIDEO_RGB_DIRECT;
			}
#else
				rgb_components[0] = makecol(0xf8, 0x00, 0x00);
				rgb_components[1] = makecol(0x00, 0xf8, 0x00);
				rgb_components[2] = makecol(0x00, 0x00, 0xf8);
			}
			else
#endif
			{
				rgb_components[0] = 0x7c00;
				rgb_components[1] = 0x03e0;
				rgb_components[2] = 0x001f;
			}
		}
	}

	// set visible area to nothing just to initialize it - it will be set by the core
	memset(&dummy_display, 0, sizeof(dummy_display));
	update_visible_area(&dummy_display);

	// indicate for later that we're just beginning
	warming_up = 1;
	return 0;
}


/* shut up the display */
void osd_close_display(void)
{
	if (gone_to_gfx_mode != 0)
	{
		/* tidy up if 15.75KHz SVGA mode used */
		if( gfx_mode != GFX_VGA && scanrate15KHz )
		{
			/* check we've got a valid driver before calling it */
			if (SVGA15KHzdriver != NULL)
			{
				SVGA15KHzdriver->resetSVGA15KHzmode();
			}
		}

		set_gfx_mode (GFX_TEXT,0,0,0,0);

		if (frames_displayed > FRAMES_TO_SKIP)
		{
			cycles_t cps = osd_cycles_per_second();
			printf("Average FPS: %f\n",(double)cps/(end_time-start_time)*(frames_displayed-FRAMES_TO_SKIP));
		}
	}
}

static void set_debugger_focus(mame_display *display, int debugger_has_focus)
{
	static int temp_afs, temp_fs, temp_throttle;

	if (show_debugger != debugger_has_focus)
	{
		show_debugger = debugger_has_focus;
		debugger_focus_changed = 1;

		if (show_debugger)
		{
			// store frameskip/throttle settings
			temp_fs       = frameskip;
			temp_afs      = autoframeskip;
			temp_throttle = throttle;

			// temporarily set them to usable values for the debugger
			frameskip     = 0;
			autoframeskip = 0;
			throttle      = 1;
		}
		else
		{
			/* silly way to clear the screen */
			mame_bitmap *clrbitmap;

			clrbitmap = bitmap_alloc_depth(gfx_display_columns,gfx_display_lines,display->debug_bitmap->depth);
			if (clrbitmap)
			{
				fillbitmap(clrbitmap,0,NULL);
				/* three times to handle triple buffering */
				bitblit( clrbitmap, 0, 0, gfx_display_columns, gfx_display_lines, 0, 0 );
				bitblit( clrbitmap, 0, 0, gfx_display_columns, gfx_display_lines, 0, 0 );
				bitblit( clrbitmap, 0, 0, gfx_display_columns, gfx_display_lines, 0, 0 );
				bitmap_free(clrbitmap);
			}

			init_palette( display->game_palette_entries );

			/* mark palette dirty */
			if( display->game_palette_dirty != NULL )
			{
				int i;

				for (i = 0; i < display->game_palette_entries; i++ )
				{
					display->game_palette_dirty[ i / 32 ] |= 1 << ( i % 32 );
				}
			}
			// restore frameskip/throttle settings
			frameskip     = temp_fs;
			autoframeskip = temp_afs;
			throttle      = temp_throttle;
		}
		display->changed_flags |= ( GAME_BITMAP_CHANGED | DEBUG_BITMAP_CHANGED | GAME_PALETTE_CHANGED | DEBUG_PALETTE_CHANGED | GAME_VISIBLE_AREA_CHANGED );
	}
}


static void bitblit_dummy( mame_bitmap *bitmap, int sx, int sy, int sw, int sh, int dx, int dy )
{
	logerror("msdos/video.c: undefined bitblit() function for %d x %d!\n",xmultiply,ymultiply);
}

INLINE void pan_display( mame_display *display )
{
	int pan_changed = 0;

	/* horizontal panning */
	if( ( !video_flipx && input_ui_pressed_repeat( IPT_UI_PAN_RIGHT, 1 ) ) ||
		( video_flipx && input_ui_pressed_repeat( IPT_UI_PAN_LEFT, 1 ) ) )
	{
		if (skipcolumns < skipcolumnsmax)
		{
			skipcolumns++;
			pan_changed = 1;
		}
	}
	if( ( !video_flipx && input_ui_pressed_repeat( IPT_UI_PAN_LEFT, 1 ) ) ||
		( video_flipx && input_ui_pressed_repeat( IPT_UI_PAN_RIGHT, 1 ) ) )
	{
		if (skipcolumns > skipcolumnsmin)
		{
			skipcolumns--;
			pan_changed = 1;
		}
	}
	/* vertical panning */
	if( ( !video_flipy && input_ui_pressed_repeat( IPT_UI_PAN_DOWN, 1 ) ) ||
		( video_flipy && input_ui_pressed_repeat( IPT_UI_PAN_UP, 1 ) ) )
	{
		if (skiplines < skiplinesmax)
		{
			skiplines++;
			pan_changed = 1;
		}
	}
	if( ( !video_flipy && input_ui_pressed_repeat( IPT_UI_PAN_UP, 1 ) ) ||
		( video_flipy && input_ui_pressed_repeat( IPT_UI_PAN_DOWN, 1 ) ) )
	{
		if (skiplines > skiplinesmin)
		{
			skiplines--;
			pan_changed = 1;
		}
	}

	if (pan_changed)
	{
		if( video_swapxy )
		{
			ui_set_visible_area(skiplines, skipcolumns, skiplines+gfx_display_lines-1, skipcolumns+gfx_display_columns-1);
		}
		else
		{
			ui_set_visible_area(skipcolumns, skiplines, skipcolumns+gfx_display_columns-1, skiplines+gfx_display_lines-1);
		}
	}
}


int osd_skip_this_frame(void)
{
	return skiptable[frameskip][frameskip_counter];
}


const char *osd_get_fps_text(const performance_info *performance)
{
	static char buffer[1024];
	char *dest = buffer;
	
	// display the FPS, frameskip, percent, fps and target fps
	dest += sprintf(dest, "%s%2d%4d%%%4d/%d fps", 
			autoframeskip ? "auto" : "fskp", frameskip, 
			(int)(performance->game_speed_percent + 0.5), 
			(int)(performance->frames_per_second + 0.5),
			(int)(Machine->refresh_rate + 0.5));

	/* for vector games, add the number of vector updates */
	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
	{
		dest += sprintf(dest, "\n %d vector updates", performance->vector_updates_last_second);
	}
	else if (performance->partial_updates_this_frame > 1)
	{
		dest += sprintf(dest, "\n %d partial updates", performance->partial_updates_this_frame);
	}
	
	/* return a pointer to the static buffer */
	return buffer;
}

void osd_set_leds(int state)
{
	int i;

	static const int led_flags[3] =
	{
		KB_NUMLOCK_FLAG,
		KB_CAPSLOCK_FLAG,
		KB_SCROLOCK_FLAG
	};

	if (use_keyboard_leds)
	{
		i = 0;
		if (state & 1) i |= led_flags[0];
		if (state & 2) i |= led_flags[1];
		if (state & 4) i |= led_flags[2];
		set_leds(i);
	}
}

//============================================================
//	win_orient_rect
//============================================================

void win_orient_rect(rectangle *_rect)
{
	int temp;

	// apply X/Y swap first
	if (video_swapxy)
	{
		temp = _rect->min_x; _rect->min_x = _rect->min_y; _rect->min_y = temp;
		temp = _rect->max_x; _rect->max_x = _rect->max_y; _rect->max_y = temp;
	}

	// apply X flip
	if (video_flipx)
	{
		temp = video_width - _rect->min_x - 1;
		_rect->min_x = video_width - _rect->max_x - 1;
		_rect->max_x = temp;
	}

	// apply Y flip
	if (video_flipy)
	{
		temp = video_height - _rect->min_y - 1;
		_rect->min_y = video_height - _rect->max_y - 1;
		_rect->max_y = temp;
	}
}

/* Update the display. */
void osd_update_video_and_audio(mame_display *display)
{
	cycles_t curr, target;
	static cycles_t prev_measure,this_frame_base;
	static cycles_t last_sound_update;

	int already_synced;
	int throttle_audio;
	cycles_t cps = osd_cycles_per_second();

	if (display->changed_flags & DEBUG_FOCUS_CHANGED)
	{
		set_debugger_focus(display, display->debug_focus);
	}

	if (display->debug_bitmap && input_ui_pressed(IPT_UI_TOGGLE_DEBUG))
	{
		set_debugger_focus(display, show_debugger ^ 1);
	}

	// if the visible area has changed, update it
	if (display->changed_flags & GAME_VISIBLE_AREA_CHANGED)
		update_visible_area(display);

	// if the refresh rate has changed, update it
	if (display->changed_flags & GAME_REFRESH_RATE_CHANGED)
	{
		video_fps = display->game_refresh_rate;
		sound_update_refresh_rate(display->game_refresh_rate);
		/* todo: vsync */
	}

	if (warming_up)
	{
		/* first time through, initialize timer */
		prev_measure = osd_cycles() - (int)((double)FRAMESKIP_LEVELS * (double)cps / video_fps);
		last_sound_update = osd_cycles() - ( 2 * cps / video_fps ) - 1;
		warming_up = 0;
	}

	if (frameskip_counter == 0)
		this_frame_base = prev_measure + (int)((double)FRAMESKIP_LEVELS * (double)cps / video_fps);

	throttle_audio = throttle;
	if( throttle_audio )
	{
		/* if too much time has passed since last sound update, disable throttling */
		/* temporarily - we wouldn't be able to keep synch anyway. */
		curr = osd_cycles();
		if ((curr - last_sound_update) > 2 * cps / video_fps)
			throttle_audio = 0;
		last_sound_update = curr;
	}
	already_synced = msdos_update_audio( throttle_audio );

	if (display->changed_flags & GAME_PALETTE_CHANGED)
	{
		// if the game palette has changed, update it
		if( !show_debugger )
		{
			update_palette(display);
		}
	}

	if ( display->changed_flags & GAME_BITMAP_CHANGED )
	{
		/* now wait until it's time to update the screen */
		if (throttle)
		{
			profiler_mark(PROFILER_IDLE);
			if (video_sync)
			{
				static cycles_t last;
	
				do
				{
					vsync();
					curr = osd_cycles();
				} while (cps / (curr - last) > video_fps * 11 /10);

				last = curr;
			}
			else
			{
				/* wait for video sync but use normal throttling */
				if (wait_vsync)
					vsync();
	
				curr = osd_cycles();

				if (already_synced == 0)
				{
				/* wait only if the audio update hasn't synced us already */
					target = this_frame_base + (int)((double)frameskip_counter * (double)cps / video_fps);

					if (curr - target < 0)
					{
						do
						{
							curr = osd_cycles();
						} while (curr - target < 0);
					}
				}
			}
			profiler_mark(PROFILER_END);
		}
		else
			curr = osd_cycles();

		if (frameskip_counter == 0)
		{
			prev_measure = curr;
		}

		/* for the FPS average calculation */
		if (++frames_displayed == FRAMES_TO_SKIP)
			start_time = curr;
		else
			end_time = curr;

		if( !show_debugger )
		{
			int dstxoffs;
			int dstyoffs;
			int srcxoffs;
			int srcyoffs;
			int srcwidth;
			int srcheight;

			dstxoffs = gfx_xoffset;
			dstyoffs = gfx_yoffset;

			srcxoffs = skipcolumns;
			srcyoffs = skiplines;

			srcwidth = gfx_display_columns;
			srcheight = gfx_display_lines;

			if( video_flipx )
			{
				srcxoffs += ( srcwidth - 1 );
			}
			if( video_flipy )
			{
				srcyoffs += ( srcheight - 1 );
			}
			if( video_swapxy )
			{
				int t;
				t = srcxoffs;
				srcxoffs = srcyoffs;
				srcyoffs = t;
			}
#if 0
			/* not working yet */
//			logerror( "game_bitmap_update %d %d %d %d\n", display->game_bitmap_update.min_x, display->game_bitmap_update.min_y, display->game_bitmap_update.max_x, display->game_bitmap_update.max_y );
//			logerror( "%d %d %d %d %d %d\n", dstxoffs, dstyoffs, srcxoffs, srcyoffs, srcwidth, srcheight );
			{
				rectangle updatebounds = display->game_bitmap_update;

				srcxoffs = 0;
				srcyoffs = 0;

				// adjust for more optimal bounds

				if( ( !video_swapxy && video_flipx ) || ( video_swapxy && video_flipy ) )
				{
					srcxoffs += updatebounds.max_x;
				}
				else
				{
					srcxoffs += updatebounds.min_x;
				}
				if( ( !video_swapxy && video_flipy ) || ( video_swapxy && video_flipx ) )
				{
					srcyoffs += updatebounds.max_y;
				}
				else
				{
					srcyoffs += updatebounds.min_y;
				}
				if( video_swapxy )
				{
					srcheight = updatebounds.max_x - updatebounds.min_x + 1;
					srcwidth = updatebounds.max_y - updatebounds.min_y + 1;
				}
				else
				{
					srcwidth = updatebounds.max_x - updatebounds.min_x + 1;
					srcheight = updatebounds.max_y - updatebounds.min_y + 1;
				}
				win_orient_rect(&updatebounds);

				dstxoffs += ( updatebounds.min_x - skipcolumns ) * xmultiply;
				dstyoffs += ( updatebounds.min_y - skiplines ) * ymultiply;
			}
//			logerror( "%d %d %d %d %d %d\n", dstxoffs, dstyoffs, srcxoffs, srcyoffs, srcwidth, srcheight );
#endif

			/* copy the bitmap to screen memory */
			profiler_mark( PROFILER_BLIT );
			if (zvg_enabled != 2)
			{
				bitblit( display->game_bitmap, srcxoffs, srcyoffs, srcwidth, srcheight, dstxoffs, dstyoffs );
			}
			profiler_mark( PROFILER_END );
		}

		/* see if we need to give the card enough time to draw both odd/even fields of the interlaced display
			(req. for 15.75KHz Arcade Monitor Modes */
		interlace_sync();

		if( throttle && autoframeskip && frameskip_counter == 0 )
		{
			const performance_info *performance = mame_get_performance_info();
	
			// if we're too fast, attempt to increase the frameskip
			if (performance->game_speed_percent >= 99.5)
			{
				frameskipadjust++;

				// but only after 3 consecutive frames where we are too fast
				if (frameskipadjust >= 3)
				{
					frameskipadjust = 0;
					if (frameskip > 0) frameskip--;
				}
			}

			// if we're too slow, attempt to decrease the frameskip
			else
			{
				// if below 80% speed, be more aggressive
				if (performance->game_speed_percent < 80)
					frameskipadjust -= (90 - performance->game_speed_percent) / 5;

				// if we're close, only force it up to frameskip 8
				else if (frameskip < 8)
					frameskipadjust--;

				// perform the adjustment
				while (frameskipadjust <= -2)
				{
					frameskipadjust += 2;
					if (frameskip < FRAMESKIP_LEVELS - 1)
						frameskip++;
				}
			}
		}
	}

	if( show_debugger )
	{
		if( display->changed_flags & DEBUG_PALETTE_CHANGED )
		{
			update_palette_debugger( display );
		}
		if( display->changed_flags & DEBUG_BITMAP_CHANGED || xdivide != 1 || ydivide != 1 )
		{
			bitblit( display->debug_bitmap, 0, 0, gfx_display_columns, gfx_display_lines, 0, 0 );
		}
	}
	else
	{
		/* Check for PGUP, PGDN and pan screen */
		pan_display( display );

		if (input_ui_pressed(IPT_UI_FRAMESKIP_INC))
		{
			if (autoframeskip)
			{
				autoframeskip = 0;
				frameskip = 0;
			}
			else
			{
				if (frameskip == FRAMESKIP_LEVELS-1)
				{
					frameskip = 0;
					autoframeskip = 1;
				}
				else
					frameskip++;
			}

			// display the FPS counter for 2 seconds
			ui_show_fps_temp(2.0);

			/* reset the frame counter every time the frameskip key is pressed, so */
			/* we'll measure the average FPS on a consistent status. */
			frames_displayed = 0;
		}

		if (input_ui_pressed(IPT_UI_FRAMESKIP_DEC))
		{
			if (autoframeskip)
			{
				autoframeskip = 0;
				frameskip = FRAMESKIP_LEVELS-1;
			}
			else
			{
				if (frameskip == 0)
					autoframeskip = 1;
				else
					frameskip--;
			}

			// display the FPS counter for 2 seconds
			ui_show_fps_temp(2.0);

			/* reset the frame counter every time the frameskip key is pressed, so */
			/* we'll measure the average FPS on a consistent status. */
			frames_displayed = 0;
		}

		if (input_ui_pressed(IPT_UI_THROTTLE))
		{
			throttle ^= 1;

			/* reset the frame counter every time the throttle key is pressed, so */
			/* we'll measure the average FPS on a consistent status. */
			frames_displayed = 0;
		}
	}

	// if the LEDs have changed, update them
	if (display->changed_flags & LED_STATE_CHANGED)
		osd_set_leds(display->led_state);

	frameskip_counter = (frameskip_counter + 1) % FRAMESKIP_LEVELS;

	poll_joysticks();
}


#define blit_swapxy video_swapxy
#define blit_flipx video_flipx
#define blit_flipy video_flipy

//============================================================
//	osd_override_snapshot
//============================================================

mame_bitmap *osd_override_snapshot(mame_bitmap *bitmap, rectangle *bounds)
{
	rectangle newbounds;
	mame_bitmap *copy;
	int x, y, w, h, t;

	// if we can send it in raw, no need to override anything
	if (!blit_swapxy && !blit_flipx && !blit_flipy)
		return NULL;

	// allocate a copy
	w = blit_swapxy ? bitmap->height : bitmap->width;
	h = blit_swapxy ? bitmap->width : bitmap->height;
	copy = bitmap_alloc_depth(w, h, bitmap->depth);
	if (!copy)
		return NULL;

	// populate the copy
	for (y = bounds->min_y; y <= bounds->max_y; y++)
		for (x = bounds->min_x; x <= bounds->max_x; x++)
		{
			int tx = x, ty = y;

			// apply the rotation/flipping
			if (blit_swapxy)
			{
				t = tx; tx = ty; ty = t;
			}
			if (blit_flipx)
				tx = copy->width - tx - 1;
			if (blit_flipy)
				ty = copy->height - ty - 1;

			// read the old pixel and copy to the new location
			switch (copy->depth)
			{
				case 15:
				case 16:
					*((UINT16 *)copy->base + ty * copy->rowpixels + tx) =
							*((UINT16 *)bitmap->base + y * bitmap->rowpixels + x);
					break;

				case 32:
					*((UINT32 *)copy->base + ty * copy->rowpixels + tx) =
							*((UINT32 *)bitmap->base + y * bitmap->rowpixels + x);
					break;
			}
		}

	// compute the oriented bounds
	newbounds = *bounds;

	// apply X/Y swap first
	if (blit_swapxy)
	{
		t = newbounds.min_x; newbounds.min_x = newbounds.min_y; newbounds.min_y = t;
		t = newbounds.max_x; newbounds.max_x = newbounds.max_y; newbounds.max_y = t;
	}

	// apply X flip
	if (blit_flipx)
	{
		t = copy->width - newbounds.min_x - 1;
		newbounds.min_x = copy->width - newbounds.max_x - 1;
		newbounds.max_x = t;
	}

	// apply Y flip
	if (blit_flipy)
	{
		t = copy->height - newbounds.min_y - 1;
		newbounds.min_y = copy->height - newbounds.max_y - 1;
		newbounds.max_y = t;
	}

	*bounds = newbounds;
	return copy;
}


Register *make_nondoubled_mode(Register *inreg,int entries)
{
	static Register outreg[32];
	int maxscan;
/* first - check's it not already a 'non doubled' line mode */
	maxscan = inreg[MAXIMUM_SCANLINE_INDEX].value;
	if ((maxscan & 1) == 0)
	/* it is, so just return the array as is */
  		return inreg;
/* copy across our standard display array */
	memcpy (&outreg, inreg, entries * sizeof(Register));
	outreg[MAXIMUM_SCANLINE_INDEX].value &= 0xfe;
	unchain_vga( outreg );
	ydivide /= 2;
	return outreg;
}

Register *make_scanline_mode(Register *inreg,int entries)
{
	static Register outreg[32];
	int maxscan,maxscanout;
	int overflow,overflowout;
	int ytotalin,ytotalout;
	int ydispin,ydispout;
	int vrsin,vrsout,vreout,vblksout,vblkeout;
/* first - check's it not already a 'non doubled' line mode */
	maxscan = inreg[MAXIMUM_SCANLINE_INDEX].value;
	if ((maxscan & 1) == 0)
	/* it is, so just return the array as is */
  		return inreg;
/* copy across our standard display array */
	memcpy (&outreg, inreg, entries * sizeof(Register));
/* keep hold of the overflow register - as we'll need to refer to it a lot */
	overflow = inreg[OVERFLOW_INDEX].value;
/* set a large line compare value  - as we won't be doing any split window scrolling etc.*/
	maxscanout = 0x40;
/* half all the y values */
/* total */
	ytotalin = inreg[V_TOTAL_INDEX].value;
	ytotalin |= ((overflow & 1)<<0x08) | ((overflow & 0x20)<<0x04);
    ytotalout = ytotalin >> 1;
/* display enable end */
	ydispin = inreg[13].value | ((overflow & 0x02)<< 0x07) | ((overflow & 0x040) << 0x03);
	ydispin ++;
	ydispout = ydispin >> 1;
	ydispout --;
	overflowout = ((ydispout & 0x100) >> 0x07) | ((ydispout && 0x200) >> 0x03);
	outreg[V_END_INDEX].value = (ydispout & 0xff);
/* avoid top over scan */
	if ((ytotalin - ydispin) < 40 && !center_y)
	{
  		vrsout = ydispout;
		/* give ourselves a scanline cushion */
		ytotalout += 2;
	}
  	else
	{
/* vertical retrace start */
		vrsin = inreg[V_RETRACE_START_INDEX].value | ((overflow & 0x04)<<0x06) | ((overflow & 0x80)<<0x02);
		vrsout = vrsin >> 1;
	}
/* check it's legal */
	if (vrsout < ydispout)
		vrsout = ydispout;
/*update our output overflow */
	overflowout |= (((vrsout & 0x100) >> 0x06) | ((vrsout & 0x200) >> 0x02));
	outreg[V_RETRACE_START_INDEX].value = (vrsout & 0xff);
/* vertical retrace end */
	vreout = vrsout + 2;
/* make sure the retrace fits into our adjusted display size */
	if (vreout > (ytotalout - 9))
		ytotalout = vreout + 9;
/* write out the vertical retrace end */
	outreg[V_RETRACE_END_INDEX].value &= ~0x0f;
	outreg[V_RETRACE_END_INDEX].value |= (vreout & 0x0f);
/* vertical blanking start */
	vblksout = ydispout + 1;
/* check it's legal */
	if(vblksout > vreout)
		vblksout = vreout;
/* save the overflow value */
	overflowout |= ((vblksout & 0x100) >> 0x05);
	maxscanout |= ((vblksout & 0x200) >> 0x04);
/* write the v blank value out */
	outreg[V_BLANKING_START_INDEX].value = (vblksout & 0xff);
/* vertical blanking end */
	vblkeout = vreout + 1;
/* make sure the blanking fits into our adjusted display size */
	if (vblkeout > (ytotalout - 9))
		ytotalout = vblkeout + 9;
/* write out the vertical blanking total */
	outreg[V_BLANKING_END_INDEX].value = (vblkeout & 0xff);
/* update our output overflow */
	overflowout |= ((ytotalout & 0x100) >> 0x08) | ((ytotalout & 0x200) >> 0x04);
/* write out the new vertical total */
	outreg[V_TOTAL_INDEX].value = (ytotalout & 0xff);

/* write out our over flows */
	outreg[OVERFLOW_INDEX].value = overflowout;
/* finally the max scan line */
	outreg[MAXIMUM_SCANLINE_INDEX].value = maxscanout;
/* and we're done */
	return outreg;

}


void center_mode(Register *pReg)
{
	int center;
	int hrt_start, hrt_end, hrt, hblnk_start, hblnk_end;
	int vrt_start, vrt_end, vert_total, vert_display, vblnk_start, vrt, vblnk_end;
/* check for empty array */
	if (!pReg)
		return;
/* vertical retrace width */
	vrt = 2;
/* check the clock speed, to work out the retrace width */
	if (pReg[CLOCK_INDEX].value == 0xe7)
		hrt = 11;
	else
		hrt = 10;
/* our center x tweak value */
	center = center_x;
/* check for double width scanline rather than half clock (15.75kHz modes) */
	if( pReg[H_TOTAL_INDEX].value > 0x96)
	{
		center<<=1;
		hrt<<=1;
	}
/* set the hz retrace */
	hrt_start = pReg[H_RETRACE_START_INDEX].value;
	hrt_start += center;
/* make sure it's legal */
	if (hrt_start <= pReg[H_DISPLAY_INDEX].value)
		hrt_start = pReg[H_DISPLAY_INDEX].value + 1;
	pReg[H_RETRACE_START_INDEX].value = hrt_start;
/* set hz retrace end */
	hrt_end = hrt_start + hrt;
/* make sure it's legal */
	if( hrt_end > pReg[H_TOTAL_INDEX].value)
		hrt_end = pReg[H_TOTAL_INDEX].value;

/* set the hz blanking */
	hblnk_start = pReg[H_DISPLAY_INDEX].value + 1;
/* make sure it's legal */
	if (hblnk_start > hrt_start)
		hblnk_start = pReg[H_RETRACE_START_INDEX].value;

	pReg[H_BLANKING_START_INDEX].value = hblnk_start;
/* the horizontal blanking end */
	hblnk_end = hrt_end + 2;
/* make sure it's legal */
	if( hblnk_end > pReg[H_TOTAL_INDEX].value)
		hblnk_end = pReg[H_TOTAL_INDEX].value;
/* write horizontal blanking - include 7th test bit (always 1) */
	pReg[H_BLANKING_END_INDEX].value = (hblnk_end & 0x1f) | 0x80;
/* include the 5th bit of the horizontal blanking in the horizontal retrace reg. */
	hrt_end = ((hrt_end & 0x1f) | ((hblnk_end & 0x20) << 2));
	pReg[H_RETRACE_END_INDEX].value = hrt_end;

/* get the vt retrace */
	vrt_start = pReg[V_RETRACE_START_INDEX].value | ((pReg[OVERFLOW_INDEX].value & 0x04) << 6) |
				((pReg[OVERFLOW_INDEX].value & 0x80) << 2);

/* set the new retrace start */
	vrt_start += center_y;
/* check it's legal, get the display line count */
	vert_display = (pReg[V_END_INDEX].value | ((pReg[OVERFLOW_INDEX].value & 0x02) << 7) |
				((pReg[OVERFLOW_INDEX].value & 0x40) << 3)) + 1;

	if (vrt_start < vert_display)
		vrt_start = vert_display;

/* and get the vertical line count */
	vert_total = pReg[V_TOTAL_INDEX].value | ((pReg[OVERFLOW_INDEX].value & 0x01) << 8) |
				((pReg[OVERFLOW_INDEX].value & 0x20) << 4);

	pReg[V_RETRACE_START_INDEX].value = (vrt_start & 0xff);
	pReg[OVERFLOW_INDEX].value &= ~0x84;
	pReg[OVERFLOW_INDEX].value |= ((vrt_start & 0x100) >> 6);
	pReg[OVERFLOW_INDEX].value |= ((vrt_start & 0x200) >> 2);

	vrt_end = vrt_start + vrt;
	if (vrt_end > vert_total)
		vrt_end = vert_total;

/* write retrace end, include CRT protection and IRQ2 bits */
	pReg[V_RETRACE_END_INDEX].value = (vrt_end  & 0x0f) | 0x80 | 0x20;

/* get the start of vt blanking */
	vblnk_start = vert_display + 1;
/* check it's legal */
	if (vblnk_start > vrt_start)
		vblnk_start = vrt_start;
/* and the end */
	vblnk_end = vrt_end + 2;
/* check it's legal */
	if (vblnk_end > vert_total)
		vblnk_end = vert_total;
/* set vblank start */
	pReg[V_BLANKING_START_INDEX].value = (vblnk_start & 0xff);
/* write out any overflows */
	pReg[OVERFLOW_INDEX].value &= ~0x08;
	pReg[OVERFLOW_INDEX].value |= ((vblnk_start & 0x100) >> 5);
	pReg[MAXIMUM_SCANLINE_INDEX].value &= ~0x20;
	pReg[MAXIMUM_SCANLINE_INDEX].value |= ((vblnk_start &0x200) >> 4);
/* set the vblank end */
	pReg[V_BLANKING_END_INDEX].value = (vblnk_end & 0xff);
}
