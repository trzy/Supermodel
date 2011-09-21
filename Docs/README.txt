TODO: convert all tabs to spaces
TODO: describe Dinputeffectsgain in config index
TODO: XInputConstForceThreshold default? Set in source code.
TODO: describe auto trigger in config index
TODO: Andy Geezer's region codes
 
 
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


================
  Introduction
================

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


==============
  Disclaimer
==============

This is an early public release of Supermodel.  It is very much preliminary,
alpha version software (hence the 'a' in the version number).  Development was
started in January of 2011 and has focused on reverse engineering aspects of 
the system that are still unknown.  Consequently, many important features, such
as a proper user interface, are not yet implemented and game compatibility is
still low.


=====================
  Table of Contents
=====================

    -- Introduction
    -- Disclaimer
    -- Table of Conents
    1. Revision History
    2. Installing Supermodel
    3. Basic Usage
    4. 


====================
  Revision History
====================

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
        - Added light gun crosshairs ('The Lost World'), enabled by default in
          full screen mode and selectable by pressing Alt-I.
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
        
        
=========================
  Installing Supermodel
=========================

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


=============================
  Compiling the Source Code
=============================

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


======================
  Running Supermodel
======================

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


==========================
  Merging Split ROM Sets
==========================

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


===================
  Supported Games
===================

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


==================
  Video Settings
==================

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


==================
  Audio Settings
==================

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
states generated with these settings may not be able to properly restore audio
when loaded after emulation is re-enabled.

Audio settings may also be specified globally or on a per-game basis in the
configuration file, described elsewhere in this document.

Please keep in mind that MPEG music emulation is preliminary and the decoder is
buggy.  Periodic squeaks and pops occur on many music tracks.  The Sega Custom
Sound Processor (SCSP) emulator, used for sound emulation, is an older version
of ElSemi's code and still quite buggy.


============
  Controls
============

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


==================
  Force Feedback
==================

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
=====================

Force feedback can be enabled and tuned in the configuration file.  Setting
'ForceFeedback' to 1 enables it:

	ForceFeedback = 1
	
There are three DirectInput effects: constant force, self centering, and
vibration.  The strength of each can be tuned with the following settings:

	DirectInputEffectsGain = 100
	DirectInputConstForceMax = 100
	DirectInputSelfCenterMax = 100
	DirectInputFrictionMax = 100
	DirectInputVibrateMax = 100
	
They are given as percentages and represent the maximum strength for each
effect.  A setting of 0 disables them entirely.  Values above 100% are
accepted but may clip or distort the effect (and possibly damage your
hardware).  By default, they are set to 100%, as shown above.

XInput devices only support vibration feedback via the left and right motors.
Because the characteristics of the motors are different, the effects will feel
somewhat asymmetric.  The constant force effect is simulated with vibrations.
The relevant settings are:

	XInputConstForceThreshold = 30
 	XInputConstForceMax = 100
  	XInputVibrateMax = 100

The constant force threshold specifies how strong a constant force command must
be before it is sent to the controller as a vibration effect (whose strength is
determined by XInputConstForceMax).  The default values are shown above and 
will require calibration by the user on a game-by-game basis.


=========================
  Save States and NVRAM  
=========================

Save states are saved and restored by pressing F5 and F7, respectively.  Up to
10 different save slots can be selected with F6.  All files are written to the
Saves/ directory, which must exist beforehand.  If you extracted the Supermodel
ZIP file correctly, it will have been created automatically.

Non-volatile memory (NVRAM) consists of battery-backed backup RAM and an EEPROM
chip.  The former is used for high score data and statistics whereas the latter
stores machine settings (often accessed using the Test buttons).  NVRAM is 
automatically saved each time Supermodel exits and is loaded at start-up.  It
can be cleared by deleting the NVRAM files or using Alt-N.


===================================
  Game-Specific Comments and Tips
===================================
    

Daytona USA 2 and Daytona USA 2 Power Edition
---------------------------------------------

To bypass the network board error, enter the Test Menu by pressing the Test
button.  Navigate to 'Game Assignments' using the Service button and select it
with Test.  Change 'Link ID' from 'Master' to 'Single'.

In 'Daytona USA 2', the region menu can be accessed by entering the Test Menu,
holding down the Start button, and pressing: VR4, VR4, VR2, VR3, VR1, VR3, VR2.
Changing the region to USA changes game text to English.


Le Mans 24
----------

