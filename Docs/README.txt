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
  Introduction
----------------

Supermodel emulates the Sega Model 3 arcade platform.  It uses OpenGL 2.1 and
is available for Windows, Linux, and Mac OS X.  In order to use it, you must 
legally possess ROM images of Model 3 games.  Learning to operate Supermodel
may come with a steep learning curve for most people.  Carefully reading this
document in its entirety before seeking out help is strongly advised.

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


---------------------
  Table of Contents
---------------------

    -- Introduction
    -- Disclaimer
    -- Table of Conents
    1. Revision History
    2. Installing Supermodel
    3. Basic Usage
    4. 


--------------------
  Revision History
--------------------

    Version 0.2a (September ?, 2011)
        - New, fully customizable input system.  Supports any combination of 
          keyboards, mice, and analog and digital controllers.  [Nik Henson]
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

    Version 0.1.2a (April 3, 2011)
        - Included missing GLEW files.

    Version 0.1.1a (April 2, 2011)
        - Minor source code update.
        - Set Render3D to NULL in the CReal3D constructor.  Fixes crashes that
          occur on some builds. [Nik Henson]
        - Cleaned up UNIX Makefile and added OS X Makefile.  [R. Belmont]
        - Small changes to ppc_ops.cpp for C++0x compliance.  [R. Belmont]
        - Included glew.h into the source tree.  [R. Belmont]
        - Changed WIN32 definition to SUPERMODEL_WIN32.
        
    Version 0.1a (April 1, 2011)
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
    Config/                 Directory where the configuration file is stored.
    Config/Supermodel.ini   Configuration file containing default input 
                            settings.
    NVRAM/                  Directory where NVRAM contents will be saved.
    Saves/                  Directory where save states will be saved.
    
Supermodel requires OpenGL 2.1 and a substantial amount of both video and
system memory.

As of this version, Linux and Mac OS X binaries are not provided.  Users must
compile their own.


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


----------------------
  Running Supermodel
----------------------

For now, Supermodel does not include a proper user interface.  It is operated
entirely from the command line.  Run 'supermodel' without any command line 
arguments for an explanation of supported options.

Supermodel uses MAME-compatible ROM sets.  It loads from ZIP archives based on
file checksums and automatically detects games; therefore, file names are not
important.  A maximum of one ZIP file can be specified on the command line. For
example:

    supermodel scud.zip -fullscreen

This will load 'scud.zip' (Scud Race) and run it in full screen mode.

Initially, inputs are assigned according to the settings in 'Supermodel.ini',
located in the 'Config' subdirectory.

Note that there is no user interface and all messages are printed to the 
command prompt.  In full screen mode, they will not be visible. 


--------------------------
  Merging Split ROM Sets
--------------------------

ROMs that are split into parent and child sets (eg., 'Scud Race Plus', whose
parent ROM set is 'Scud Race') must be combined into a single ZIP file.  ROM
files from the parent set that have the same IC numbers (usually the file 
extension but sometimes the number in the file name itself) as child ROMs
should be deleted, otherwise Supermodel may choose to load the parent game. 

For example, 'Scud Race Plus' is normally distributed containing only the
following files:

    epr-20092a.17
    epr-20093a.18
    epr-20094a.19
    epr-20095a.20
    epr-20096a.21
    mpr-20097.13
    mpr-20098.14
    mpr-20099.15
    mpr-20100.16
    mpr-20101.24
    
To merge with the parent ROM set, copy over all files from 'Scud Race' except
those with extension numbers 17-21, 13-16, and 24.  Some 'Scud Race Plus' ROM
sets may have 'mpr-20101.23' instead of 'mpr-20101.24'.  They are the same file
and in both cases should replace the file with extension 24 from 'Scud Race'
('mpr-19671.24').


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
can be disabled with the '-no-throttle' option.  Some video drivers may
continue to lock to the refresh rate.

To change the resolution, use the '-res' command line option.  For full screen
mode, use '-fullscreen'.  For example, to set a full screen 1920x1080 mode,
try:

	supermodel game.zip -res=1920,1080 -fullscreen

