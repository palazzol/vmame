###########################################################################
#
#   makefile
#
#   Core makefile for building MAME and derivatives
#
#   Copyright (c) 1996-2006, Nicola Salmoria and the MAME Team.
#   Visit http://mamedev.org for licensing and usage restrictions.
#
###########################################################################


###########################################################################
#################   BEGIN USER-CONFIGURABLE OPTIONS   #####################
###########################################################################


#-------------------------------------------------
# specify core target: mame, mess, tiny, etc.
# build rules will be included from $(TARGET).mak
#-------------------------------------------------

ifndef TARGET
TARGET = mame
endif



#-------------------------------------------------
# specify operating system: windows, msdos, etc.
# build rules will be includes from $(MAMEOS)/$(MAMEOS).mak
#-------------------------------------------------

ifndef MAMEOS
MAMEOS = windows
endif



#-------------------------------------------------
# specify program options; see each option below 
# for details
#-------------------------------------------------

# uncomment next line to include the debugger
# DEBUG = 1

# uncomment next line to use the new multiwindow debugger
NEW_DEBUGGER = 1

# uncomment next line to use the new rendering system
# NEW_RENDER = 1

# uncomment next line to use DRC MIPS3 engine
X86_MIPS3_DRC = 1

# uncomment next line to use DRC PowerPC engine
X86_PPC_DRC = 1

# uncomment next line to use DRC Voodoo rasterizers
# X86_VOODOO_DRC = 1



#-------------------------------------------------
# specify build options; see each option below 
# for details
#-------------------------------------------------

# uncomment one of the next lines to build a target-optimized build
# ATHLON = 1
# I686 = 1
# P4 = 1
# PM = 1
# AMD64 = 1

# uncomment next line if you are building for a 64-bit target
# PTR64 = 1

# uncomment next line to use cygwin compiler
# COMPILESYSTEM_CYGWIN	= 1

# uncomment next line to build expat as part of MAME build
BUILD_EXPAT = 1

# uncomment next line to build zlib as part of MAME build
BUILD_ZLIB = 1

# uncomment next line to include the symbols
# SYMBOLS = 1

# uncomment next line to generate a link map for exception handling in windows
# MAP = 1


###########################################################################
##################   END USER-CONFIGURABLE OPTIONS   ######################
###########################################################################


#-------------------------------------------------
# sanity check the configuration
#-------------------------------------------------

# disable DRC cores for 64-bit builds
ifdef PTR64
X86_MIPS3_DRC =
X86_PPC_DRC =
endif



#-------------------------------------------------
# platform-specific definitions
#-------------------------------------------------

# extension for executables
EXE = .exe

# compiler, linker and utilities
AR = @ar
CC = @gcc
LD = @gcc
ASM = @nasm
ASMFLAGS = -f coff
MD = -mkdir.exe
RM = @rm -f



#-------------------------------------------------
# form the name of the executable
#-------------------------------------------------

ifeq ($(MAMEOS),msdos)
PREFIX = d
endif

# by default, compile for Pentium target
NAME = $(PREFIX)$(TARGET)
ARCH = -march=pentium

# architecture-specific builds get extra options
ifdef ATHLON
NAME = $(PREFIX)$(TARGET)at
ARCH = -march=athlon
endif

ifdef I686
NAME = $(PREFIX)$(TARGET)pp
ARCH = -march=pentiumpro
endif

ifdef P4
NAME = $(PREFIX)$(TARGET)p4
ARCH = -march=pentium4
endif

ifdef AMD64
NAME = $(PREFIX)$(TARGET)64
ARCH = -march=athlon64
endif

ifdef PM
NAME = $(PREFIX)$(TARGET)pm
ARCH = -march=pentium3 -msse2
endif

# debug builds just get the 'd' suffix and nothing more
ifdef DEBUG
NAME = $(PREFIX)$(TARGET)d
endif

EMULATOR = $(NAME)$(EXE)

# build the targets in different object dirs, since mess changes
# some structures and thus they can't be linked against each other.
OBJ = obj/$(NAME)



#-------------------------------------------------
# compile-time definitions
#-------------------------------------------------