The region can be changed by entering the test menu (press the Test button) and
pressing: Start, Start, Service, Service, Start, Test.


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

The region can be changed by entering the test menu (press the Test button) and
then pressing the Service button four times for short durations, twice for long
durations, twice for short durations, and once again for a long duration.


The Lost World
--------------

To reload, the light gun must be pointed off-screen by pressing (and holding)
the 'off-screen' button and, simultaneously, pressing the trigger to shoot.

The region can be changed by entering the test menu (press the Test button) and
pressing: Start, Start, Service, Start, Service, Test.  Use the player 1 Start
button.


Virtua Fighter 3
----------------

The game is playable but there are numerous graphical glitches, particularly 
during transition scenes.


Virtua Striker 2 '98 (Step 1.5 and 2.0 versions)
------------------------------------------------

The region can be changed by entering the test menu (press the Test button)
and, in the 'Game Assignments' menu, performing the following sequence:

	1. Press the Service button once for about 5 seconds.
	2. Press the Service button three times shortly.
	3. Press the Service button again one time for about 5 seconds.


==========================
  The Configuration File
==========================

Supermodel reads configuration settings from 'Supermodel.ini' located in the
'Config' subdirectory.  If Supermodel was installed properly, a default file
should have been created.  When starting up, Supermodel parses settings in the
following order:

	1. Global settings are read from 'Supermodel.ini'.  These include input
	   mappings and apply to all games.
	2. If the ROM set was loaded correctly, game-specific settings are read
	   from 'Supermodel.ini', overriding settings from step 1.
	3. Command line options are applied, overriding settings from the previous
	   steps.
	   
In other words, command line options have the highest precedence, followed by
game-specific settings, and lastly, global settings.

An index of all allowed settings is provided further below in this document.

NOTE: Type carefully!  Supermodel will not report syntax errors or detect 
typos.  Carefully read the discussion of the file syntax below.  To verify that
your intended settings are taking effect, check the 'error.log' file that is
produced by Supermodel during each run.


File Structure and Syntax
-------------------------

The configuration file is a list of settings grouped under sections.  All names
and settings in the file are case sensitive.  Settings take the form:

	Name = Argument
	
Names are case sensitive.  Only one setting per line is allowed.  Only two
types of arguments are allowed: integers and strings.  The choice of which to
use is determined by the setting.  Integers are unsigned and begin at 0.
Strings can contain any characters and are enclosed by double quotes.  
Examples:

	IntegerSetting1 = 0
	IntegerSetting2 = 65536
	StringSetting1 = "This is a string."
	StringSetting2 = "123"
	StringSetting3 = "1+1, abc, these are all valid characters!"

Many settings are simply boolean variables expecting an integer value of either
0 (disabled) or 1 (enabled).  Some settings require values and others take 
strings for file names or input mappings.

Section names appear in between square brackets on their own lines.

	[ SectionName ]
	
Settings that appear at the beginning of the file without a preceeding section
are automatically assigned to 'Global'.

Comments begin with a semicolon and extend until the end of the line.

	; This is a comment.


The Purpose of Sections
-----------------------

