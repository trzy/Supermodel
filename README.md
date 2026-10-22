# Supermodel: A Sega Model 3 Arcade Emulator
*Copyright 2003-2026 The Supermodel Team*

## Overview

**Supermodel** emulates Sega's Model 3 arcade platform, allowing you to relive state-of-the-art 3D arcade gaming as it existed from 1996 through 1999. It uses OpenGL, [SDL2](https://libsdl.org), and can run on Windows, Linux, and macOS. It also supports network play on low-latency network connections. The source code is freely available under the terms of the [GNU General Public License](http://www.gnu.org/copyleft/gpl.html).

<p align="center">
  <img src="Docs/Images/Daytona2_1.gif" width="375" height="250" /> <img src="Docs/Images/LAMachin.gif" width="375" height="250" />
  <img src="Docs/Images/StarWars.gif" width="375" height="250" /> <img src="Docs/Images/FVipers2.gif" width="375" height="250" />
</p>

<img src="Docs/Images/Real3D_Logo.png" align="right" height="100"> Model 3 first made its debut in 1996 with Virtua Fighter 3 and Scud Race, and for the subsequent three years boasted the most powerful 3D hardware of any gaming platform. Developed by Real3D, then a Lockheed Martin company, and with a heritage rooted in advanced flight simulator technology, Model 3 featured capabilities that would not appear on PCs for several years. Using an on-board scene graph and geometry processor, it could store, transform, light, and rasterize tens of thousands of polygons per frame at a fluid 57.524 frames per second.

The aim of the Supermodel project is to develop an emulator that is both accurate and playable. As with virtually all arcade hardware, no public documentation for the Model 3 platform exists. What is known so far has been painstakingly reverse engineered from scratch.

## How To Get It