DEFS = -DX86_ASM -DLSB_FIRST -DINLINE="static __inline__" -Dasm=__asm__ -DCRLF=3

ifdef PTR64
DEFS += -DPTR64
endif

ifdef DEBUG
DEFS += -DMAME_DEBUG
endif

ifdef NEW_DEBUGGER
DEFS += -DNEW_DEBUGGER
endif

ifdef NEW_RENDER
DEFS += -DNEW_RENDER
endif

ifdef X86_VOODOO_DRC
DEFS += -DVOODOO_DRC
endif



#-------------------------------------------------
# compile and linking flags
#-------------------------------------------------

CFLAGS = -std=gnu89 -Isrc -Isrc/includes -Isrc/$(MAMEOS)

ifdef SYMBOLS
CFLAGS += -O0 -Wall -Wno-unused -g
else
CFLAGS += -DNDEBUG \
	$(ARCH) -O3 -fno-strict-aliasing \
	-Werror -Wall \
	-Wno-sign-compare \
	-Wno-unused-functions \
	-Wpointer-arith \
	-Wbad-function-cast \
	-Wcast-align \
	-Wstrict-prototypes \
	-Wundef \
	-Wformat-security \
	-Wwrite-strings \
	-Wdeclaration-after-statement
endif

# extra options needed *only* for the osd files
CFLAGSOSDEPEND = $(CFLAGS)

LDFLAGS = -WO

ifdef SYMBOLS
LDFLAGS =
else
LDFLAGS += -s
endif

ifdef MAP
MAPFLAGS = -Wl,-Map,$(NAME).map
else
MAPFLAGS =
endif

ifdef COMPILESYSTEM_CYGWIN
CFLAGS += -mno-cygwin
LDFLAGS	+= -mno-cygwin
endif



#-------------------------------------------------
# define the dependency search paths
#-------------------------------------------------

