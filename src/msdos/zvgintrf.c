/* Frank Palazzolo - 20-July-03 */
/* 05/01/05 -	Added zvg game artwork
 *		Added zvg overlay colors
 *		Added zvg enabled mode=3 to allow menus on monitor
 *		Added zvg menu enabled to not close zvg card due to menu bug (Mark Perreira)
 */

#include <math.h>
#include "osinline.h"
#include "driver.h"
#include "osdepend.h"
#include "vidhrdw/vector.h"
#include "usrintrf.h"
#include "mame.h"

#include "zvgintrf.h"
#include "zvgFrame.h"

/* from video.c */
extern int video_flipx;
extern int video_flipy;
extern int video_swapxy;
extern int stretch;

int zvg_enabled = 0;
int zvg_menu_enabled = 0;
int zvg_artwork_enabled = 0;
int zvg_overlay_enabled = 0;

int zvg_artwork_game = 0;
int zvg_overlay_game = 0;

static float cached_gamma = -1;
static UINT8 Tgamma[256];         /* quick gamma anti-alias table  */

/* Size of the MAME visible region */
static int xsize, ysize;
/* Center points (defined as midpoints of the MAME visible region) */
static int xcenter, ycenter;
/* ZVG sizes for this game, without considering overscan */
static int width, height;

static void recalc_gamma_table(float _gamma)
{
	int i, h;

	for (i = 0; i < 256; i++)
	{
		h = 255.0*pow(i/255.0, 1.0/_gamma);
		if( h > 255) h = 255;
		Tgamma[i]= h;
	}
	cached_gamma = _gamma;
}

static void init_scaling(void)
{
	/* We must handle our own flipping, swapping, and clipping */

	if (video_swapxy)
	{
		if (stretch)
		{
			width = Y_MAX - Y_MIN;
			height = X_MAX - X_MIN;
		}
		else
		{
			width = Y_MAX - Y_MIN;
			height = (Y_MAX - Y_MIN)*3/4;
		}
	}
	else
	{
		width = X_MAX - X_MIN;
		height = Y_MAX - Y_MIN;
	}
}

static void transform_coords(int *x, int *y)
{
	float xf, yf;

	xf = *x / 65536.0;
	yf = *y / 65536.0;

	/* [1] scale coordinates to ZVG */

	*x = (xf - xcenter)*width/xsize;
	*y = (ycenter - (yf))*height/ysize;

	/* [2] fix display orientation */

	if (video_flipx)
		*x = ~(*x);
	if (video_flipy)
		*y = ~(*y);

	if (video_swapxy)
	{
		int temp;
		temp = *x;
		*x = *y;
		*y = temp;
	}
}

static int x1clip = 0;
static int y1clip = 0;
static int x2clip = 0;
static int y2clip = 0;

