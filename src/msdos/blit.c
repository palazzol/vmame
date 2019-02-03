#include "driver.h"
#include "mamalleg.h"
#include "blit.h"

/* from video.c */
extern int mmxlfb;

static int display_pos;
static int display_pages;
static int display_page_size;

static int blit_laceline;
static int blit_lacecolumn;

static int blit_screenwidth;
static int blit_sbpp;
static int blit_dbpp;
static int blit_xmultiply;
static int blit_ymultiply;
static int blit_sxdivide;
static int blit_sydivide;
static int blit_dxdivide;
static int blit_dydivide;
static int blit_hscanlines;
static int blit_vscanlines;
static int blit_flipx;
static int blit_flipy;
static int blit_swapxy;
static unsigned long blit_cmultiply = 0x00000001;

static unsigned char line_buf[ 65536 ];

UINT32 blit_lookup_low[ 65536 ];
UINT32 blit_lookup_high[ 65536 ];

static unsigned char *copyline_raw( unsigned char *p_src, unsigned int n_width, int n_pixelmodulo )
{
	return p_src;
}

static unsigned char *copyline_invalid( unsigned char *p_src, unsigned int n_width, int n_pixelmodulo )
{
	logerror( "copyline_invalid\n" );
	return p_src;
}

static unsigned char *( *blit_copyline )( unsigned char *p_src, unsigned int n_width, int n_pixelmodulo ) = copyline_invalid;

#define COPYLINE( NAME ) \
static unsigned char *NAME( unsigned char *p_src, unsigned int n_width, int n_pixelmodulo ) \
{ \
	unsigned int n_pos; \
	unsigned char *p_dst; \
\
	p_dst = line_buf; \
\
	n_pos = n_width; \
	while( n_pos > 0 ) \
	{ \

#define COPYLINE_END \
		n_pos--; \
	} \
	return line_buf; \
} \

/* 8bpp */

COPYLINE( copyline_1x_8bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x000000ff ) | ( n_p2 & 0x0000ff00 ) | ( n_p3 & 0x00ff0000 ) | ( n_p4 & 0xff000000 ); p_dst += 4;
COPYLINE_END

#define copyline_1xs_8bpp_palettized_8bpp copyline_1x_8bpp_palettized_8bpp

COPYLINE( copyline_2x_8bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ) | ( n_p2 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p3 & 0x0000ffff ) | ( n_p4 & 0xffff0000 ); p_dst += 4;
COPYLINE_END

#define copyline_2xs_8bpp_palettized_8bpp copyline_2x_8bpp_palettized_8bpp

COPYLINE( copyline_3x_8bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x00ffffff ) | ( n_p2 & 0xff000000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 & 0x0000ffff ) | ( n_p3 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p3 & 0x000000ff ) | ( n_p4 & 0xffffff00 ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3xs_8bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ) | ( n_p2 & 0xff000000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 & 0x000000ff ) | ( n_p3 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p4 & 0x00ffff00 ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4x_8bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

#define copyline_4xs_8bpp_palettized_8bpp copyline_4x_8bpp_palettized_8bpp

#define copyline_1x_8bpp_direct_8bpp copyline_invalid
#define copyline_1xs_8bpp_direct_8bpp copyline_invalid
#define copyline_2x_8bpp_direct_8bpp copyline_invalid
#define copyline_2xs_8bpp_direct_8bpp copyline_invalid
#define copyline_3x_8bpp_direct_8bpp copyline_invalid
#define copyline_3xs_8bpp_direct_8bpp copyline_invalid
#define copyline_4x_8bpp_direct_8bpp copyline_invalid
#define copyline_4xs_8bpp_direct_8bpp copyline_invalid

COPYLINE( copyline_1x_16bpp_palettized_8bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT16 *)p_dst ) = ( n_p1 & 0x00ff ) | ( n_p2 & 0xff00 ); p_dst += 2;
COPYLINE_END

#define copyline_1xs_16bpp_palettized_8bpp copyline_1x_16bpp_palettized_8bpp

COPYLINE( copyline_2x_16bpp_palettized_8bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT16 *)p_dst ) = n_p1; p_dst += 2;
	*( (UINT16 *)p_dst ) = n_p2; p_dst += 2;
COPYLINE_END

#define copyline_2xs_16bpp_palettized_8bpp   copyline_2x_16bpp_palettized_8bpp

COPYLINE( copyline_3x_16bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x00ffffff ) | ( n_p2 & 0xff000000 ); p_dst += 4;
	*( (UINT16 *)p_dst ) = ( n_p2 & 0x0000ffff ); p_dst += 2;
COPYLINE_END

COPYLINE( copyline_3xs_16bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ) | ( n_p2 & 0xff000000 ); p_dst += 4;
	*( (UINT16 *)p_dst ) = ( n_p2 & 0x000000ff ); p_dst += 2;
COPYLINE_END

