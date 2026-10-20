# Supermodel libretro core

Supermodel can be built as a [libretro](https://www.libretro.com) core
(`supermodel_libretro.so`, `.dll` or `.dylib`) for RetroArch and other libretro
frontends. The core uses the same emulation code as the standalone emulator;
only the platform layer (video, audio, input, configuration, threads, file
locations) is different. Its sources are in `Src/OSD/Libretro`.

## Building

The core is built with the usual Makefiles and the `LIBRETRO=1` option:

```
make -f Makefiles/Makefile.UNIX  LIBRETRO=1     # Linux / BSD -> bin/supermodel_libretro.so
make -f Makefiles/Makefile.Win32 LIBRETRO=1     # MSYS2 MinGW-w64 -> bin64/supermodel_libretro.dll
make -f Makefiles/Makefile.OSX   LIBRETRO=1     # macOS (clang) -> bin/supermodel_libretro.dylib
```

The core does not use SDL. It needs only OpenGL and zlib. Its objects go to a
separate directory (`obj*_libretro`), so the standalone emulator and the core
can be built from the same tree.

`LIBRETRO_NET=1` (together with `LIBRETRO=1`) also builds the net board, the
Pro-1000 SCSI TCP bridge and the network outputs. These use SDL2_net, so SDL2
and SDL2_net are then needed to build and run the core.

The core follows the same compiler requirements as the rest of Supermodel:
C++17 and C11, built with GCC, MinGW-w64, Clang and MSVC.

## Installation

| What | Where |
|------|-------|
| Core | the frontend's cores directory |
| `Src/OSD/Libretro/supermodel_libretro.info` | the frontend's core info directory |
| `Config/Games.xml` (optional, a copy is built into the core) | `<system>/supermodel/Games.xml` |
| `Config/Music.xml` (optional, custom MPEG music) | `<system>/supermodel/Music.xml` |
| `Assets/p1crosshair.bmp`, `Assets/p2crosshair.bmp` (bitmap crosshairs) | `<system>/supermodel/Assets/` |

`<system>` is the frontend's system (BIOS) directory. A `Games.xml` found
there takes precedence over the built-in copy.

ROM sets are the same `.zip` files as for the standalone emulator. A clone
set looks for its parent set in the same directory. When a zip contains more
than one ROM set, Supermodel chooses one of them, exactly as the standalone
emulator does.

NVRAM (EEPROM and backup RAM) is written to `<save>/supermodel/NVRAM/<game>.nv`
when the game is closed, in the standalone emulator's format. Save states are
handled by the frontend and also use Supermodel's save state format.

## Video

Rendering uses the frontend's OpenGL context: core profile 4.1 for the new 3D
engine, compatibility profile for the legacy 3D engine (not available on
macOS). The frame is rendered at the internal resolution chosen in the core
options; the reported aspect ratio is that of the frame, which already
contains the black borders that keep the Model 3 aspect ratio (unless
'Stretch' is enabled).

## Audio

The sound board produces one frame of audio (735 samples at 44.1 kHz) per
emulated frame; the four Model 3 channels are mixed to stereo with the
per-game mixing rules of the standalone emulator. The sample rate reported to
the frontend is 735 x the refresh rate (44100 Hz at 60 Hz, 42280 Hz at
57.524 Hz), so audio stays in step with video.

## Controls

The `Input*` settings of `Supermodel.ini` are replaced by a fixed RetroPad
layout. Controls are remapped with the frontend's controller settings, which
show the names of the game's controls.

| Games | Layout |
|-------|--------|
| All | Start = Start, Coin = Select, Service = L3, Test = R3 (port 2: Service B, Test B) |
| Joystick | D-pad or left stick |
| Fighting (Virtua Fighter 3, Fighting Vipers 2) | Punch = Y, Kick = B, Guard = X, Escape = A |
| Spikeout | Beat = Y, Charge = X, Jump = B, Shift = A |
| Virtua Striker 2 | Short Pass = B, Long Pass = A, Shoot = Y |
| Driving | Steering = left stick or D-pad, Accelerator = R2, Brake = L2, Shift Up/Down = R1/L1, VR1-VR4 = Y/X/B/A, View Change = Y, Hand Brake = B, Rear Brake = B, Music Select = X |
| Virtual On | Levers = left and right sticks, Shot = L1/R1, Turbo = L2/R2; the D-pad and face buttons drive the single-stick macros |
| Star Wars Trilogy | Stick = left stick or D-pad, Trigger = B or R1, Event = A or L1, Trigger 2 = Y, Event 2 = X |
| Light gun games | Light gun on ports 1 and 2 (or the left stick to aim), Trigger = B, Reload = A |
| Ski Champ | Left stick or D-pad, Poles = L1/R1, Select 1-3 = Y/X/A |
| Magical Truck Adventure | Lever = left stick or D-pad, Pedal = B |
| Sega Bass Fishing / Get Bass | Rod = left stick or D-pad, Stick = right stick, Reel = R2, Tension = L2, Cast = B, Select = A |

Analog L2/R2 are used when the frontend supports analog triggers; otherwise
the digital buttons act as pedals, with the 'Digital Sensitivity' option.

For light gun games, select 'Light Gun' as the device of port 1 and/or 2.
Reload (or pulling the trigger off screen) points the gun off screen; with
'Light Gun Auto-Reload' it also fires, as needed to reload.

With 'Force Feedback' enabled, the force feedback of the drive boards is
turned into controller rumble: steering wheel torque drives the strong motor
and vibration the weak motor.

## Core options

| Core option | Standalone equivalent | Applied |
|-------------|-----------------------|---------|
| PowerPC Frequency | `-ppc-frequency` | immediately |
| Multi-Threading | `-threads` / `-no-threads` | on restart |
| GPU Multi-Threading | `-gpu-multi-threaded` / `-no-gpu-thread` | on restart |
| Refresh Rate | `-true-hz` | immediately |
| Clear NVRAM at Startup | UI key Alt+N | on restart |
| 3D Engine | `-new3d` / `-legacy3d` | on restart |
| Internal Resolution | `-res` | on restart |
| Supersampling | `-ss` | on restart |
| Wide Screen | `-wide-screen` | on restart |
| Wide Background | `-wide-bg` | on restart |
| Stretch | `-stretch` | on restart |
| 2D Upscaling Filter | `-upscalemode` | on restart |
| CRT Colors | `-crtcolors` | on restart |
| Quad Rendering | `-quad-rendering` / `-no-quad-rendering` | on restart |
| Multi-Texture (Legacy 3D Engine) | `-multi-texture` | on restart |
| Disable White Flash | `-no-white-flash` | on restart |
| Crosshairs | `-crosshairs` | immediately |
| Crosshair Style | `-crosshair-style` | immediately |
| Sound Volume | `-sound-volume` | immediately |
| Music Volume | `-music-volume` | immediately |
| SCSP Balance | `-balance` | immediately |
| Left/Right Balance | `BalanceLeftRight` | immediately |
| Front/Rear Balance | `BalanceFrontRear` | immediately |
| Swap Stereo Channels | `-flip-stereo` | immediately |
| Sound Board | `-sound` / `-no-sound` | immediately |
| Digital Sound Board | `-dsb` / `-no-dsb` | immediately |
| SCSP Engine | `-new-scsp` / `-legacy-scsp` | on restart |
| Force Feedback | `-force-feedback` | on restart |
| Rumble Strength | `SDLConstForceMax`, `SDLVibrateMax`, ... | immediately |
| Analog Dead Zone | `InputJoy*DeadZone` | immediately |
| Analog Saturation | `InputJoy*Saturation` | immediately |
| Digital Sensitivity | `InputDigitalSensitivity` | immediately |
| Digital Decay Speed | `InputDigitalDecaySpeed` | immediately |
| Light Gun Auto-Reload | `InputAutoTrigger` | immediately |
| Net Board (`LIBRETRO_NET=1`) | `-net` / `-no-net` | on restart |
| Net Board Mode (`LIBRETRO_NET=1`) | `-simulate-netboard` / `-emulate-netboard` | on restart |
| Net Board Input/Output Port (`LIBRETRO_NET=1`) | `PortIn` / `PortOut` | on restart |
| Outputs (`LIBRETRO_NET=1`) | `-outputs` | on restart |

Per-game settings are made with the frontend's per-game core option
overrides, in place of the game sections of `Supermodel.ini`.

Options that do not apply to a libretro core: window, full screen,
borderless and window position, vsync, frame rate limiting, frame rate
display, save state loading, input configuration and input system selection,
logging, the number of host audio channels (the output is always stereo), the
host audio frequency (derived from the refresh rate), external legacy
shaders, memory and texture dumps, and the Pro-1000 serial console.
