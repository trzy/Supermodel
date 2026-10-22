/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * LibretroInputs.cpp
 *
 * The RetroPad layout of every Supermodel control. A single table drives both
 * the Supermodel mapping strings (see LibretroInputSystem.h for the device
 * names) and the input descriptors shown by the frontend, so the two can
 * never disagree.
 *
 * Default layout (users remap it in the frontend):
 *
 *   All games       Start = START, Coin = SELECT, Service = L3, Test = R3
 *                   (port 2: Start2, Coin2, Service B, Test B)
 *   Joystick        D-pad or left stick
 *   Fighting        Punch = Y, Kick = B, Guard = X, Escape = A
 *   Spikeout        Beat = Y, Charge = X, Jump = B, Shift = A
 *   Soccer          Short Pass = B, Long Pass = A, Shoot = Y
 *   Driving         Steering = left stick or D-pad, Accelerator = R2,
 *                   Brake = L2, Shift Up/Down = R1/L1,
 *                   VR1-VR4 = Y/X/B/A, View Change = Y, Hand Brake = B,
 *                   Rear Brake = B, Music Select = X
 *   Virtual On      Levers = left/right stick, Shot = L1/R1,
 *                   Turbo = L2/R2; D-pad and face buttons drive the
 *                   single-stick macros
 *   Analog stick    Stick = left stick or D-pad, Trigger = B/R1,
 *                   Event = A/L1, Trigger 2 = Y, Event 2 = X
 *   Light guns      Light gun device (or left stick to aim), Trigger = B,
 *                   Reload = A
 *   Ski Champ       Left stick or D-pad, Poles = L1/R1, Select 1-3 = Y/X/A
 *   Magical Truck   Lever = left stick Y or D-pad, Pedal = B
 *   Bass Fishing    Rod = left stick or D-pad, Stick = right stick,
 *                   Reel = R2, Tension = L2, Cast = B, Select = A
 */

#include "LibretroInputs.h"
#include "Game.h"

#include <map>
#include <set>
#include <string>
#include <tuple>

namespace
{
  enum class Kind
  {
    Button,   // RetroPad button (D-pad buttons become POV directions)
    Stick,    // RetroPad analog stick axis
    Trigger,  // RetroPad analog L2/R2
    Gun       // libretro light gun
  };

  struct Binding
  {
    const char *input;      // Supermodel input id (without the "Input" prefix)
    uint32_t flags;         // Game::INPUT_* group the input belongs to
    unsigned port;
    Kind kind;
    unsigned id;            // RetroPad button id (Button, Trigger) or analog axis id (Stick)
    unsigned index;         // analog stick index (Stick)
    const char *part;       // Supermodel axis/button part (Stick, Trigger, Gun)
    const char *desc;       // descriptor text (nullptr: no descriptor)
  };

  constexpr unsigned L = RETRO_DEVICE_INDEX_ANALOG_LEFT;
  constexpr unsigned R = RETRO_DEVICE_INDEX_ANALOG_RIGHT;
  constexpr unsigned AX = RETRO_DEVICE_ID_ANALOG_X;
  constexpr unsigned AY = RETRO_DEVICE_ID_ANALOG_Y;

  #define BTN(input, flags, port, button, desc)       { input, flags, port, Kind::Button,  RETRO_DEVICE_ID_JOYPAD_##button, 0, nullptr, desc }
  #define STICK(input, flags, port, part, stick, axis, desc) { input, flags, port, Kind::Stick, axis, stick, part, desc }
  #define TRIG(input, flags, port, part, button, desc) { input, flags, port, Kind::Trigger, RETRO_DEVICE_ID_JOYPAD_##button, 0, part, desc }
  #define GUN(input, flags, port, part)               { input, flags, port, Kind::Gun, 0, 0, part, nullptr }