VPATH = src $(wildcard src/cpu/*)



#-------------------------------------------------
# define the standard object directories
#-------------------------------------------------

OBJDIRS = obj $(OBJ) $(OBJ)/cpu $(OBJ)/sound $(OBJ)/$(MAMEOS) \
	$(OBJ)/drivers $(OBJ)/machine $(OBJ)/vidhrdw $(OBJ)/sndhrdw $(OBJ)/debug

ifdef MESS
OBJDIRS += $(OBJ)/mess $(OBJ)/mess/systems $(OBJ)/mess/machine \
	$(OBJ)/mess/vidhrdw $(OBJ)/mess/sndhrdw $(OBJ)/mess/tools
endif



#-------------------------------------------------
# define standard libarires for CPU and sounds
#-------------------------------------------------

CPULIB = $(OBJ)/libcpu.a

SOUNDLIB = $(OBJ)/libsound.a



#-------------------------------------------------
# either build or link against the included 
# libraries
#-------------------------------------------------

# start with an empty set of libs
LIBS = 

# add expat XML library
ifdef BUILD_EXPAT
CFLAGS += -Isrc/expat
OBJDIRS += $(OBJ)/expat
EXPAT = $(OBJ)/libexpat.a
else
LIBS += -lexpat
EXPAT =
endif

# add ZLIB compression library
ifdef BUILD_ZLIB
CFLAGS += -Isrc/zlib
OBJDIRS += $(OBJ)/zlib
ZLIB = $(OBJ)/libz.a
else
LIBS += -lz
ZLIB =
endif



#-------------------------------------------------
# 'all' target needs to go here, before the 
# include files which define additional targets
#-------------------------------------------------

all: maketree emulator extra



#-------------------------------------------------
# include the various .mak files
#-------------------------------------------------

# include OS-specific rules first
include src/$(MAMEOS)/$(MAMEOS).mak

# then the various core pieces
include src/core.mak
include src/$(TARGET).mak
include src/cpu/cpu.mak
include src/sound/sound.mak

# combine the various definitions to one
CDEFS = $(DEFS) $(COREDEFS) $(CPUDEFS) $(SOUNDDEFS) $(ASMDEFS)



#-------------------------------------------------
# primary targets
#-------------------------------------------------

emulator: maketree $(EMULATOR)

extra: $(TOOLS)

maketree: $(sort $(OBJDIRS)) $(OSPREBUILD)

clean:
	@echo Deleting object tree $(OBJ)...
	$(RM) -r $(OBJ)
	@echo Deleting $(EMULATOR)...
	$(RM) $(EMULATOR)
	@echo Deleting $(TOOLS)...
	$(RM) $(TOOLS)

check: $(EMULATOR) xml2info$(EXE)
	./$(EMULATOR) -listxml > $(NAME).xml
	./xml2info < $(NAME).xml > $(NAME).lst
	./xmllint --valid --noout $(NAME).xml



#-------------------------------------------------
# directory targets
#-------------------------------------------------

$(sort $(OBJDIRS)):
	$(MD) $@



#-------------------------------------------------
# executable targets and dependencies
#-------------------------------------------------

$(EMULATOR): $(COREOBJS) $(OSOBJS) $(CPULIB) $(SOUNDLIB) $(DRVLIBS) $(EXPAT) $(ZLIB) $(OSDBGOBJS)
# always recompile the version string
	$(CC) $(CDEFS) $(CFLAGS) -c src/version.c -o $(OBJ)/version.o
	@echo Linking $@...
	$(LD) $(LDFLAGS) $(OSDBGLDFLAGS) $^ $(LIBS) -o $@ $(MAPFLAGS)

romcmp$(EXE): $(OBJ)/romcmp.o $(OBJ)/unzip.o $(ZLIB) $(OSDBGOBJS)

chdman$(EXE): $(OBJ)/chdman.o $(OBJ)/chd.o $(OBJ)/chdcd.o $(OBJ)/cdrom.o $(OBJ)/md5.o $(OBJ)/sha1.o $(OBJ)/version.o $(ZLIB) $(OSTOOLOBJS) $(OSDBGOBJS)

xml2info$(EXE): $(OBJ)/xml2info.o $(EXPAT) $(OSDBGOBJS)

jedutil$(EXE): $(OBJ)/jedutil.o $(OBJ)/jedparse.o $(OSDBGOBJS)



#-------------------------------------------------
# library targets and dependencies
#-------------------------------------------------

$(CPULIB): $(CPUOBJS)

ifdef DEBUG
$(CPULIB): $(DBGOBJS)
endif

$(SOUNDLIB): $(SOUNDOBJS)

$(OBJ)/libexpat.a: $(OBJ)/expat/xmlparse.o $(OBJ)/expat/xmlrole.o $(OBJ)/expat/xmltok.o

$(OBJ)/libz.a: $(OBJ)/zlib/adler32.o $(OBJ)/zlib/compress.o $(OBJ)/zlib/crc32.o $(OBJ)/zlib/deflate.o \
				$(OBJ)/zlib/gzio.o $(OBJ)/zlib/inffast.o $(OBJ)/zlib/inflate.o $(OBJ)/zlib/infback.o \
				$(OBJ)/zlib/inftrees.o $(OBJ)/zlib/trees.o $(OBJ)/zlib/uncompr.o $(OBJ)/zlib/zutil.o



#-------------------------------------------------
# generic rules
#-------------------------------------------------

$(OBJ)/$(MAMEOS)/%.o: src/$(MAMEOS)/%.c
	@echo Compiling $<...
	$(CC) $(CDEFS) $(CFLAGSOSDEPEND) -c $< -o $@

$(OBJ)/%.o: src/%.c
	@echo Compiling $<...
	$(CC) $(CDEFS) $(CFLAGS) -c $< -o $@

$(OBJ)/%.pp: src/%.c
	@echo Compiling $<...
	$(CC) $(CDEFS) $(CFLAGS) -E $< -o $@

$(OBJ)/%.s: src/%.c
	@echo Compiling $<...
	$(CC) $(CDEFS) $(CFLAGS) -S $< -o $@

$(OBJ)/%.a:
	@echo Archiving $@...
	$(RM) $@
	$(AR) -cr $@ $^
	
%$(EXE):
	@echo Linking $@...
	$(LD) $(LDFLAGS) $(OSDBGLDFLAGS) $^ $(LIBS) -o $@