Video settings may also be specified globally or on a per-game basis in the
configuration file, described elsewhere in this document.


------------------
  Audio Settings
------------------

All Model 3 games have a sound board that is used for sound effects and, in
some games, background music.  A few games use additional Digital Sound Boards
(DSB) for MPEG music.  'Music' in Supermodel refers exclusively to MPEG music
produced by the DSB and 'sound' refers to both the sound effects and background
music produced by the regular sound board.

Model 3 sound and MPEG music are generated separately and then mixed by an 
amplifier.  The relative signal levels are not known, so Supermodel simply
outputs all audio at full volume.  This causes the MPEG music to be too quiet
in some games ('Scud Race', 'Daytona USA 2') and too loud in others ('Star Wars
Trilogy').  The '-sound-volume' and '-music-volume' options can be used to 
change the volume.  As arguments, they take a volume level in percent ranging
from 0 (muted) to 200% (maximum, doubled amplitude).  For example:

	supermodel game.zip -sound-volume=50 -music-volume=170
	
This command line cuts the sound volume in half and increases the music volume
by 70%.

The F9 and F10 keys can be used to adjust music volume during run-time, while
F11 and F12 control sound volume.

Clipping and distortion will occur if the combined sound and music volume
levels become too high.

To disable sound and music board emulation altogether, use the '-no-sound' and
'-no-dsb' options.  These will not merely mute the corresponding audio channels
but will prevent the CPUs and audio chips from being emulated altogether.  Save
states generate with these disabled may not be able to restore audio properly.

Audio settings may also be specified globally or on a per-game basis in the
configuration file, described elsewhere in this document.

Please keep in mind that MPEG music emulation is preliminary and the decoder is
buggy.  Periodic squeaks and pops occur on many music tracks.  The Sega Custom
Sound Processor (SCSP) emulator, used for sound emulation, is an older version
of ElSemi's code and still quite buggy.


------------
  Controls
------------

Supermodel only supports the keyboard and mouse at present.  Emulator functions
are listed below and cannot be changed.

    Function                                Key Assignment
    --------                                --------------
    Exit                                    Escape
    Pause                                   Alt-P
    Reset                                   Alt-R
    Clear NVRAM                             Alt-N
    Crosshairs (for light gun games)        Alt-I
    Toggle 60 Hz Frame Limiting             Alt-T
    Save State                              F5
    Load State                              F7
    Change Save Slot                        F6
    Decrease Music Volume                   F9
    Increase Music Volume                   F10
    Decrease Sound Volume                   F11
    Increase Sound Volume                   F12


Learning and Configuring Game Controls
--------------------------------------

Settings for game controls are stored in 'Supermodel.ini'.  To learn the
current configuration, use the '-print-inputs' command line option.  If you
delete 'Supermodel.ini', all inputs will become unmapped and will have to be
reconfigured!

    supermodel -print-inputs
    
If you wish to reconfigure, use the '-config-inputs' option.  A blank window
will open up and instructions will be printed to the command prompt.  Read them
carefully!  Pressing the Escape key exits without saving changes.  Pressing 'q'
will save changes to 'Supermodel.ini'.  You can choose which input to configure
using the Up and Down arrow keys.  Clearing an input means it will be unusable.

If the configuration dialog is not responding to your key presses, make sure
that the blank window rather than the command prompt is selected.  This may 
seem counter-intuitive but inputs are captured by the graphical window while
messages are printed to the command prompt.


XBox 360 Controllers
--------------------

For full XBox 360 controller support, the XInput system must be used
('-input-system=xinput').  Otherwise, the left and right trigger buttons cannot
be mapped individually and force feedback will not work.  Please read the
section titled 'Input Systems', further below.


Analog Controls
---------------

Analog controls, such as steering wheels, pedals, and the light gun axes, can
be mapped to analog controllers, such as joysticks, wheels, and mice, if
available.  Under digital control, the analog value will increment or decrement
until it reaches its maximum/minimum values.  The rate of change can only be
set manually in the configuration file.  These options are described elsewhere
in this document.

Most analog inputs, such as the steering wheel, require two settings to be 
controlled digitally.  For example, the steering wheel can be turned left and
right by setting 'Steer Left' and 'Steer Right', or it can be mapped to a
single analog control ('Full Steering').


Shifting Gears
--------------

Racing game gears can be mapped to individual buttons, including one for the
neutral position, or can be shifted sequentially.  Games which only provide two
gears name them 'Up' and 'Down'.  These should not be confused with 'Shift Up'
and 'Shift Down', the sequential shift commands.


Virtual On Twin Joysticks
-------------------------

'Virtual On Oratorio Tangram' features a twin joystick control scheme similar
to a tracked vehicle (e.g. a tank).  Movement is accomplished by pushing both
joysticks in the same direction.  Pushing and pulling in opposite directions
will turn the robot, while pulling the joysticks apart sideways or pushing them
inwards is for jumping and falling back down to the ground, respectively.

Supermodel supports mapping the individual joysticks but also provides 'macro'
controls by default.  These allow all the necessary twin joystick commands to
be replicated with individual controls.  The mapping is below:

    Macro Control       Twin Joystick Equivalent
    -------------       ------------------------
    Turn Left           Left joystick down, right joystick up.
    Turn Right          Left joystick up, right joystick down.
    Forward             Both joysticks up.
    Reverse             Both joysticks down.
    Strafe Left         Both joysticks left.
    Strafe Right        Both joysticks right.
    Jump                Left joystick left, right joystick right.
    Crouch              Left joystick right, right joystick left.


Light Guns
----------

Light gun axes can be mapped to mice, analog controls, or even digital buttons.
To simulate pointing off-screen (required in order to reload), a 'point off-
screen' input is provided which, for as long as it is pressed, will aim the gun
off-screen.  To reload, hold down this button and then, while holding it, press
the trigger.

Crosshairs for both players will be visible in full screen mode and can also be
enabled in windowed modes.  Use Alt-I to select the crosshair mode.

For multiple mouse support, allowing two mice or PC light guns to be used, Raw
Input must be used.  This is supported only on Windows and is described below.


Input Systems
-------------

Supermodel supports multiple input APIs to provide the best possible
compatibility for different input devices and configuration schemes.  On
Windows, the default is DirectInput.  On all other platforms, SDL is the only
option available.

Windows users can select between four different input systems:

	- DirectInput.  Selected with '-input-system=dinput'.  This is the default.
	  It provides the best support for PC game controllers and, when emulating
	  force feedback, allows all effects (if the controller supports them).
	- XInput.  Selected with '-input-system=xinput'.  This must be used with
	  XBox 360 controllers, otherwise some buttons will not be usable and force
	  feedback will not work.
	- Raw Input.  Selected with '-input-system=rawinput'.  This is intended for
	  use with multiple mice and keyboards but is not recommended otherwise.
	- SDL.  Selected with '-input-system=sdl'.  The standard, cross-platform
	  input system intended for non-Windows builds.  It is accessible on
	  Windows but does not provide full support for all devices.

When switching input systems with '-input-system', you must also configure your
inputs using the same option.  For example, when running Supermodel with
XInput ('supermodel game.zip -input-system=xinput'), you must configure with
XInput ('supermodel -config-inputs -input-system=xinput').  Many settings are
not compatible between input systems.

A common mistake is to configure inputs using one system and then launch
Supermodel with another.


------------------
  Force Feedback
------------------

Force feedback is presently supported in 'Scud Race' (including 'Scud Race 
Plus'), 'Daytona USA 2' (both editions), and 'Sega Rally 2' on Windows only. To
enable it, use the '-force-feedback' option.

Drive board ROMs are required.

	Game			Drive Board ROM File	Size		Checksum (CRC32)
	----     		--------------------	----		----------------
	Daytona USA 2	epr-20985.bin   		64 KB		B139481D
	Scud Race		epr-19338a.bin     		64 KB		C9FAC464
	Sega Rally 2	epr-20512.bin    		64 KB		CF64350D

The sizes and checksums must match those listed above.  The file names may be
different but will almost certainly contain the same identifying numbers.
Ensure that the appropriate drive board ROM files are present in the 
corresponding games' ZIP archives, otherwise Supermodel will silently proceed
without force feedback.

Force feedback will only work with the DirectInput (the default on Windows) and
XInput input systems.  XInput is intended only for XBox 360 controllers, which
do not support force feedback through DirectInput.


Tuning Force Feedback
---------------------

Force feedback can be enabled and tuned in the configuration file.  Setting
'ForceFeedback' to 1 enables it:

	ForceFeedback = 1
	
There are three DirectInput effects: constant force, self centering, and
vibration.  The strength of each can be tuned with the following settings:

	DirectInputConstForceMax = 100
	DirectInputSelfCenterMax = 100
	DirectInputVibrateMax = 100
	
They are given as percentages and represent the maximum strength for each
effect.  A setting of 0 disables them entirely.  Values above 100% are
accepted but may clip or distort the effect (and possibly damage your
hardware).  By default, they are set to 100%, as shown above.

XInput devices only support vibration feedback via the left and right motors.
Because the characteristics of the motors are different, the effects will feel
somewhat asymmetric.  The constant force effect is simulated with vibrations.
The relevant settings are:

	XInputConstForceThreshold = 65
 	XInputConstForceMax = 100
  	XInputVibrateMax = 100

The constant force threshold specifies how strong a constant force command must
be before it is sent to the controller as a vibration effect (whose strength is
determined by XInputConstForceMax).  The default values are shown above and 
will require calibration by the user on a game-by-game basis.


-------------------------
  Save States and NVRAM  
-------------------------

Save states are saved and restored by pressing F5 and F7, respectively.  Up to
10 different save slots can be selected with F6.  All files are written to the
Saves/ directory, which must exist beforehand.  If you extracted the Supermodel
ZIP file correctly, it will have been created automatically.

Non-volatile memory (NVRAM) consists of battery-backed backup RAM and an EEPROM
chip.  The former is used for high score data and statistics whereas the latter
stores machine settings (often accessed using the Test buttons).  NVRAM is 
automatically saved each time Supermodel exits and is loaded at start-up.  It
can be cleared by deleting the NVRAM files or using Alt-N.


-----------------------------------
  Game-Specific Comments and Tips
-----------------------------------
    
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


--------------------------
  The Configuration File
--------------------------

---------------------------------
  Index of Command Line Options
---------------------------------

----------------------------------------
  Index of Configuration File Settings
----------------------------------------

-----------------------
  Contact Information
-----------------------

The official Supermodel web site is:

    http://www.Supermodel3.com
    
Questions? Comments? Contributions? Your feedback is welcome! We only ask that
you refrain from making feature requests or asking about ROMs. The primary
author, Bart Trzynadlowski, can be reached at:

    supermodel.emu@gmail.com


-------------------
  Acknowledgments
-------------------

Numerous people contributed their precious time and energy to this project.
Without them, Supermodel would not have been possible.  In no particular order,
we would like to thank:

    - Ville Linde, original Supermodel team member and MAMEDev extraordinaire
    - Stefano Teso, original Supermodel team member
    - ElSemi, for all sorts of technical information and insight
    - Naibo Zhang, for his work on Model 3 graphics
    - R. Belmont, for all sorts of help
    - Andrew Lewis (a.k.a. Andy Geezer), for dumping the drive board ROMs and
      providing region codes
    - The Guru, for his efforts in dumping Model 3 ROM sets
    - Abelardo Vidal Martos, for providing extremely useful video recordings of
      actual Model 3 games
    - Andrew Gardner, for fruitful discussion
    - Chad Reker, for being an especially thorough play-tester
    - And of course, my sister Nicole, for help with web site images

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