  const uint32_t COMMON   = Game::INPUT_COMMON;
  const uint32_t JOY1     = Game::INPUT_JOYSTICK1;
  const uint32_t JOY2     = Game::INPUT_JOYSTICK2;
  const uint32_t FIGHT    = Game::INPUT_FIGHTING;
  const uint32_t SPIKE    = Game::INPUT_SPIKEOUT;
  const uint32_t SOCCER   = Game::INPUT_SOCCER;
  const uint32_t VEHICLE  = Game::INPUT_VEHICLE;
  const uint32_t VR4      = Game::INPUT_VR4;
  const uint32_t VIEW     = Game::INPUT_VIEWCHANGE;
  const uint32_t HANDBRK  = Game::INPUT_HANDBRAKE;
  const uint32_t HARLEY   = Game::INPUT_HARLEY;
  const uint32_t TWIN     = Game::INPUT_TWIN_JOYSTICKS;
  const uint32_t ANAJOY   = Game::INPUT_ANALOG_JOYSTICK;
  const uint32_t GUN1     = Game::INPUT_GUN1;
  const uint32_t GUN2     = Game::INPUT_GUN2;
  const uint32_t AGUN1    = Game::INPUT_ANALOG_GUN1;
  const uint32_t AGUN2    = Game::INPUT_ANALOG_GUN2;
  const uint32_t SKI      = Game::INPUT_SKI;
  const uint32_t MAGTRUCK = Game::INPUT_MAGTRUCK;
  const uint32_t FISHING  = Game::INPUT_FISHING;

