

  ####                                                      ###           ###
 ##  ##                                                      ##            ##
 ###     ##  ##  ## ###   ####   ## ###  ##  ##   ####       ##   ####     ##
  ###    ##  ##   ##  ## ##  ##   ### ## ####### ##  ##   #####  ##  ##    ##
    ###  ##  ##   ##  ## ######   ##  ## ####### ##  ##  ##  ##  ######    ##
 ##  ##  ##  ##   #####  ##       ##     ## # ## ##  ##  ##  ##  ##        ##
  ####    ### ##  ##      ####   ####    ##   ##  ####    ### ##  ####    ####
                 ####

                       A Sega Model 3 Arcade Emulator.
                   Copyright 2003-2023 The Supermodel Team


                        USER MANUAL FOR SUPERMODEL


================
  Introduction
================

Supermodel emulates the Sega Model 3 arcade platform.  It uses OpenGL 2.1 and
SDL, and can run on Windows, Linux, and Mac OS X.  Development began in January
of 2011 and continues to focus on reverse engineering all aspects of the Model
3 arcade platform.

In order to use Supermodel, you must legally possess ROM images of Model 3
games.  Learning to operate Supermodel will come with a steep learning curve for
most people.  Before seeking out help, please read this user manual carefully.

Supermodel is distributed as free software under the terms of the GNU General
Public License, included in LICENSE.txt.  Additional copyright information for
software included within Supermodel is located at the bottom of this file.

Windows 64-bit builds are updated automatically and are available for download
on the official Supermodel download page.  Linux and MacOS users currently have
to compile/build their own executable from the GitHub source code repository.

The official Supermodel website:

    http://www.Supermodel3.com

Supermodel's GitHub source code repository:

    https://github.com/trzy/Supermodel


==============
  Disclaimer
==============

Supermodel is a work-in-progress, open source emulator.  Emulation in Supermodel
has evolved to the point where most games run well, however there may be some
minor visual issues.  Your experience may vary.


=====================
  Table of Contents
=====================

    --- Introduction
    --- Disclaimer
    --- Table of Contents
     1. Installing Supermodel
     2. Running Supermodel
     3. Game Compatibility
     4. Video Settings
     5. Audio Settings
     6. Controls
     7. Force Feedback
     8. Save States and NVRAM
     9. Game-Specific Comments and Tips
    10. The Configuration File
    11. Index of Command Line Options
    12. Index of Configuration File Settings
    13. Compiling the Source Code
    14. Contact Information
    15. Acknowledgments


============================
  1. Installing Supermodel
============================

To install Supermodel on Windows, extract the ZIP archive containing the
Supermodel executable to a folder of your choice.  The following files and
directories should be created:

    Name                    Description
    ----                    -----------
    Supermodel.exe          Supermodel program.  Run this.
    README.txt              This text file.
    LICENSE.txt             Supermodel license and terms of use.
    CHANGES.txt             Change log directly produces directly from the
                            source code repository.
    Assets/                 Directory containing multimedia assets required by
                            Supermodel.
    Assets/p1crosshair.bmp  Bitmap crosshair image for player 1, used when bit-
                            mapped crosshairs are enabled
                            ('--crosshair-style=bmp').
    Assets/p2crosshair.bmp  Bitmap crosshair image for player 2.
    Config/                 Directory where required configuration files are
                            stored.
    Config/Supermodel.ini   Configuration file containing default input
                            settings.
    Config/Games.xml        Game and ROM set definitions.
    NVRAM/                  Directory where NVRAM contents will be saved.
    ROMs/                   Directory conveniently included (but not required)
                            for placing ROM sets.
    Saves/                  Directory where save states will be saved.

Supermodel requires OpenGL 2.1 and a substantial amount of both video and system
memory.  A modern 64-bit CPU and a mid range GPU are the minimum recommendations
to achieve consistent frame rates of 60 FPS.


=========================
  2. Running Supermodel
=========================

For now, Supermodel does not include a proper user interface.  It is operated
entirely from the command line.  Run 'supermodel' without any command line
arguments for an explanation of supported options.

Supermodel uses MAME-compatible ROM sets.  It loads from ZIP archives based on
file checksums and automatically detects games; therefore, file names are not
important.  Only one ZIP file can be specified on the command line. For
example:

    supermodel scud.zip -fullscreen

This will load 'scud.zip' (Scud Race) and run it in full screen mode.

Initially, inputs are assigned according to the settings in 'Supermodel.ini',
located in the 'Config' subdirectory.

Note that there is no user interface and all messages are printed to the
command prompt.  In full screen mode, they will not be visible.


=========================
  3. Game Compatibility
=========================

Supermodel recognizes all known Model 3 ROM sets. Below is a compatibility
matrix.  Other than minor graphical issues, if any, major issues are reported
below:

   ROM Set                    Title                           Comments
