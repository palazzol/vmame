#ifndef ZVGINTRF_H
#define ZVGINTRF_H

enum
{
	GAME_NONE,

	GAME_ARMORA,
	GAME_BARRIER,
	GAME_SOLARQ,
	GAME_STARCAS,
	GAME_SUNDANCE,
	GAME_TAILG,
	GAME_WARRIOR,

	GAME_ASTDELUX,
	GAME_BZONE,
	GAME_REDBARON,

	GAME_OMEGRACE,

	GAME_DEMON
};

extern int zvg_enabled;
extern int zvg_menu_enabled;
extern int zvg_artwork_enabled;
extern int zvg_overlay_enabled;

int  zvgIFrameOpen(void);
void zvgIFrameClose(void);
int  zvgIFrameSend(void);
int  zvgIFrameVector(int xStart, int yStart, int xEnd, int yEnd);
void zvgIFrameSetRGB24(int red, int green, int blue);
void zvgIFrameSetClipWin(int xMin, int yMin, int xMax, int yMax);
void zvgIFrameSetClipOverscan(void);
void zvgIFrameSetClipNoOverscan(void);

int determine_artwork_game(void);
int determine_overlay_game(void);

int zvg_open(void);
void zvg_close(void);

#endif