COPYLINE( copyline_4x_16bpp_palettized_8bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

#define copyline_4xs_16bpp_palettized_8bpp copyline_4x_16bpp_palettized_8bpp

#define copyline_1x_16bpp_direct_8bpp copyline_1x_16bpp_palettized_8bpp
#define copyline_1xs_16bpp_direct_8bpp copyline_1xs_16bpp_palettized_8bpp
#define copyline_2x_16bpp_direct_8bpp copyline_2x_16bpp_palettized_8bpp
#define copyline_2xs_16bpp_direct_8bpp copyline_2xs_16bpp_palettized_8bpp
#define copyline_3x_16bpp_direct_8bpp copyline_3x_16bpp_palettized_8bpp
#define copyline_3xs_16bpp_direct_8bpp copyline_3xs_16bpp_palettized_8bpp
#define copyline_4x_16bpp_direct_8bpp copyline_4x_16bpp_palettized_8bpp
#define copyline_4xs_16bpp_direct_8bpp copyline_4xs_16bpp_palettized_8bpp

#define copyline_1x_32bpp_palettized_8bpp copyline_invalid
#define copyline_1xs_32bpp_palettized_8bpp copyline_invalid
#define copyline_2x_32bpp_palettized_8bpp copyline_invalid
#define copyline_2xs_32bpp_palettized_8bpp copyline_invalid
#define copyline_3x_32bpp_palettized_8bpp copyline_invalid
#define copyline_3xs_32bpp_palettized_8bpp copyline_invalid
#define copyline_4x_32bpp_palettized_8bpp copyline_invalid
#define copyline_4xs_32bpp_palettized_8bpp copyline_invalid

COPYLINE( copyline_1x_32bpp_direct_8bpp )
	UINT8 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT8 *)p_dst ) = n_p1; p_dst += 1;
COPYLINE_END

#define copyline_1xs_32bpp_direct_8bpp copyline_1x_32bpp_direct_8bpp

COPYLINE( copyline_2x_32bpp_direct_8bpp )
	UINT16 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT16 *)p_dst ) = n_p1; p_dst += 2;
COPYLINE_END

#define copyline_2xs_32bpp_direct_8bpp copyline_2x_32bpp_direct_8bpp

COPYLINE( copyline_3x_32bpp_direct_8bpp )
	UINT16 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT16 *)p_dst ) = n_p1; p_dst += 2;
	*( (UINT8 *)p_dst ) = n_p1; p_dst += 1;
COPYLINE_END

COPYLINE( copyline_3xs_32bpp_direct_8bpp )
	UINT16 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT16 *)p_dst ) = n_p1; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_4x_32bpp_direct_8bpp )
	UINT32 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
COPYLINE_END

#define copyline_4xs_32bpp_direct_8bpp copyline_4x_32bpp_direct_8bpp

/* 16bpp */

COPYLINE( copyline_1x_8bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ) | ( n_p2 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p3 & 0x0000ffff ) | ( n_p4 & 0xffff0000 ); p_dst += 4;
COPYLINE_END

#define copyline_1xs_8bpp_palettized_16bpp   copyline_1x_8bpp_palettized_16bpp

COPYLINE( copyline_2x_8bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

#define copyline_2xs_8bpp_palettized_16bpp copyline_2x_8bpp_palettized_16bpp

COPYLINE( copyline_3x_8bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ) | ( n_p2 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p3 & 0x0000ffff ) | ( n_p4 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3xs_8bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 & 0x0000ffff ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p4 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p4 & 0x0000ffff ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4x_8bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

#define copyline_4xs_8bpp_palettized_16bpp copyline_4x_8bpp_palettized_16bpp

#define copyline_1x_8bpp_direct_16bpp copyline_invalid
#define copyline_1xs_8bpp_direct_16bpp copyline_invalid
#define copyline_2x_8bpp_direct_16bpp copyline_invalid
#define copyline_2xs_8bpp_direct_16bpp copyline_invalid
#define copyline_3x_8bpp_direct_16bpp copyline_invalid
#define copyline_3xs_8bpp_direct_16bpp copyline_invalid
#define copyline_4x_8bpp_direct_16bpp copyline_invalid
#define copyline_4xs_8bpp_direct_16bpp copyline_invalid

COPYLINE( copyline_1x_16bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ) | ( n_p2 & 0xffff0000 ); p_dst += 4;
COPYLINE_END

#define copyline_1xs_16bpp_palettized_16bpp copyline_1x_16bpp_palettized_16bpp

COPYLINE( copyline_2x_16bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

#define copyline_2xs_16bpp_palettized_16bpp copyline_2x_16bpp_palettized_16bpp

COPYLINE( copyline_3x_16bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ) | ( n_p2 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3xs_16bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 & 0xffff0000 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 & 0x0000ffff ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4x_16bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4xs_16bpp_palettized_16bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 & 0x0000ffff ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_1x_16bpp_direct_16bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;
	n_p2 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p2 << 16 ) ); p_dst += 4;
COPYLINE_END

#define copyline_1xs_16bpp_direct_16bpp copyline_1x_16bpp_direct_16bpp

COPYLINE( copyline_2x_16bpp_direct_16bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;
	n_p2 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p1 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 | ( n_p2 << 16 ) ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_2xs_16bpp_direct_16bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;
	n_p2 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3x_16bpp_direct_16bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;
	n_p2 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p1 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p2 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 | ( n_p2 << 16 ) ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3xs_16bpp_direct_16bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;
	n_p2 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p1 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 << 16 ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4x_16bpp_direct_16bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;
	n_p2 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p1 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p1 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 | ( n_p2 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 | ( n_p2 << 16 ) ); p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4xs_16bpp_direct_16bpp )
	UINT16 n_p1;
	UINT16 n_p2;

	n_p1 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;
	n_p2 = *( (UINT16 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = ( n_p1 | ( n_p1 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p2 | ( n_p2 << 16 ) ); p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

#define copyline_1x_32bpp_palettized_16bpp copyline_invalid
#define copyline_1xs_32bpp_palettized_16bpp copyline_invalid
#define copyline_2x_32bpp_palettized_16bpp copyline_invalid
#define copyline_2xs_32bpp_palettized_16bpp copyline_invalid
#define copyline_3x_32bpp_palettized_16bpp copyline_invalid
#define copyline_3xs_32bpp_palettized_16bpp copyline_invalid
#define copyline_4x_32bpp_palettized_16bpp copyline_invalid
#define copyline_4xs_32bpp_palettized_16bpp copyline_invalid

COPYLINE( copyline_1x_32bpp_direct_16bpp )
	UINT32 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT16 *)p_dst ) = n_p1; p_dst += 2;
COPYLINE_END

#define copyline_1xs_32bpp_direct_16bpp copyline_1x_32bpp_direct_16bpp

COPYLINE( copyline_2x_32bpp_direct_16bpp )
	UINT32 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
COPYLINE_END

#define copyline_2xs_32bpp_direct_16bpp copyline_2x_32bpp_direct_16bpp

COPYLINE( copyline_3x_32bpp_direct_16bpp )
	UINT32 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT16 *)p_dst ) = n_p1; p_dst += 2;
COPYLINE_END

COPYLINE( copyline_3xs_32bpp_direct_16bpp )
	UINT32 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
COPYLINE_END

COPYLINE( copyline_4x_32bpp_direct_16bpp )
	UINT32 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4xs_32bpp_direct_16bpp )
	UINT32 n_p1;

	n_p1 = blit_lookup_high[ *( (UINT32 *)p_src ) >> 16 ] | blit_lookup_low[ *( (UINT32 *)p_src ) & 0xffff ];
	p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = ( n_p1 & 0x0000ffff ); p_dst += 4;
