TODO: convert all tabs to spaces
 
 
  ####                                                      ###           ###
 ##  ##                                                      ##            ##
 ###     ##  ##  ## ###   ####   ## ###  ##  ##   ####       ##   ####     ##
  ###    ##  ##   ##  ## ##  ##   ### ## ####### ##  ##   #####  ##  ##    ##
    ###  ##  ##   ##  ## ######   ##  ## ####### ##  ##  ##  ##  ######    ##
 ##  ##  ##  ##   #####  ##       ##     ## # ## ##  ##  ##  ##  ##        ##
  ####    ### ##  ##      ####   ####    ##   ##  ####    ### ##  ####    ####
                 ####                                                            

                           Supermodel Version 0.2a
                       A Sega Model 3 Arcade Emulator.
                Copyright 2011 Bart Trzynadlowski, Nik Henson


----------------
  Terms of Use
----------------

Supermodel is distributed as free software under the terms of the GNU General
Public License, included in LICENSE.txt.  Additional copyright information for
software included within Supermodel is located at the bottom of this file.

The source code may be obtained from the official Supermodel web site:

    http://www.Supermodel3.com


--------------
  Disclaimer
--------------

This is an early public release of Supermodel.  It is very much preliminary,
alpha version software (hence the 'a' in the version number).  Development was
started in January of 2011 and has focused on reverse engineering aspects of 
the system that are still unknown.  Consequently, many important features, such
as a proper user interface, are not yet implemented and game compatibility is
still low.


--------------------
  Revision History
--------------------

    Version 0.2a
    	- New, fully customizable input system.  Supports any combination of 
    	  keyboards, mice, and analog and digital game pads.  [Nik Henson]
    	- Texture offsets.  Fixes models in 'Fighting Vipers 2', 'Virtual On',
    	  cars in the 'Scud Race' and 'Daytona USA 2' selection screens, etc.
    	- Fixed a 2D palette bug that would cause black pixels to occasionally
    	  turn transparent.
    	- Added all remaining ROM sets.  [krom]
    	- Got some new games to boot: 'Spikeout', 'Ski Champ', 'Sega Bass 
    	  Fishing', 'Dirt Devils', etc.
        - Sound support.  Special thanks to ElSemi for contributing his SCSP
          emulator and Karl Stenerud for allowing us to use his 68K emulator.
        - Multi-threading support.  Sound and drive board emulation are off-
          loaded to a separate thread, substantially enhancing performance. 
          [Nik Henson]
        - Z80 emulation based on code by Frank D. Cringle and YAZE-AG by 
          Andreas Gerlich.
        - Digital Sound Board (MPEG music) emulation courtesy of R. Belmont and
          the MPEG decoder library by Tomislav Uzelac.
        - Improved ROM loading.  It should no longer be as easily confused by
          combined ROM sets as long as unused files are removed.
        - Configuration file now supports more settings and allows game-
		  specific customization.
		- Added light gun crosshairs ('Lost World'), enabled by default in full
		  screen mode and selectable by pressing Alt-I.
    	- Drive board and force feedback emulation for 'Scud Race', 'Daytona
    	  USA 2', and 'Sega Rally 2'.  [Nik Henson]
        - Viewable display area properly clipped.  Ghost artifacts no longer
          appear in border regions when the resolution exceeds the display 
          area.
    	- Changed gear shifting: added a dedicated neutral gear and sequential
    	  shifting.
    	- Console-based debugger (not enabled by default, must be
    	  enabled during compile-time).  [Nik Henson]
    	- Source code and Makefile cleanup.

    Version 0.1.2a
        - Included missing GLEW files.

    Version 0.1.1a
        - Minor source code update.
        - Set Render3D to NULL in the CReal3D constructor.  Fixes crashes that
          occur on some builds. [Nik Henson]
        - Cleaned up UNIX Makefile and added OS X Makefile.  [R. Belmont]
        - Small changes to ppc_ops.cpp for C++0x compliance.  [R. Belmont]
        - Included glew.h into the source tree.  [R. Belmont]
        - Changed WIN32 definition to SUPERMODEL_WIN32.
        
    Version 0.1a
        - Initial public alpha release.
        
        
