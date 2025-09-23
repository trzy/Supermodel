/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson
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
 * Inputs.h
 *
 * Header file for CInputs, a class which manages all individual inputs.
 */
 
#ifndef INCLUDED_INPUTS_H
#define INCLUDED_INPUTS_H

#include "InputTypes.h"
#include "Types.h"
#include "Util/NewConfig.h"
#include <vector>
#include <memory>

class CInputSystem;
class CInput;
struct Game;

/*
 * Represents the collection of Model3 inputs.
 */
class CInputs
{
private:
  // Assigned input system
  std::shared_ptr<CInputSystem> m_system;

  // Vector of all created inputs
  std::vector<std::shared_ptr<CInput>> m_inputs;

  /*
   * Adds a switch input (eg button) to this collection.
   */ 
  std::shared_ptr<CSwitchInput> AddSwitchInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping,
    UINT16 offVal = 0x00, UINT16 onVal = 0x01);

  /*
   * Adds an analog input (eg pedal) to this collection.
   */
  std::shared_ptr<CAnalogInput> AddAnalogInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping,
    UINT16 minVal = 0x00, UINT16 maxVal = 0xFF);

  /*
   * Adds an axis input (eg jostick axis, light gun axis or steering wheel) to this collection.
   */
  std::shared_ptr<CAxisInput> AddAxisInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping,
      std::shared_ptr<CAnalogInput> axisNeg, std::shared_ptr<CAnalogInput> axisPos, UINT16 minVal = 0x00, UINT16 offVal = 0x80, UINT16 maxVal = 0xFF);

  /*
   * Adds a 4-gear shifter input to this collection.
   */
  std::shared_ptr<CGearShift4Input> AddGearShift4Input(const char *id, const char *label, unsigned gameFlags,
      std::shared_ptr<CSwitchInput> shift1, std::shared_ptr<CSwitchInput> shift2, std::shared_ptr<CSwitchInput> shift3, std::shared_ptr<CSwitchInput> shift4, std::shared_ptr<CSwitchInput> shiftN, std::shared_ptr<CSwitchInput> shiftUp, std::shared_ptr<CSwitchInput> shiftDown);

  /*
   * Adds a lightgun trigger input to this collection.
   */
  std::shared_ptr<CTriggerInput> AddTriggerInput(const char *id, const char *label, unsigned gameFlags,
      std::shared_ptr<CSwitchInput> trigger, std::shared_ptr<CSwitchInput> offscreen, UINT16 offVal = 0x00, UINT16 onVal = 0x01);

  void PrintHeader(const char *fmt, ...);

  void PrintConfigureInputsHelp();

public:
  // UI controls
  std::shared_ptr<CSwitchInput> uiExit;
  std::shared_ptr<CSwitchInput> uiReset;
  std::shared_ptr<CSwitchInput> uiPause;
  std::shared_ptr<CSwitchInput> uiFullScreen;
  std::shared_ptr<CSwitchInput> uiSaveState;
  std::shared_ptr<CSwitchInput> uiChangeSlot;
  std::shared_ptr<CSwitchInput> uiLoadState;
  std::shared_ptr<CSwitchInput> uiMusicVolUp;
  std::shared_ptr<CSwitchInput> uiMusicVolDown;
  std::shared_ptr<CSwitchInput> uiSoundVolUp;
  std::shared_ptr<CSwitchInput> uiSoundVolDown;
  std::shared_ptr<CSwitchInput> uiClearNVRAM;
  std::shared_ptr<CSwitchInput> uiSelectCrosshairs;
  std::shared_ptr<CSwitchInput> uiToggleFrLimit;
  std::shared_ptr<CSwitchInput> uiDumpInpState;
  std::shared_ptr<CSwitchInput> uiDumpTimings;
  std::shared_ptr<CSwitchInput> uiScreenshot;
#ifdef SUPERMODEL_DEBUGGER
  std::shared_ptr<CSwitchInput> uiEnterDebugger;