Sections determine whether settings are applied globally, to all games, or to
specific games.  Game-specific settings will override global settings and can
be used to tune Supermodel on a game-by-game basis.  Global settings should be
placed in the 'Global' section and game-specific settings in sections named
after the ROM sets.  ROM sets must be typed in lower case and use the MAME
(http://www.mamedev.org) naming convention.  They can be obtained by running
'supermodel -print-games'.

Input mappings are special.  They are only read from the 'Global' section!

In the example below, custom configurations are created for 'Scud Race' and
'The Lost World'.  All other games will use the global settings.

	[ Global ]
	
	; Run full screen at 1024x768
	XResolution = 1024
	YResolution = 768
	FullScreen = 1

	; Scud Race
	[ scud ]
	
	; Run at 1080p
	XResolution = 1920
	YResolution = 1080
	
	; Music is too quiet by default
	SoundVolume = 50
	MusicVolume = 200
	
	; The Lost World
	[ lostwsga ]
	
	PowerPCFrequency = 25	; run PowerPC at 25 MHz

In this example, only 'Scud Race' will run at 1920x1080.  All other games will
use 1024x768.  'Scud Race' will also have altered volume settings.  'The Lost
World' will be run with a lower PowerPC frequency but all other games will use
the default.


Input Mappings
--------------

Input mappings are specified with strings that have their own internal syntax.
They may only be specified in the 'Global' section and are ignored elsewhere.

... TODO: write me

The complete list of input settings can be found in the settings index below or
by generating a configuration file using '-config-inputs'.  


=================================
  Index of Command Line Options
=================================

All valid command line settings are listed here, ordered by category.  Defaults
are given under the assumption that they also have not been changed in the 
configuration file.

Square brackets ('[' and ']') indicate optional parameters.  Angled brackets
('<' and '>') indicate mandatory parameters.  Do not type the brackets.  No 
spaces may appear inside of an option.  For example, 'supermodel game.zip
-ppc-frequency=25' is correct but 'supermodel game.zip -ppc-frequency = 25' is
not.  All options are case sensitive.


	Option:			-?
					-h
	
	Description:	Prints a message with an overview of the command line 
					options and how to run Supermodel.
					
	----------------
	
	Option:			-print-games
	
	Description:	Lists all supported ROM sets.  Not all supported ROM sets
					are playable yet.
	
	----------------
	
	Option:			-no-threads
	
	Description:	Disables multi-threading.  When enabled (the default), the
					PowerPC, graphics rendering, and user interface are run in
					one	thread, sound emulation in a second thread, and the 
					drive board in a third.  It is enabled by default.  On 
					single-core systems, multi-threading adds overhead and 
					slows Supermodel down but can make audio sound a bit 
					smoother.
	
	----------------
	
	Option:			-ppc-frequency=<f>
	
	Description:	Sets the PowerPC frequency in MHz.  The default is 50. 
					Higher frequencies will allow games to do more processing
					per	frame, making them run faster in 'virtual' time, but 
					may	slow down Supermodel on weaker computers.  Supermodel's
					performance can frequently be improved by lowering the
					PowerPC frequency (25 MHz works in many games).  In some
					games, this will cause timing problems.  The optimum value
					for this setting will vary from game to game and system to
					system.  Valid values are 1 to 1000.
	
	----------------
	
	Option:			-fullscreen
	
	Description:	Runs in full screen mode.  The default is to run in a
					window.
	
	----------------
	
	Option:			-no-throttle
	
	Description:	Disables 60 FPS throttling.  The Model 3 runs at a 60 Hz
					refresh rate, which Supermodel enforces if this option is 
					omitted.  Unthrottled operation may not work on some 
					systems because graphics drivers may lock the refresh rate.
	
	----------------
	
	Option:			-print-gl-info
	
	Description:	Prints OpenGL driver information and quits.
	
	----------------
	
	Option:			-res=<x>,<y>
	
	Description:	Resolution of the display in pixels.  The default is
					496x384, the Model 3's native resolution.
	
	----------------
	
	Option:			-show-fps
	
	Description:	Shows the frame rate in the window title bar.
	
	----------------
	
	Option:			-frag-shader=<file>
					-vert-shader=<file>
					
	Description:	Allows external fragment and vertex shaders to be used for
					3D rendering.  Files should be ASCII text files containing
					GLSL source code.  The file extension is not important.  By
					default, Supermodel's internal shader programs are used.
	
	----------------
	
	Option:			-flip-stereo
	
	Description:	Swaps the left and right audio channels.
	
	----------------
	
	Option:			-no-music
	
	Description:	Disables Digital Sound Board (MPEG music) emulation.  See
					the section on audio settings for more information.
	
	----------------
	
	Option:			-no-sound
	
	Description:	Disables sound board (sound effects) emulation.  See the
					section on audio settings for more information.
	
	----------------
	
	Option:			-music-volume=<v>
					-sound-volume=<v>
	
	Description:	Specifies the volume of MPEG music produced by the Digital
					Sound Board and the audio produced by the sound board in 
					percent.  The default is 100, which is full volume, and the
					valid range is 0 (muted) to 200%.  See the section on audio
					settings for more information.
	
	----------------
	
	Option:			-config-inputs
	
	Description:	Launches the interacive input configuration utility in the
					command prompt window.  Allows controls to be remapped.
					See the section on controls for more information.
	
	----------------
	
	Option:			-force-feedback
	
	Description:	Enables force feedback.  Force feedback is only supported
					in a few games (see the section on force feedback for more
					details).
	
	----------------
	
	Option:			-input-system=<s>
	
	Description:	Sets the input system.  This is only available on Windows,
					where the default is 'dinput' (DirectInput).  SDL is used
					for all other platforms.  Valid input systems are:
					
						dinput		DirectInput.
						xinput		XInput
						rawinput	Raw Input.
						sdl			SDL.
						
					See the section on input systems for more details.
	
	----------------
	
	Option:			-print-inputs
	
	Description:	Prints the current input configuration.


========================================
  Index of Configuration File Settings
========================================

All valid configuration file settings are listed here, ordered by category.
Please read the section describing the configuration file for more information.

All settings are case sensitive.

	
	Name:			MultiThreaded
	
	Argument:		Integer.
	
	Description:	If set to 1, enables multi-threading; if set to 0, runs all
					emulation in a single thread.  Read the description of the
					'-no-threads' command line option for more information.
					
	----------------
	
	Name:			PowerPCFrequency
	
	Argument:		Integer.
	
	Description:	The PowerPC frequency in MHz.  The default is 50.  
					Equivalent to the '-ppc-frequency' command line option.
					
	----------------
	
	Name:			FullScreen
	
	Argument:		Integer.
	
	Description:	If set to 1, runs in full screen mode; if set to 0, runs in
					a window.  Disabled by default.  Equivalent to the 
					'-fullscreen' command line option.

	----------------
	
	Name:			ShowFrameRate
	
	Argument:		Integer.
	
	Description:	Shows the frame rate in the window title bar when set to 1.
					If set to 0, the frame rate is not computed.  Disabled by
					default.  Equivalent to the '-show-fps' command line
					option.

	----------------
	
	Name:			Throttle
	
	Argument:		Integer.
	
	Description:	Controls 60 FPS throttling.  It is enabled by setting to 1
					and disabled by setting to 0.  For more information, read
					the description of the '-no-throttle' command line option.

	----------------
	
	Name:			XResolution
					YResolution
	
	Argument:		Integer.
	
	Description:	Resolution of the display in pixels.  The default is
					496x384, the Model 3's native resolution.  Equivalent to
					the '-res' command line option.

	----------------
	
	Name:			FragmentShader
					VertexShader
	
	Argument:		String.
	
	Description:	Path to the external fragment or vertex shader file to use
					for 3D	rendering.  By default, these are not set and the 
					internal default shaders are used.  These are equivalent to
					the '-frag-shader' and '-vert-shader' command line options.

	----------------
	
	Name:			EmulateMusic
	
	Argument:		Integer.
	
	Description:	Emulates the Digital Sound Board if set to 1, disables it
					if set to 0.  See the section on audio settings for more
					information.  A setting of 0 is equivalent to the 
					'-no-music' command line option. 

	----------------
	
	Name:			EmulateSound
	
	Argument:		Integer.
	
	Description:	Emulates the sound board and its two Sega Custom Sound 
					Processors if set to 1, disables it if set to 0.  See the
					section on audio settings for more information.  A setting
					of 0 is equivalent to the '-no-sound' command line option.
	
	----------------
	
	Name:			FlipStereo
	
	Argument:		Integer.
	
	Description:	Swaps the left and right stereo channels if set to 1.  If
					set to 0, outputs the channels normally.  Disabled by
					default.  A setting of 1 is equivalent to using the 
					'-flip-stereo' command line option.
					
	----------------
	
	Name:			MusicVolume
					SoundVolume
	
	Argument:		Integer.
	
	Description:	Specifies the volume of MPEG music produced by the Digital
					Sound Board and the audio produced by the sound board in 
					percent.  The default is 100, which is full volume, and the
					valid range is 0 (muted) to 200%.  See the section on audio
					settings for more information.  The equivalent command line
					options are '-music-volume' and '-sound-volume'.

	----------------
	
	Name:			ForceFeedback
	
	Argument:		Integer.
	
	Description:	If set to 1, enables force feedback emulation; if set to 0,
					disables it (the default behavior).  Equivalent to the 
					'-force-feedback' command line option.
	
	----------------
	
	Name:			DirectInputConstForceMax
					DirectInputFrictionMax
					DirectInputSelfCenterMax
					DirectInputVibrateMax
	
	Argument:		Integer value.
	
	Description:	Sets strength of the four DirectInput force feedback
					effects in percent.  Default is 100, indicating	full 
					strength.  Values exceeding 100% will distort the effects
					and may damage your controller.
	
	----------------
	
	Name: 			DirectInputEffectsGain
	
	Argument:		Integer value.
	
	Description:	

	----------------
		
	Name:			XInputConstForceMax
					XInputVibrateMax
	
	Argument:		Integer value.
	
	Description:	Sets strength of XInput force feedback effects in percent.
					Default is 100, indicating full strength.  Values exceeding
					100% will distort the effects and may damage your 
					controller.  The constant force effect is simulated using 
					vibration.
	
	----------------
	
	Name:			XInputConstForceThreshold
	
	Argument:		Integer value.
	
	Description:	Minimum strength above which a Model 3 constant force
					command will be simulated on an XInput device.  
					XInputConstForceMax determines the vibration strength for
					this effect.  The default value is 30.
					
	----------------
		
	Names:			InputStart1
					InputStart2
					InputCoin1
					InputCoin2
					InputServiceA
					InputServiceB
					InputTestA
					InputTestB
				
	Argument:		String.
	
	Description:	Mappings for common inputs: player 1 and 2 Start buttons,
					both coin slots, and both Service and Test buttons.  Can
					only be set in the 'Global' section.
					
	----------------
					
	Names:			InputJoyDown
					InputJoyDown2
					InputJoyLeft
					InputJoyLeft2
					InputJoyRight
					InputJoyRight2
					InputJoyUp
					InputJoyUp2
	
	Argument:		String.
	
	Description:	Mappings for 4-way digital joysticks (players 1 and 2).
					These controls appear in 'Virtua Fighter 3', 'Fighting
					Vipers 2', 'Spikeout', etc.  Can only be set in the
					'Global' section.
	
	----------------
	
	Names:			InputEscape
					InputEscape2
					InputGuard
					InputGuard2
					InputKick
					InputKick2
					InputPunch
					InputPunch2
	
	Argument:		String.
	
	Description:	Mappings for fighting game buttons (players 1 and 2).  Can
					only be set in the 'Global' section.

	----------------
	
	Names:			InputBeat
					InputCharge
					InputJump
					InputShift
	
	Argument:		String.
	
	Description:	Mappings for 'Spikeout' and 'Spikeout Final Edition'
					buttons.  Can only be set in the 'Global' section.
					
	----------------
	
	Name:			InputLongPass
					InputLongPass2
					InputShortPass
					InputShortPass2
					InputShoot
					InputShoot2
	
	Argument:		String.
	
	Description:	Mappings for the 'Virtua Striker 2' series of games
					(players 1 and 2).  Can only be set in the 'Global' 
					section.
					
	----------------
					
	Name:			InputSteering
	
	Argument:		String.
	
	Description:	Mapping for the analog steering wheel axis, used in all 
					driving games.  Can only be set in the 'Global' section.
	
	----------------
	
	Name:			InputSteeringLeft
					InputSteeringRight
	
	Argument:		String.
	
	Description:	Mappings for digital control of the steering wheel, 
					intended for users who do not have an analog controller.
					Can only be set in the 'Global' section.
					
	----------------
	
	Name:			InputBrake
					InputAccelerator
	
	Argument:		String.
	
	Description:	Mappings for brake and accelerator pedals used in driving
					games.  These are analog axes but can also be mapped to
					digital inputs if needed.  Can only be set in the 'Global'
					section.
					
	----------------
	
	Name:			InputGearShift1
					InputGearShift2
					InputGearShift3
					InputGearShift4
					InputGearShiftN
	
	Argument:		String.
	
	Description:	Mappings for the gear box used in driving games.  In games
					with only two gears (e.g. 'Le Mans 24'), InputGearShift1
					maps to 'Up' and InputGearShift2 to 'Down'.  A neutral 
					position can also be selected.  Can only be set in the
					'Global' section.
					
	----------------
	
	Name:			InputGearShiftDown
					InputGearShiftUp
	
	Argument:		String.
	
	Description:	Mappings for sequential shifting.  These allow gears to be
					selected sequentially: N, 1, 2, 3, 4.  Can only be set in
					the 'Global' section.

	----------------
											
	Name:			InputVR1
					InputVR2
					InputVR3
					InputVR4
	
	Argument:		String.
	
	Description:	Mappings for the 'VR' (color-coded view select) buttons in
					racing games (e.g. 'Scud Race', 'Daytona USA 2', etc.)  Can
					only be set in the 'Global' section.
	
	----------------
	
	Name:			InputViewChange
	
	Argument:		String.
	
	Description:	Mapping for the view change button used in 'Sega Rally 2'
					and 'Dirt Devils'.  Can only be set in the 'Global' 
					section.
	
	----------------
	
	Name:			InputHandBrake
	
	Argument:		String.
	
	Description:	Mapping for the hand brake button used in 'Sega Rally 2'.
					Can only be set in the 'Global' section.

	----------------
	
	Name:			InputTwinJoyCrouch
					InputTwinJoyForward
					InputTwinJoyJump
					InputTwinJoyReverse
					InputTwinJoyStrafeLeft
					InputTwinJoyStrafeRight
					InputTwinJoyTurnLeft
					InputTwinJoyTurnRight
	
	Argument:		String.
	
	Description:	Mappings for 'macro' commands for 'Virtual On Oratorio
					Tangram'.  These allow movements to be mapped to single
					digital inputs and do not require the use of two joysticks.
					Can only be set in the 'Global' section.
	
	----------------
	
	Name:			InputTwinJoyDown1
					InputTwinJoyDown2
					InputTwinJoyLeft1
					InputTwinJoyLeft2
					InputTwinJoyRight1
					InputTwinJoyRight2
					InputTwinJoyUp1
					InputTwinJoyUp2
	
	Argument:		String.
	
	Description:	Mappings for the individual joysticks used by 'Virtual On
					Oratorio Tangram'.  The left stick is stick #1 and the
					right stick is #2.  Can only be set in the 'Global' 
					section.
					
	----------------
	
	Name:			InputTwinJoyShot1
					InputTwinJoyShot2
					InputTwinJoyTurbo1
					InputTwinJoyTurbo2
	
	Argument:		String.
	
	Description:	Mappings for the shot (trigger) and turbo buttons located
					on the left (1) and right (2) joysticks in 'Virtual On
					Oratorio Tangram'.  Can only be set in the 'Global' 
					section.
					
	----------------
	
	Name:			InputAnalogJoyDown
					InputAnalogJoyLeft
					InputAnalogJoyRight
					InputAnalogJoyUp
	
	Argument:		String.
	
	Description:	Mappings for digital control of the analog joystick used in
					'Star Wars Trilogy'.  Two inputs are provided for the two
					directions available on each axis.  Can only be set in the
					'Global' section.
	
	----------------
	
	Name:			InputAnalogJoyX
					InputAnalogJoyY
	
	Argument:		String.
	
	Description:	Mappings for X and Y axes of the analog joystick in 'Star
					Wars Trilogy'.  Can only be set in the 'Global' section.
	
	----------------
	
	Name:			InputAnalogJoyTrigger
					InputAnalogJoyEvent
	
	Argument:		String.
	
	Description:	Mappings for the trigger and Event button in 'Star Wars
					Trilogy'.  Can only be set in the 'Global' section.
					
	----------------
	
	Name:			InputGunDown
					InputGunDown2
					InputGunLeft
					InputGunLeft2
					InputGunRight
					InputGunRight2
					InputGunUp
					InputGunUp2
	
	Argument:		String.
	
	Description:	Mappings for digital control of the light guns in 'The Lost
					World'.  Inputs for motion in each direction are provided.
					Can only be set in the 'Global' section.
					
	----------------
	
	Name:			InputGunX
					InputGunX2
					InputGunY
					InputGunY2
	
	Argument:		String.
	
	Description:	Mappings for both analog axes of the light guns in 'The
					Lost World'.  Can only be set in the 'Global' section.

	----------------
	
	Name:			InputOffscreen
					InputOffscreen2
					InputTrigger
					InputTrigger2
	
	Argument:		String.
	
	Description:	Mappings for the light gun triggers and off-screen
					controls in 'The Lost World'.  The off-screen button aims
					the light gun off-screen while it is pressed, which makes
					re-loading possible.  Can only be set in the 'Global' 
					section.

	----------------
	
	Name:			InputAutoTrigger
					InputAutoTrigger2
	
	Argument:		Integer.
	
	Description:	?  Can only be set in the 'Global' section.
	

=======================
  Contact Information
=======================

The official Supermodel web site is:

    http://www.Supermodel3.com
    
Questions? Comments? Contributions? Your feedback is welcome! We only ask that
you refrain from making feature requests or asking about ROMs. The primary
author, Bart Trzynadlowski, can be reached at:

    supermodel.emu@gmail.com


===================
  Acknowledgments
===================

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
