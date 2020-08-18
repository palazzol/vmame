
# VectorMAME #

This is a MS-DOS version of the MAME emulator, for use with the ZVG vector-generator board.

http://www.zektor.com/zvg/vectormame.htm

The last version of MAME available from the ZVG folks is based on MAME 0.96,
but MAME supported dos up until version 0.106.  This repo is based on 0.106.

## Differences from Version 0.106

Version 0.106A:

1) Bug fix for issues when running with no game selected
2) Add skip_disclaimer and skip_warnings options

## Build Instructions: ##

Your best bet for building DOS mame is to use a Virtual Machine like VirtualBox, with an OS image of Windows98SE.
Most importantly, Win98 supports long filenames, which are used in the build process for DOS MAME.  

I used djgpp based on gcc3.4.3, and the sources for mame and dosmame.
The fixes I did to dosmame all apply to the msdos-specific code.

I have included this version of djgpp, as well as the original MAME sources, in the tools directory

To install djgpp and the extra stuff you need:

1) Unzip the file **tools\djgpp-gcc343.zip** into a directory on Win98, such as **c:\djsetup**

2) type: **cd c:\djsetup** (Change to that directory)

3) type: **set OS=Windows_NT**  (This is a workaround for a small bug in this install package)
   
4) type: **install.bat** (This will build and install djgpp and install the graphics, sound, and zvg support files)

Note: this script sets the environment variable DJGPP=c:\djgpp\djgpp.env, and puts c:\djgpp\bin in your path
You will need to do this again if you restart your system for some reason

Now, to build mame, go into your repo directory and type: 

**make TARGET=vmame MAMEOS=msdos**

If all goes well you should get a fresh **dvmame.exe** in the current directory

If you would also like to compress the dvmame.exe file, type:

**upx dvmame.exe**

This shrinks the file from over 3Mb to about 1Mb

## Binaries ##

Binaries are available in the bin directory.  You may also want to get the menuing software from the original ZVG site.