COPYLINE_END

/* 24bpp */

COPYLINE( copyline_1x_8bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
COPYLINE_END

#define copyline_1xs_8bpp_palettized_24bpp copyline_1x_8bpp_palettized_24bpp

COPYLINE( copyline_2x_8bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_2xs_8bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 6;
COPYLINE_END

COPYLINE( copyline_3x_8bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_3xs_8bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 6;
COPYLINE_END

COPYLINE( copyline_4x_8bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_4xs_8bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 6;
COPYLINE_END

#define copyline_1x_8bpp_direct_24bpp copyline_invalid
#define copyline_1xs_8bpp_direct_24bpp copyline_invalid
#define copyline_2x_8bpp_direct_24bpp copyline_invalid
#define copyline_2xs_8bpp_direct_24bpp copyline_invalid
#define copyline_3x_8bpp_direct_24bpp copyline_invalid
#define copyline_3xs_8bpp_direct_24bpp copyline_invalid
#define copyline_4x_8bpp_direct_24bpp copyline_invalid
#define copyline_4xs_8bpp_direct_24bpp copyline_invalid

COPYLINE( copyline_1x_16bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
COPYLINE_END

#define copyline_1xs_16bpp_palettized_24bpp copyline_1x_16bpp_palettized_24bpp

COPYLINE( copyline_2x_16bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_2xs_16bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 6;
COPYLINE_END

COPYLINE( copyline_3x_16bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_3xs_16bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 6;
COPYLINE_END

COPYLINE( copyline_4x_16bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_4xs_16bpp_palettized_24bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 6;
COPYLINE_END

#define copyline_1x_16bpp_direct_24bpp copyline_1x_16bpp_palettized_24bpp
#define copyline_1xs_16bpp_direct_24bpp copyline_1xs_16bpp_palettized_24bpp
#define copyline_2x_16bpp_direct_24bpp copyline_2x_16bpp_palettized_24bpp
#define copyline_2xs_16bpp_direct_24bpp copyline_2xs_16bpp_palettized_24bpp
#define copyline_3x_16bpp_direct_24bpp copyline_3x_16bpp_palettized_24bpp
#define copyline_3xs_16bpp_direct_24bpp copyline_3xs_16bpp_palettized_24bpp
#define copyline_4x_16bpp_direct_24bpp copyline_4x_16bpp_palettized_24bpp
#define copyline_4xs_16bpp_direct_24bpp copyline_4xs_16bpp_palettized_24bpp

#define copyline_1x_32bpp_palettized_24bpp copyline_invalid
#define copyline_1xs_32bpp_palettized_24bpp copyline_invalid
#define copyline_2x_32bpp_palettized_24bpp copyline_invalid
#define copyline_2xs_32bpp_palettized_24bpp copyline_invalid
#define copyline_3x_32bpp_palettized_24bpp copyline_invalid
#define copyline_3xs_32bpp_palettized_24bpp copyline_invalid
#define copyline_4x_32bpp_palettized_24bpp copyline_invalid
#define copyline_4xs_32bpp_palettized_24bpp copyline_invalid

COPYLINE( copyline_1x_32bpp_direct_24bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
COPYLINE_END

#define copyline_1xs_32bpp_direct_24bpp copyline_1x_32bpp_direct_24bpp

COPYLINE( copyline_2x_32bpp_direct_24bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_2xs_32bpp_direct_24bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
COPYLINE_END

COPYLINE( copyline_3x_32bpp_direct_24bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_3xs_32bpp_direct_24bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
COPYLINE_END

COPYLINE( copyline_4x_32bpp_direct_24bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
COPYLINE_END

COPYLINE( copyline_4xs_32bpp_direct_24bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 3;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 6;
COPYLINE_END

/* 32bpp */

COPYLINE( copyline_1x_8bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

#define copyline_1xs_8bpp_palettized_32bpp copyline_1x_8bpp_palettized_32bpp

COPYLINE( copyline_2x_8bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_2xs_8bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 8;
COPYLINE_END

COPYLINE( copyline_3x_8bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3xs_8bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 8;
COPYLINE_END

COPYLINE( copyline_4x_8bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4xs_8bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;
	UINT32 n_p3;
	UINT32 n_p4;

	n_p1 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p3 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;
	n_p4 = blit_lookup_low[ *( p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p3; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p4; p_dst += 8;
COPYLINE_END

#define copyline_1x_8bpp_direct_32bpp copyline_invalid
#define copyline_1xs_8bpp_direct_32bpp copyline_invalid
#define copyline_2x_8bpp_direct_32bpp copyline_invalid
#define copyline_2xs_8bpp_direct_32bpp copyline_invalid
#define copyline_3x_8bpp_direct_32bpp copyline_invalid
#define copyline_3xs_8bpp_direct_32bpp copyline_invalid
#define copyline_4x_8bpp_direct_32bpp copyline_invalid
#define copyline_4xs_8bpp_direct_32bpp copyline_invalid

COPYLINE( copyline_1x_16bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

#define copyline_1xs_16bpp_palettized_32bpp copyline_1x_16bpp_palettized_32bpp

COPYLINE( copyline_2x_16bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_2xs_16bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 8;
COPYLINE_END

COPYLINE( copyline_3x_16bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3xs_16bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 8;
COPYLINE_END

COPYLINE( copyline_4x_16bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4xs_16bpp_palettized_32bpp )
	UINT32 n_p1;
	UINT32 n_p2;

	n_p1 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;
	n_p2 = blit_lookup_low[ *( (UINT16 *)p_src ) ]; p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p2; p_dst += 8;
COPYLINE_END

#define copyline_1x_16bpp_direct_32bpp copyline_1x_16bpp_palettized_32bpp
#define copyline_1xs_16bpp_direct_32bpp copyline_1xs_16bpp_palettized_32bpp
#define copyline_2x_16bpp_direct_32bpp copyline_2x_16bpp_palettized_32bpp
#define copyline_2xs_16bpp_direct_32bpp copyline_2xs_16bpp_palettized_32bpp
#define copyline_3x_16bpp_direct_32bpp copyline_3x_16bpp_palettized_32bpp
#define copyline_3xs_16bpp_direct_32bpp copyline_3xs_16bpp_palettized_32bpp
#define copyline_4x_16bpp_direct_32bpp copyline_4x_16bpp_palettized_32bpp
#define copyline_4xs_16bpp_direct_32bpp copyline_4xs_16bpp_palettized_32bpp

#define copyline_1x_32bpp_palettized_32bpp copyline_invalid
#define copyline_1xs_32bpp_palettized_32bpp copyline_invalid
#define copyline_2x_32bpp_palettized_32bpp copyline_invalid
#define copyline_2xs_32bpp_palettized_32bpp copyline_invalid
#define copyline_3x_32bpp_palettized_32bpp copyline_invalid
#define copyline_3xs_32bpp_palettized_32bpp copyline_invalid
#define copyline_4x_32bpp_palettized_32bpp copyline_invalid
#define copyline_4xs_32bpp_palettized_32bpp copyline_invalid

COPYLINE( copyline_1x_32bpp_direct_32bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
COPYLINE_END

#define copyline_1xs_32bpp_direct_32bpp copyline_1x_32bpp_direct_32bpp

COPYLINE( copyline_2x_32bpp_direct_32bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_2xs_32bpp_direct_32bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
COPYLINE_END

COPYLINE( copyline_3x_32bpp_direct_32bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_3xs_32bpp_direct_32bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
COPYLINE_END

COPYLINE( copyline_4x_32bpp_direct_32bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
COPYLINE_END

COPYLINE( copyline_4xs_32bpp_direct_32bpp )
	UINT32 n_p1;

	n_p1 = *( (UINT32 *)p_src ); p_src += n_pixelmodulo;

	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 4;
	*( (UINT32 *)p_dst ) = n_p1; p_dst += 8;
COPYLINE_END


#define COPYLINE_VSL( TYPE, SBPP, DBPP, XMULTIPLY, VSL ) copyline_##XMULTIPLY##x##VSL##_##SBPP##bpp_##TYPE##_##DBPP##bpp

#define COPYLINE_XMULTIPLY( TYPE, SBPP, DBPP, XMULTIPLY ) \
{ \
	COPYLINE_VSL( TYPE, SBPP, DBPP, XMULTIPLY, ), \
	COPYLINE_VSL( TYPE, SBPP, DBPP, XMULTIPLY, s ) \
} \

#define COPYLINE_DBPP( TYPE, SBPP, DBPP ) \
{ \
	COPYLINE_XMULTIPLY( TYPE, SBPP, DBPP, 1 ), \
	COPYLINE_XMULTIPLY( TYPE, SBPP, DBPP, 2 ), \
	COPYLINE_XMULTIPLY( TYPE, SBPP, DBPP, 3 ), \
	COPYLINE_XMULTIPLY( TYPE, SBPP, DBPP, 4 ) \
} \

#define COPYLINE_SBPP( TYPE, SBPP ) \
{ \
	COPYLINE_DBPP( TYPE, SBPP, 8 ), \
	COPYLINE_DBPP( TYPE, SBPP, 16 ), \
	COPYLINE_DBPP( TYPE, SBPP, 24 ), \
	COPYLINE_DBPP( TYPE, SBPP, 32 ) \
} \

#define COPYLINE_TYPE( TYPE ) static unsigned char *( *blit_copyline_##TYPE[ 3 ][ 4 ][ 4 ][ 2 ] )( unsigned char *p_src, unsigned int n_width, int n_pixelmodulo ) = \
{ \
	COPYLINE_SBPP( TYPE, 8 ), \
	COPYLINE_SBPP( TYPE, 16 ), \
	COPYLINE_SBPP( TYPE, 32 ) \
}; \

COPYLINE_TYPE( direct )
COPYLINE_TYPE( palettized )


#define BITBLIT( type ) void bitblit_##type( mame_bitmap *bitmap, int sx, int sy, int sw, int sh, int dx, int dy ) \
{ \
	int n_line; \
	int n_lines; \
	int n_columns; \
	int n_xoffset; \
	int n_yoffset; \
	int n_ymultiply; \
	int n_srcwidth; \
	int n_dstwidth; \
	int n_dstoffset; \
	int n_lineoffset; \
	int n_pixeloffset; \
	unsigned char *p_src; \
	unsigned long n_dstaddress; \
\
	/* Align on a quadword */ \
	n_xoffset = ( dx / blit_dxdivide ); \
	n_yoffset = ( dy / blit_dydivide ); \
	n_columns = sw; \
	n_lines = sh; \
	p_src = ( (UINT8 *)bitmap->line[ sy ] ) + ( blit_sbpp / 8 ) * sx; \
	if( blit_swapxy ) \
	{ \
		n_lineoffset = ( blit_sbpp / 8 ); \
		n_pixeloffset = ( ( (UINT8 *)bitmap->line[ 1 ] ) - ( (UINT8 *)bitmap->line[ 0 ] ) ); \
	} \
	else \
	{ \
		n_lineoffset = ( ( (UINT8 *)bitmap->line[ 1 ] ) - ( (UINT8 *)bitmap->line[ 0 ] ) ); \
		n_pixeloffset = ( blit_sbpp / 8 ); \
	} \
	if( blit_flipx ) \
	{ \
		n_pixeloffset *= -1; \
	} \
	if( blit_flipy ) \
	{ \
		n_lineoffset *= -1; \
	} \
\
	p_src += ( blit_lacecolumn * n_pixeloffset ) + ( blit_laceline * n_lineoffset ); \
	blit_lacecolumn++; \
	blit_lacecolumn %= blit_sxdivide; \
	blit_laceline++; \
	blit_laceline %= blit_sydivide; \
\
	n_lineoffset *= blit_sydivide; \
	n_pixeloffset *= blit_sxdivide; \
	n_srcwidth = ( ( ( ( n_columns + blit_sxdivide - 1 ) / blit_sxdivide ) * ( blit_sbpp / 8 ) ) + 3 ) / 4; \
	n_line = ( n_lines + blit_sydivide - 1 ) / blit_sydivide; \
	n_dstwidth = ( ( ( ( n_columns + blit_sxdivide - 1 ) / blit_sxdivide ) * ( blit_dbpp / 8 ) * blit_xmultiply ) + 3 ) / 4; \
	n_dstoffset = blit_screenwidth; \

#define BLITSCREEN_END }


/* vga */

BITBLIT( vga )
{
	n_dstaddress = 0xa0000 + ( n_yoffset * n_dstoffset ) + n_xoffset;

	while( n_line != 0 )
	{
		n_ymultiply = blit_ymultiply - blit_hscanlines;
		while( n_ymultiply != 0 )
		{
			_dosmemputl( blit_copyline( p_src, n_srcwidth, n_pixeloffset ), n_dstwidth, n_dstaddress );
			n_dstaddress += n_dstoffset;
			n_ymultiply--;
		}
		if( blit_hscanlines )
		{
			n_dstaddress += n_dstoffset;
		}
		p_src += n_lineoffset;
		n_line--;
	}
}
BLITSCREEN_END


/* unchained vga */

#define UNCHAINEDMEMPUT_START \
	__asm__ __volatile__ ( \
/* save es and set it to our video selector */ \
	"pushw  %%es \n" \
	"pushl  %%ebx \n" \
	"movw   %%dx,%%es \n" \
	"movw   $0x102,%%ax \n" \
	"cld \n" \
	".align 4 \n" \
/* --bit plane loop-- */ \
	"0:\n" \
/* save everything */ \
	"pushl  %%ebx \n" \
	"pushl  %%edi \n" \
	"pushl  %%eax \n" \
	"pushl  %%ecx \n" \
/* set the bit plane */ \
	"movw   $0x3c4,%%dx \n" \
	"outw   %%ax,%%dx \n" \
/* --width loop-- */ \
	"1:\n" \
/* get the 4 bytes */ \
/* bswap should be faster than shift, */ \
/* movl should be faster then movb */ \
	"movl   %%ds:12(%%ebx),%%eax \n" \
	"movb   %%ds:8(%%ebx),%%ah \n" \
	"bswap  %%eax \n" \
	"movb   %%ds:(%%ebx),%%al \n" \
	"movb   %%ds:4(%%ebx),%%ah \n" \
/* write the thing to video */ \
	"stosl \n" \
/* move onto next source address */ \
	"addl   $16,%%ebx \n" \
	"loop	1b \n"

#define UNCHAINEDMEMPUT_END \
/* --end of width loop-- */ \
/* get everything back */ \
	"popl   %%ecx \n" \
	"popl   %%eax \n" \
	"popl   %%edi \n" \
	"popl   %%ebx \n" \
/* move onto next bit plane */ \
	"incl   %%ebx \n" \
	"shlb   $1,%%ah \n" \
/* check if we've done all 4 or not */ \
	"testb  $0x10,%%ah \n" \
	"jz		0b \n" \
/* --end of bit plane loop-- */ \
/* restore es */ \
	"popl   %%ebx \n" \
	"popw   %%es \n" \
/* outputs */ \
	: \
/* inputs */ \
/* ebx = src, ecx = width */ \
/* edx = seg,  edi = address */ \
	: \
	"b" (p_src), \
	"c" (n_width), \
	"d" (n_seg), \
	"D" (n_address) \
/* registers modified */ \
	: \
	"ax", \
	"cc", \
	"memory" \
	);

/*asm routine for unchained blit - writes 4 bytes at a time */
static void unchainedmemput_dword( unsigned char *p_src, short n_seg, unsigned long n_address, int n_width )
{
/* straight forward double word blit */
	UNCHAINEDMEMPUT_START
	UNCHAINEDMEMPUT_END
}

/*asm routine for unchained blit - writes 4 bytes at a time, then 3 'odd' bytes at the end of each row */
static void unchainedmemput_triple( unsigned char *p_src, short n_seg, unsigned long n_address, int n_width )
{
	UNCHAINEDMEMPUT_START
/* get the extra 2 bytes at end of the row */
	"movl   %%ds:(%%ebx),%%eax \n"
	"movb   %%ds:4(%%ebx),%%ah \n"
/* write the thing to video */
	"stosw \n"
	"movb   %%ds:8(%%ebx),%%al \n"
	"stosb \n"
	UNCHAINEDMEMPUT_END
}

/*asm routine for unchained blit - writes 4 bytes at a time, then 2 'odd' bytes at the end of each row */
static void unchainedmemput_word( unsigned char *p_src, short n_seg, unsigned long n_address, int n_width )
{
	UNCHAINEDMEMPUT_START
/* get the extra 2 bytes at end of the row */
	"movl   %%ds:(%%ebx),%%eax \n"
	"movb   %%ds:4(%%ebx),%%ah \n"
/* write the thing to video */
	"stosw \n"
	UNCHAINEDMEMPUT_END
}

/*asm routine for unchained blit - writes 4 bytes at a time, then 1 'odd' byte at the end of each row */
static void unchainedmemput_byte( unsigned char *p_src, short n_seg, unsigned long n_address, int n_width )
{
	UNCHAINEDMEMPUT_START
/* get the extra byte at end of the row */
	"movb   %%ds:(%%ebx),%%al \n"
/* write the thing to video */
	"stosb \n"
	UNCHAINEDMEMPUT_END
}

/* for flipping between the unchained VGA pages */
INLINE void unchained_pageflip( void )
{
	int n_offsethigh;
	int n_offsetlow;

	n_offsethigh = ( ( display_pos & 0xff00 ) | 0x0c );
	n_offsetlow = ( ( ( display_pos << 8 ) & 0xff00 ) | 0x0d );

	display_pos = ( display_pos + display_page_size ) % ( display_pages * display_page_size );
	
/* need to change the offset address during the active display interval */
/* as the value is read during the vertical retrace */
	__asm__ __volatile__
	(
		"movw   $0x3da,%%dx \n"
		"cli \n"
		".align 4                \n"
/* check for active display interval */
		"0:\n"
		"inb    %%dx,%%al \n"
		"testb  $1,%%al \n"
		"jz  	0b \n"
/* change the offset address */
		"movw   $0x3d4,%%dx \n"
		"movw   %%cx,%%ax \n"
		"outw   %%ax,%%dx \n"
		"movw   %%bx,%%ax \n"
		"outw   %%ax,%%dx \n"
		"sti \n"

/* outputs  (none)*/
		:
/* inputs - ebx = n_offsetlow, ecx = n_offsethigh */
		:
		"b" (n_offsetlow),
		"c" (n_offsethigh)
/* registers modified */
		:
		"ax",
		"dx",
		"cc",
		"memory"
	);
}

BITBLIT( unchained_vga )
{
	void (*unchainedmemput)( unsigned char *, short, unsigned long, int );

	switch( n_dstwidth & 3 )
	{
	case 1:
		unchainedmemput = unchainedmemput_byte;
		break;
	case 2:
		unchainedmemput = unchainedmemput_word;
		break;
	case 3:
		unchainedmemput = unchainedmemput_triple;
		break;
	default:
		unchainedmemput = unchainedmemput_dword;
		break;
	}

	n_dstaddress = 0xa0000 + display_pos + ( ( ( n_yoffset * n_dstoffset ) + n_xoffset ) >> 2 );
	n_dstwidth >>= 2;
	n_dstoffset >>= 2;

	while( n_line != 0 )
	{
		n_ymultiply = blit_ymultiply - blit_hscanlines;
		while( n_ymultiply != 0 )
		{
			unchainedmemput( blit_copyline( p_src, n_srcwidth, n_pixeloffset ), screen->seg, n_dstaddress, n_dstwidth );
			n_dstaddress += n_dstoffset;
			n_ymultiply--;
		}
		if( blit_hscanlines )
		{
			n_dstaddress += n_dstoffset;
		}
		p_src += n_lineoffset;
		n_line--;
	}
	unchained_pageflip();
}
BLITSCREEN_END


/* vesa */

static void (*blit_asmblit)( int width, int lines, unsigned char *src, int line_offs, unsigned long address, unsigned long address_offset ) = NULL;

#define ASMBLIT_PROTO_SL( BPP, PALETTIZED, MX, MY, HSL ) extern void asmblit_##MX##x_##MY##y_##HSL##sl_##BPP##bpp##PALETTIZED(int, int, unsigned char *, int, unsigned long, unsigned long); \

#define ASMBLIT_PROTO_MY( BPP, PALETTIZED, MX, MY ) \
ASMBLIT_PROTO_SL( BPP, PALETTIZED, MX, MY, 0 ) \
ASMBLIT_PROTO_SL( BPP, PALETTIZED, MX, MY, 1 ) \

#define ASMBLIT_PROTO_MX( BPP, PALETTIZED, MX ) \
ASMBLIT_PROTO_MY( BPP, PALETTIZED, MX, 1 ) \
ASMBLIT_PROTO_MY( BPP, PALETTIZED, MX, 2 ) \

#define ASMBLIT_PROTO_BPP( BPP, PALETTIZED ) \
ASMBLIT_PROTO_MX( BPP, PALETTIZED, 1 ) \
ASMBLIT_PROTO_MX( BPP, PALETTIZED, 2 ) \

ASMBLIT_PROTO_BPP( 8, )
ASMBLIT_PROTO_BPP( 16, )
ASMBLIT_PROTO_BPP( 16, _palettized )
ASMBLIT_PROTO_BPP( 32, )

#define ASMBLIT_SL( BPP, PALETTIZED, MX, MY, HSL ) asmblit_##MX##x_##MY##y_##HSL##sl_##BPP##bpp##PALETTIZED \

#define ASMBLIT_MY( BPP, PALETTIZED, MX, MY ) \
{ \
	ASMBLIT_SL( BPP, PALETTIZED, MX, MY, 0 ), \
	ASMBLIT_SL( BPP, PALETTIZED, MX, MY, 1 ) \
} \

#define ASMBLIT_MX( BPP, PALETTIZED, MX ) \
{ \
	ASMBLIT_MY( BPP, PALETTIZED, MX, 1 ), \
	ASMBLIT_MY( BPP, PALETTIZED, MX, 2 ) \
} \

#define ASMBLIT_BPP( BPP, PALETTIZED ) \
{ \
	ASMBLIT_MX( BPP, PALETTIZED, 1 ), \
	ASMBLIT_MX( BPP, PALETTIZED, 2 ) \
} \

static void (*asmblit[ 2 ][ 3 ][ 2 ][ 2 ][ 2 ])( int width, int lines, unsigned char *src, int line_offs, unsigned long address, unsigned long address_offset ) =
{
	{
		ASMBLIT_BPP( 8, ),
		ASMBLIT_BPP( 16, ),
		ASMBLIT_BPP( 32, )
	},
	{
		ASMBLIT_BPP( 8, ),
		ASMBLIT_BPP( 16, _palettized ),
		ASMBLIT_BPP( 32, )
	}
};

static void vesa_flippage( void )
{
	/* use a real mode interrupt call as allegro waits for vsync */
	__dpmi_regs _dpmi_reg;

	_dpmi_reg.x.ax = 0x4f07;
	_dpmi_reg.x.bx = 0;
	_dpmi_reg.x.cx = display_pos;
	_dpmi_reg.x.dx = 0;
	__dpmi_int( 0x10, &_dpmi_reg );

	display_pos = ( display_pos + display_page_size ) % ( display_pages * display_page_size );
}

BITBLIT( vesa )
{
	int n_vesaline;

	n_dstaddress = ( n_xoffset + display_pos ) * ( blit_dbpp / 8 );
	n_vesaline = n_yoffset;
	if( blit_asmblit != NULL )
	{
		unsigned long n_dstbase;

		n_dstbase = bmp_write_line( screen, n_vesaline );
		_farsetsel( screen->seg );
		blit_asmblit( n_srcwidth, n_lines, p_src, n_lineoffset, n_dstbase + n_dstaddress,
			bmp_write_line( screen, n_vesaline + 1 ) - n_dstbase );
	}
	else
	{
		unsigned char *p_copy;

		while( n_line != 0 )
		{
			p_copy = blit_copyline( p_src, n_srcwidth, n_pixeloffset );
			p_src += n_lineoffset;
			n_ymultiply = blit_ymultiply - blit_hscanlines;
			while( n_ymultiply != 0 )
			{
				_movedatal( _my_ds(), (unsigned long)p_copy, screen->seg, bmp_write_line( screen, n_vesaline ) + n_dstaddress, n_dstwidth );
				n_vesaline++;
				n_ymultiply--;
			}
			if( blit_hscanlines )
			{
				n_vesaline++;
			}
			n_line--;
		}
	}
	if( display_pages != 1 )
	{
		vesa_flippage();
	}
}
BLITSCREEN_END


static int sbpp_pos( int sbpp )
{
	if( sbpp == 8 )
	{
		return 0;
	}
	else if( sbpp == 16 )
	{
		return 1;
	}
	else
	{
		return 2;
	}
}

static int dbpp_pos( int dbpp )
{
	return ( dbpp / 8 ) - 1;
}

unsigned long blit_setup( int dw, int dh, int sbpp, int dbpp, int video_attributes, int xmultiply, int ymultiply, int xdivide, int ydivide,
	int vscanlines, int hscanlines, int flipx, int flipy, int swapxy )
{
	int i;
	int cmultiply;
	int cmask;

	if( sbpp == 15 )
	{
		blit_sbpp = 16;
	}
	else
	{
		blit_sbpp = sbpp;
	}
	if( dbpp == 15 )
	{
		blit_dbpp = 16;
	}
	else
	{
		blit_dbpp = dbpp;
	}

	blit_xmultiply = xmultiply;
	blit_ymultiply = ymultiply;
	blit_sxdivide = xdivide;
	blit_sydivide = ydivide;
	blit_dxdivide = xdivide;
	blit_dydivide = ydivide;
	blit_vscanlines = vscanlines;
	blit_hscanlines = hscanlines;
	blit_flipx = flipx;
	blit_flipy = flipy;
	blit_swapxy = swapxy;

	blit_screenwidth = ( dw / blit_dxdivide );

	/* don't multiply & divide at the same time if possible */
	if( ( blit_xmultiply % blit_sxdivide ) == 0 )
	{
		blit_xmultiply /= blit_sxdivide;
		blit_sxdivide = 1;
	}
	if( ( blit_ymultiply % blit_sydivide ) == 0 )
	{
		blit_ymultiply /= blit_sydivide;
		blit_sydivide = 1;
	}

	/* disable scanlines if not multiplying */
	if( blit_xmultiply > 1 && blit_vscanlines != 0 )
	{
		blit_vscanlines = 1;
	}
	else
	{
		blit_vscanlines  = 0;
	}
	if( blit_ymultiply > 1 && blit_hscanlines != 0 )
	{
		blit_hscanlines = 1;
	}
	else
	{
		blit_hscanlines = 0;
	}

	blit_lacecolumn %= blit_sxdivide;
	blit_laceline %= blit_sydivide;

	if( video_attributes & VIDEO_RGB_DIRECT )
	{
		if( blit_sbpp == blit_dbpp && !blit_swapxy && !blit_flipx && !blit_flipy && blit_dxdivide == 1 && blit_xmultiply <= 2 && blit_ymultiply <= 2 && mmxlfb )
		{
			blit_asmblit = asmblit[ 0 ][ sbpp_pos( blit_sbpp ) ][ blit_xmultiply - 1 ][ blit_ymultiply - 1 ][ blit_hscanlines ];
		}
		else
		{
			blit_asmblit = NULL;
		}

		if( blit_sbpp == blit_dbpp && !blit_swapxy && !blit_flipx && blit_xmultiply == 1 && blit_dxdivide == 1 )
		{
			blit_copyline = copyline_raw;
		}
		else
		{
			blit_copyline = blit_copyline_direct[ sbpp_pos( blit_sbpp )][ dbpp_pos( blit_dbpp ) ][ blit_xmultiply - 1 ][ blit_vscanlines ];
		}
	}
	else
	{
		if( blit_sbpp == blit_dbpp && !blit_swapxy && !blit_flipx && !blit_flipy && blit_dxdivide == 1 && blit_xmultiply <= 2 && blit_ymultiply <= 2 && mmxlfb )
		{
			blit_asmblit = asmblit[ 1 ][ sbpp_pos( blit_sbpp ) ][ blit_xmultiply - 1 ][ blit_ymultiply - 1 ][ blit_hscanlines ];
		}
		else
		{
			blit_asmblit = NULL;
		}

		if( blit_sbpp == blit_dbpp && !blit_swapxy && !blit_flipx && blit_xmultiply == 1 && blit_dxdivide == 1 && blit_sbpp == 8 )
		{
			blit_copyline = copyline_raw;
		}
		else
		{
			blit_copyline = blit_copyline_palettized[ sbpp_pos( blit_sbpp ) ][ dbpp_pos( blit_dbpp ) ][ blit_xmultiply - 1 ][ blit_vscanlines ];
		}
	}

	memset( line_buf, 0x00, sizeof( line_buf ) );

	if( blit_dbpp == 8 )
	{
		if( blit_xmultiply == 2 && blit_vscanlines != 0 )
		{
			cmultiply = 0x00010001;
		}
		else if( blit_xmultiply == 4 && blit_vscanlines != 0 )
		{
			cmultiply = 0x00010101;
		}
		else
		{
			cmultiply = 0x01010101;
		}
		cmask = 0xff;
	}
	else if( blit_dbpp == 16 )
	{
		if( blit_xmultiply == 2 && blit_vscanlines != 0 )
		{
			cmultiply = 0x00000001;
		}
		else
		{
			cmultiply = 0x00010001;
		}
		cmask = 0xffff;
	}
	else
	{
		cmultiply = 0x00000001;
		cmask = 0xffffffff;
	}
	if( blit_cmultiply != cmultiply )
	{
		blit_cmultiply = cmultiply;
		for( i = 0; i < 65536; i++ )
		{
			blit_lookup_high[ i ] = ( blit_lookup_high[ i ] & cmask ) * blit_cmultiply;
			blit_lookup_low[ i ] = ( blit_lookup_low[ i ] & cmask ) * blit_cmultiply;
		}
	}
	return blit_cmultiply;
}

void blit_set_buffers( int pages, int page_size )
{
	display_pos = 0;
	display_pages = pages;
	display_page_size = page_size;
}