+------------+-----------------------------------------+-----------------------+
| bassdx     | Sega Bass Fishing (USA)                 |                       |
|  getbassdx | Get Bass: Sega Bass Fishing (Deluxe)    |                       |
|  getbassur | Get Bass: Sega Bass Fishing (Upright)   |                       |
|  getbass   | Get Bass: Sega Bass Fishing             |                       |
+------------+-----------------------------------------+-----------------------+
| daytona2   | Daytona USA 2 Battle on the Edge        |                       |
|  dayto2pe  | Daytona USA 2 Power Edition             |                       |
+------------+-----------------------------------------+-----------------------+
| dirtdvls   | Dirt Devils (Export, Revision A)        |                       |
|  dirtdvlsu | Dirt Devils (USA, Revision A)           |                       |
|  dirtdvlsj | Dirt Devils (Japan, Revision A)         |                       |
|  dirtdvlsau| Dirt Devils (Australia, Revision A)     |                       |
|  dirtdvlsg | Dirt Devils (Export, Ver. G?)           |                       |
+------------+-----------------------------------------+-----------------------+
| eca        | Emergency Call Ambulance (Export)       |                       |
|  ecau      | Emergency Call Ambulance (USA)          |                       |
|  ecaj      | Emergency Call Ambulance (Japan)        |                       |
|  ecap      | Emergency Call Ambulance (US proto?)    |                       |
+------------+-----------------------------------------+-----------------------+
| fvipers2   | Fighting Vipers 2 (Japan, Revision A)   |                       |
|  fvipers2o | Fighting Vipers 2 (Japan)               |                       |
+------------+-----------------------------------------+-----------------------+
| harley     | Harley Davidson & L.A. Riders (Rev. B)  |                       |
|  harleya   | Harley Davidson & L.A. Riders (Rev. A)  |                       |
+------------+-----------------------------------------+-----------------------+
| lamachin   | L.A. Machineguns: Rage of the Machines  |                       |
+------------+-----------------------------------------+-----------------------+
| lemans24   | Le Mans 24 (Japan, Revision B)          |                       |
+------------+-----------------------------------------+-----------------------+
| lostwsga   | The Lost World Arcade (Japan, Rev A)    |                       |
|  lostwsgp  | The Lost World Aracde (Location Test)   |                       |
+------------+-----------------------------------------+-----------------------+
| magtruck   | Magical Truck Adventure (Japan)         |                       |
|  mgtrkbad  | Magical Truck Adventure (bit rot dump)  |                       |
+------------+-----------------------------------------+-----------------------+
| oceanhun   | The Ocean Hunter (Japan, Revision A)    |                       |
|  oceanhuna | The Ocean Hunter (Japan)                |                       |
+------------+-----------------------------------------+-----------------------+
| scud       | Scud Race (Export, Twin/DX)             |                       |
|  scudau    | Scud Race (Australia, Twin/DX)          |                       |
|  scuddx    | Scud Race (Export, Deluxe, Revision A)  |                       |
|  scudplus  | Scud Race (Export, Twin/DX, Revision A) |                       |
|  scudplusa | Scud Race (Export, Twin/DX)             |                       |
+------------+-----------------------------------------+-----------------------+
| skichamp   | Ski Champ (Japan)                       |                       |
+------------+-----------------------------------------+-----------------------+
| spikeofe   | Spikeout Final Edition (Export)         |                       |
+------------+-----------------------------------------+-----------------------+
| spikeout   | Spikeout (Export, Revision C)           |                       |
+------------+-----------------------------------------+-----------------------+
| srally2    | Sega Rally 2 (Export)                   |                       |
|  srally2p  | Sega Rally 2 (Prototype)                | proto is non working  |
|  srally2pa | Sega Rally 2 (Prototype Version A)      | proto is non working  |
|  srally2dx | Sega Rally 2 (Export, Deluxe)           |  DX set may run slow  |
+------------+-----------------------------------------+-----------------------+
| swtrilgy   | Star Wars Trilogy (Export, Revision A)  |   see note 1 below    |
|  swtrilgya | Star Wars Trilogy (Export)              |   see note 1 below    |
|  swtrilgyp | Star Wars Trilogy (Location Test)       |   see note 2 below    |
+------------+-----------------------------------------+-----------------------+
| vf3        | Virtua Fighter 3 (Japan, Revision D)    |                       |
|  vf3c      | Virtua Fighter 3 (Japan, Revision C)    |                       |
|  vf3a      | Virtua Fighter 3 (Japan, Revision A)    |                       |
|  vf3tb     | Virtua Fighter 3 Team Battle (Japan)    |                       |
+------------+-----------------------------------------+-----------------------+
| von2       | Virtual On Oratorio Tangram (Revision B)|                       |
|  von2a     | Virtual On Oratorio Tangram (Revision A)|                       |
|  von2o     | Virtual On Oratorio Tangram             |                       |
|  von254g   | Virtual On Oratorio Tangram (Ver. 5.4g) |                       |
+------------+-----------------------------------------+-----------------------+
| vs2        | Virtua Striker 2 (Step 2.0, Export, USA)|                       |
|  vs215     | Virtua Striker 2 (Step 1.5, Export, USA)|                       |
|  vs215o    | Virtua Striker 2 (Step 1.5, Japan test?)|                       |
+------------+-----------------------------------------+-----------------------+
| vs298      | Virtua Striker 2 '98 (Step 2.0, Japan)  |                       |
|  vs29815   | Virtua Striker 2 '98 (Step 1.5, Japan)  |                       |
+------------+-----------------------------------------+------------+----------+
| vs2v991    | Virtua Striker 2 '99.1 (Step 2.1 Export, USA, Rev B) |          |
|  vs299a    | Virtua Striker 2 '99   (Step 2.1 Export, USA, Rev A) |          |
|  vs299     | Virtua Striker 2 '99   (Step 2.1 Export, USA)        |          |
|  vs299j    | Virtua Striker 2 '99.1 (Step 2.1 Japan, Rev B)       |          |
|  vs29915   | Virtua Striker 2 '99.1 (Step 1.5 Export, USA, Rev B) |          |
|  vs29915a  | Virtua Striker 2 '99   (Step 1.5 Export, USA)        |          |
|  vs29915j  | Virtua Striker 2 '99.1 (Step 1.5 Japan, Rev B)       |          |
+------------+------------------------------------------------------+----------+

Note 1: Set game to U/R in game assignment in the TEST MENU to bypass the
        "WAIT SETUP THE FEEDBACK STICK" screen
Note 2: Set game to SD in game assignment in the TEST MENU or wait a few seconds
        for the game to continue past the "WAIT SETUP THE FEEDBACK STICK" screen


=====================
  4. Video Settings
=====================

Supermodel may be run in either windowed (default) or full screen mode.  By
default, it automatically adjusts the display area to retain the aspect ratio of
the Model 3's native (and Supermodel's default) resolution, 496x384, introducing
letterboxing (black borders) as needed.

To stretch the image to fill the display, use the '-stretch' option.

Wide aspect ratios that extend the field of view horizontally are supported by
using the '-wide-screen' option.  This works well for most common aspect ratio
and additional geometry will be visible but at extremely wide aspect ratios,
objects will be culled by games, which are not aware of the extended field of
view.  Tile-based 2D background layers will not be stretched and will be
letterboxed.

To stretch the 2D background layers, use '-wide-bg'.  Many games use 2D back-
grounds instead of 3D skyboxes and when playing in wide screen mode, this option
often enhances their appearance.

By default, Supermodel limits the frame rate to 60 frames per second.  This has
no impact on performance except when the frame rate exceeds 60 FPS on fast
systems.  Frame rate limiting can be disabled with the '-no-throttle' option.
Some video drivers may continue to lock to the refresh rate.

The actual Model 3 refresh rate is 57.524 Hz.  The '-true-hz' option will switch
to this rate.

To change the resolution, use the '-res' command line option.  For full screen
mode, use '-fullscreen'.  For example, to set a full screen 1920x1080 mode,
try:

    supermodel game.zip -res=1920,1080 -fullscreen

Video settings may also be specified globally or on a per-game basis in the
configuration file, described elsewhere in this manual.

Changing video modes at run-time is not yet supported.


=====================
  5. Audio Settings
=====================

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
but will prevent the audio co-processors from being emulated altogether and
may slightly improve performance, especially on single-core systems.  Save
states generated with these settings may not be able to properly restore audio
when loaded after emulation is re-enabled.

Audio settings may also be specified globally or on a per-game basis in the
configuration file, described elsewhere in this document.

Please keep in mind that MPEG music emulation is preliminary and the decoder is
buggy.  Periodic squeaks and pops occur on many music tracks.  The Sega Custom
Sound Processor (SCSP) emulator, used for sound emulation, is also still quite
buggy.  Sound glitches are known to occur now and then, and many sounds and
tunes do not sound quite correct yet.


===============
  6. Controls
===============

Game controls are fully configurable and can be mapped to keyboards, mice, and
game controllers.  Emulator functions, on the other hand, cannot be changed and
are listed below.

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
current configuration, use the '-print-inputs' command line option.

    supermodel -print-inputs

If you delete 'Supermodel.ini', all inputs will become unmapped and will have
to be reconfigured!

To reconfigure the inputs, use the '-config-inputs' option.  A blank window
will open up and instructions will be printed to the command prompt.  Read them
carefully!  Pressing the Escape key exits without saving changes.  Pressing 'q'
will save changes to 'Supermodel.ini'.  You can choose which input to configure
using the Up and Down arrow keys.  Clearing an input means it will be unusable.

If the configuration dialog is not responding to your key presses or mouse and
joystick movements, make sure that the blank pop-up window rather than the
command prompt is selected.  This may seem counter-intuitive but inputs are
captured by the graphical window while messages are printed to the command
prompt.


Calibrating Controls
--------------------