int zvg_update(point *p, int num_points)
{
	int i;

	static int x1 = 0;
	static int yy1 = 0;
	int x2, y2;
	double current_gamma;
	int x1_limit, y1_limit, x2_limit, y2_limit;

	rgb_t col;

	/* Important Note: */
	/* This center position is calculated the same way that MAME calculates */
	/* the "center of the bitmap".  We are using is as our ZVG (0,0),       */
	/* although it may not be the same coordinate as the original beam      */
	/* center position on the real thing */

	xsize = (Machine->visible_area.max_x - Machine->visible_area.min_x);
	ysize = (Machine->visible_area.max_y - Machine->visible_area.min_y);

	xcenter = xsize/2;
	ycenter = ysize/2;

	/* if we are displaying a vertical game on a horizontal monitor */
	/* or vice versa, we want overscan in one dimension but not in  */
	/* the other */

	if (video_swapxy && (!stretch))
	{
		zvgFrameSetClipWin(X_MIN*3/4,Y_MIN_O,X_MAX*3/4,Y_MAX_O);
	}
	else
	{
		zvgFrameSetClipOverscan();
	}

	/* recalculate gamma_correction if necessary */

	current_gamma = vector_get_gamma();
	if (cached_gamma != current_gamma)
	{
		recalc_gamma_table(current_gamma);
	}

	for (i = 0; i < num_points; i++)
	{
		x2 = p->x;
		y2 = p->y;

		transform_coords(&x2, &y2);

		/* process the list entry */

		if (p->status == VCLIP)
		{
			x1clip = p->x;
			y1clip = p->y;
			x2clip = p->arg1;
			y2clip = p->arg2;

			transform_coords(&x1clip, &y1clip);
			transform_coords(&x2clip, &y2clip);

			zvgFrameSetClipWin(x1clip, y1clip, x2clip, y2clip);
		}
		else
		{
			if (p->intensity != 0)
			{
				if (p->callback)
					col = Tinten(Tgamma[p->intensity], p->callback());
				else
					col = Tinten(Tgamma[p->intensity], p->col);


				/* Check the vector boundaries to see both endpoints  */
				/* are outside the overscan area on the same side     */
				/* if so, move them to the overscan boundary, so      */
				/* that you can see the overscan effects in Star Wars */

				x1_limit = x1;
				y1_limit = yy1;
				x2_limit = x2;
				y2_limit = y2;


				if ((x1 < X_MIN_O) && (x2 < X_MIN_O))
				{
					x1_limit = x2_limit = X_MIN_O;
				}
				if ((x1 > X_MAX_O) && (x2 > X_MAX_O))
				{
					x1_limit = x2_limit = X_MAX_O;
				}
				if ((yy1 < Y_MIN_O) && (y2 < Y_MIN_O))
				{
					y1_limit = y2_limit = Y_MIN_O;
				}
				if ((yy1 > Y_MAX_O) && (y2 > Y_MAX_O))
				{
					y1_limit = y2_limit = Y_MAX_O;
				}

				switch (zvg_overlay_game) {
					case GAME_ARMORA:  // FIX upper right overlay not covering all letters
						if (y1_limit >= (ycenter * 0.57) && x1_limit <= -(xcenter * 0.57))
							zvgFrameSetRGB24(0x99,0x99,0x20);
						else if (y1_limit >= (ycenter * 0.57) && x1_limit >= (xcenter * 0.57))
							zvgFrameSetRGB24(0x99,0x99,0x20);
						else if (y1_limit <= -(ycenter * 0.57) && x1_limit >= (xcenter * 0.57))
							zvgFrameSetRGB24(0x99,0x99,0x20);
						else if (y1_limit <= -(ycenter * 0.57) && x1_limit <= -(xcenter * 0.57))
							zvgFrameSetRGB24(0x99,0x99,0x20);
						else
							zvgFrameSetRGB24(0x20,0x7e,0x20);
						break;
					case GAME_BARRIER:
						zvgFrameSetRGB24(0x20,0x20,0x7e);
						break;
					case GAME_TAILG:
						zvgFrameSetRGB24(0x20,0xff,0xff);
						break;
					case GAME_SOLARQ:
						if (y1_limit > (ycenter * 0.85) && y2_limit > (ycenter * 0.85))
							zvgFrameSetRGB24(0xff,0x20,0x20);
						else if (pow(x1_limit, 2) + pow(y1_limit, 2) < pow(xsize*0.03,2) && pow(x2_limit, 2) + pow(y2_limit, 2) < pow(xsize*0.03,2))
							zvgFrameSetRGB24(0xff,0xff,0x20);
						else
							zvgFrameSetRGB24(0x20,0x20,0xff);
						break;
					case GAME_STARCAS:
						if (pow(x1_limit, 2) + pow(y1_limit, 2) < pow(xsize*0.0725,2) && pow(x2_limit, 2) + pow(y2_limit, 2) < pow(xsize*0.0725,2))
							zvgFrameSetRGB24(0xff,0xff,0x20);
						else if (pow(x1_limit, 2) + pow(y1_limit, 2) < pow(xsize*0.0950,2) && pow(x2_limit, 2) + pow(y2_limit, 2) < pow(xsize*0.0950,2))
							zvgFrameSetRGB24(0xff,0x80,0x10);
						else if (pow(x1_limit, 2) + pow(y1_limit, 2) < pow(xsize*0.1225,2) && pow(x2_limit, 2) + pow(y2_limit, 2) < pow(xsize*0.1225,2))
							zvgFrameSetRGB24(0xff,0x20,0x20);
						else
							zvgFrameSetRGB24(0x00,0x3c,0xff);
						break;
					case GAME_SUNDANCE:
						zvgFrameSetRGB24(0xff,0xff,0x20);
						break;
					case GAME_ASTDELUX:
						zvgFrameSetRGB24(0x88,0xff,0xff);
						break;
					case GAME_BZONE:
						if (y1_limit > (ycenter * 0.9))
							zvgFrameSetRGB24(0xff,0x20,0x20);
                                                else
							zvgFrameSetRGB24(0x20,0xff,0x20);
						
						break;
					case GAME_REDBARON:
						zvgFrameSetRGB24(0x88,0xff,0xff);
						break;
					case GAME_OMEGRACE:
						zvgFrameSetRGB24(0xff,0xe4,0x57);
						break;
					case GAME_DEMON:
						if (y1_limit > (ycenter * 0.82) && y2_limit > (ycenter * 0.82))
							zvgFrameSetRGB24(0x7e,0x20,0x20);
						else if (y1_limit <= (ycenter * 0.65) && y2_limit >= (ycenter * 0.20) && x1_limit >= -(xcenter * 0.43) && x2_limit <= -(xcenter * 0.23) && y1_limit >= (ycenter * 0.20) && y2_limit <= (ycenter * 0.65) && x1_limit <= -(xcenter * 0.23) && x2_limit >= -(xcenter * 0.43))
							zvgFrameSetRGB24(0xff,0xff,0x20);
						else if (y1_limit >= -(ycenter * 0.45) && y1_limit <= -(ycenter * 0.17) && y2_limit >= -(ycenter * 0.45) && y2_limit <= -(ycenter * 0.17) && x1_limit >= -(xcenter * 0.26) && x1_limit <= (xcenter * 0.24) && x2_limit >= -(xcenter * 0.26) && x2_limit <= (xcenter * 0.24))
							zvgFrameSetRGB24(0xff,0xff,0x20);
						else
							zvgFrameSetRGB24(0x20,0x20,0x7e);
						break;
					default:
						zvgFrameSetRGB24(RGB_RED(col),RGB_GREEN(col),RGB_BLUE(col));
						break;
				}
				zvgFrameVector(x1_limit, y1_limit, x2_limit, y2_limit);
			}
			x1 = x2;
			yy1 = y2;
		}

		p++;
	}

	switch (zvg_artwork_game) {
		case GAME_ARMORA:
			zvgFrameSetRGB24(0x99,0x99,0x10);

			// Upper Right Quadrant
			zvgFrameVector((xcenter*0.683), (ycenter*0.000), (xcenter*0.683), (ycenter*0.086));		// Outer structure
			zvgFrameVector((xcenter*0.683), (ycenter*0.086), (xcenter*0.933), (ycenter*0.086));		
			zvgFrameVector((xcenter*0.933), (ycenter*0.086), (xcenter*0.933), (ycenter*0.494));
			zvgFrameVector((xcenter*0.933), (ycenter*0.494), (xcenter*0.623), (ycenter*0.494));
			zvgFrameVector((xcenter*0.623), (ycenter*0.494), (xcenter*0.623), (ycenter*0.575));
			zvgFrameVector((xcenter*0.623), (ycenter*0.575), (xcenter*0.560), (ycenter*0.575));
			zvgFrameVector((xcenter*0.560), (ycenter*0.575), (xcenter*0.560), (ycenter*0.657));
			zvgFrameVector((xcenter*0.560), (ycenter*0.657), (xcenter*0.498), (ycenter*0.657));
			zvgFrameVector((xcenter*0.498), (ycenter*0.657), (xcenter*0.498), (ycenter*0.905));
			zvgFrameVector((xcenter*0.498), (ycenter*0.905), (xcenter*0.125), (ycenter*0.905));
			zvgFrameVector((xcenter*0.125), (ycenter*0.905), (xcenter*0.125), (ycenter*0.575));
			zvgFrameVector((xcenter*0.125), (ycenter*0.575), (xcenter*0.000), (ycenter*0.575));

			zvgFrameVector((xcenter*0.000), (ycenter*0.159), (xcenter*0.251), (ycenter*0.159));		// Center Structure
			zvgFrameVector((xcenter*0.251), (ycenter*0.159), (xcenter*0.251), (ycenter*0.337));
			zvgFrameVector((xcenter*0.251), (ycenter*0.337), (xcenter*0.187), (ycenter*0.337));
			zvgFrameVector((xcenter*0.187), (ycenter*0.337), (xcenter*0.187), (ycenter*0.413));
			zvgFrameVector((xcenter*0.187), (ycenter*0.413), (xcenter*0.126), (ycenter*0.413));
			zvgFrameVector((xcenter*0.126), (ycenter*0.413), (xcenter*0.126), (ycenter*0.497));
			zvgFrameVector((xcenter*0.126), (ycenter*0.497), (xcenter*0.000), (ycenter*0.497));

			zvgFrameVector((xcenter*0.435), (ycenter*0.079), (xcenter*0.562), (ycenter*0.079));		// Big Structure
			zvgFrameVector((xcenter*0.562), (ycenter*0.079), (xcenter*0.562), (ycenter*0.164));
			zvgFrameVector((xcenter*0.562), (ycenter*0.164), (xcenter*0.810), (ycenter*0.164));
			zvgFrameVector((xcenter*0.810), (ycenter*0.164), (xcenter*0.810), (ycenter*0.337));
			zvgFrameVector((xcenter*0.810), (ycenter*0.337), (xcenter*0.435), (ycenter*0.337));
			zvgFrameVector((xcenter*0.435), (ycenter*0.337), (xcenter*0.435), (ycenter*0.079));

			zvgFrameVector((xcenter*0.246), (ycenter*0.492), (xcenter*0.375), (ycenter*0.492));		// Small Structure
			zvgFrameVector((xcenter*0.375), (ycenter*0.492), (xcenter*0.375), (ycenter*0.753));
			zvgFrameVector((xcenter*0.375), (ycenter*0.753), (xcenter*0.183), (ycenter*0.753));
			zvgFrameVector((xcenter*0.183), (ycenter*0.753), (xcenter*0.183), (ycenter*0.578));
			zvgFrameVector((xcenter*0.183), (ycenter*0.578), (xcenter*0.248), (ycenter*0.578));
			zvgFrameVector((xcenter*0.248), (ycenter*0.578), (xcenter*0.248), (ycenter*0.492));

			// Upper Left Quadrant
			zvgFrameVector(-(xcenter*0.683), (ycenter*0.000), -(xcenter*0.683), (ycenter*0.086));		// Outer structure
			zvgFrameVector(-(xcenter*0.683), (ycenter*0.086), -(xcenter*0.933), (ycenter*0.086));		
			zvgFrameVector(-(xcenter*0.933), (ycenter*0.086), -(xcenter*0.933), (ycenter*0.494));
			zvgFrameVector(-(xcenter*0.933), (ycenter*0.494), -(xcenter*0.623), (ycenter*0.494));
			zvgFrameVector(-(xcenter*0.623), (ycenter*0.494), -(xcenter*0.623), (ycenter*0.575));
			zvgFrameVector(-(xcenter*0.623), (ycenter*0.575), -(xcenter*0.560), (ycenter*0.575));
			zvgFrameVector(-(xcenter*0.560), (ycenter*0.575), -(xcenter*0.560), (ycenter*0.657));
			zvgFrameVector(-(xcenter*0.560), (ycenter*0.657), -(xcenter*0.498), (ycenter*0.657));
			zvgFrameVector(-(xcenter*0.498), (ycenter*0.657), -(xcenter*0.498), (ycenter*0.905));
			zvgFrameVector(-(xcenter*0.498), (ycenter*0.905), -(xcenter*0.125), (ycenter*0.905));
			zvgFrameVector(-(xcenter*0.125), (ycenter*0.905), -(xcenter*0.125), (ycenter*0.575));
			zvgFrameVector(-(xcenter*0.125), (ycenter*0.575), -(xcenter*0.000), (ycenter*0.575));

			zvgFrameVector(-(xcenter*0.000), (ycenter*0.159), -(xcenter*0.251), (ycenter*0.159));		// Center Structure
			zvgFrameVector(-(xcenter*0.251), (ycenter*0.159), -(xcenter*0.251), (ycenter*0.337));
			zvgFrameVector(-(xcenter*0.251), (ycenter*0.337), -(xcenter*0.187), (ycenter*0.337));
			zvgFrameVector(-(xcenter*0.187), (ycenter*0.337), -(xcenter*0.187), (ycenter*0.413));
			zvgFrameVector(-(xcenter*0.187), (ycenter*0.413), -(xcenter*0.126), (ycenter*0.413));
			zvgFrameVector(-(xcenter*0.126), (ycenter*0.413), -(xcenter*0.126), (ycenter*0.497));
			zvgFrameVector(-(xcenter*0.126), (ycenter*0.497), -(xcenter*0.000), (ycenter*0.497));

			zvgFrameVector(-(xcenter*0.435), (ycenter*0.079), -(xcenter*0.562), (ycenter*0.079));		// Big Structure
			zvgFrameVector(-(xcenter*0.562), (ycenter*0.079), -(xcenter*0.562), (ycenter*0.164));
			zvgFrameVector(-(xcenter*0.562), (ycenter*0.164), -(xcenter*0.810), (ycenter*0.164));
			zvgFrameVector(-(xcenter*0.810), (ycenter*0.164), -(xcenter*0.810), (ycenter*0.337));
			zvgFrameVector(-(xcenter*0.810), (ycenter*0.337), -(xcenter*0.435), (ycenter*0.337));
			zvgFrameVector(-(xcenter*0.435), (ycenter*0.337), -(xcenter*0.435), (ycenter*0.079));

			zvgFrameVector(-(xcenter*0.246), (ycenter*0.492), -(xcenter*0.375), (ycenter*0.492));		// Small Structure
			zvgFrameVector(-(xcenter*0.375), (ycenter*0.492), -(xcenter*0.375), (ycenter*0.753));
			zvgFrameVector(-(xcenter*0.375), (ycenter*0.753), -(xcenter*0.183), (ycenter*0.753));
			zvgFrameVector(-(xcenter*0.183), (ycenter*0.753), -(xcenter*0.183), (ycenter*0.578));
			zvgFrameVector(-(xcenter*0.183), (ycenter*0.578), -(xcenter*0.248), (ycenter*0.578));
			zvgFrameVector(-(xcenter*0.248), (ycenter*0.578), -(xcenter*0.248), (ycenter*0.492));

			// Lower Right Quadrant
			zvgFrameVector((xcenter*0.683), -(ycenter*0.000), (xcenter*0.683), -(ycenter*0.086));		// Outer structure
			zvgFrameVector((xcenter*0.683), -(ycenter*0.086), (xcenter*0.933), -(ycenter*0.086));		
			zvgFrameVector((xcenter*0.933), -(ycenter*0.086), (xcenter*0.933), -(ycenter*0.494));
			zvgFrameVector((xcenter*0.933), -(ycenter*0.494), (xcenter*0.623), -(ycenter*0.494));
			zvgFrameVector((xcenter*0.623), -(ycenter*0.494), (xcenter*0.623), -(ycenter*0.575));
			zvgFrameVector((xcenter*0.623), -(ycenter*0.575), (xcenter*0.560), -(ycenter*0.575));
			zvgFrameVector((xcenter*0.560), -(ycenter*0.575), (xcenter*0.560), -(ycenter*0.657));
			zvgFrameVector((xcenter*0.560), -(ycenter*0.657), (xcenter*0.498), -(ycenter*0.657));
			zvgFrameVector((xcenter*0.498), -(ycenter*0.657), (xcenter*0.498), -(ycenter*0.905));
			zvgFrameVector((xcenter*0.498), -(ycenter*0.905), (xcenter*0.125), -(ycenter*0.905));
			zvgFrameVector((xcenter*0.125), -(ycenter*0.905), (xcenter*0.125), -(ycenter*0.575));
			zvgFrameVector((xcenter*0.125), -(ycenter*0.575), (xcenter*0.000), -(ycenter*0.575));

			zvgFrameVector((xcenter*0.000), -(ycenter*0.159), (xcenter*0.251), -(ycenter*0.159));		// Center Structure
			zvgFrameVector((xcenter*0.251), -(ycenter*0.159), (xcenter*0.251), -(ycenter*0.337));
			zvgFrameVector((xcenter*0.251), -(ycenter*0.337), (xcenter*0.187), -(ycenter*0.337));
			zvgFrameVector((xcenter*0.187), -(ycenter*0.337), (xcenter*0.187), -(ycenter*0.413));
			zvgFrameVector((xcenter*0.187), -(ycenter*0.413), (xcenter*0.126), -(ycenter*0.413));
			zvgFrameVector((xcenter*0.126), -(ycenter*0.413), (xcenter*0.126), -(ycenter*0.497));
			zvgFrameVector((xcenter*0.126), -(ycenter*0.497), (xcenter*0.000), -(ycenter*0.497));

			zvgFrameVector((xcenter*0.435), -(ycenter*0.079), (xcenter*0.562), -(ycenter*0.079));		// Big Structure
			zvgFrameVector((xcenter*0.562), -(ycenter*0.079), (xcenter*0.562), -(ycenter*0.164));
			zvgFrameVector((xcenter*0.562), -(ycenter*0.164), (xcenter*0.810), -(ycenter*0.164));
			zvgFrameVector((xcenter*0.810), -(ycenter*0.164), (xcenter*0.810), -(ycenter*0.337));
			zvgFrameVector((xcenter*0.810), -(ycenter*0.337), (xcenter*0.435), -(ycenter*0.337));
			zvgFrameVector((xcenter*0.435), -(ycenter*0.337), (xcenter*0.435), -(ycenter*0.079));

			zvgFrameVector((xcenter*0.246), -(ycenter*0.492), (xcenter*0.375), -(ycenter*0.492));		// Small Structure
			zvgFrameVector((xcenter*0.375), -(ycenter*0.492), (xcenter*0.375), -(ycenter*0.753));
			zvgFrameVector((xcenter*0.375), -(ycenter*0.753), (xcenter*0.183), -(ycenter*0.753));
			zvgFrameVector((xcenter*0.183), -(ycenter*0.753), (xcenter*0.183), -(ycenter*0.578));
			zvgFrameVector((xcenter*0.183), -(ycenter*0.578), (xcenter*0.248), -(ycenter*0.578));
			zvgFrameVector((xcenter*0.248), -(ycenter*0.578), (xcenter*0.248), -(ycenter*0.492));

			// Lower Left Quadrant
			zvgFrameVector(-(xcenter*0.683), -(ycenter*0.000), -(xcenter*0.683), -(ycenter*0.086));		// Outer structure
			zvgFrameVector(-(xcenter*0.683), -(ycenter*0.086), -(xcenter*0.933), -(ycenter*0.086));		
			zvgFrameVector(-(xcenter*0.933), -(ycenter*0.086), -(xcenter*0.933), -(ycenter*0.494));
			zvgFrameVector(-(xcenter*0.933), -(ycenter*0.494), -(xcenter*0.623), -(ycenter*0.494));
			zvgFrameVector(-(xcenter*0.623), -(ycenter*0.494), -(xcenter*0.623), -(ycenter*0.575));
			zvgFrameVector(-(xcenter*0.623), -(ycenter*0.575), -(xcenter*0.560), -(ycenter*0.575));
			zvgFrameVector(-(xcenter*0.560), -(ycenter*0.575), -(xcenter*0.560), -(ycenter*0.657));
			zvgFrameVector(-(xcenter*0.560), -(ycenter*0.657), -(xcenter*0.498), -(ycenter*0.657));
			zvgFrameVector(-(xcenter*0.498), -(ycenter*0.657), -(xcenter*0.498), -(ycenter*0.905));
			zvgFrameVector(-(xcenter*0.498), -(ycenter*0.905), -(xcenter*0.125), -(ycenter*0.905));
			zvgFrameVector(-(xcenter*0.125), -(ycenter*0.905), -(xcenter*0.125), -(ycenter*0.575));
			zvgFrameVector(-(xcenter*0.125), -(ycenter*0.575), -(xcenter*0.000), -(ycenter*0.575));

			zvgFrameVector(-(xcenter*0.000), -(ycenter*0.159), -(xcenter*0.251), -(ycenter*0.159));		// Center Structure
			zvgFrameVector(-(xcenter*0.251), -(ycenter*0.159), -(xcenter*0.251), -(ycenter*0.337));
			zvgFrameVector(-(xcenter*0.251), -(ycenter*0.337), -(xcenter*0.187), -(ycenter*0.337));
			zvgFrameVector(-(xcenter*0.187), -(ycenter*0.337), -(xcenter*0.187), -(ycenter*0.413));
			zvgFrameVector(-(xcenter*0.187), -(ycenter*0.413), -(xcenter*0.126), -(ycenter*0.413));
			zvgFrameVector(-(xcenter*0.126), -(ycenter*0.413), -(xcenter*0.126), -(ycenter*0.497));
			zvgFrameVector(-(xcenter*0.126), -(ycenter*0.497), -(xcenter*0.000), -(ycenter*0.497));

			zvgFrameVector(-(xcenter*0.435), -(ycenter*0.079), -(xcenter*0.562), -(ycenter*0.079));		// Big Structure
			zvgFrameVector(-(xcenter*0.562), -(ycenter*0.079), -(xcenter*0.562), -(ycenter*0.164));
			zvgFrameVector(-(xcenter*0.562), -(ycenter*0.164), -(xcenter*0.810), -(ycenter*0.164));
			zvgFrameVector(-(xcenter*0.810), -(ycenter*0.164), -(xcenter*0.810), -(ycenter*0.337));
			zvgFrameVector(-(xcenter*0.810), -(ycenter*0.337), -(xcenter*0.435), -(ycenter*0.337));
			zvgFrameVector(-(xcenter*0.435), -(ycenter*0.337), -(xcenter*0.435), -(ycenter*0.079));

			zvgFrameVector(-(xcenter*0.246), -(ycenter*0.492), -(xcenter*0.375), -(ycenter*0.492));		// Small Structure
			zvgFrameVector(-(xcenter*0.375), -(ycenter*0.492), -(xcenter*0.375), -(ycenter*0.753));
			zvgFrameVector(-(xcenter*0.375), -(ycenter*0.753), -(xcenter*0.183), -(ycenter*0.753));
			zvgFrameVector(-(xcenter*0.183), -(ycenter*0.753), -(xcenter*0.183), -(ycenter*0.578));
			zvgFrameVector(-(xcenter*0.183), -(ycenter*0.578), -(xcenter*0.248), -(ycenter*0.578));
			zvgFrameVector(-(xcenter*0.248), -(ycenter*0.578), -(xcenter*0.248), -(ycenter*0.492));
			break;

		case GAME_WARRIOR:
			zvgFrameSetRGB24(0x20,0x20,0x40);
			zvgFrameVector(-(xcenter*0.42), (ycenter*0.09), -(xcenter*0.09), (ycenter*0.09));	// Top - Left Cavern
			zvgFrameVector(-(xcenter*0.42), -(ycenter*0.33), -(xcenter*0.09), -(ycenter*0.33));	// Bottom - Left Cavern
			zvgFrameVector(-(xcenter*0.42), (ycenter*0.09), -(xcenter*0.42), -(ycenter*0.33));	// Left - Left Cavern
			zvgFrameVector(-(xcenter*0.09), (ycenter*0.09), -(xcenter*0.09), -(ycenter*0.33));	// Right - Left Cavern

			zvgFrameVector((xcenter*0.11), (ycenter*0.22), (xcenter*0.44), (ycenter*0.22));		// Top - Right Cavern
			zvgFrameVector((xcenter*0.11), -(ycenter*0.19), (xcenter*0.44), -(ycenter*0.19));	// Bottom - Right Cavern
			zvgFrameVector((xcenter*0.11), (ycenter*0.22), (xcenter*0.11), -(ycenter*0.19));	// Left - Right Cavern
			zvgFrameVector((xcenter*0.44), (ycenter*0.22), (xcenter*0.44), -(ycenter*0.19));	// Right - Right Cavern
			break;
	}

	zvgFrameSend();

	/* if 2, disable MAME renderer */

	if (zvg_enabled == 2 || zvg_enabled == 3)
		return 0;
	else
		return 1;
}

