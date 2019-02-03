/* blit.h */

void bitblit_vga( mame_bitmap *bitmap, int sx, int sy, int sw, int sh, int dx, int dy );
void bitblit_unchained_vga( mame_bitmap *bitmap, int sx, int sy, int sw, int sh, int dx, int dy );
void bitblit_vesa( mame_bitmap *bitmap, int sx, int sy, int sw, int sh, int dx, int dy );
unsigned long blit_setup( int dw, int dh, int sbpp, int dbpp, int video_attributes, int xmultiply, int ymultiply, int xdivide, int ydivide,
	int vscanlines, int hscanlines, int flipx, int flipy, int swapxy );
void blit_set_buffers( int pages, int page_size );

#define MAX_X_MULTIPLY 4
#define MAX_Y_MULTIPLY 4

extern UINT32 blit_lookup_low[ 65536 ];
extern UINT32 blit_lookup_high[ 65536 ];
