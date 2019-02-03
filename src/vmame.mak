
#
# Tiny Compile file for compiling only the Vector games.
#

# Setup the CORE definitions

COREDEFS += -DTINY_NAME="driver_alphaona,driver_alphaone,driver_armora,driver_armorap,driver_armorar,driver_astdelu1,driver_astdelux,driver_asteroi1,driver_asteroib,driver_asteroid,driver_aztarac,driver_barrier,driver_boxingb,driver_bradley,driver_bwidow,driver_bzone,driver_bzone2,driver_bzonec,driver_cchasm,driver_cchasm1,driver_demon,driver_elim2,driver_elim2a,driver_elim4,driver_esb,driver_gravitar,driver_gravitr2,driver_gravp,driver_llander,driver_llander1,driver_lunarbat,driver_mhavoc,driver_mhavoc2,driver_mhavocp,driver_mhavocrv,driver_omegrace,driver_quantum,driver_quantum1,driver_qb3,driver_quantump,driver_redbaron,driver_ripoff,driver_spacduel,driver_solarq,driver_spacewar,driver_spacfura,driver_spacfury,driver_speedfrk,driver_starcas,driver_starcas1,driver_starcasp,driver_starcase,driver_starhawk,driver_startrek,driver_starwar1,driver_starwars,driver_stellcas,driver_sundance,driver_tacscan,driver_tailg,driver_tempest,driver_tempest1,driver_tempest2,driver_temptube,driver_warrior,driver_wotw,driver_zektor"
COREDEFS += -DTINY_POINTER="&driver_alphaona,&driver_alphaone,&driver_armora,&driver_armorap,&driver_armorar,&driver_astdelu1,&driver_astdelux,&driver_asteroi1,&driver_asteroib,&driver_asteroid,&driver_aztarac,&driver_barrier,&driver_boxingb,&driver_bradley,&driver_bwidow,&driver_bzone,&driver_bzone2,&driver_bzonec,&driver_cchasm,&driver_cchasm1,&driver_demon,&driver_elim2,&driver_elim2a,&driver_elim4,&driver_esb,&driver_gravitar,&driver_gravitr2,&driver_gravp,&driver_llander,&driver_llander1,&driver_lunarbat,&driver_mhavoc,&driver_mhavoc2,&driver_mhavocp,&driver_mhavocrv,&driver_omegrace,&driver_qb3,&driver_quantum,&driver_quantum1,&driver_quantump,&driver_redbaron,&driver_ripoff,&driver_spacewar,&driver_spacfura,&driver_spacduel,&driver_solarq,&driver_spacfury,&driver_speedfrk,&driver_starcas,&driver_starcas1,&driver_starcasp,&driver_starcase,&driver_starhawk,&driver_startrek,&driver_starwar1,&driver_starwars,&driver_stellcas,&driver_sundance,&driver_tacscan,&driver_tailg,&driver_tempest,&driver_tempest1,&driver_tempest2,&driver_temptube,&driver_warrior,&driver_wotw,&driver_zektor"

# Vector games use these CPUs

CPUS+=CCPU
CPUS+=I8035
CPUS+=M6502
CPUS+=M68000
CPUS+=M6809
CPUS+=Z80

# Vector games use these SOUNDs (and SOUND CPUs)

SOUNDS+=AY8910
SOUNDS+=CUSTOM
SOUNDS+=DAC
SOUNDS+=DISCRETE
SOUNDS+=POKEY
SOUNDS+=SAMPLES
SOUNDS+=SN76496
SOUNDS+=SP0250
SOUNDS+=TMS36XX
SOUNDS+=TMS5220
CPUS+=N7751

# Vector game specific OBJs

DRVLIBS = \
$(OBJ)/machine/segacrpt.o $(OBJ)/sndhrdw/segasnd.o \
$(OBJ)/vidhrdw/sega.o $(OBJ)/sndhrdw/sega.o $(OBJ)/drivers/sega.o \
$(OBJ)/vidhrdw/segar.o $(OBJ)/sndhrdw/segar.o $(OBJ)/machine/segar.o $(OBJ)/drivers/segar.o \
$(OBJ)/machine/z80ctc.o \
$(OBJ)/machine/z80sio.o \
$(OBJ)/machine/z80pio.o \
$(OBJ)/machine/atari_vg.o $(OBJ)/vidhrdw/avgdvg.o \
$(OBJ)/machine/asteroid.o $(OBJ)/sndhrdw/asteroid.o \
$(OBJ)/sndhrdw/llander.o $(OBJ)/drivers/asteroid.o \
$(OBJ)/drivers/bwidow.o \
$(OBJ)/sndhrdw/bzone.o	$(OBJ)/drivers/bzone.o \
$(OBJ)/sndhrdw/redbaron.o \
$(OBJ)/drivers/tempest.o \
$(OBJ)/machine/starwars.o $(OBJ)/machine/mathbox.o \
$(OBJ)/drivers/starwars.o $(OBJ)/sndhrdw/starwars.o \
$(OBJ)/machine/mhavoc.o $(OBJ)/drivers/mhavoc.o \
$(OBJ)/drivers/quantum.o \
$(OBJ)/machine/slapstic.o \
$(OBJ)/vidhrdw/cinemat.o $(OBJ)/sndhrdw/cinemat.o $(OBJ)/drivers/cinemat.o \
$(OBJ)/machine/cchasm.o $(OBJ)/vidhrdw/cchasm.o $(OBJ)/sndhrdw/cchasm.o $(OBJ)/drivers/cchasm.o \
$(OBJ)/drivers/omegrace.o \
$(OBJ)/vidhrdw/aztarac.o $(OBJ)/sndhrdw/aztarac.o $(OBJ)/drivers/aztarac.o \

# Also needed are these specific core objs

COREOBJS += $(OBJ)/tiny.o $(OBJ)/cheat.o