int determine_artwork_game() {
        if (strcmp(Machine->gamedrv->name, "armora\0") == 0)
        	return GAME_ARMORA;
        if (strcmp(Machine->gamedrv->name, "armorap\0") == 0)
        	return GAME_ARMORA;
        if (strcmp(Machine->gamedrv->name, "armorar\0") == 0)
        	return GAME_ARMORA;
        if (strcmp(Machine->gamedrv->name, "warrior\0") == 0)
        	return GAME_WARRIOR;

	return GAME_NONE;
}

int determine_overlay_game() {
        if (strcmp(Machine->gamedrv->name, "armora\0") == 0)
        	return GAME_ARMORA;
        if (strcmp(Machine->gamedrv->name, "armorap\0") == 0)
        	return GAME_ARMORA;
        if (strcmp(Machine->gamedrv->name, "armorar\0") == 0)
        	return GAME_ARMORA;
        if (strcmp(Machine->gamedrv->name, "barrier\0") == 0)
        	return GAME_BARRIER;
        if (strcmp(Machine->gamedrv->name, "solarq\0") == 0)
        	return GAME_SOLARQ;
	if (strcmp(Machine->gamedrv->name, "starcas\0") == 0)
        	return GAME_STARCAS;
	if (strcmp(Machine->gamedrv->name, "starcas1\0") == 0)
        	return GAME_STARCAS;
	if (strcmp(Machine->gamedrv->name, "starcasp\0") == 0)
        	return GAME_STARCAS;
	if (strcmp(Machine->gamedrv->name, "starcase\0") == 0)
        	return GAME_STARCAS;
	if (strcmp(Machine->gamedrv->name, "stellcas\0") == 0)
        	return GAME_STARCAS;
        if (strcmp(Machine->gamedrv->name, "sundance\0") == 0)
        	return GAME_SUNDANCE;
        if (strcmp(Machine->gamedrv->name, "tailg\0") == 0)
        	return GAME_TAILG;
        if (strcmp(Machine->gamedrv->name, "astdelux\0") == 0)
        	return GAME_ASTDELUX;
        if (strcmp(Machine->gamedrv->name, "astdelu1\0") == 0)
        	return GAME_ASTDELUX;
        if (strcmp(Machine->gamedrv->name, "bzone\0") == 0)
        	return GAME_BZONE;
        if (strcmp(Machine->gamedrv->name, "bzone2\0") == 0)
        	return GAME_BZONE;
        if (strcmp(Machine->gamedrv->name, "bzonec\0") == 0)
        	return GAME_BZONE;
        if (strcmp(Machine->gamedrv->name, "redbaron\0") == 0)
        	return GAME_REDBARON;
        if (strcmp(Machine->gamedrv->name, "omegrace\0") == 0)
        	return GAME_OMEGRACE;
        if (strcmp(Machine->gamedrv->name, "deltrace\0") == 0)
        	return GAME_OMEGRACE;
        if (strcmp(Machine->gamedrv->name, "demon\0") == 0)
        	return GAME_DEMON;

	return GAME_NONE;
}

int zvg_open()
{
	int rv;

	if (zvg_enabled)
	{

		if (zvg_artwork_enabled)
			zvg_artwork_game = determine_artwork_game();

		if (zvg_overlay_enabled) 
			zvg_overlay_game = determine_overlay_game();

		if ((rv = zvgFrameOpen()))
		{
			zvgError(rv);
			return 1;
		}
		vector_register_aux_renderer(zvg_update);

		init_scaling();
	}
	return 0;
}

void zvg_close()
{
	if (zvg_enabled && !zvg_menu_enabled)
	{
		zvgFrameClose();
	}
}
