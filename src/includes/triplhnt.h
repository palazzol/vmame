/*************************************************************************

    Atari Triple Hunt hardware

*************************************************************************/

#include "sound/discrete.h"

/* Discrete Sound Input Nodes */
#define TRIPLHNT_BEAR_ROAR_DATA	NODE_01
#define TRIPLHNT_BEAR_EN		NODE_02
#define TRIPLHNT_SHOT_DATA		NODE_03
#define TRIPLHNT_SCREECH_EN		NODE_04
#define TRIPLHNT_LAMP_EN		NODE_05


/*----------- defined in sndhrdw/triplhnt.c -----------*/

extern struct discrete_sound_block triplhnt_discrete_interface[];


/*----------- defined in vidhrdw/triplhnt.c -----------*/

VIDEO_START( triplhnt );
VIDEO_UPDATE( triplhnt );

extern UINT8* triplhnt_playfield_ram;
extern UINT8* triplhnt_vpos_ram;
extern UINT8* triplhnt_hpos_ram;
extern UINT8* triplhnt_code_ram;
extern UINT8* triplhnt_orga_ram;

extern int triplhnt_sprite_zoom;
extern int triplhnt_sprite_bank;