-------------------------
  Installing Supermodel
-------------------------

To install Supermodel on Windows, extract the ZIP archive containing the
Supermodel executable to a folder of your choice.  The following files and
directories should be created:

    Name                    Description
    ----                    -----------
    Supermodel.exe          Supermodel program. Run this.
    SDL.dll                 The SDL library. Use the bundled DLL file.
    README.txt              This text file.
    LICENSE.txt             Supermodel license and terms of use.
    Config/                 Directory where configuration files will be stored.
    NVRAM/                  Directory where NVRAM contents will be saved.
    Saves/                  Directory where save states will be saved.
    
Supermodel requires OpenGL 2.0 and a substantial amount of both video and
system memory.  The 'error.log' file, which is produced during each session,
records the amount of video memory allocated for vertex buffers.  If either the
static or dynamic vertex buffer are substantially less than 16 MB or 4 MB,
respectively, Supermodel may run abnormally slowly and drop some geometry.


-----------------------------
  Compiling the Source Code
-----------------------------

First, ensure that OpenGL, SDL (http://www.libsdl.org), and zlib
(http://zlib.net) are installed. GLEW (http://glew.sourceforge.net) is now
included in the source tree and should not need to be installed separately.

Next, extract the Supermodel source code and copy the appropriate Makefile 
from the Makefiles/ directory to the base directory (that is, the one above
Src/ and Makefiles/).  Makefiles for 32-bit Windows (Microsoft Visual C++ 
2008), Linux/UNIX (GCC), and Mac OS X (GCC) are provided, all requiring GNU
Make.  For Windows developers, MinGW (http://www.mingw.org) provides GNU Make.
Alternatively, you can write a Makefile compatible with Microsoft Nmake and 
submit it to me for inclusion in the next release. ;)  Consult the Visual 
Studio documentation to learn how to configure the Microsoft compiler for 
command line operation.

Edit SDL_LIBPATH and SDL_INCLUDEPATH to reflect the location of the SDL
development library and header files.  This should only be necessary for 
Windows.  The UNIX and Mac OS X Makefiles should be able to automatically
locate SDL if it was installed properly.

Finally, run 'make'.  If all goes well it should produce a Supermodel binary.


---------------
  Basic Usage
---------------

For now, Supermodel does not include a proper user interface.  It is operated
entirely from the command line.  Run 'Supermodel' without any command line 
arguments for an explanation of supported options.

Supermodel uses MAME-compatible ROM sets.  It loads from ZIP archives based on
file checksums and automatically detects games; therefore, file names are not
important.  ROMs that are split into parent and child sets (eg., 'Scud Race
Plus', whose parent ROM set is 'Scud Race') must be combined into a single ZIP 
file with the child ROM set appearing first to ensure Supermodel detects the 
correct game to load.  The best way to do this is to add the contents of the
parent ROM set into the child ROM set ZIP file.  If for some reason Supermodel
detects the wrong game, you should try to combine them the opposite way.

To improve performance, Supermodel underclocks the PowerPC down to 25 MHz by
default.  This may be the cause of some games running too slowly even when the
frame rate is at 60 FPS (which is the native rate).  You may want to experiment
with increasing the clock frequency, although this comes with a performance 
penalty.

Note that there is no user interface and all messages are printed to the 
terminal.  In full screen mode, they will not be visible.


-------------------
  Supported Games
-------------------

The following games and ROM sets are supported by this version of Supermodel.
Not all of them are playable.

    ROM Set        Game Title
    -------        ----------
    vf3            Virtua Fighter 3
    lemans24       Le Mans 24
    scud           Scud Race
    scudp          Scud Race Plus
    lostwsga       The Lost World
    von2           Virtual On Oratorio Tangram
    vs298          Virtua Striker 2 '98
    srally2        Sega Rally 2
    daytona2       Daytona USA 2
    dayto2pe       Daytona USA 2 Power Edition
    fvipers2       Fighting Vipers 2
    harley         Harley Davidson & L.A. Riders
    lamachin       L.A. Machineguns
    oceanhun       The Ocean Hunter
    swtrilgy       Star Wars Trilogy
    eca            Emergency Car Ambulance

The '-print-games' option can be used to obtain this list.


------------------
  Video Settings
------------------

Supermodel may be run in either windowed (default) or full screen mode.  It 
automatically adjusts the display area to retain the aspect ratio of the Model
3's native (and Supermodel's default) resolution, 496x384.  Currently, this
cannot be overriden.  Changing video modes at run-time is not yet supported.

By default, Supermodel limits the frame rate to 60 Hz.  This has no impact on
performance except when the frame rate exceeds 60 FPS on fast systems.  This
can be disabled with the '-no-throttle' option.

The mouse cursor is disabled by default in full screen modes but can be toggled
with the Alt-I command.


------------
  Controls
------------

Supermodel only supports the keyboard and mouse at present.  Emulator functions
are listed below and cannot be changed.

    Function                                Key Assignment
    --------                                --------------
    Exit                                    Escape
    Reset                                   Alt-R
    Clear NVRAM                             Alt-N
    Toggle Mouse Cursor (Full Screen Mode)  Alt-I
    Toggle 60 Hz Frame Limiting             Alt-T
    Save State                              F5
    Load State                              F7
    Change Save Slot                        F6

Game controls can be configured using the '-config-inputs' command line option.
This starts a configuration utility and requires each button for all supported
games to be defined.  Pressing Escape retains the current setting.  The blank
window that opens up must be selected when pressing keys.  To choose a mouse
button, click the black area of the window.  New configurations are saved to
Config/Supermodel.ini.  The Config/ directory should have been automatically
created when Supermodel was extracted from its ZIP file.

Default settings are given below.

    Game/Input Type                    Input                Default Assignment
    ---------------                    -----                ------------------
    All                                Start 1              1
    All                                Start 2              2
    All                                Coin 1               3
    All                                Coin 2               4
    All                                Service A            5
    All                                Test A               6
    All                                Service B            7
    All                                Test B               8
    Digital joystick                   Up                   Up arrow
    Digital joystick                   Down                 Down arrow
    Digital joystick                   Left                 Left arrow
    Digital joystick                   Right                Right arrow
    Fighting games                     Punch                A
    Fighting games                     Kick                 S
    Fighting games                     Guard                D
    Fighting games                     Escape               F
    Racing games                       Steering             Left/Right arrows
    Racing games                       Accelerate           Up arrow
    Racing games                       Brake                Down arrow
    Racing games                       Shift 1/Up           Q
    Racing games                       Shift 2/Down         W
    Racing games                       Shift 3              E
    Racing games                       Shift 4              R
    Racing games                       VR1 (Red)            A
    Racing games                       VR1 (Blue)           S
    Racing games                       VR1 (Yellow)         D
    Racing games                       VR1 (Green)          F
    Sega Rally 2                       View Change          A
    Sega Rally 2                       Hand Brake           S
    Virtua Striker 2                   Short Pass           A
    Virtua Striker 2                   Long Pass            S
    Virtua Striker 2                   Shoot                D
    Virtual On Oratorio Tangram        Turn Left            Q        
    Virtual On Oratorio Tangram        Turn Right           W
    Virtual On Oratorio Tangram        Forward              Up arrow
    Virtual On Oratorio Tangram        Reverse              Down arrow
    Virtual On Oratorio Tangram        Strafe Left          Left arrow
    Virtual On Oratorio Tangram        Strafe Right         Right arrow
    Virtual On Oratorio Tangram        Jump                 E
    Virtual On Oratorio Tangram        Crouch               R
    Virtual On Oratorio Tangram        Left Shot Trigger    A
    Virtual On Oratorio Tangram        Right Shot Trigger   S
    Virtual On Oratorio Tangram        Left Turbo           Z
    Virtual On Oratorio Tangram        Right Turbo          X
    Analog joystick                    Motion               Mouse
    Analog joystick                    Trigger Button       A
    Analog joystick                    Event Button         S
    Lightgun                           Aim                  Mouse
    Lightgun                           Trigger              Left mouse button
    Lightgun                           Point Off-screen     Right mouse button

The lightgun will point off-screen as long as the 'point off-screen' button is
held down.  To shoot off-screen, hold this button and then simultaneously press
the trigger.


-------------------------
  Save States and NVRAM  
-------------------------

Save states are saved and restored by pressing F5 and F7, respectively.  Up to
10 different save slots can be selected with F6.  All files are written to the
Saves/ directory, which must exist.  If you extracted the Supermodel ZIP file
correctly, they will have automatically been created.

Non-volatile memory (NVRAM) consists of battery-backed backup RAM and an EEPROM
chip.  The former is used for things like high score data whereas the latter 
stores machine settings (often accessed using the Test buttons).  NVRAM is 
automatically saved each time Supermodel exits and is loaded at start-up.  It
can be cleared by deleting the NVRAM files or using Alt-N.


-----------------------------------
  Game-Specific Comments and Tips
-----------------------------------

The following games are not playable due to unimplemented controls, severe
graphical problems, or other issues:

    - L.A. Machineguns
    - The Ocean Hunter
    - Harley Davidson & L.A. Riders
    - Emergency Call Ambulance
    
Daytona USA 2 and Daytona USA 2 Power Edition
---------------------------------------------

To bypass the network board error, enter the Test Menu by pressing the Test
button.  Navigate to 'Game Assignments' using the Service button and select it
with Test.  Change 'Link ID' from 'Master' to 'Single'.

In 'Daytona USA 2', the region menu can be accessed by entering the Test Menu,
holding down the Start button, and pressing: VR4, VR4, VR2, VR3, VR1, VR3, VR2.
Changing the region to USA changes game text to English.

Star Wars Trilogy
-----------------

Inserting coins and starting a game before any 3D graphics have been displayed
in attract mode will result in a PowerPC crash before the stage loads.  This is
caused by an unknown emulation bug.  Simply wait until the Darth Vader sequence
appears before attempting to start a game.

If Star Wars Trilogy is booting directly into the stage select screen, it is
probably because you exited Supermodel with credits still in the machine.
Clear the NVRAM (Alt-N) and reset the game (Alt-R).

Sega Rally 2
------------

As with Star Wars Trilogy, you may experience problems if you attempt to start
a game before any 3D graphics are displayed (for example, during the Sega
logo).

Virtua Fighter 3
----------------

The game is playable but there are numerous graphical glitches, particularly 
during transition scenes.


-----------------------
  Contact Information
-----------------------

The official Supermodel web site is:

    http://www.Supermodel3.com
    
Questions? Comments? Contributions? Your feedback is welcome! I only ask that
you refrain from making feature requests or asking about ROMs. The primary
author, Bart Trzynadlowski, can be reached at:

    supermodel.emu@gmail.com


-------------------
  Acknowledgments
-------------------

Numerous people contributed their precious time and energy to this project.
Without them, Supermodel would not have been possible.  In no particular order,
I would like to thank:

    - Ville Linde, original Supermodel team member and MAMEDev extraordinaire
    - Stefano Teso, original Supermodel team member
    - ElSemi, for all sorts of technical information and insight
    - Naibo Zhang, for his work on Model 3 graphics
    - R. Belmont, for all sorts of help
    - The Guru, for his efforts in dumping Model 3 ROM sets
    - Andrew Lewis, for his arcade know-how and helpfulness
    - Abelardo Vidal Martos, for providing extremely useful video recordings of
      actual Model 3 games
    - Andrew Gardner, for fruitful discussion
    - Chad Reker, for being an especially thorough play-tester
    - And, of course, my sister Nicole, for help with web site images

Supermodel includes code from the following projects:

    zlib:                       http://zlib.net
    minizip:                    http://www.winimage.com/zLibDll/minizip.html
    The OpenGL Extension 
    Wrangler Library (GLEW):    http://glew.sourceforge.net
    
The OpenGL Extension Wrangler Library
Copyright (C) 2002-2008, Milan Ikits <milan ikits[]ieee org>
Copyright (C) 2002-2008, Marcelo E. Magallon <mmagallo[]debian org>
Copyright (C) 2002, Lev Povalahev
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, 
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, 
  this list of conditions and the following disclaimer in the documentation 
  and/or other materials provided with the distribution.
* The name of the author may be used to endorse or promote products 
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