Some controls, in particular the accelerator and brake pedals of steering
wheels, may need calibrating before they will function properly with
Supermodel.

In order to calibrate a control, press 'b' when configuring the inputs and
follow the on-screen instructions carefully.

NOTE: After calibration, you must remap the control again or it will not
function properly!

Several input-related settings are stored in the configuration file.  Refer to
the section discussing the configuration file for more details.


XBox 360 Controllers
--------------------

For full XBox 360 controller support, the XInput system must be used on Windows
('-input-system=xinput').  Otherwise, the left and right trigger buttons cannot
be mapped individually and force feedback will not work.  Please read the
section titled 'Input Systems', further below.

Mac OS X users can use Colin Munro's XBox 360 controller driver:
http://tattiebogle.net/index.php/ProjectRoot/Xbox360Controller/OsxDriver


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
inwards are for jumping and falling back down to the ground, respectively.

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
the trigger.  For easier reloading, the 'InputAutoTrigger' setting can be
enabled in the configuration file (described elsewhere in this manual).

Crosshairs will be visible in full screen mode and can also be enabled in
windowed modes.  Use Alt-I to cycle through different crosshair options (enable
for a single player, both, or none).

For multiple mouse support, allowing two mice or PC light guns to be mapped,
Raw Input must be used.  This is supported only on Windows and is described
below.

For specific information on using Sinden Light guns with Supermodel, refer to
the Sinden Light gun Wiki:

https://www.sindenwiki.org/wiki/Supermodel_M3


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
      XBox 360 controllers, otherwise some buttons will not function properly
      and force feedback will not work.
    - Raw Input.  Selected with '-input-system=rawinput'.  This is intended for
      use with multiple mice and keyboards but is not recommended otherwise.
    - SDL.  Selected with '-input-system=sdl'.  The standard, cross-platform
      input system intended for non-Windows builds.  It is accessible on
      Windows but does not provide full support for all devices.

When switching input systems with '-input-system', you must also configure your
inputs using the same option.  For example, when running Supermodel with XInput
('supermodel game.zip -input-system=xinput'), you must also configure with
XInput ('supermodel -config-inputs -input-system=xinput').  Many settings are
not compatible between input systems.

A common mistake is to configure inputs using one system and then launch
Supermodel with another.


Troubleshooting
---------------

Common input-related problems are discussed below.


    Problem:
        No response while configuring inputs.

    Solution:
        Make sure the main window is top window, especially with the SDL input
        system.

    ---------

    Problem:
        An attached controller is not recognized at all.

    Solution:
        - On the input configuration screen, press 'I' to see information about
          the input system and check that the required controller is in the
          list.
        - If it is not in the list, check that it is plugged in and visible in
          the operating system.  If a controller is plugged in while Supermodel
          is running, Supermodel will need to be restarted in order for it to
          recognize the new controller.
        - On Windows, if after checking the above points the controller is
          still not in the list, try running Supermodel with the SDL input
          system by specifying '-input-system=sdl' on the command line.

    ---------

    Problem:
        Unable to select a particular joystick axis when configuring a mapping.

    Solution:
        This may mean that the joystick axis needs calibrating.  This can be
        done by pressing 'b' on the configuration screen and following the
        instructions.

    ---------

    Problem:
        Steering wheel pedals are not being recognized.

    Solution:
        For some steering wheels where the pedals have been configured in a
        'split axis' mode (ie. not sharing a combined axis), the values that
        the pedals send are inverted.  This means that without configuration,
        Supermodel will be unable to recognize them.  To fix this, they should
        be calibrated on the calibration screen in the same way as above.

    ---------

    Problem:
        A control isn't working despite having been calibrated.

    Solution:
        After calibration, while still in the configuration dialog, you must
        remap the control again for the new settings to take effect.

    ---------

    Problem:
        XBox 360 controller trigger buttons cannot be triggered simultaneously
        or no rumble effects.

    Solution:
        On Windows, make sure that you specify '-input-system=xinput'.


=====================
  7. Force Feedback
=====================