  const Binding s_bindings[] =
  {
    // Common controls
    BTN("Start1",   COMMON, 0, START,  "Start"),
    BTN("Coin1",    COMMON, 0, SELECT, "Coin"),
    BTN("ServiceA", COMMON, 0, L3,     "Service"),
    BTN("TestA",    COMMON, 0, R3,     "Test"),
    BTN("Start2",   COMMON, 1, START,  "Start"),
    BTN("Coin2",    COMMON, 1, SELECT, "Coin"),
    BTN("ServiceB", COMMON, 1, L3,     "Service B"),
    BTN("TestB",    COMMON, 1, R3,     "Test B"),

    // 4-way joysticks
    BTN  ("JoyUp",     JOY1, 0, UP,    "Up"),
    BTN  ("JoyDown",   JOY1, 0, DOWN,  "Down"),
    BTN  ("JoyLeft",   JOY1, 0, LEFT,  "Left"),
    BTN  ("JoyRight",  JOY1, 0, RIGHT, "Right"),
    STICK("JoyUp",     JOY1, 0, "YAXIS_NEG", L, AY, "Joystick Up/Down"),
    STICK("JoyDown",   JOY1, 0, "YAXIS_POS", L, AY, nullptr),
    STICK("JoyLeft",   JOY1, 0, "XAXIS_NEG", L, AX, "Joystick Left/Right"),
    STICK("JoyRight",  JOY1, 0, "XAXIS_POS", L, AX, nullptr),
    BTN  ("JoyUp2",    JOY2, 1, UP,    "Up"),
    BTN  ("JoyDown2",  JOY2, 1, DOWN,  "Down"),
    BTN  ("JoyLeft2",  JOY2, 1, LEFT,  "Left"),
    BTN  ("JoyRight2", JOY2, 1, RIGHT, "Right"),
    STICK("JoyUp2",    JOY2, 1, "YAXIS_NEG", L, AY, "Joystick Up/Down"),
    STICK("JoyDown2",  JOY2, 1, "YAXIS_POS", L, AY, nullptr),
    STICK("JoyLeft2",  JOY2, 1, "XAXIS_NEG", L, AX, "Joystick Left/Right"),
    STICK("JoyRight2", JOY2, 1, "XAXIS_POS", L, AX, nullptr),

    // Fighting games
    BTN("Punch",   FIGHT, 0, Y, "Punch"),
    BTN("Kick",    FIGHT, 0, B, "Kick"),
    BTN("Guard",   FIGHT, 0, X, "Guard"),
    BTN("Escape",  FIGHT, 0, A, "Escape"),
    BTN("Punch2",  FIGHT, 1, Y, "Punch"),
    BTN("Kick2",   FIGHT, 1, B, "Kick"),
    BTN("Guard2",  FIGHT, 1, X, "Guard"),
    BTN("Escape2", FIGHT, 1, A, "Escape"),

    // Spikeout
    BTN("Beat",   SPIKE, 0, Y, "Beat"),
    BTN("Charge", SPIKE, 0, X, "Charge"),
    BTN("Jump",   SPIKE, 0, B, "Jump"),
    BTN("Shift",  SPIKE, 0, A, "Shift"),

    // Virtua Striker
    BTN("ShortPass",  SOCCER, 0, B, "Short Pass"),
    BTN("LongPass",   SOCCER, 0, A, "Long Pass"),
    BTN("Shoot",      SOCCER, 0, Y, "Shoot"),
    BTN("ShortPass2", SOCCER, 1, B, "Short Pass"),
    BTN("LongPass2",  SOCCER, 1, A, "Long Pass"),
    BTN("Shoot2",     SOCCER, 1, Y, "Shoot"),

    // Vehicles
    BTN  ("SteeringLeft",  VEHICLE, 0, LEFT,  "Steer Left"),
    BTN  ("SteeringRight", VEHICLE, 0, RIGHT, "Steer Right"),
    STICK("Steering",      VEHICLE, 0, "XAXIS", L, AX, "Steering"),
    TRIG ("Accelerator",   VEHICLE, 0, "RZAXIS_POS", R2, "Accelerator"),
    BTN  ("Accelerator",   VEHICLE, 0, R2, nullptr),
    TRIG ("Brake",         VEHICLE, 0, "ZAXIS_POS", L2, "Brake"),
    BTN  ("Brake",         VEHICLE, 0, L2, nullptr),
    BTN  ("GearShiftUp",   VEHICLE, 0, R, "Shift Up"),
    BTN  ("GearShiftDown", VEHICLE, 0, L, "Shift Down"),
    BTN  ("VR1",           VR4,     0, Y, "VR1 (Red)"),
    BTN  ("VR2",           VR4,     0, X, "VR2 (Blue)"),
    BTN  ("VR3",           VR4,     0, B, "VR3 (Yellow)"),
    BTN  ("VR4",           VR4,     0, A, "VR4 (Green)"),
    BTN  ("ViewChange",    VIEW,    0, Y, "View Change"),
    BTN  ("HandBrake",     HANDBRK, 0, B, "Hand Brake"),
    BTN  ("RearBrake",     HARLEY,  0, B, "Rear Brake"),
    BTN  ("MusicSelect",   HARLEY,  0, X, "Music Select"),

    // Virtual On: individually mapped levers
    STICK("TwinJoyLeft1",  TWIN, 0, "XAXIS_NEG",  L, AX, "Left Lever Left/Right"),
    STICK("TwinJoyRight1", TWIN, 0, "XAXIS_POS",  L, AX, nullptr),
    STICK("TwinJoyUp1",    TWIN, 0, "YAXIS_NEG",  L, AY, "Left Lever Up/Down"),
    STICK("TwinJoyDown1",  TWIN, 0, "YAXIS_POS",  L, AY, nullptr),
    STICK("TwinJoyLeft2",  TWIN, 0, "RXAXIS_NEG", R, AX, "Right Lever Left/Right"),
    STICK("TwinJoyRight2", TWIN, 0, "RXAXIS_POS", R, AX, nullptr),
    STICK("TwinJoyUp2",    TWIN, 0, "RYAXIS_NEG", R, AY, "Right Lever Up/Down"),
    STICK("TwinJoyDown2",  TWIN, 0, "RYAXIS_POS", R, AY, nullptr),
    BTN  ("TwinJoyShot1",  TWIN, 0, L,  "Left Shot"),
    BTN  ("TwinJoyShot2",  TWIN, 0, R,  "Right Shot"),
    BTN  ("TwinJoyTurbo1", TWIN, 0, L2, "Left Turbo"),
    BTN  ("TwinJoyTurbo2", TWIN, 0, R2, "Right Turbo"),
    // Virtual On: macros for a single stick
    BTN  ("TwinJoyForward",     TWIN, 0, UP,    "Forward"),
    BTN  ("TwinJoyReverse",     TWIN, 0, DOWN,  "Reverse"),
    BTN  ("TwinJoyStrafeLeft",  TWIN, 0, LEFT,  "Strafe Left"),
    BTN  ("TwinJoyStrafeRight", TWIN, 0, RIGHT, "Strafe Right"),
    BTN  ("TwinJoyTurnLeft",    TWIN, 0, Y,     "Turn Left"),
    BTN  ("TwinJoyTurnRight",   TWIN, 0, A,     "Turn Right"),
    BTN  ("TwinJoyJump",        TWIN, 0, X,     "Jump"),
    BTN  ("TwinJoyCrouch",      TWIN, 0, B,     "Crouch"),

    // Analog joystick (Star Wars Trilogy)
    BTN  ("AnalogJoyLeft",     ANAJOY, 0, LEFT,  "Stick Left"),
    BTN  ("AnalogJoyRight",    ANAJOY, 0, RIGHT, "Stick Right"),
    BTN  ("AnalogJoyUp",       ANAJOY, 0, UP,    "Stick Up"),
    BTN  ("AnalogJoyDown",     ANAJOY, 0, DOWN,  "Stick Down"),
    STICK("AnalogJoyX",        ANAJOY, 0, "XAXIS", L, AX, "Stick X"),
    STICK("AnalogJoyY",        ANAJOY, 0, "YAXIS", L, AY, "Stick Y"),
    BTN  ("AnalogJoyTrigger",  ANAJOY, 0, B, "Trigger"),
    BTN  ("AnalogJoyTrigger",  ANAJOY, 0, R, "Trigger"),
    BTN  ("AnalogJoyEvent",    ANAJOY, 0, A, "Event"),
    BTN  ("AnalogJoyEvent",    ANAJOY, 0, L, "Event"),
    BTN  ("AnalogJoyTrigger2", ANAJOY, 0, Y, "Trigger 2"),
    BTN  ("AnalogJoyEvent2",   ANAJOY, 0, X, "Event 2"),

    // Light guns (Lost World)
    GUN  ("GunX",       GUN1, 0, "XAXIS"),
    STICK("GunX",       GUN1, 0, "XAXIS", L, AX, "Aim X"),
    GUN  ("GunY",       GUN1, 0, "YAXIS"),
    STICK("GunY",       GUN1, 0, "YAXIS", L, AY, "Aim Y"),
    BTN  ("GunLeft",    GUN1, 0, LEFT,  "Aim Left"),
    BTN  ("GunRight",   GUN1, 0, RIGHT, "Aim Right"),
    BTN  ("GunUp",      GUN1, 0, UP,    "Aim Up"),
    BTN  ("GunDown",    GUN1, 0, DOWN,  "Aim Down"),
    GUN  ("Trigger",    GUN1, 0, "LEFT_BUTTON"),
    BTN  ("Trigger",    GUN1, 0, B, "Trigger"),
    GUN  ("Offscreen",  GUN1, 0, "RIGHT_BUTTON"),
    BTN  ("Offscreen",  GUN1, 0, A, "Reload"),
    GUN  ("GunX2",      GUN2, 1, "XAXIS"),
    STICK("GunX2",      GUN2, 1, "XAXIS", L, AX, "Aim X"),
    GUN  ("GunY2",      GUN2, 1, "YAXIS"),
    STICK("GunY2",      GUN2, 1, "YAXIS", L, AY, "Aim Y"),
    BTN  ("GunLeft2",   GUN2, 1, LEFT,  "Aim Left"),
    BTN  ("GunRight2",  GUN2, 1, RIGHT, "Aim Right"),
    BTN  ("GunUp2",     GUN2, 1, UP,    "Aim Up"),
    BTN  ("GunDown2",   GUN2, 1, DOWN,  "Aim Down"),
    GUN  ("Trigger2",   GUN2, 1, "LEFT_BUTTON"),
    BTN  ("Trigger2",   GUN2, 1, B, "Trigger"),
    GUN  ("Offscreen2", GUN2, 1, "RIGHT_BUTTON"),
    BTN  ("Offscreen2", GUN2, 1, A, "Reload"),

    // Analog guns (Ocean Hunter, LA Machineguns)
    GUN  ("AnalogGunX",          AGUN1, 0, "XAXIS"),
    STICK("AnalogGunX",          AGUN1, 0, "XAXIS", L, AX, "Aim X"),
    GUN  ("AnalogGunY",          AGUN1, 0, "YAXIS"),
    STICK("AnalogGunY",          AGUN1, 0, "YAXIS", L, AY, "Aim Y"),
    BTN  ("AnalogGunLeft",       AGUN1, 0, LEFT,  "Aim Left"),
    BTN  ("AnalogGunRight",      AGUN1, 0, RIGHT, "Aim Right"),
    BTN  ("AnalogGunUp",         AGUN1, 0, UP,    "Aim Up"),
    BTN  ("AnalogGunDown",       AGUN1, 0, DOWN,  "Aim Down"),
    GUN  ("AnalogTriggerLeft",   AGUN1, 0, "LEFT_BUTTON"),
    BTN  ("AnalogTriggerLeft",   AGUN1, 0, B, "Left Trigger"),
    GUN  ("AnalogTriggerRight",  AGUN1, 0, "RIGHT_BUTTON"),
    GUN  ("AnalogTriggerRight",  AGUN1, 0, "MIDDLE_BUTTON"),
    BTN  ("AnalogTriggerRight",  AGUN1, 0, A, "Right Trigger"),
    GUN  ("AnalogGunX2",         AGUN2, 1, "XAXIS"),
    STICK("AnalogGunX2",         AGUN2, 1, "XAXIS", L, AX, "Aim X"),
    GUN  ("AnalogGunY2",         AGUN2, 1, "YAXIS"),
    STICK("AnalogGunY2",         AGUN2, 1, "YAXIS", L, AY, "Aim Y"),
    BTN  ("AnalogGunLeft2",      AGUN2, 1, LEFT,  "Aim Left"),
    BTN  ("AnalogGunRight2",     AGUN2, 1, RIGHT, "Aim Right"),
    BTN  ("AnalogGunUp2",        AGUN2, 1, UP,    "Aim Up"),
    BTN  ("AnalogGunDown2",      AGUN2, 1, DOWN,  "Aim Down"),
    GUN  ("AnalogTriggerLeft2",  AGUN2, 1, "LEFT_BUTTON"),
    BTN  ("AnalogTriggerLeft2",  AGUN2, 1, B, "Left Trigger"),
    GUN  ("AnalogTriggerRight2", AGUN2, 1, "RIGHT_BUTTON"),
    GUN  ("AnalogTriggerRight2", AGUN2, 1, "MIDDLE_BUTTON"),
    BTN  ("AnalogTriggerRight2", AGUN2, 1, A, "Right Trigger"),

    // Ski Champ
    BTN  ("SkiLeft",      SKI, 0, LEFT,  "Ski Left"),
    BTN  ("SkiRight",     SKI, 0, RIGHT, "Ski Right"),
    BTN  ("SkiUp",        SKI, 0, UP,    "Ski Up"),
    BTN  ("SkiDown",      SKI, 0, DOWN,  "Ski Down"),
    STICK("SkiX",         SKI, 0, "XAXIS", L, AX, "Ski X"),
    STICK("SkiY",         SKI, 0, "YAXIS", L, AY, "Ski Y"),
    BTN  ("SkiPollLeft",  SKI, 0, L, "Pole Left"),
    BTN  ("SkiPollRight", SKI, 0, R, "Pole Right"),
    BTN  ("SkiSelect1",   SKI, 0, Y, "Select 1"),
    BTN  ("SkiSelect2",   SKI, 0, X, "Select 2"),
    BTN  ("SkiSelect3",   SKI, 0, A, "Select 3"),

    // Magical Truck Adventure
    BTN  ("MagicalLeverUp1",   MAGTRUCK, 0, UP,   "Lever Up"),
    BTN  ("MagicalLeverDown1", MAGTRUCK, 0, DOWN, "Lever Down"),
    STICK("MagicalLever1",     MAGTRUCK, 0, "YAXIS", L, AY, "Lever"),
    BTN  ("MagicalPedal1",     MAGTRUCK, 0, B, "Pedal"),
    BTN  ("MagicalLeverUp2",   MAGTRUCK, 1, UP,   "Lever Up"),
    BTN  ("MagicalLeverDown2", MAGTRUCK, 1, DOWN, "Lever Down"),
    STICK("MagicalLever2",     MAGTRUCK, 1, "YAXIS", L, AY, "Lever"),
    BTN  ("MagicalPedal2",     MAGTRUCK, 1, B, "Pedal"),

    // Sega Bass Fishing / Get Bass
    BTN  ("FishingRodLeft",  FISHING, 0, LEFT,  "Rod Left"),
    BTN  ("FishingRodRight", FISHING, 0, RIGHT, "Rod Right"),
    BTN  ("FishingRodUp",    FISHING, 0, UP,    "Rod Up"),
    BTN  ("FishingRodDown",  FISHING, 0, DOWN,  "Rod Down"),
    STICK("FishingRodX",     FISHING, 0, "XAXIS",  L, AX, "Rod X"),
    STICK("FishingRodY",     FISHING, 0, "YAXIS",  L, AY, "Rod Y"),
    STICK("FishingStickX",   FISHING, 0, "RXAXIS", R, AX, "Stick X"),
    STICK("FishingStickY",   FISHING, 0, "RYAXIS", R, AY, "Stick Y"),
    TRIG ("FishingReel",     FISHING, 0, "RZAXIS_POS", R2, "Reel"),
    BTN  ("FishingReel",     FISHING, 0, R2, nullptr),
    TRIG ("FishingTension",  FISHING, 0, "ZAXIS_POS", L2, "Tension"),
    BTN  ("FishingTension",  FISHING, 0, L2, nullptr),
    BTN  ("FishingCast",     FISHING, 0, B, "Cast"),
    BTN  ("FishingSelect",   FISHING, 0, A, "Select"),
  };