#endif

  // Common controls between all games
  std::shared_ptr<CSwitchInput> coin[2];
  std::shared_ptr<CSwitchInput> start[2];
  std::shared_ptr<CSwitchInput> test[2];
  std::shared_ptr<CSwitchInput> service[2];

  // Joysticks (players 1 and 2)
  std::shared_ptr<CSwitchInput> up[2];
  std::shared_ptr<CSwitchInput> down[2];
  std::shared_ptr<CSwitchInput> left[2];
  std::shared_ptr<CSwitchInput> right[2];

  // Fighting game controls (players 1 and 2)
  std::shared_ptr<CSwitchInput> punch[2];
  std::shared_ptr<CSwitchInput> kick[2];
  std::shared_ptr<CSwitchInput> guard[2];
  std::shared_ptr<CSwitchInput> escape[2];
  
  // Spikeout controls
  std::shared_ptr<CSwitchInput> shift;
  std::shared_ptr<CSwitchInput> beat;
  std::shared_ptr<CSwitchInput> charge;
  std::shared_ptr<CSwitchInput> jump;

  // Soccer game controls (players 1 and 2)
  std::shared_ptr<CSwitchInput> shortPass[2];
  std::shared_ptr<CSwitchInput> longPass[2];
  std::shared_ptr<CSwitchInput> shoot[2];

  // Vehicle controls
  std::shared_ptr<CAxisInput> steering;
  std::shared_ptr<CAnalogInput> accelerator;
  std::shared_ptr<CAnalogInput> brake;

  // VR view buttons: VR1 Red, VR2 Blue, VR3 Yellow, VR4 Green
  std::shared_ptr<CSwitchInput> vr[4];
 
  // Up/down gear shift
  std::shared_ptr<CSwitchInput> gearShiftUp;
  std::shared_ptr<CSwitchInput> gearShiftDown;
  
  // 4-speed gear shift
  std::shared_ptr<CGearShift4Input> gearShift4;

  // Rally controls
  std::shared_ptr<CSwitchInput> viewChange;
  std::shared_ptr<CSwitchInput> handBrake;

  // Harley Davidson controls
  std::shared_ptr<CAnalogInput> rearBrake;
  std::shared_ptr<CSwitchInput> musicSelect;

  // Twin joysticks (individually mapped version; 1 = left stick, 2 = right stick)
  std::shared_ptr<CSwitchInput> twinJoyLeft1;
  std::shared_ptr<CSwitchInput> twinJoyLeft2;
  std::shared_ptr<CSwitchInput> twinJoyRight1;
  std::shared_ptr<CSwitchInput> twinJoyRight2;
  std::shared_ptr<CSwitchInput> twinJoyUp1;
  std::shared_ptr<CSwitchInput> twinJoyUp2;
  std::shared_ptr<CSwitchInput> twinJoyDown1;
  std::shared_ptr<CSwitchInput> twinJoyDown2;
  std::shared_ptr<CSwitchInput> twinJoyShot1;
  std::shared_ptr<CSwitchInput> twinJoyShot2;
  std::shared_ptr<CSwitchInput> twinJoyTurbo1;
  std::shared_ptr<CSwitchInput> twinJoyTurbo2;
  
  // Twin joysticks (macro mapping, for users w/out dual joysticks) 
  std::shared_ptr<CSwitchInput> twinJoyTurnLeft;
  std::shared_ptr<CSwitchInput> twinJoyTurnRight;
  std::shared_ptr<CSwitchInput> twinJoyStrafeLeft;
  std::shared_ptr<CSwitchInput> twinJoyStrafeRight;
  std::shared_ptr<CSwitchInput> twinJoyForward;
  std::shared_ptr<CSwitchInput> twinJoyReverse;
  std::shared_ptr<CSwitchInput> twinJoyJump;
  std::shared_ptr<CSwitchInput> twinJoyCrouch;
  
  // Analog joystick
  std::shared_ptr<CAxisInput> analogJoyX;
  std::shared_ptr<CAxisInput> analogJoyY;
  std::shared_ptr<CSwitchInput> analogJoyTrigger1;
  std::shared_ptr<CSwitchInput> analogJoyTrigger2;
  std::shared_ptr<CSwitchInput> analogJoyEvent1;
  std::shared_ptr<CSwitchInput> analogJoyEvent2;

  // Light gun controls (players 1 and 2)
  std::shared_ptr<CAxisInput> gunX[2];
  std::shared_ptr<CAxisInput> gunY[2];
  std::shared_ptr<CTriggerInput> trigger[2];
  
  // Analog gun controls (players 1 and 2)
  std::shared_ptr<CAxisInput> analogGunX[2];
  std::shared_ptr<CAxisInput> analogGunY[2];
  std::shared_ptr<CSwitchInput> analogTriggerLeft[2];
  std::shared_ptr<CSwitchInput> analogTriggerRight[2];

  // Ski Champ controls
  std::shared_ptr<CAxisInput> skiX;
  std::shared_ptr<CAxisInput> skiY;
  std::shared_ptr<CSwitchInput> skiPollLeft;
  std::shared_ptr<CSwitchInput> skiPollRight;
  std::shared_ptr<CSwitchInput> skiSelect1;
  std::shared_ptr<CSwitchInput> skiSelect2;
  std::shared_ptr<CSwitchInput> skiSelect3;

  // Magical Truck Adventure controls
  std::shared_ptr<CAxisInput> magicalLever1;
  std::shared_ptr<CAxisInput> magicalLever2;
  std::shared_ptr<CSwitchInput> magicalPedal1;
  std::shared_ptr<CSwitchInput> magicalPedal2;

  // Sega Bass Fishing controls
  std::shared_ptr<CAxisInput> fishingRodX;
  std::shared_ptr<CAxisInput> fishingRodY;
  std::shared_ptr<CAnalogInput> fishingReel;
  std::shared_ptr<CAxisInput> fishingStickX;
  std::shared_ptr<CAxisInput> fishingStickY;
  std::shared_ptr<CSwitchInput> fishingCast;
  std::shared_ptr<CSwitchInput> fishingSelect;
  std::shared_ptr<CAnalogInput> fishingTension;

  /*
   * Creates a set of inputs with the given input system.
   */
  CInputs(std::shared_ptr<CInputSystem> system);
  
  /*
   * CInputs destructor.
   */
  ~CInputs();

  /*
   * Returns the number of available inputs.
   */
  unsigned Count();

  /*
   * Returns the input with the given index.
   */
  std::shared_ptr<CInput> operator[](const unsigned index);

  /*
   * Returns the input with the given id or label.
   */
  std::shared_ptr<CInput> operator[](const char *idOrLabel);

  /*
   * Returns the assigned input system.
   */
  std::shared_ptr<CInputSystem> GetInputSystem();

  /*
   * Initializes the inputs.  Must be called before any other methods are used.
   */ 
  bool Initialize();

  /*
   * Loads the input mapping assignments from the given config object.
   */
  void LoadFromConfig(const Util::Config::Node &config);
  
  /*
   * Stores the current input mapping assignments to the given config object.
   */
  void StoreToConfig(Util::Config::Node *config);

  /*
   * Configures the current input mapping assignments for the given game, or all inputs if game is NULL, by asking the user for input.
   * Returns true if the inputs were configured okay or false if the user exited without requesting to save changes.
   */
  bool ConfigureInputs(const Game &game);
  
  /*
   * Configures the current input mapping assignments for the given game, or all inputs if game is NULL, by asking the user for input.
   * Takes display geometry if this has not been set previously by a call to Poll().
   * Returns true if the inputs were configured okay or false if the user exited without requesting to save changes.
   */
  bool ConfigureInputs(const Game &game, unsigned dispX, unsigned dispY, unsigned dispW, unsigned dispH);

  std::vector<std::shared_ptr<CInput>> GetGameInputs(const Game& game);

  void CalibrateJoysticks();

  void CalibrateJoystick(int joyNum);

  /*
   * Prints to stdout the current input mapping assignments for the given game, or all inputs if game is NULL.
   */
  void PrintInputs(const Game *game);

  /*
   * Polls (updates) the inputs for the given game, or all inputs if game is NULL, updating their values from their respective input sources.
   * First the input system is polled (CInputSystem.Poll()) and then each input is polled (CInput.Poll()).
   */
  bool Poll(const Game *game, unsigned dispX, unsigned dispY, unsigned dispW, unsigned dispH);

  /*
   * Prints the current values of the inputs for the given game, or all inputs if game is NULL, to stdout for debugging purposes.
   */
  void DumpState(const Game *game);
};

#endif  // INCLUDED_INPUTS_H
