
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

To install djgpp and the extra stuff you need:

1) Unzip the file **tools\djgpp-gcc343.zip** into a directory on Win98, such as **c:\djsetup**

2) type: **cd c:\djsetup** (Change to that directory)

2) type: **set OS=Windows_NT**  (This is a workaround for a small bug in this install package)
   
3) type: **install.bat** (This will build and install djgpp and install the graphics, sound, and zvg support files)

Note: this script sets the environment variable DJGPP=c:\djgpp\djgpp.env, and puts c:\djgpp\bin in your path
You will need to do this again if you restart your system for some reason

Now, to build mame, go into your repo directory and type: 

**make TARGET=vmame MAMEOS=msdos**

If all goes well you should get a fresh **dvmame.exe** in the current directory

## Binaries ##

Binaries are available in the bin directory.  You may also want to get the menuing software from the original ZVG site.