  #undef BTN
  #undef STICK
  #undef TRIG
  #undef GUN

  // Every input Supermodel creates whose mapping is configurable (see
  // CInputs::CInputs()); the ones without a binding above are set to NONE
  const char *s_allInputs[] =
  {
    "UIExit", "UIReset", "UIPause", "UIFullScreen", "UISaveState", "UIChangeSlot", "UILoadState",
    "UIMusicVolUp", "UIMusicVolDown", "UISoundVolUp", "UISoundVolDown", "UIClearNVRAM",
    "UISelectCrosshairs", "UIToggleFrameLimit", "UIDumpInputState", "UIDumpTimings", "UIScreenShot",
    "UIEnterDebugger",
    "Start1", "Start2", "Coin1", "Coin2", "ServiceA", "ServiceB", "TestA", "TestB",
    "JoyUp", "JoyDown", "JoyLeft", "JoyRight", "JoyUp2", "JoyDown2", "JoyLeft2", "JoyRight2",
    "Punch", "Kick", "Guard", "Escape", "Punch2", "Kick2", "Guard2", "Escape2",
    "Shift", "Beat", "Charge", "Jump",
    "ShortPass", "LongPass", "Shoot", "ShortPass2", "LongPass2", "Shoot2",
    "SteeringLeft", "SteeringRight", "Steering", "Accelerator", "Brake", "GearShiftUp", "GearShiftDown",
    "GearShift1", "GearShift2", "GearShift3", "GearShift4", "GearShiftN",
    "VR1", "VR2", "VR3", "VR4", "ViewChange", "HandBrake", "RearBrake", "MusicSelect",
    "TwinJoyTurnLeft", "TwinJoyTurnRight", "TwinJoyForward", "TwinJoyReverse", "TwinJoyStrafeLeft",
    "TwinJoyStrafeRight", "TwinJoyJump", "TwinJoyCrouch", "TwinJoyLeft1", "TwinJoyLeft2",
    "TwinJoyRight1", "TwinJoyRight2", "TwinJoyUp1", "TwinJoyUp2", "TwinJoyDown1", "TwinJoyDown2",
    "TwinJoyShot1", "TwinJoyShot2", "TwinJoyTurbo1", "TwinJoyTurbo2",
    "AnalogJoyLeft", "AnalogJoyRight", "AnalogJoyUp", "AnalogJoyDown", "AnalogJoyX", "AnalogJoyY",
    "AnalogJoyTrigger", "AnalogJoyTrigger2", "AnalogJoyEvent", "AnalogJoyEvent2",
    "GunLeft", "GunRight", "GunUp", "GunDown", "GunX", "GunY", "Trigger", "Offscreen",
    "GunLeft2", "GunRight2", "GunUp2", "GunDown2", "GunX2", "GunY2", "Trigger2", "Offscreen2",
    "AnalogGunLeft", "AnalogGunRight", "AnalogGunUp", "AnalogGunDown", "AnalogGunX", "AnalogGunY",
    "AnalogTriggerLeft", "AnalogTriggerRight",
    "AnalogGunLeft2", "AnalogGunRight2", "AnalogGunUp2", "AnalogGunDown2", "AnalogGunX2", "AnalogGunY2",
    "AnalogTriggerLeft2", "AnalogTriggerRight2",
    "SkiLeft", "SkiRight", "SkiUp", "SkiDown", "SkiX", "SkiY", "SkiPollLeft", "SkiPollRight",
    "SkiSelect1", "SkiSelect2", "SkiSelect3",
    "MagicalLeverUp1", "MagicalLeverDown1", "MagicalLeverUp2", "MagicalLeverDown2",
    "MagicalLever1", "MagicalLever2", "MagicalPedal1", "MagicalPedal2",
    "FishingRodLeft", "FishingRodRight", "FishingRodUp", "FishingRodDown", "FishingStickLeft",
    "FishingStickRight", "FishingStickUp", "FishingStickDown", "FishingRodX", "FishingRodY",
    "FishingReel", "FishingStickX", "FishingStickY", "FishingCast", "FishingSelect", "FishingTension",
  };

