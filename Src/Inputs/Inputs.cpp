/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2019 Bart Trzynadlowski, Nik Henson, Ian Curtis,
 **                     Harry Tuttle, and Spindizzi
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
 * Inputs.cpp
 *
 * Input management. Implementation of CInputs, which stores, configures, and
 * maintains all individual inputs.
 */

#include "Inputs.h"

#include "Supermodel.h"
#include "InputSystem.h"
#include "InputTypes.h"
#include "Game.h"
#include <cstdarg>
#include <vector>
#include <string>
#include <iostream>
using namespace std;

CInputs::CInputs(CInputSystem *system)
  : m_system(system)
{
	// UI controls are hard coded here, everything else is initialized to NONE so that it can be loaded from
	// the config file.

	// UI Controls
	uiExit             = AddSwitchInput("UIExit",             "Exit UI",               Game::INPUT_UI, "KEY_ESCAPE");
	uiReset            = AddSwitchInput("UIReset",            "Reset",                 Game::INPUT_UI, "KEY_ALT+KEY_R");
	uiPause            = AddSwitchInput("UIPause",            "Pause",                 Game::INPUT_UI, "KEY_ALT+KEY_P");
	uiFullScreen       = AddSwitchInput("UIFullScreen",       "Toggle Fullscreen",     Game::INPUT_UI, "KEY_ALT+KEY_RETURN");
	uiSaveState        = AddSwitchInput("UISaveState",        "Save State",            Game::INPUT_UI, "KEY_F5");
	uiChangeSlot       = AddSwitchInput("UIChangeSlot",       "Change Save Slot",      Game::INPUT_UI, "KEY_F6");
	uiLoadState        = AddSwitchInput("UILoadState",        "Load State",            Game::INPUT_UI, "KEY_F7");
	uiMusicVolUp       = AddSwitchInput("UIMusicVolUp",       "Increase Music Volume", Game::INPUT_UI, "KEY_F10");
	uiMusicVolDown     = AddSwitchInput("UIMusicVolDown",     "Decrease Music Volume", Game::INPUT_UI, "KEY_F9");
	uiSoundVolUp       = AddSwitchInput("UISoundVolUp",       "Increase Sound Volume", Game::INPUT_UI, "KEY_F12");
	uiSoundVolDown     = AddSwitchInput("UISoundVolDown",     "Decrease Sound Volume", Game::INPUT_UI, "KEY_F11");
	uiClearNVRAM       = AddSwitchInput("UIClearNVRAM",       "Clear NVRAM",           Game::INPUT_UI, "KEY_ALT+KEY_N");
	uiSelectCrosshairs = AddSwitchInput("UISelectCrosshairs", "Select Crosshairs",     Game::INPUT_UI, "KEY_ALT+KEY_I");
	uiToggleFrLimit    = AddSwitchInput("UIToggleFrameLimit", "Toggle Frame Limiting", Game::INPUT_UI, "KEY_ALT+KEY_T");
	uiDumpInpState     = AddSwitchInput("UIDumpInputState",   "Dump Input State",      Game::INPUT_UI, "KEY_ALT+KEY_U");
	uiDumpTimings      = AddSwitchInput("UIDumpTimings",      "Dump Frame Timings",    Game::INPUT_UI, "KEY_ALT+KEY_O");
	uiScreenshot       = AddSwitchInput("UIScreenShot",	      "Screenshot",            Game::INPUT_UI, "KEY_ALT+KEY_S");
#ifdef SUPERMODEL_DEBUGGER
	uiEnterDebugger    = AddSwitchInput("UIEnterDebugger",    "Enter Debugger",        Game::INPUT_UI, "KEY_ALT+KEY_B");
#endif

	// Common Controls
	start[0]           = AddSwitchInput("Start1",   "P1 Start",  Game::INPUT_COMMON, "NONE");
	start[1]           = AddSwitchInput("Start2",   "P2 Start",  Game::INPUT_COMMON, "NONE");
	coin[0]            = AddSwitchInput("Coin1",    "P1 Coin",   Game::INPUT_COMMON, "NONE");
	coin[1]            = AddSwitchInput("Coin2",    "P2 Coin",   Game::INPUT_COMMON, "NONE");
	service[0]         = AddSwitchInput("ServiceA", "Service A", Game::INPUT_COMMON, "NONE");
	service[1]         = AddSwitchInput("ServiceB", "Service B", Game::INPUT_COMMON, "NONE");
	test[0]            = AddSwitchInput("TestA",    "Test A",    Game::INPUT_COMMON, "NONE");
	test[1]            = AddSwitchInput("TestB",    "Test B",    Game::INPUT_COMMON, "NONE");

	// 4-Way Joysticks
	up[0]              = AddSwitchInput("JoyUp",     "P1 Joystick Up",    Game::INPUT_JOYSTICK1, "NONE");
	down[0]            = AddSwitchInput("JoyDown",   "P1 Joystick Down",  Game::INPUT_JOYSTICK1, "NONE");
	left[0]            = AddSwitchInput("JoyLeft",   "P1 Joystick Left",  Game::INPUT_JOYSTICK1, "NONE");
	right[0]           = AddSwitchInput("JoyRight",  "P1 Joystick Right", Game::INPUT_JOYSTICK1, "NONE");
	up[1]              = AddSwitchInput("JoyUp2",    "P2 Joystick Up",    Game::INPUT_JOYSTICK2, "NONE");
	down[1]            = AddSwitchInput("JoyDown2",  "P2 Joystick Down",  Game::INPUT_JOYSTICK2, "NONE");
	left[1]            = AddSwitchInput("JoyLeft2",  "P2 Joystick Left",  Game::INPUT_JOYSTICK2, "NONE");
	right[1]           = AddSwitchInput("JoyRight2", "P2 Joystick Right", Game::INPUT_JOYSTICK2, "NONE");

	// Fighting Game Buttons
	punch[0]           = AddSwitchInput("Punch",   "P1 Punch",  Game::INPUT_FIGHTING, "NONE");
	kick[0]            = AddSwitchInput("Kick",    "P1 Kick",   Game::INPUT_FIGHTING, "NONE");
	guard[0]           = AddSwitchInput("Guard",   "P1 Guard",  Game::INPUT_FIGHTING, "NONE");
	escape[0]          = AddSwitchInput("Escape",  "P1 Escape", Game::INPUT_FIGHTING, "NONE");
	punch[1]           = AddSwitchInput("Punch2",  "P2 Punch",  Game::INPUT_FIGHTING, "NONE");
	kick[1]            = AddSwitchInput("Kick2",   "P2 Kick",   Game::INPUT_FIGHTING, "NONE");
	guard[1]           = AddSwitchInput("Guard2",  "P2 Guard",  Game::INPUT_FIGHTING, "NONE");
	escape[1]          = AddSwitchInput("Escape2", "P2 Escape", Game::INPUT_FIGHTING, "NONE");

	// Spikeout Buttons
	shift              = AddSwitchInput("Shift",   "Shift", Game::INPUT_SPIKEOUT, "NONE");
	beat               = AddSwitchInput("Beat",    "Beat",  Game::INPUT_SPIKEOUT, "NONE");
	charge             = AddSwitchInput("Charge",  "Charge",Game::INPUT_SPIKEOUT, "NONE");
	jump               = AddSwitchInput("Jump",    "Jump",  Game::INPUT_SPIKEOUT, "NONE");

	// Virtua Striker Buttons
	shortPass[0]       = AddSwitchInput("ShortPass",  "P1 Short Pass", Game::INPUT_SOCCER, "NONE");
	longPass[0]        = AddSwitchInput("LongPass",   "P1 Long Pass",  Game::INPUT_SOCCER, "NONE");
	shoot[0]           = AddSwitchInput("Shoot",      "P1 Shoot",      Game::INPUT_SOCCER, "NONE");
	shortPass[1]       = AddSwitchInput("ShortPass2", "P2 Short Pass", Game::INPUT_SOCCER, "NONE");
	longPass[1]        = AddSwitchInput("LongPass2",  "P2 Long Pass",  Game::INPUT_SOCCER, "NONE");
	shoot[1]           = AddSwitchInput("Shoot2",     "P2 Shoot",      Game::INPUT_SOCCER, "NONE");

	// Racing Game Steering Controls
	CAnalogInput *steeringLeft  = AddAnalogInput("SteeringLeft",  "Steer Left",  Game::INPUT_VEHICLE, "NONE");
	CAnalogInput *steeringRight = AddAnalogInput("SteeringRight", "Steer Right", Game::INPUT_VEHICLE, "NONE");

	steering           = AddAxisInput  ("Steering",      "Full Steering",           Game::INPUT_VEHICLE, "NONE", steeringLeft, steeringRight);
	accelerator        = AddAnalogInput("Accelerator",   "Accelerator Pedal",       Game::INPUT_VEHICLE, "NONE");
	brake              = AddAnalogInput("Brake",         "Brake Pedal/Front Brake", Game::INPUT_VEHICLE, "NONE");
	gearShiftUp        = AddSwitchInput("GearShiftUp",   "Shift Up",                Game::INPUT_VEHICLE, "NONE");
	gearShiftDown      = AddSwitchInput("GearShiftDown", "Shift Down",              Game::INPUT_VEHICLE, "NONE");

	// Racing Game Gear Shift
	CSwitchInput *shift1 = AddSwitchInput("GearShift1", "Shift 1",       Game::INPUT_SHIFT4,  "NONE");
	CSwitchInput *shift2 = AddSwitchInput("GearShift2", "Shift 2",       Game::INPUT_SHIFT4,  "NONE");
	CSwitchInput *shift3 = AddSwitchInput("GearShift3", "Shift 3",       Game::INPUT_SHIFT4,  "NONE");
	CSwitchInput *shift4 = AddSwitchInput("GearShift4", "Shift 4",       Game::INPUT_SHIFT4,  "NONE");
	CSwitchInput *shiftN = AddSwitchInput("GearShiftN", "Shift Neutral", Game::INPUT_SHIFT4,  "NONE");

	gearShift4         = AddGearShift4Input("GearShift", "Gear Shift", Game::INPUT_SHIFT4,  shift1, shift2, shift3, shift4, shiftN, gearShiftUp, gearShiftDown);

	// Racing Game 4 VR View Buttons
	vr[0]              = AddSwitchInput("VR1", "VR1", Game::INPUT_VR4, "NONE");
	vr[1]              = AddSwitchInput("VR2", "VR2", Game::INPUT_VR4, "NONE");
	vr[2]              = AddSwitchInput("VR3", "VR3", Game::INPUT_VR4, "NONE");
	vr[3]              = AddSwitchInput("VR4", "VR4", Game::INPUT_VR4, "NONE");

	// Racing Game Single View Change Button
	viewChange         = AddSwitchInput("ViewChange", "View Change", Game::INPUT_VIEWCHANGE, "NONE");

	// Racing Game Handbrake
	handBrake          = AddSwitchInput("HandBrake",  "Hand Brake",  Game::INPUT_HANDBRAKE, "NONE");

	// Harley Davidson Controls
	rearBrake          = AddAnalogInput("RearBrake",   "Rear Brake",      Game::INPUT_HARLEY, "NONE");
	musicSelect        = AddSwitchInput("MusicSelect", "Music Selection", Game::INPUT_HARLEY, "NONE");

	// Virtual On Controls
	twinJoyTurnLeft    = AddSwitchInput("TwinJoyTurnLeft",    "Macro Turn Left",    	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyTurnRight   = AddSwitchInput("TwinJoyTurnRight",   "Macro Turn Right",   	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyForward     = AddSwitchInput("TwinJoyForward",     "Macro Forward",      	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyReverse     = AddSwitchInput("TwinJoyReverse",     "Macro Reverse",      	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyStrafeLeft  = AddSwitchInput("TwinJoyStrafeLeft",  "Macro Strafe Left",  	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyStrafeRight = AddSwitchInput("TwinJoyStrafeRight", "Macro Strafe Right", 	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyJump        = AddSwitchInput("TwinJoyJump",        "Macro Jump",         	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyCrouch      = AddSwitchInput("TwinJoyCrouch",      "Macro Crouch",       	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyLeft1	     = AddSwitchInput("TwinJoyLeft1",       "Left Joystick Left", 	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyLeft2	     = AddSwitchInput("TwinJoyLeft2",       "Right Joystick Left",	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyRight1	     = AddSwitchInput("TwinJoyRight1",      "Left Joystick Right", 	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyRight2	     = AddSwitchInput("TwinJoyRight2",      "Right Joystick Right", 	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyUp1	   	   = AddSwitchInput("TwinJoyUp1",         "Left Joystick Up", 		Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyUp2	   	   = AddSwitchInput("TwinJoyUp2",         "Right Joystick Up", 		Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyDown1	     = AddSwitchInput("TwinJoyDown1",       "Left Joystick Down", 	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyDown2	     = AddSwitchInput("TwinJoyDown2",       "Right Joystick Down", 	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyShot1       = AddSwitchInput("TwinJoyShot1",       "Left Shot Trigger",  	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyShot2       = AddSwitchInput("TwinJoyShot2",       "Right Shot Trigger", 	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyTurbo1      = AddSwitchInput("TwinJoyTurbo1",      "Left Turbo",         	Game::INPUT_TWIN_JOYSTICKS, "NONE");
	twinJoyTurbo2      = AddSwitchInput("TwinJoyTurbo2",      "Right Turbo",        	Game::INPUT_TWIN_JOYSTICKS, "NONE");

	// Analog Joystick
	CAnalogInput *analogJoyLeft  = AddAnalogInput("AnalogJoyLeft",  "Analog Left",  Game::INPUT_ANALOG_JOYSTICK, "NONE");
	CAnalogInput *analogJoyRight = AddAnalogInput("AnalogJoyRight", "Analog Right", Game::INPUT_ANALOG_JOYSTICK, "NONE");
	CAnalogInput *analogJoyUp    = AddAnalogInput("AnalogJoyUp",    "Analog Up",    Game::INPUT_ANALOG_JOYSTICK, "NONE");
	CAnalogInput *analogJoyDown  = AddAnalogInput("AnalogJoyDown",  "Analog Down",  Game::INPUT_ANALOG_JOYSTICK, "NONE");

	analogJoyX         = AddAxisInput  ("AnalogJoyX",        "Analog X-Axis",    Game::INPUT_ANALOG_JOYSTICK, "NONE", analogJoyLeft, analogJoyRight);
	analogJoyY         = AddAxisInput  ("AnalogJoyY",        "Analog Y-Axis",    Game::INPUT_ANALOG_JOYSTICK, "NONE", analogJoyUp,   analogJoyDown);
	analogJoyTrigger1  = AddSwitchInput("AnalogJoyTrigger",  "Trigger Button 1", Game::INPUT_ANALOG_JOYSTICK, "NONE");
	analogJoyTrigger2  = AddSwitchInput("AnalogJoyTrigger2", "Trigger Button 2", Game::INPUT_ANALOG_JOYSTICK, "NONE");
	analogJoyEvent1    = AddSwitchInput("AnalogJoyEvent",    "Event Button 1",   Game::INPUT_ANALOG_JOYSTICK, "NONE");
	analogJoyEvent2    = AddSwitchInput("AnalogJoyEvent2",   "Event Button 2",   Game::INPUT_ANALOG_JOYSTICK, "NONE");

	// Light guns
	CAnalogInput *gun1Left  = AddAnalogInput("GunLeft",  "P1 Gun Left",  Game::INPUT_GUN1, "NONE");
	CAnalogInput *gun1Right = AddAnalogInput("GunRight", "P1 Gun Right", Game::INPUT_GUN1, "NONE");
	CAnalogInput *gun1Up    = AddAnalogInput("GunUp",    "P1 Gun Up",    Game::INPUT_GUN1, "NONE");
	CAnalogInput *gun1Down  = AddAnalogInput("GunDown",  "P1 Gun Down",  Game::INPUT_GUN1, "NONE");

	gunX[0]            = AddAxisInput("GunX", "P1 Gun X-Axis", Game::INPUT_GUN1, "NONE", gun1Left, gun1Right, 150, 400, 651); // normalize to [150,651]
	gunY[0]            = AddAxisInput("GunY", "P1 Gun Y-Axis", Game::INPUT_GUN1, "NONE", gun1Up,   gun1Down,  80,  272, 465); // normalize to [80,465]

	CSwitchInput *gun1Trigger   = AddSwitchInput("Trigger",   "P1 Trigger",          Game::INPUT_GUN1, "NONE");
	CSwitchInput *gun1Offscreen = AddSwitchInput("Offscreen", "P1 Point Off-screen", Game::INPUT_GUN1, "NONE");

	trigger[0]         = AddTriggerInput("AutoTrigger", "P1 Auto Trigger", Game::INPUT_GUN1, gun1Trigger, gun1Offscreen);

	CAnalogInput *gun2Left  = AddAnalogInput("GunLeft2",  "P2 Gun Left",  Game::INPUT_GUN2, "NONE");
	CAnalogInput *gun2Right = AddAnalogInput("GunRight2", "P2 Gun Right", Game::INPUT_GUN2, "NONE");
	CAnalogInput *gun2Up    = AddAnalogInput("GunUp2",    "P2 Gun Up",    Game::INPUT_GUN2, "NONE");
	CAnalogInput *gun2Down  = AddAnalogInput("GunDown2",  "P2 Gun Down",  Game::INPUT_GUN2, "NONE");

	gunX[1]            = AddAxisInput("GunX2", "P2 Gun X-Axis", Game::INPUT_GUN2, "NONE", gun2Left, gun2Right, 150, 400, 651); // normalize to [150,651]
	gunY[1]            = AddAxisInput("GunY2", "P2 Gun Y-Axis", Game::INPUT_GUN2, "NONE", gun2Up,   gun2Down,  80,  272, 465); // normalize to [80,465]

	CSwitchInput *gun2Trigger   = AddSwitchInput("Trigger2",   "P2 Trigger",          Game::INPUT_GUN2, "NONE");
	CSwitchInput *gun2Offscreen = AddSwitchInput("Offscreen2", "P2 Point Off-screen", Game::INPUT_GUN2, "NONE");

	trigger[1]         = AddTriggerInput("AutoTrigger2", "P2 Auto Trigger", Game::INPUT_GUN2, gun2Trigger, gun2Offscreen);

	// Analog guns
	CAnalogInput *analogGun1Left  = AddAnalogInput("AnalogGunLeft",  "P1 Analog Gun Left",  Game::INPUT_ANALOG_GUN1, "NONE");
	CAnalogInput *analogGun1Right = AddAnalogInput("AnalogGunRight", "P1 Analog Gun Right", Game::INPUT_ANALOG_GUN1, "NONE");
	CAnalogInput *analogGun1Up    = AddAnalogInput("AnalogGunUp",    "P1 Analog Gun Up",    Game::INPUT_ANALOG_GUN1, "NONE");
	CAnalogInput *analogGun1Down  = AddAnalogInput("AnalogGunDown",  "P1 Analog Gun Down",  Game::INPUT_ANALOG_GUN1, "NONE");

	analogGunX[0] = AddAxisInput("AnalogGunX", "P1 Analog Gun X-Axis", Game::INPUT_ANALOG_GUN1, "NONE", analogGun1Left, analogGun1Right, 0, 0x80, 0xFF);
	analogGunY[0] = AddAxisInput("AnalogGunY", "P1 Analog Gun Y-Axis", Game::INPUT_ANALOG_GUN1, "NONE", analogGun1Up,   analogGun1Down,  0xFF, 0x80, 0);

	analogTriggerLeft[0]  = AddSwitchInput("AnalogTriggerLeft",  "P1 Analog Gun Left Trigger",  Game::INPUT_ANALOG_GUN1, "NONE");
	analogTriggerRight[0] = AddSwitchInput("AnalogTriggerRight", "P1 Analog Gun Right Trigger", Game::INPUT_ANALOG_GUN1, "NONE");

	CAnalogInput *analogGun2Left  = AddAnalogInput("AnalogGunLeft2",  "P2 Analog Gun Left",  Game::INPUT_ANALOG_GUN2, "NONE");
	CAnalogInput *analogGun2Right = AddAnalogInput("AnalogGunRight2", "P2 Analog Gun Right", Game::INPUT_ANALOG_GUN2, "NONE");
	CAnalogInput *analogGun2Up    = AddAnalogInput("AnalogGunUp2",    "P2 Analog Gun Up",    Game::INPUT_ANALOG_GUN2, "NONE");
	CAnalogInput *analogGun2Down  = AddAnalogInput("AnalogGunDown2",  "P2 Analog Gun Down",  Game::INPUT_ANALOG_GUN2, "NONE");

	analogGunX[1] = AddAxisInput("AnalogGunX2", "P2 Analog Gun X-Axis", Game::INPUT_ANALOG_GUN2, "NONE", analogGun2Left, analogGun2Right, 0, 0x80, 0xFF);
	analogGunY[1] = AddAxisInput("AnalogGunY2", "P2 Analog Gun Y-Axis", Game::INPUT_ANALOG_GUN2, "NONE", analogGun2Up,   analogGun2Down,  0xFF, 0x80, 0);

	analogTriggerLeft[1]  = AddSwitchInput("AnalogTriggerLeft2",  "P2 Analog Gun Left Trigger",  Game::INPUT_ANALOG_GUN2, "NONE");
	analogTriggerRight[1] = AddSwitchInput("AnalogTriggerRight2", "P2 Analog Gun Right Trigger", Game::INPUT_ANALOG_GUN2, "NONE");

	// Ski controls
	CAnalogInput *skiLeft  = AddAnalogInput("SkiLeft",  "Ski Champ Left",  Game::INPUT_SKI, "NONE");
	CAnalogInput *skiRight = AddAnalogInput("SkiRight", "Ski Champ Right", Game::INPUT_SKI, "NONE");
	CAnalogInput *skiUp    = AddAnalogInput("SkiUp",    "Ski Champ Up",    Game::INPUT_SKI, "NONE");
	CAnalogInput *skiDown  = AddAnalogInput("SkiDown",  "Ski Champ Down",  Game::INPUT_SKI, "NONE");

	skiX          = AddAxisInput  ("SkiX",         "Ski Champ X-Axis",     Game::INPUT_SKI, "NONE", skiLeft, skiRight, 0xFF, 0x80, 0);
	skiY          = AddAxisInput  ("SkiY",         "Ski Champ Y-Axis",     Game::INPUT_SKI, "NONE", skiUp,   skiDown);
	skiPollLeft   = AddSwitchInput("SkiPollLeft",  "Ski Champ Poll Left",  Game::INPUT_SKI, "NONE");
	skiPollRight  = AddSwitchInput("SkiPollRight", "Ski Champ Poll Right", Game::INPUT_SKI, "NONE");
	skiSelect1    = AddSwitchInput("SkiSelect1",   "Ski Champ Select 1",   Game::INPUT_SKI, "NONE");
	skiSelect2    = AddSwitchInput("SkiSelect2",   "Ski Champ Select 2",   Game::INPUT_SKI, "NONE");
	skiSelect3    = AddSwitchInput("SkiSelect3",   "Ski Champ Select 3",   Game::INPUT_SKI, "NONE");

	// Magical truck controls
	CAnalogInput *magicalLeverUp1   = AddAnalogInput("MagicalLeverUp1",   "P1 Magical Lever Up",    Game::INPUT_MAGTRUCK, "NONE");
	CAnalogInput *magicalLeverDown1 = AddAnalogInput("MagicalLeverDown1", "P1 Magical Lever Down",  Game::INPUT_MAGTRUCK, "NONE");
	CAnalogInput *magicalLeverUp2   = AddAnalogInput("MagicalLeverUp2",   "P2 Magical Lever Up",    Game::INPUT_MAGTRUCK, "NONE");
	CAnalogInput *magicalLeverDown2 = AddAnalogInput("MagicalLeverDown2", "P2 Magical Lever Down",  Game::INPUT_MAGTRUCK, "NONE");

	magicalLever1 = AddAxisInput(   "MagicalLever1", "P1 Magical Lever",  Game::INPUT_MAGTRUCK, "NONE", magicalLeverUp1, magicalLeverDown1, 0xFF, 0x80, 0);
	magicalLever2 = AddAxisInput(   "MagicalLever2", "P2 Magical Lever",  Game::INPUT_MAGTRUCK, "NONE", magicalLeverUp2, magicalLeverDown2, 0xFF, 0x80, 0);
	magicalPedal1 = AddSwitchInput( "MagicalPedal1", "P1 Magical Pedal",  Game::INPUT_MAGTRUCK, "NONE");
	magicalPedal2 = AddSwitchInput( "MagicalPedal2", "P2 Magical Pedal",  Game::INPUT_MAGTRUCK, "NONE");

	// Sega Bass Fishing controls
	CAnalogInput *fishingRodLeft    = AddAnalogInput("FishingRodLeft",    "Rod Left",     Game::INPUT_FISHING, "NONE");
	CAnalogInput *fishingRodRight   = AddAnalogInput("FishingRodRight",   "Rod Right",    Game::INPUT_FISHING, "NONE");
	CAnalogInput *fishingRodUp      = AddAnalogInput("FishingRodUp",      "Rod Up",       Game::INPUT_FISHING, "NONE");
	CAnalogInput *fishingRodDown    = AddAnalogInput("FishingRodDown",    "Rod Down",     Game::INPUT_FISHING, "NONE");
	CAnalogInput *fishingStickLeft  = AddAnalogInput("FishingStickLeft",  "Stick Left",   Game::INPUT_FISHING, "NONE");
	CAnalogInput *fishingStickRight = AddAnalogInput("FishingStickRight", "Stick Right",  Game::INPUT_FISHING, "NONE");
	CAnalogInput *fishingStickUp    = AddAnalogInput("FishingStickUp",    "Stick Up",     Game::INPUT_FISHING, "NONE");
	CAnalogInput *fishingStickDown  = AddAnalogInput("FishingStickDown",  "Stick Down",   Game::INPUT_FISHING, "NONE");

	fishingRodX   = AddAxisInput(   "FishingRodX",    "Rod X-Axis",     Game::INPUT_FISHING, "NONE", fishingRodLeft, fishingRodRight, 0, 0x80, 0xFF);
	fishingRodY   = AddAxisInput(   "FishingRodY",    "Rod Y-Axis",     Game::INPUT_FISHING, "NONE", fishingRodUp, fishingRodDown, 0, 0x80, 0xFF);
	fishingReel   = AddAnalogInput( "FishingReel",    "Reel Speed",     Game::INPUT_FISHING, "NONE");
	fishingStickX = AddAxisInput(   "FishingStickX",  "Stick X-Axis",   Game::INPUT_FISHING, "NONE", fishingStickLeft, fishingStickRight, 0, 0x80, 0xFF);
	fishingStickY = AddAxisInput(   "FishingStickY",  "Stick Y-Axis",   Game::INPUT_FISHING, "NONE", fishingStickUp, fishingStickDown, 0, 0x80, 0xFF);
	fishingCast   = AddSwitchInput( "FishingCast",    "Cast",           Game::INPUT_FISHING, "NONE");
	fishingSelect = AddSwitchInput( "FishingSelect",  "Select",         Game::INPUT_FISHING, "NONE");
	fishingTension= AddAnalogInput("FishingTension", "Tension",	      Game::INPUT_FISHING, "NONE"); // getbass only
}

CInputs::~CInputs()
{
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
		delete *it;
	m_inputs.clear();
}

CSwitchInput *CInputs::AddSwitchInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping, 
	UINT16 offVal, UINT16 onVal)
{
	CSwitchInput *input = new CSwitchInput(id, label, gameFlags, defaultMapping, offVal, onVal);
	m_inputs.push_back(input);
	return input;
}

CAnalogInput *CInputs::AddAnalogInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping, 
	UINT16 minVal, UINT16 maxVal)
{
	CAnalogInput *input = new CAnalogInput(id, label, gameFlags, defaultMapping, minVal, maxVal);
	m_inputs.push_back(input);
	return input;
}

CAxisInput *CInputs::AddAxisInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping, 
	CAnalogInput *axisNeg, CAnalogInput *axisPos, UINT16 minVal, UINT16 offVal, UINT16 maxVal)
{
	CAxisInput *input = new CAxisInput(id, label, gameFlags, defaultMapping, axisNeg, axisPos, minVal, offVal, maxVal);
	m_inputs.push_back(input);
	return input;
}

CGearShift4Input *CInputs::AddGearShift4Input(const char *id, const char *label, unsigned gameFlags, 
	CSwitchInput *shift1, CSwitchInput *shift2, CSwitchInput *shift3, CSwitchInput *shift4, CSwitchInput *shiftN, CSwitchInput *shiftUp, CSwitchInput *shiftDown)
{
	CGearShift4Input *input = new CGearShift4Input(id, label, gameFlags, shift1, shift2, shift3, shift4, shiftN, shiftUp, shiftDown);
	m_inputs.push_back(input);
	return input;
}

CTriggerInput *CInputs::AddTriggerInput(const char *id, const char *label, unsigned gameFlags, 
	CSwitchInput *_trigger, CSwitchInput *offscreen, UINT16 offVal, UINT16 onVal)
{
	CTriggerInput *input = new CTriggerInput(id, label, gameFlags, _trigger, offscreen, offVal, onVal);
	m_inputs.push_back(input);
	return input;
}

void CInputs::PrintHeader(const char *fmt, ...)
{
	char header[1024];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(header, sizeof(header), fmt, vl);
	va_end(vl);

	puts(header);
	const size_t s = strlen(header);
	for (size_t i = 0; i < s; i++)
		putchar('-');
	printf("\n\n");
}

void CInputs::PrintConfigureInputsHelp()
{
	puts("For each control, use the following keys to map the inputs:");
	puts("");
	puts("  Return  Set current input mapping and move to next control");
	puts("  c       Clear current input mapping and remain there");
	puts("  s       Set the current input mapping and remain there");
	puts("  a       Append to current input mapping (for multiple assignments)");
	puts("          and remain there");
	puts("  r       Reset current input mapping to default and remain there");
	puts("  Down    Move onto next control");
	puts("  Up      Go back to previous control");
	puts("  b       Calibrate joystick axes");
	puts("  i       Display information about input system and attached devices");
	puts("  h       Display this help again");
	puts("  q       Finish and save all changes");
	puts("  Esc     Finish without saving any changes");
	puts("");
	puts("To assign input(s), simply press the appropriate key(s), mouse button(s),");
	puts("joystick button(s), or move the mouse along one or more axes, or move a");
	puts("joystick's axis or POV hat controller(s).  The input(s) will be accepted");
	puts("as soon as all pressed keys and buttons are released and all moved mouse");
	puts("and joystick controllers are returned to their rest positions.");
	puts("");
	puts("Notes:");
	puts(" - In order to assign keys the configuration window must on top.");
	puts(" - In order to assign mouse buttons the mouse must be clicked within the");
	puts("   window.");
	puts(" - In order to assign mouse axes, the cursor must first be placed in the");
	puts("   center of the window and then moved in the corresponding direction to the");
	puts("   window's edge and then returned to the center.");
	puts("");
}

unsigned CInputs::Count()
{
	return (unsigned)m_inputs.size();
}

CInput *CInputs::operator[](const unsigned index)
{
	return m_inputs[index];
}

CInput *CInputs::operator[](const char *idOrLabel)
{
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
	{
		if (stricmp((*it)->id, idOrLabel) == 0 || stricmp((*it)->label, idOrLabel) == 0)
			return *it;
	}
	return NULL;
}

CInputSystem *CInputs::GetInputSystem()
{
	return m_system;
}

bool CInputs::Initialize()
{
	// Make sure the input system is initialized too
	if (!m_system->Initialize())
		return false;

	// Initialize all the inputs
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
		(*it)->Initialize(m_system);

	return true;
}

void CInputs::LoadFromConfig(const Util::Config::Node &config)
{
	m_system->LoadFromConfig(config);

	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
		(*it)->LoadFromConfig(config);
}

void CInputs::StoreToConfig(Util::Config::Node *config)
{
	m_system->StoreToConfig(config);

	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
		(*it)->StoreToConfig(config);
}

bool CInputs::ConfigureInputs(const Game &game)
{
	m_system->UngrabMouse();

	// Print header and help message
	uint32_t gameFlags;
	if (!game.name.empty())
	{
		PrintHeader("Configure Inputs for '%s'", game.title.c_str());
		gameFlags = game.inputs;
	}
	else
	{
		PrintHeader("Configure Inputs");
		gameFlags = Game::INPUT_ALL;
	}
	PrintConfigureInputsHelp();

	// Get all inputs to be configured
	vector<CInput*> toConfigure;
	vector<CInput*>::iterator it;
	for (it = m_inputs.begin(); it != m_inputs.end(); ++it)
	{
		if ((*it)->IsConfigurable() && ((*it)->gameFlags & gameFlags))
			toConfigure.push_back(*it);
	}

	// Remember current mappings for each input in case changes need to be undone later
	vector<string> oldMappings(toConfigure.size());
	size_t index = 0;
	for (it = toConfigure.begin(); it != toConfigure.end(); ++it)
		oldMappings[index++] = (*it)->GetMapping();

	const char *groupLabel = NULL;

	bool cancelled = false;
		
	// Loop through all the inputs to be configured
	index = 0;
	while (index < toConfigure.size())
	{
		// Get the current input
		CInput *input = toConfigure[index];

		// If have moved to a new input group, print the group heading
		const char *itGroupLabel = input->GetInputGroup();
		if (groupLabel == NULL || stricmp(groupLabel, itGroupLabel) != 0)
		{
			groupLabel = itGroupLabel;
			printf("%s:\n", groupLabel);
		}

Redisplay:
		// Print the input label, current input mapping and available options
		if (index > 0)
			printf(" %s [%s]: Ret/c/s/a/r/Up/Down/b/h/q/Esc? ", input->label, input->GetMapping());
		else
			printf(" %s [%s]: Ret/c/s/a/r/Down/h/b/q/Esc? ", input->label, input->GetMapping());
		fflush(stdout);	// required on terminals that use buffering

		// Loop until user has selected a valid option
		bool done = false;
		while (!done)
		{
			char mapping[50];
			// Wait for input from user
			if (!m_system->ReadMapping(mapping, 50, false, READ_KEYBOARD|READ_MERGE, uiExit->GetMapping()))
			{
				// If user pressed aborted input, then undo all changes and finish configuration
				index = 0;
				for (it = toConfigure.begin(); it != toConfigure.end(); ++it)
				{
					(*it)->SetMapping(oldMappings[index].c_str());
					index++;
				}

				cancelled = true;
				goto Finish;
			}

			if (stricmp(mapping, "KEY_RETURN") == 0 || stricmp(mapping, "KEY_S") == 0)
			{
				// Set the input mapping
				printf("Setting... ");
				fflush(stdout);	// required on terminals that use buffering
				if (input->Configure(false, uiExit->GetMapping()))
				{
					puts(input->GetMapping());
					if (stricmp(mapping, "KEY_RETURN") == 0)
						index++;
					done = true;
				}
				else
				{
					puts("[Cancelled]");
					goto Redisplay;
				}
			}
			else if (stricmp(mapping, "KEY_A") == 0)
			{
				// Append to the input mapping(s)
				printf("Appending... ");
				fflush(stdout);	// required on terminals that use buffering
				if (input->Configure(true, uiExit->GetMapping()))
					puts(input->GetMapping());
				else
					puts("[Cancelled]");
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_C") == 0)
			{
				// Clear the input mapping(s)
				input->SetMapping("NONE");
				puts("Cleared");
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_R") == 0)
			{
				// Reset the input mapping(s) to the default
				input->ResetToDefaultMapping();
				puts("Reset");
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_DOWN") == 0)
			{
				// Move forward to the next mapping
				puts("");
				index++;
				done = true;
			}
			else if (stricmp(mapping, "KEY_UP") == 0)
			{
				// Move back to the previous mapping
				if (index > 0)
				{
					puts("");
					index--;
					done = true;
				}
			}
			else if (stricmp(mapping, "KEY_HOME") == 0)
			{
				// Move to first input
				puts("");
				index = 0;
				done = true;
			}
			else if (stricmp(mapping, "KEY_B") == 0)
			{
				// Calibrate joysticks
				printf("\n\n");
				CalibrateJoysticks();
				puts("");
				goto Redisplay;
			}	
			else if (stricmp(mapping, "KEY_I") == 0)
			{
				// Print info about input system
				printf("\n\n");
				m_system->PrintSettings();
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_H") == 0)
			{
				// Print the help message again
				printf("\n\n");
				PrintConfigureInputsHelp();
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_Q") == 0)
				goto Finish;
		}
	}

Finish:
	printf("\n\n");

	m_system->GrabMouse();
	return !cancelled;
}

bool CInputs::ConfigureInputs(const Game &game, unsigned dispX, unsigned dispY, unsigned dispW, unsigned dispH)
{
	// Let the input system know the display geometry
	m_system->SetDisplayGeom(dispX, dispY, dispW, dispH);

	return ConfigureInputs(game);
}

void CInputs::CalibrateJoysticks()
{
	int numJoys = m_system->GetNumJoysticks();
	if (numJoys == 0 || numJoys == ANY_JOYSTICK)
		puts("No joysticks attached to calibrate!");
	else
	{
		puts("Choose joystick to calibrate (or press Esc to cancel):");
		if (numJoys > 10)
			numJoys = 10;
		for (int i = 0; i < numJoys; i++)
		{
			const JoyDetails *joyDetails = m_system->GetJoyDetails(i);
			unsigned dispNum = (i == 9 ? 0 : i + 1);
			printf(" %u: %s\n", dispNum, joyDetails->name);
		}

		char mapping[50];
		while (m_system->ReadMapping(mapping, 50, false, READ_KEYBOARD|READ_MERGE, uiExit->GetMapping()))
		{
			if (strlen(mapping) != 5 || strncmp(mapping, "KEY_", 4) != 0)
				continue;
			char c = mapping[4];
			if (!isdigit(c))
				continue;
			int joyNum = c - '0';
			joyNum = (joyNum == 0 ? 9 : joyNum - 1);
			if (joyNum >= numJoys)
				continue;
			puts("");
			CalibrateJoystick(joyNum);
			return;
		}
	}
}

void CInputs::CalibrateJoystick(int joyNum)
{
	const JoyDetails *joyDetails = m_system->GetJoyDetails(joyNum);
	if (joyDetails == NULL || joyDetails->numAxes == 0)
	{
		puts("No axes available to calibrate on joystick!");
		return;
	}

	printf("Calibrating joystick '%s'.\n\n", joyDetails->name);

	puts("Choose axis to calibrate (or press Esc to cancel):");
	vector<unsigned> axisNumList;
	for (unsigned axisNum = 0; axisNum < NUM_JOY_AXES; axisNum++)
	{
		if (!joyDetails->hasAxis[axisNum])
			continue;
		axisNumList.push_back(axisNum);
		printf(" %u: %s\n", (unsigned) axisNumList.size(), joyDetails->axisName[axisNum]);
	}
	printf(" 0: Unsure - help me choose...\n");
	
	char mapping[50];
	while (m_system->ReadMapping(mapping, 50, false, READ_KEYBOARD|READ_MERGE, uiExit->GetMapping()))
	{
		unsigned axisNum;
		if (stricmp(mapping, "KEY_0") == 0)
		{
			puts("");
			if (!m_system->DetectJoystickAxis(joyNum, axisNum))
				return;
		}
		else
		{
			if (strlen(mapping) != 5 || strncmp(mapping, "KEY_", 4) != 0)
				continue;
			char c = mapping[4];
			if (!isdigit(c))
				continue;
			unsigned optNum = c - '0';
			if (optNum == 0 || optNum > axisNumList.size())
				continue;
			axisNum = axisNumList[optNum - 1];
		}
		puts("");
		if (m_system->CalibrateJoystickAxis(joyNum, axisNum))
		{
			for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
				(*it)->InputSystemChanged();
		}
		return;
	}
}

void CInputs::PrintInputs(const Game *game)
{
	// Print header
	int gameFlags;
	if (game != NULL)
	{
		PrintHeader("Input Assignments for %s", game->title.c_str());
		gameFlags = game->inputs;
	}
	else
	{
		PrintHeader("Input Assignments");
		gameFlags = Game::INPUT_ALL;
	}

	const char *groupLabel = NULL;
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
	{
		if (!(*it)->IsConfigurable() || !((*it)->gameFlags & gameFlags))
			continue;

		const char *itGroupLabel = (*it)->GetInputGroup();
		if (groupLabel == NULL || stricmp(groupLabel, itGroupLabel) != 0)
		{
			groupLabel = itGroupLabel;
			printf("%s:\n", groupLabel);
		}

		printf(" %s = %s\n", (*it)->label, (*it)->GetMapping());
	}

	puts("");
}

bool CInputs::Poll(const Game *game, unsigned dispX, unsigned dispY, unsigned dispW, unsigned dispH)
{
	// Update the input system with the current display geometry
	m_system->SetDisplayGeom(dispX, dispY, dispW, dispH);

	// Poll the input system
	if (!m_system->Poll())
		return false;

	// Poll all UI inputs and all the inputs used by the current game, or all inputs if game is NULL
	uint32_t gameFlags = game ? game->inputs : Game::INPUT_ALL;
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
	{
		if ((*it)->IsUIInput() || ((*it)->gameFlags & gameFlags))
			(*it)->Poll();
	}
	return true;
}

void CInputs::DumpState(const Game *game)
{
	// Print header
	int gameFlags;
	if (game != NULL)
	{
		PrintHeader("Input States for %s", game->title.c_str());
		gameFlags = game->inputs;
	}
	else
	{
		PrintHeader("Input States");
		gameFlags = Game::INPUT_ALL;
	}

	// Loop through the inputs used by the current game, or all inputs if game is NULL, and dump their values to stdout
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); ++it)
	{
		if (!(*it)->IsUIInput() && !((*it)->gameFlags & gameFlags))
			continue;

		if ((*it)->IsVirtual())
			printf("%s = (%d)\n", (*it)->id, (*it)->value);
		else
			printf("%s [%s] = (%d)\n", (*it)->id, (*it)->GetMapping(), (*it)->value);
	}
}
