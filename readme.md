
# VectorMAME #

This is a DOS version of the MAME emulator, for use with the ZVG vector-generator board.

http://www.zektor.com/zvg/vectormame.htm

The last version of MAME available from the ZVG folks is based on MAME 0.96,
but MAME supported dos up until version 0.106.  This repo is based on 0.106,
with a few tweaks to fix some bad bugs which can cause crashes.

(Setting up a DOS machine is hard enough without having MAME crash on top of it.)

## Build Instructions: ##

Your best bet for building DOS mame is to use a Virtual Machine like VirtualBox, with an OS image of Windows98SE.
Most importantly, Win98 supports long filenames, which are used in the build process for DOS MAME.  

I used djgpp based on gcc3.4.3, and the sources for mame and dosmame.
The fixes I did to dosmame all apply to the msdos-specific code.

I have included this version of djgpp, as well as the original MAME sources, in the tools directory

If djgpp is installed in c:\djgpp

```
set DJGPP=c:\djgpp\djgpp.env
```

also, add c:\djgpp\bin to your path

now to build, type:

```
make TARGET=vmame MAMEOS=msdos
```