  std::string Source(const Binding &b)
  {
    std::string joy = "JOY" + std::to_string(b.port + 1) + "_";
    switch (b.kind)
    {
    case Kind::Button:
      switch (b.id)
      {
      case RETRO_DEVICE_ID_JOYPAD_UP:    return joy + "POV1_UP";
      case RETRO_DEVICE_ID_JOYPAD_DOWN:  return joy + "POV1_DOWN";
      case RETRO_DEVICE_ID_JOYPAD_LEFT:  return joy + "POV1_LEFT";
      case RETRO_DEVICE_ID_JOYPAD_RIGHT: return joy + "POV1_RIGHT";
      default:                           return joy + "BUTTON" + std::to_string(b.id + 1);
      }
    case Kind::Stick:
    case Kind::Trigger:
      return joy + b.part;
    case Kind::Gun:
      return "MOUSE" + std::to_string(b.port + 1) + "_" + b.part;
    }
    return "NONE";
  }
}

namespace LibretroInputs
{
  void StoreMappings(Util::Config::Node *config, bool autoTrigger)
  {
    std::map<std::string, std::string> mappings;
    for (const char *input : s_allInputs)
      mappings[input] = "";
    for (const Binding &b : s_bindings)
    {
      std::string &mapping = mappings[b.input];
      if (!mapping.empty())
        mapping += ",";
      mapping += Source(b);
    }
    for (auto &it : mappings)
      config->Set("Input" + it.first, it.second.empty() ? std::string("NONE") : it.second);

    config->Set("InputAutoTrigger", (unsigned) (autoTrigger ? 1 : 0));
    config->Set("InputAutoTrigger2", (unsigned) (autoTrigger ? 1 : 0));
  }