Automated builds for Windows, macOS and Linux are now available here on GitHub.  
Check out the releases page [here](https://github.com/trzy/Supermodel/releases).

## Community

<table border="0">
  <tr>
    <td><a href="https://discord.gg/XDd7TMMUMz"><img width="32" src="Docs/Images/Discord.svg"></a></td><td><a href="https://discord.gg/XDd7TMMUMz">Join the Discord server</a>.</td>
  </tr>
</table>

We lost our web hosting and have no backup of the recent forum. For now, please use this GitHub page for official releases and [Discord](https://discord.gg/XDd7TMMUMz) for asking questions.

## Build Instructions

### Windows

The preferred method for building Supermodel is to use GCC and MSYS2. After installing [MSYS2](https://msys2.org), open the MSYS2 shell and install the required dependencies using the pacman package manager:

- GCC (```mingw64/mingw-w64-x86_64-gcc```)
- Make (```mingw64/mingw-w64-x86_64-make```)
- SDL2 (```mingw64/mingw-w64-x86_64-SDL2```, ```mingw64/mingw-w64-x86_64-SDL2_net```)

This can be done using the following commands:

```
pacman -S mingw64/mingw-w64-x86_64-gcc
pacman -S mingw64/mingw-w64-x86_64-make
pacman -S mingw64/mingw-w64-x86_64-SDL2
pacman -S mingw64/mingw-w64-x86_64-SDL2_net
```

At this point, you can continue using either the MSYS2 shell or Windows Command Prompt but ensure that both ```gcc``` and ```mingw32-make``` are in your path. In MSYS2, the location of these binaries will be ```/mingw64/bin``` and for Command Prompt, assuming MSYS2 was installed in the default location, add ```C:\msys64\mingw64\bin``` to your Windows ```PATH``` variable.

To build Supermodel use (Network support built in by default now, no need to specify anymore):

```
mingw32-make -f Makefiles/Makefile.Win32
```

If you wish to build via Visual Studio 2026,
Make sure to run the Get_Libs.cmd files in the libraries directory. It will automatically pull in the required dependencies. Then either build inside the GUI or use the Build.cmd file in the MSVC folder.

### Linux

Ensure SDL2 is installed. Most package managers ought to have this available. For example, on Ubuntu, it should be sufficient to run:

```
sudo apt install libsdl2-dev
sudo apt install libsdl2-net-dev
sudo apt install libglu1-mesa-dev
```

And then build Supermodel:

```
make -f Makefiles/Makefile.UNIX
```

### macOS

Ensure Apple's Xcode Command Line Tools are installed:

From a terminal:
```
xcode-select --install
```

Ensure SDL2 is installed.  Download the latest *.dmg files from both of the links below, and install per the READMEs in the .dmgs (i.e. in "/Library/Frameworks")

* SDL2: https://github.com/libsdl-org/SDL/releases

* SDL_net: https://github.com/libsdl-org/SDL_net/releases


And then build Supermodel:

```
make -f Makefiles/Makefile.OSX
```

### libretro core

Supermodel can also be built as a [libretro](https://www.libretro.com) core for RetroArch and other libretro frontends. Add `LIBRETRO=1` to the usual `make` command; everything else (compilers, MSYS2 setup) is the same as for the standalone emulator above.

The core does not use SDL: it only needs OpenGL and zlib, so the SDL2 packages are not required unless you build with `LIBRETRO_NET=1` (see below). Without `LIBRETRO_NET=1` the network (net board, Pro-1000 SCSI TCP bridge, network outputs) is not available.

#### Linux

```
sudo apt install build-essential libgl-dev zlib1g-dev
make -f Makefiles/Makefile.UNIX LIBRETRO=1
```

Output: `bin/supermodel_libretro.so`

#### Windows (MSYS2 MinGW-w64)

Install GCC and Make as described above (zlib is `mingw64/mingw-w64-x86_64-zlib`, normally already installed with GCC), then:

```
mingw32-make -f Makefiles/Makefile.Win32 LIBRETRO=1
```

Output: `bin64/supermodel_libretro.dll` (the C/C++ runtimes and zlib are linked statically, so no MinGW DLLs are needed next to it).

#### macOS

With the Xcode Command Line Tools installed (`xcode-select --install`):

```
make -f Makefiles/Makefile.OSX LIBRETRO=1
```

Output: `bin/supermodel_libretro.dylib`

#### Build options

| Option | Effect |
|--------|--------|
| `LIBRETRO=1` | Build the libretro core instead of the standalone emulator. |
| `LIBRETRO_NET=1` | Together with `LIBRETRO=1`: build the core with SDL2 and SDL2_net, so that the network works: the net board (linked cabinets), the Pro-1000 SCSI TCP bridge and the network outputs. SDL2 and SDL2_net must then be installed to build and to run the core. |
| `DEBUG=1` | Debug build (no optimization, symbols kept). |
| `-j<n>` | Parallel build, e.g. `-j8`. |

The core is compiled into its own object directory (`obj_libretro`, `obj64_libretro` on Windows, `..._net` with `LIBRETRO_NET=1`), so the standalone emulator and the core can be built from the same source tree without cleaning in between. `make ... LIBRETRO=1 clean` removes the core's object directory and the whole output directory (`bin` / `bin64`), including a standalone binary built there.

A copy of `Config/Games.xml` is compiled into the core, so the core works without any extra file.

#### Installing in RetroArch

1. Copy `supermodel_libretro.so` / `.dll` / `.dylib` to RetroArch's `cores` directory.
2. Copy `Src/OSD/Libretro/supermodel_libretro.info` to RetroArch's `info` directory (core name, supported extensions, required OpenGL version).
3. Optional, in RetroArch's system directory (`system/supermodel/`):
   - `Games.xml`: replaces the built-in game list (e.g. a newer one).
   - `Music.xml`: custom MPEG music.
   - `Assets/p1crosshair.bmp` and `Assets/p2crosshair.bmp`: needed only for the "Bitmap" crosshair style (the default "Vector" crosshairs need no file).
4. Load a ROM set (`.zip`, the same files as for the standalone emulator) with the core. Use the `glcore` video driver for the new 3D engine (OpenGL 4.1 core profile) or the `gl` driver if you switch to the legacy 3D engine, which needs a compatibility profile context.

See [Docs/LIBRETRO.md](Docs/LIBRETRO.md) for the controls (RetroPad layout per game type), the core options and how they map to the standalone emulator's command line options.

### Note: running on macOS
If you try and run a macOS binary that was downloaded from the internet and/or built on a different machine, you need to grant macOS permission to execute the binary (just 1-time):

* Open the folder containing the binary in Finder, and right (or ctrl) click on it:

* Click "Open" when the following dialogue box appears : "macOS cannot verify the developer of “supermodel-git-xxxx”. Are you sure you want to open it?"

* Close the terminal window that opens (after clicking open)

Details: https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unidentified-developer-mh40616/mac
