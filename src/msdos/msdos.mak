ASM = @nasm

# check that the required libraries are available
ifeq ($(wildcard $(DJDIR)/lib/liballeg.a),)
noallegro:
	@echo Missing Allegro library! Get it from http://www.talula.demon.co.uk/allegro/
endif
ifeq ($(wildcard $(DJDIR)/lib/libaudio.a),)
noseal:
	@echo Missing SEAL library! Get it from http://www.egerter.com/
endif
ifeq ($(wildcard $(DJDIR)/lib/libz.a),)
nozlib:
	@echo Missing zlib library! Get it from http://www.cdrom.com/pub/infozip/zlib/
endif

CFLAGS += -D_HUGE=HUGE_VAL

# add allegro, audio & fpu emulator libs
LIBS += -lalleg -laudio -lemu -lzvg -lm

# only MS-DOS specific output files and rules
OSOBJS = $(OBJ)/msdos/msdos.o $(OBJ)/msdos/video.o $(OBJ)/msdos/blit.o $(OBJ)/msdos/asmblit.o \
	$(OBJ)/msdos/gen15khz.o $(OBJ)/msdos/ati15khz.o $(OBJ)/msdos/twkuser.o \
	$(OBJ)/msdos/sound.o $(OBJ)/msdos/input.o $(OBJ)/msdos/rc.o $(OBJ)/msdos/misc.o \
	$(OBJ)/msdos/ticker.o $(OBJ)/msdos/config.o $(OBJ)/msdos/fronthlp.o \
	$(OBJ)/msdos/fileio.o $(OBJ)/msdos/snprintf.o $(OBJ)/msdos/zvgintrf.o \
	$(OBJ)/msdos/debugwin.o

OSTOOLOBJS = \
	$(OBJ)/$(MAMEOS)/osd_tool.o

# video blitting functions
$(OBJ)/msdos/asmblit.o: src/msdos/asmblit.asm
	@echo Assembling $<...
	$(ASM) -o $@ $(ASMFLAGS) $(subst -D,-d,$(ASMDEFS)) $<

# if we are not using x86drc.o, we should be
ifndef X86_MIPS3_DRC
COREOBJS += $(OBJ)/x86drc.o
endif