  std::vector<retro_input_descriptor> GetDescriptors(uint32_t gameInputs)
  {
    std::vector<retro_input_descriptor> descriptors;
    std::set<std::tuple<unsigned, unsigned, unsigned, unsigned>> used;
    for (const Binding &b : s_bindings)
    {
      if (!b.desc || !(b.flags & gameInputs))
        continue;
      retro_input_descriptor d;
      d.port = b.port;
      d.description = b.desc;
      switch (b.kind)
      {
      case Kind::Button:
      case Kind::Trigger:
        d.device = RETRO_DEVICE_JOYPAD;
        d.index = 0;
        d.id = b.id;
        break;
      case Kind::Stick:
        d.device = RETRO_DEVICE_ANALOG;
        d.index = b.index;
        d.id = b.id;
        break;
      default:
        continue;
      }
      // A RetroPad control shared by several inputs is described once
      if (!used.insert(std::make_tuple(d.port, d.device, d.index, d.id)).second)
        continue;
      descriptors.push_back(d);
    }
    descriptors.push_back(retro_input_descriptor{ 0, 0, 0, 0, nullptr });
    return descriptors;
  }

  bool UsesLightGuns(uint32_t gameInputs)
  {
    return (gameInputs & (Game::INPUT_GUN1 | Game::INPUT_GUN2 | Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) != 0;
  }
}