Force feedback is presently supported in 'Scud Race' (including 'Scud Race
Plus'), 'Daytona USA 2' (both editions), and 'Sega Rally 2' on Windows only. To
enable it, use the '-force-feedback' option.

Drive board ROMs are required.  They first appear in the MAME 0.143u6 ROM
catalog and at the time of this writing, have not yet widely proliferated.

    Game            Drive Board ROM File    Size        Checksum (CRC32)
    ----            --------------------    ----        ----------------
    Daytona USA 2   epr-20985.bin           64 KB       B139481D
    Scud Race       epr-19338a.bin          64 KB       C9FAC464
    Sega Rally 2    epr-20512.bin           64 KB       CF64350D

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

There are four DirectInput effects: constant force, self centering, friction,
and vibration.  The strength of each can be tuned with the following settings:

    DirectInputConstForceMax = 100
    DirectInputSelfCenterMax = 100
    DirectInputFrictionMax = 100
    DirectInputVibrateMax = 100

They are given as percentages and represent the maximum strength for each
effect.  A setting of 0 disables them entirely.  Values above 100% are
accepted but may clip or distort the effect.  By default, they are set to 100%,
as shown above.

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
will require calibration by the user on a game-by-game basis to achieve the
best feel.


============================
  8. Save States and NVRAM
============================

Save states are saved and restored by pressing F5 and F7, respectively.  Up to
10 different save slots can be selected with F6.  All files are written to the
Saves/ directory, which must exist beforehand.  If you extracted the Supermodel
ZIP file correctly, it will have been created automatically.

If a Model 3 co-processor (ie. sound board, DSB, drive board) is disabled when
a save state is taken, it will not resume normal operation when the state is
loaded, even if Supermodel is running with the co-processor re-enabled.  The
drive board (and consequently, force feedback effects) is explicitly disabled
for the remainder of the session if a save state with an inactive drive board
is loaded.  Audio co-processors are not, and therefore audio playback may
eventually resume after the audio boards have booted themselves up.

Non-volatile memory (NVRAM) consists of battery-backed backup RAM and an EEPROM
chip.  The former is used for high score data and statistics whereas the latter
stores machine settings (often accessed using the Test buttons).  NVRAM is
automatically saved each time Supermodel exits and is loaded at start-up.  It
can be cleared by deleting the NVRAM files or pressing Alt-N.


=======================================
  9. Game-Specific Comments and Tips
=======================================

This section contains additional game-specific setup information, workarounds
for emulation issues, and region codes for changing the region.


Cyber Troopers Virtual-On Oratorio Tangram
------------------------------------------

To change the region, enter Test mode and press Start, Start, Service, Start,
Start, Start, Service, Service, Test.


Daytona USA 2 and Daytona USA 2 Power Edition
---------------------------------------------

By default, the 'Power Edition' ROM set features remixed music lyrics by
Takenobu Mitsuyoshi.  These can be changed back to the Dennis St. James version
in the Test Menu, under 'Game Assignments'.

The region change menu can be accessed in two different ways:

  1. Enter Test mode and push the Service and Test buttons simultaneously.
  2. Enter Test mode. Then, hold Start and press VR4, VR4, VR2, VR3, VR1, VR3,
     VR2.


Dirt Devils
-----------

To access the region change menu, enter Test mode and press Start, Start,
Service, Start, Start, Start, Service, Test.


Emergency Call Ambulance
------------------------

To play as the paramedics pushing a stretcher, highlight 'Manual' at the
transmission select screen and then perform the following sequences of gear
shifts: Shift Up, Shift Up, Shift Down, Shift Down, Shift Up.

Supermodel presently only emulates a sequential shifter but cabinets with a 4-
way H-type shifter exist and the shift sequence on this units is: 3, 3, 4, 4, 3
or 1, 1, 2, 2, 1.


Fighting Vipers 2
-----------------

To change the region, in the 'Game Assignments' screen in Test mode, set the
cursor at the 'Country' line and press: Left, Left, Left, Right, Right, Left.
The country can then be changed using the Test switch.


Get Bass and Sega Bass Fishing
------------------------------

To change the region, enter Test mode and go to the second screen of the CRT
test ('C.R.T. Test 2/2'). Press the Service button four times and then exit the
CRT test.  Next, enter 'Game Assignments', press Service three times, then press
and hold Service and while holding, press Test.  The region select menu will
appear.


Harley-Davidson and L.A. Riders
-------------------------------

To change the region, enter Test mode and in the 'Game Assignments' screen,
press Shift Up, Shift Up, Shift Down, Shift Down, View, Music, View, Music.


L.A. Machineguns
----------------

To change the region, enter Test mode and press P1 Start, P1 Start, Service, P1
Start, P1 Start, P1 Start, Service, Test.


Le Mans 24
----------

To change the region, enter Test mode and press Start, Start, Service, Service,
Start, Test.


Magical Truck Adventure
-----------------------

To change the region, enter Test mode and press P1 Start, P1 Start, Service, P1
Start, Service, Test.


Sega Rally 2
------------

As with 'Star Wars Trilogy', you may experience problems if you attempt to
start a game before any 3D graphics are displayed (for example, during the Sega
logo).

To change the region, enter Test mode and perform the following series of short
(press and release) and long (press and hold for 4-5 seconds) Service button
presses: 4 short, 2 long, 2 short, 1 long.


Ski Champ
---------

To change the region, enter Test mode and press Select 1, Select 3, Select 1,
Select 3, Service, Service.


Spikeout
--------

To change the region, enter Test mode, place the cursor on the 'Game
Assignments' line and press Charge, Start, Jump, Start, Start, Start, Shift,
Start, Start.  The region change menu will be revealed.


Spikeout Final Edition
----------------------

To change the region, enter Test mode, place the cursor on the
'Game Assignments' line, and while holding the Service button press Jump, Start,
Jump, Start.  Then, release the Service button and press Shift, Start, Start,
Start, Charge, Start, Start, Shift, Start, Test.  The region change menu will be
revealed.


Star Wars Trilogy
-----------------

Inserting coins and starting a game before any 3D graphics have been displayed
in attract mode will result in a PowerPC crash before the stage loads.  This is
caused by an unknown emulation bug.  Simply wait until the Darth Vader sequence
appears before attempting to start a game.

If 'Star Wars Trilogy' is booting directly into the stage select screen, it is
probably because you exited Supermodel with credits still in the machine.  Clear
the NVRAM (Alt-N) and reset the game (Alt-R).

To change the region, enter Test mode and perform the following series of short
(press and release) and long (press and hold for 4-5 seconds) Service button
presses: 3 short, 2 long, 2 short, 1 long.


The Lost World
--------------

To reload, the light gun must be pointed off-screen by pressing (and holding)
the 'off-screen' button and, simultaneously, pressing the trigger to shoot.
This behavior can be changed with the 'InputAutoTrigger' setting in the
configuration file.

To change the region, enter Test mode and press P1 Start, P1 Start, Service, P1
Start, Service, Test.


The Ocean Hunter
----------------

To change the region, enter Test mode and in 'Game Assignments', press P1 Start,
P2 Start, P1 Start, P2 Start, P1 Start, P2 Start, P2 Start.


Virtua Striker 2 '98
--------------------

To change the region, enter Test mode and in 'Game Assignments', perform the
following series of short (press and release) and long (press and hold for 4-5
seconds) Service button presses: 1 long, 3 short, 1 long.


==============================
  10. The Configuration File
==============================

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

NOTE: Type carefully!  Supermodel will not report syntax errors nor detect
typos.  Carefully read the discussion of the file syntax below.  To verify that
your intended settings are taking effect, check the 'error.log' file that is
produced by Supermodel during each run.


File Structure and Syntax
-------------------------

The configuration file is a list of settings grouped by sections.  All setting
names and their arguments are case sensitive.  They take the form:

    Name = Argument

Only one setting per line is allowed.  Only two types of arguments are allowed:
integers and strings.  The choice of which to use is determined by the setting.
Integers can be negative.  Strings can contain any characters and are enclosed
by double quotes.  Examples:

    IntegerSetting1 = 0
    IntegerSetting2 = 65536
    IntegerSetting3 = -32768
    StringSetting1 = "This is a string."
    StringSetting2 = "123"
    StringSetting3 = "1+1, abc, these are all valid characters!"

Many settings are simply boolean variables expecting an integer value of either
0 (disabled) or 1 (enabled).  Some settings require values and others take
strings for file names or input mappings.

Section names appear in between square brackets on their own lines.

    [ SectionName ]

Settings that appear at the beginning of the file without a preceding section
are automatically assigned to 'Global'.

Comments begin with a semicolon and extend until the end of the line.

    ; This is a comment.


Global and Game-Specific Sections
---------------------------------

Sections determine whether settings are applied globally, to all games, or to
specific games.  Game-specific settings will override global settings and can
be used to tune Supermodel on a game-by-game basis.  Global settings must be
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

    PowerPCFrequency = 25   ; run PowerPC at 25 MHz

In this example, only 'Scud Race' will run at 1920x1080.  All other games will
use 1024x768.  'Scud Race' will also have altered volume settings.  'The Lost
World' will be run with a lower PowerPC frequency but all other games will use
the default.


Input Mappings
--------------

Input mappings are specified with strings that have their own internal syntax.
They may only be specified in the 'Global' section and are ignored elsewhere.

A mapping may be specified in one of the following three ways:

    KEY_XXX     For a key XXX on a keyboard (eg. KEY_A, KEY_SPACE, KEY_ALT).

    MOUSE_XXX   For a mouse axis (X or Y), wheel, or button (1-5) (eg.
                MOUSE_XAXIS, MOUSE_WHEEL_UP, MOUSE_BUTTON3).

    JOY1_XXX    For a joystick axis (X, Y, Z, RX, RY, RZ), POV controller
    JOY2_XXX    (1-4), or button (1-12) (e.g. JOY1_XAXIS, JOY3_POV2_UP,
    ...         JOY2_BUTTON4).

Joysticks, including game pads and steering wheels, are numbered from 1 on up.
Their ordering is system dependent and if controllers are swapped between
invocations of Supermodel, then the ordering may change.  If the number is
omitted then the mapping applies to all joysticks, eg. JOY_BUTTON1 means button
1 on any attached joystick.

In addition to the above, when using the Raw Input system on Windows (command
line option '-input-system=rawinput') it is possible to refer to a particular
mouse or keyboard by including its number in the mapping, eg. KEY2_A or
MOUSE3_XAXIS.  This allows the independent mapping of dual mice or dual light
guns (which present themselves as mice to Supermodel) for 'The Lost World'.

Mappings may be combined with a plus '+' or a comma ',' and negated with an
exclamation mark '!'.  The plus signifies that both mappings must be active to
trigger the input and the comma means that just one needs to be active.  The
exclamation mark means that a mapping must not be active.

Of the above the comma is the most useful.  It allows the mapping of several
different control methods to a single input.  For example, both a keyboard and
a joystick could be mapped to the same input.  When combining mappings in this
way, the ordering is important as mappings earlier in the list will take
precedence over later ones (see example 3 below).

When an axis is mapped to a digital input, its direction must be qualified with
a _POS or _NEG after the axis name.  For example, JOY1_XAXIS_NEG indicates that
joystick 1 must be pushed left (negative direction) in order to activate the
input.  Additionally, the following shortcuts are provided for some common
joystick axis directions:

    JOY1_LEFT  is the same as JOY1_XAXIS_NEG
    JOY1_RIGHT is the same as JOY1_XAXIS_POS
    JOY1_UP    is the same as JOY1_YAXIS_NEG
    JOY1_DOWN  is the same as JOY1_YAXIS_POS

Another axis manipulator is _INV.  This inverts an axis when it is mapped to an
analog input.  JOY1_YAXIS_INV could be used for example to invert the y-axis
of joystick 1 in Star Wars Trilogy so that it acts like a plane's yoke.

Examples:

    1. InputLeft = "KEY_LEFT,JOY1_LEFT"

        Digital input InputLeft will be triggered whenever the left arrow key
        is pressed or joystick 1 is pushed left.

    2. InputStart1 = "KEY_ALT+KEY_S"

        Both Alt and S must be pressed at the same time in order to trigger the
        digital input InputStart1 (player 1 Start button).

    3. InputSteering = "JOY1_XAXIS,MOUSE_XAXIS"

        Analog steering can be controlled with either the joystick or by moving
        the mouse.  Due to the ordering used, the joystick will take preference
        and so the mouse can only control the steering when the joystick is
        released.

The complete list of input mappings can be found in the settings index below or
by generating a configuration file using '-config-inputs'.


Controller Settings
-------------------

Several settings are available to fine tune the way Supermodel handles
controllers.


Keyboard Settings:

The rate at which analog controls' values increase or decrease when controlled
by a key can be set with the InputyKeySensitivity option.  It takes a value in
the range 1-100, with 100 being the most sensitive (fastest response) and 1
being the least sensitive (slowest response).  The default value is 25.

The speed with which analog controls return to their rest or 'off' values after
a key has been released is configured with InputKeyDecaySpeed.  This takes a
value in the range 1-200 and is expressed as a percentage of the attack speed.
The default setting is 50, which means that analog controls take twice as long
to return to their off values as they do to reach their maximum and minimum
values.


Mouse Settings:

For a mouse it is possible to specify a rectangular area in the center of
Supermodel's display within which the mouse is considered to be inactive.  The
width and height of this zone is set with the options 'InputMouseXDeadZone' and
'InputMouseYDeadZone'.  They are expressed as a percentage 0-99 of the width
and height of the display.  The default value is 0 which disables this option.


Joystick Options:

Each joystick axis has a set of parameters that can be configured and these are
given below.  The joystick number and axis name (X, Y, Z, RX, RY, or RZ) must
be substituted in for '**' like so: InputJoy1XDeadZone.  If the joystick number
is omitted, then the option becomes the default for all joysticks, eg.
InputJoyYSaturation.

InputJoy**Deadzone: The dead zone of a joystick axis is the size of the zone
around its central or 'off' position within which the axis is considered to be
inactive.  It is important to specify this since joysticks rarely return to
their perfect center when released.  The dead zone is expressed as a percentage
0-99 of the total range of the axis and the default value is 2%.

InputJoy**Saturation: The saturation of a joystick axis is the point at which
the axis is considered to be at its most extreme position.  It can be thought
of as a measure of the sensitivity of the axis.  Like the dead zone, it is
expressed as a percentage of the axis range but its value may be larger than
100, up to a maximum of 200.  A value of 50 means that the joystick only needs
to be moved halfway in order for Supermodel to see it as fully extended.
Conversely a value of 200 means that when the joystick is at its extreme
position Supermodel will see it as only halfway.  The default sensitivity is
100%, which corresponds to a 1-to-1 mapping.  For playing driving games with a
game pad, it is sometimes a good idea to use a value larger than 100% so that
the steering feels less sensitive on a thumbstick.

InputJoy**MinVal, InputJoy**OffVal, InputJoy**MaxVal: Normally a joystick axis
will send values to Supermodel that fall within the range -32768 to +32767,
with 0 representing the central or 'off' position.  However, in some cases an
axis may behave differently.  This has been observed with certain steering
wheels that have their pedals configured in a 'split axis' mode.  The pedals
invert their range so that they send -32768 when released and +32767 when fully
depressed.  In order for Supermodel to be able to handle such cases the
minimum, off, and maximum values of the axis can be specified with these three
options.  The default values are MinVal -32768, OffVal 0, and MaxVal 32767 but
with the pedal example just given, they would need to be set to MinVal 32767,
OffVal 32767, and MaxVal -32768.

In most cases it will be unnecessary to alter the DeadZone, MinVal, OffVal, and
MaxVal options by hand as calibrating a joystick axis from within the input
configuration screens of Supermodel will set them automatically.  Only the
sensitivity option is not configured in this way.


Additional Options:

InputAutoTrigger, InputAutoTrigger2: When playing light gun games such as 'The
Lost World', the gun must be pointed off-screen and then fired in order to
reload.  With a mouse and the default mappings, this requires clicking both the
right and left mouse buttons.  In order to be able to reload the gun with just
a single click of the right mouse button, enable these options for players 1
and 2 by setting them to 1.  By default they are disabled (value 0).

Enabling these options is also recommended when playing with PC light guns as
these often map the action "point off screen and press trigger" to a single
right mouse button click.  Supermodel has been tested successfully with
UltiMarc's AimTrak light-guns.  In order to use dual PC light-guns for two
player games, the Raw Input system must be selected with the command line
option '-input-system=rawinput' and the input mappings configured for each gun.


=====================================
  11. Index of Command Line Options
=====================================

All valid command line settings are listed here, ordered by category.  Defaults
are given under the assumption that they have not been changed in the
configuration file.

Square brackets ('[' and ']') indicate optional parameters.  Angled brackets
('<' and '>') indicate mandatory parameters.  Do not type the brackets.  No
spaces may appear inside of an option.  For example, 'supermodel game.zip
-ppc-frequency=25' is correct but 'supermodel game.zip -ppc-frequency = 25' is
not.  All options are case sensitive.


    Option:         -?
                    -h

    Description:    Prints a message with an overview of the command line
                    options and how to run Supermodel.

    ----------------

    Option:         -print-games

    Description:    Lists all supported ROM sets.  Not all supported ROM sets
                    are playable yet.

    ----------------

    Option:         -no-threads

    Description:    Disables multi-threading.  When enabled (the default), the
                    PowerPC, graphics rendering, and user interface are run in
                    one thread, sound emulation in a second thread, and the
                    drive board in a third.  It is enabled by default.  On
                    single-core systems, multi-threading adds overhead and
                    slows Supermodel down but can make audio sound a bit
                    smoother.

    ----------------

    Option:         -ppc-frequency=<f>

    Description:    Sets the PowerPC frequency in MHz.  The default is 50.
                    Higher frequencies will allow games to do more processing
                    per frame, making them run faster in 'virtual' time, but
                    may slow down Supermodel on weaker computers.  Supermodel's
                    performance can frequently be improved by lowering the
                    PowerPC frequency (25 MHz works in many games).  In some
                    games, this will cause timing problems and jerky
                    performance.  The optimum frequency will vary from game to
                    game and system to system.  Valid values for <f> are 1 to
                    1000.

    ----------------

    Option:         -fullscreen

    Description:    Runs in full screen mode.  The default is to run in a
                    window.

    ----------------

    Option:         -no-throttle

    Description:    Disables 60 FPS throttling.  The Model 3 runs at a 60 Hz
                    refresh rate, which Supermodel enforces if this option is
                    omitted.  Unthrottled operation may not work on some
                    systems because graphics drivers may lock the refresh rate
                    on their own.

    ----------------

    Option:         -print-gl-info

    Description:    Prints OpenGL driver information and quits.

    ----------------

    Option:         -res=<x>,<y>

    Description:    Resolution of the display in pixels, with <x> being width
                    and <y> being height.  The default is 496x384, the Model
                    3's native resolution.

    ----------------

    Option:         -show-fps

    Description:    Shows the frame rate in the window title bar.

    ----------------

    Option:         -frag-shader=<file>
                    -vert-shader=<file>

    Description:    Allows external fragment and vertex shaders to be used for
                    3D rendering.  <file> is the file path.  Files should be
                    ASCII text files containing GLSL source code.  The file
                    extension is not important.  By default, Supermodel's
                    internal shader programs are used.  These options are
                    intended for future extensibility and for use by
                    developers.

    ----------------

    Option:         -flip-stereo

    Description:    Swaps the left and right audio channels.

    ----------------

    Option:         -no-dsb

    Description:    Disables Digital Sound Board (MPEG music) emulation.  See
                    the section on audio settings for more information.

    ----------------

    Option:         -no-sound

    Description:    Disables sound board (sound effects) emulation.  See the
                    section on audio settings for more information.

    ----------------

    Option:         -music-volume=<v>
                    -sound-volume=<v>

    Description:    Specifies the volume of MPEG music produced by the Digital
                    Sound Board and the audio produced by the sound board in
                    percent.  The default is 100, which is full volume, and the
                    valid range of <v> is 0 (muted) to 200.  See the section on
                    audio settings for more information.

    ----------------

    Option:         -config-inputs

    Description:    Launches the interactive input configuration utility in the
                    command prompt window.  Allows controls to be remapped.
                    See the section on controls for more information.

    ----------------

    Option:         -force-feedback

    Description:    Enables force feedback.  Force feedback is only supported
                    in a few games (see the section on force feedback for more
                    details).  Available only on Windows.

    ----------------

    Option:         -input-system=<s>

    Description:    Sets the input system.  This is only available on Windows,
                    where the default is 'dinput' (DirectInput).  SDL is used
                    for all other platforms.  Valid choices for <s> are:

                        dinput      DirectInput.
                        xinput      XInput
                        rawinput    Raw Input.
                        sdl         SDL.

                    See the section on input systems for more details.

    ----------------

    Option:         -print-inputs

    Description:    Prints the current input configuration.


============================================
  12. Index of Configuration File Settings
============================================

All valid configuration file settings are listed here, ordered by category.
Please read the section describing the configuration file for more information.

All settings are case sensitive.


    Name:           MultiThreaded

    Argument:       Integer.

    Description:    If set to 1, enables multi-threading; if set to 0, runs all
                    emulation in a single thread.  Read the description of the
                    '-no-threads' command line option for more information.

    ----------------

    Name:           PowerPCFrequency

    Argument:       Integer.

    Description:    The PowerPC frequency in MHz.  The default is 50.
                    Equivalent to the '-ppc-frequency' command line option.

    ----------------

    Name:           FullScreen

    Argument:       Integer.

    Description:    If set to 1, runs in full screen mode; if set to 0, runs in
                    a window.  Disabled by default.  Equivalent to the
                    '-fullscreen' command line option.

    ----------------

    Name:           ShowFrameRate

    Argument:       Integer.

    Description:    Shows the frame rate in the window title bar when set to 1.
                    If set to 0, the frame rate is not computed.  Disabled by
                    default.  Equivalent to the '-show-fps' command line
                    option.

    ----------------

    Name:           Throttle

    Argument:       Integer.

    Description:    Controls 60 FPS throttling.  It is enabled by setting to 1
                    and disabled by setting to 0.  For more information, read
                    the description of the '-no-throttle' command line option.

    ----------------

    Name:           XResolution
                    YResolution

    Argument:       Integer.

    Description:    Resolution of the display in pixels.  The default is
                    496x384, the Model 3's native resolution.  Equivalent to
                    the '-res' command line option.

    ----------------

    Name:           FragmentShader
                    VertexShader

    Argument:       String.

    Description:    Path to the external fragment or vertex shader file to use
                    for 3D  rendering.  By default, these are not set and the
                    internal default shaders are used.  These are equivalent to
                    the '-frag-shader' and '-vert-shader' command line options.

    ----------------

    Name:           EmulateDSB

    Argument:       Integer.

    Description:    Emulates the Digital Sound Board if set to 1, disables it
                    if set to 0.  See the section on audio settings for more
                    information.  A setting of 0 is equivalent to the
                    '-no-dsb' command line option.

    ----------------

    Name:           EmulateSound

    Argument:       Integer.

    Description:    Emulates the sound board and its two Sega Custom Sound
                    Processors if set to 1, disables it if set to 0.  See the
                    section on audio settings for more information.  A setting
                    of 0 is equivalent to the '-no-sound' command line option.

    ----------------

    Name:           FlipStereo

    Argument:       Integer.

    Description:    Swaps the left and right stereo channels if set to 1.  If
                    set to 0, outputs the channels normally.  Disabled by
                    default.  A setting of 1 is equivalent to using the
                    '-flip-stereo' command line option.

    ----------------

    Name:           MusicVolume
                    SoundVolume

    Argument:       Integer.

    Description:    Specifies the volume of MPEG music produced by the Digital
                    Sound Board and the audio produced by the sound board in
                    percent.  The default is 100, which is full volume, and the
                    valid range is 0 (muted) to 200%.  See the section on audio
                    settings for more information.  The equivalent command line
                    options are '-music-volume' and '-sound-volume'.

    ----------------

    Name:           ForceFeedback

    Argument:       Integer.

    Description:    If set to 1, enables force feedback emulation; if set to 0,
                    disables it (the default behavior).  Equivalent to the
                    '-force-feedback' command line option.  Available only on
                    Windows.

    ----------------

    Name:           DirectInputConstForceMax
                    DirectInputFrictionMax
                    DirectInputSelfCenterMax
                    DirectInputVibrateMax

    Argument:       Integer value.

    Description:    Sets strength of the four DirectInput force feedback
                    effects in percent.  Default is 100, indicating full
                    strength.  Values exceeding 100% will distort the effects.
                    Available only on Windows.

    ----------------

    Name:           XInputConstForceMax
                    XInputVibrateMax

    Argument:       Integer value.

    Description:    Sets strength of XInput force feedback effects in percent.
                    Default is 100, indicating full strength.  Values exceeding
                    100% will distort the effects.  The constant force effect
                    is simulated using vibration.  Available only on Windows.

    ----------------

    Name:           XInputConstForceThreshold

    Argument:       Integer value.

    Description:    Minimum strength above which a Model 3 constant force
                    command will be simulated on an XInput device.
                    XInputConstForceMax determines the vibration strength for
                    this effect.  The default value is 30.  Available only on
                    Windows.

    ----------------

    Names:          InputStart1
                    InputStart2
                    InputCoin1
                    InputCoin2
                    InputServiceA
                    InputServiceB
                    InputTestA
                    InputTestB

    Argument:       String.

    Description:    Mappings for common inputs: player 1 and 2 Start buttons,
                    both coin slots, and both Service and Test buttons.  Can
                    only be set in the 'Global' section.

    ----------------

    Names:          InputJoyDown
                    InputJoyDown2
                    InputJoyLeft
                    InputJoyLeft2
                    InputJoyRight
                    InputJoyRight2
                    InputJoyUp
                    InputJoyUp2

    Argument:       String.

    Description:    Mappings for 4-way digital joysticks (players 1 and 2).
                    These controls appear in 'Virtua Fighter 3', 'Fighting
                    Vipers 2', 'Spikeout', etc.  Can only be set in the
                    'Global' section.

    ----------------

    Names:          InputEscape
                    InputEscape2
                    InputGuard
                    InputGuard2
                    InputKick
                    InputKick2
                    InputPunch
                    InputPunch2

    Argument:       String.

    Description:    Mappings for fighting game buttons (players 1 and 2).  Can
                    only be set in the 'Global' section.

    ----------------

    Names:          InputBeat
                    InputCharge
                    InputJump
                    InputShift

    Argument:       String.

    Description:    Mappings for 'Spikeout' and 'Spikeout Final Edition'
                    buttons.  Can only be set in the 'Global' section.

    ----------------

    Name:           InputLongPass
                    InputLongPass2
                    InputShortPass
                    InputShortPass2
                    InputShoot
                    InputShoot2

    Argument:       String.

    Description:    Mappings for the 'Virtua Striker 2' series of games
                    (players 1 and 2).  Can only be set in the 'Global'
                    section.

    ----------------

    Name:           InputSteering

    Argument:       String.

    Description:    Mapping for the analog steering wheel axis, used in all
                    driving games.  Can only be set in the 'Global' section.

    ----------------

    Name:           InputSteeringLeft
                    InputSteeringRight

    Argument:       String.

    Description:    Mappings for digital control of the steering wheel,
                    intended for users who do not have an analog controller.
                    Can only be set in the 'Global' section.

    ----------------

    Name:           InputBrake
                    InputAccelerator

    Argument:       String.

    Description:    Mappings for brake and accelerator pedals used in driving
                    games.  These are analog axes but can also be mapped to
                    digital inputs if needed.  Can only be set in the 'Global'
                    section.

    ----------------

    Name:           InputGearShift1
                    InputGearShift2
                    InputGearShift3
                    InputGearShift4
                    InputGearShiftN

    Argument:       String.

    Description:    Mappings for the gear box used in driving games.  In games
                    with only two gears (e.g. 'Le Mans 24'), InputGearShift1
                    maps to 'Up' and InputGearShift2 to 'Down'.  A neutral
                    position can also be selected.  Can only be set in the
                    'Global' section.

    ----------------

    Name:           InputGearShiftDown
                    InputGearShiftUp

    Argument:       String.

    Description:    Mappings for sequential shifting.  These allow gears to be
                    selected sequentially: N, 1, 2, 3, 4.  Can only be set in
                    the 'Global' section.

    ----------------

    Name:           InputVR1
                    InputVR2
                    InputVR3
                    InputVR4

    Argument:       String.

    Description:    Mappings for the 'VR' (color-coded view select) buttons in
                    racing games (e.g. 'Scud Race', 'Daytona USA 2', etc.)  Can
                    only be set in the 'Global' section.

    ----------------

    Name:           InputViewChange

    Argument:       String.

    Description:    Mapping for the view change button used in 'Sega Rally 2',
                    'Dirt Devils', and 'Emergency Car Ambulance'.  Can only be
                    set in the 'Global' section.

    ----------------

    Name:           InputHandBrake

    Argument:       String.

    Description:    Mapping for the hand brake button used in 'Sega Rally 2'.
                    Can only be set in the 'Global' section.

    ----------------

    Name:           InputTwinJoyCrouch
                    InputTwinJoyForward
                    InputTwinJoyJump
                    InputTwinJoyReverse
                    InputTwinJoyStrafeLeft
                    InputTwinJoyStrafeRight
                    InputTwinJoyTurnLeft
                    InputTwinJoyTurnRight

    Argument:       String.

    Description:    Mappings for 'macro' commands for 'Virtual On Oratorio
                    Tangram'.  These allow movements to be mapped to single
                    digital inputs and do not require the use of two joysticks.
                    Can only be set in the 'Global' section.

    ----------------

    Name:           InputTwinJoyDown1
                    InputTwinJoyDown2
                    InputTwinJoyLeft1
                    InputTwinJoyLeft2
                    InputTwinJoyRight1
                    InputTwinJoyRight2
                    InputTwinJoyUp1
                    InputTwinJoyUp2

    Argument:       String.

    Description:    Mappings for the individual joysticks used by 'Virtual On
                    Oratorio Tangram'.  The left stick is stick #1 and the
                    right stick is #2.  Can only be set in the 'Global'
                    section.

    ----------------

    Name:           InputTwinJoyShot1
                    InputTwinJoyShot2
                    InputTwinJoyTurbo1
                    InputTwinJoyTurbo2

    Argument:       String.

    Description:    Mappings for the shot (trigger) and turbo buttons located
                    on the left (1) and right (2) joysticks in 'Virtual On
                    Oratorio Tangram'.  Can only be set in the 'Global'
                    section.

    ----------------

    Name:           InputAnalogJoyDown
                    InputAnalogJoyLeft
                    InputAnalogJoyRight
                    InputAnalogJoyUp

    Argument:       String.

    Description:    Mappings for digital control of the analog joystick used in
                    'Star Wars Trilogy'.  Two inputs are provided for the two
                    directions available on each axis.  Can only be set in the
                    'Global' section.

    ----------------

    Name:           InputAnalogJoyX
                    InputAnalogJoyY

    Argument:       String.

    Description:    Mappings for X and Y axes of the analog joystick in 'Star
                    Wars Trilogy'.  Can only be set in the 'Global' section.

    ----------------

    Name:           InputAnalogJoyTrigger
                    InputAnalogJoyEvent

    Argument:       String.

    Description:    Mappings for the trigger and Event button in 'Star Wars
                    Trilogy'.  Can only be set in the 'Global' section.

    ----------------

    Name:           InputGunDown
                    InputGunDown2
                    InputGunLeft
                    InputGunLeft2
                    InputGunRight
                    InputGunRight2
                    InputGunUp
                    InputGunUp2

    Argument:       String.

    Description:    Mappings for digital control of the light guns in 'The Lost
                    World'.  Inputs for motion in each direction are provided.
                    Can only be set in the 'Global' section.

    ----------------

    Name:           InputGunX
                    InputGunX2
                    InputGunY
                    InputGunY2

    Argument:       String.

    Description:    Mappings for both analog axes of the light guns in 'The
                    Lost World'.  Can only be set in the 'Global' section.

    ----------------

    Name:           InputOffscreen
                    InputOffscreen2
                    InputTrigger
                    InputTrigger2

    Argument:       String.

    Description:    Mappings for the light gun triggers and off-screen
                    controls in 'The Lost World'.  The off-screen button aims
                    the light gun off-screen while it is pressed, which makes
                    re-loading possible.  Can only be set in the 'Global'
                    section.

    ----------------

    Name:           InputAutoTrigger
                    InputAutoTrigger2

    Argument:       Integer.

    Description:    When set to 1, the off-screen button will also
                    automatically reload (no need to pull the trigger).  Can
                    only be set in the 'Global' section.

    ----------------

    Name:           InputKeySensitivity

    Argument:       Integer.

    Description:    The rate at which analog control values increase/decrease
                    when controlled by a key.  Valid range is 1-100, with 1
                    being the least sensitive (slowest response).  The default
                    is 25.  Can only be set in the 'Global' section.

    ----------------

    Name:           InputKeyDecaySpeed

    Argument:       Integer.

    Description:    The rate at which analog control values return to their
                    rest or 'off' position after a key has been released.  The
                    value is expressed as a percentage of the attack speed,
                    ranging from 1-200.  The default is 50, meaning analog
                    controls take twice as long to return to their rest state
                    as they do to reach their minimum/maximum values.  Can only
                    be set in the 'Global' section.

    ----------------

    Name:           InputMouseXDeadZone
                    InputMouseYDeadZone

    Argument:       Integer.

    Description:    Specifies a rectangular region in the center of the display
                    within which the mouse is considered inactive.  Expressed
                    as percentages, from 0-99, of the width and height of the
                    display.  The default values are 0, which disable this
                    option.  Can only be set in the 'Global' section.

    ----------------

    Name:           InputJoy**DeadZone

    Argument:       Integer.

    Description:    '**' must be the joystick number and axis (eg. '1X', '2RZ',
                    etc.).  If the joystick number is omitted, the setting will
                    apply to all joysticks.  Specifies the dead zone as a
                    percentage, 0-99, of the total range of the axis.  Within
                    the dead zone, the joystick is inactive.  This is useful
                    for joysticks that are 'noisy' when at rest.  Can only be
                    set in the 'Global' section.

    ----------------

    Name:           InputJoy**Saturation

    Argument:       Integer.

    Description:    '**' must be the joystick number and axis (eg. '1X', '2RZ',
                    etc.).  If the joystick number is omitted, the setting will
                    apply to all joysticks.  Specifies the saturation, the
                    position at which the joystick is interpreted as being in
                    its most extreme position, as a percentage of the total
                    range of the axis, from 0-200.  Values exceeding 100% mean
                    the axis will not use its full range.  A value of 200 means
                    that the range will be halved (fully pressed will only
                    register as 50%).  The default is 100, a 1-to-1 mapping.
                    Can only be set in the 'Global' section.

    ----------------

    Name:           InputJoy**MinVal
                    InputJoy**MaxVal
                    InputJoy**OffVal

    Argument:       Integer.

    Description:    '**' must be the joystick number and axis (eg. '1X', '2RZ',
                    etc.).  If the joystick number is omitted, the setting will
                    apply to all joysticks.  Specifies the maximum, minimum, or
                    offset value of an axis.  The range is -32768 to 32768.  By
                    default, MinVal is -32768, OffVal is 0, and MaxVal is
                    32767.  Can only be set in the 'Global' section.


=================================
  13. Compiling the Source Code
=================================

First, ensure that OpenGL, SDL (http://www.libsdl.org), and zlib
(http://zlib.net) are installed. GLEW (http://glew.sourceforge.net) is
included in the source tree and does not need to be installed separately.

Next, extract the Supermodel source code and copy the appropriate Makefile
from the Makefiles/ directory to the base directory (that is, the one above
Src/ and Makefiles/).  Makefiles for 32-bit Windows (Microsoft Visual C++
2008), Linux/UNIX (GCC), and Mac OS X (GCC) are provided, all requiring GNU
Make.  For Windows developers, MinGW (http://www.mingw.org) provides GNU Make.
Alternatively, you can write a Makefile compatible with Microsoft Nmake and
submit it for inclusion in the next release. ;)  Consult the Visual Studio
documentation to learn how to configure the Microsoft compiler for command line
operation.

Edit SDL_LIBPATH and SDL_INCLUDEPATH to reflect the location of the SDL
development library and header files.  This should only be necessary for
Windows.  The UNIX and Mac OS X Makefiles should be able to automatically
locate SDL if it was installed properly.

On Windows, Supermodel is compiled with the multi-threaded, static version of
the run-time library (/MT option).  However, the SDL and zlib development
libraries, and the SDL run-time DLL, are distributed for use with the dynamic
run-time (/MD option).  They must all be recompiled using /MT to work with
Supermodel.  Alternatively, Supermodel's Makefile can be edited to use the /MD
option.

When everything is ready, rename the appropriate Makefile to 'Makefile' and run
'make'.  If all goes well it should produce a Supermodel binary.


===========================
  14. Contact Information
===========================

The official Supermodel web site is:

    http://www.Supermodel3.com

Questions?  Comments?  Contributions?  Your feedback is welcome!  A discussion
forum is available at the web site and the primary author, Bart Trzynadlowski,
can be reached by email at:

    supermodel.emu@gmail.com

We ask that you remain mindful of the following courtesies:

    - Do NOT ask about ROMs.
    - Do NOT request features.
    - Do NOT ask about release dates of future versions.


=======================
  15. Acknowledgments
=======================

Numerous people contributed their precious time and energy to this project.
Without them, Supermodel would not have been possible.  In no particular order,
we would like to thank:

    - Ville Linde, original Supermodel team member and MAMEDev extraordinaire
    - Stefano Teso, original Supermodel team member
    - ElSemi, for all sorts of technical information and insight
    - R. Belmont, for DSB code and modified Amp library from his music player,
      M1, and the Mac OS X port
    - Naibo Zhang, for his work on Model 3 graphics
    - krom, for adding in the remaining ROM sets
    - Andrew Lewis (a.k.a. Andy Geezer), for dumping the drive board ROMs and
      providing region codes
    - The Guru, for his efforts in dumping Model 3 ROM sets
    - Brian Troha & The Dumping Union, for their continuing efforts in adding
      new Model 3 ROM sets
    - Abelardo Vidal Martos, for providing extremely useful video recordings of
      actual Model 3 games
    - Andrew Gardner, for fruitful discussion
    - Chad Reker, Alex Corrigan, pcvideogamer, and Groni, for being especially
      thorough play-testers
    - Charles MacDonald, for his helpful description of the System 24 tile
      generator
    - Andreas Naive, Olivier Galibert, David Haywood & MAME for the interface
      to and decryption code for the Sega 315-5881 protection device

Supermodel includes code from the following projects:

    - zlib:                     http://zlib.net
    - minizip:                  http://www.winimage.com/zLibDll/minizip.html
    - YAZE-AG:                  http://www.mathematik.uni-ulm.de/users/ag/yaze/
    - Amp by Tomislav Uzalec
    - The OpenGL Extension Wrangler Library (GLEW):
                                http://glew.sourceforge.net

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
