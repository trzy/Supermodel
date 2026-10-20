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
 * LibretroInputSystem.h
 *
 * Input system that reads the libretro frontend's input devices. Each
 * libretro port is presented to Supermodel's input layer as a joystick, so
 * that all of the existing input logic (analog ramps, axes, gear shifters,
 * light gun triggers) is reused unchanged:
 *
 *   Port n RetroPad   -> JOY<n+1>
 *     BUTTON<id+1>      RetroPad button RETRO_DEVICE_ID_JOYPAD_<id>
 *                       (B=1 Y=2 SELECT=3 START=4 A=9 X=10 L=11 R=12
 *                        L2=13 R2=14 L3=15 R3=16)
 *     POV1_UP/DOWN/...  D-pad
 *     XAXIS/YAXIS       left analog stick
 *     RXAXIS/RYAXIS     right analog stick
 *     ZAXIS/RZAXIS      analog L2/R2 triggers (0 = released, 32767 = fully pressed)
 *
 *   Port n Lightgun   -> MOUSE<n+1> (ports 1 and 2 only)
 *     XAXIS/YAXIS       absolute screen position
 *     LEFT_BUTTON       trigger (on screen)
 *     RIGHT_BUTTON      reload, or trigger pulled while pointing off screen
 *     MIDDLE_BUTTON     Aux A
 *     BUTTONX1/X2       Aux B/Aux C
 *
 * Keyboards are not used: controls are remapped with the frontend's own
 * controller configuration.
 */

#ifndef INCLUDED_LIBRETROINPUTSYSTEM_H
#define INCLUDED_LIBRETROINPUTSYSTEM_H

#include "Inputs/InputSystem.h"
#include "libretro.h"

#include <atomic>
#include <cstdint>

class CLibretroInputSystem : public CInputSystem
{
public:
  static constexpr int NUM_PORTS = 4;      // RetroPads
  static constexpr int NUM_GUN_PORTS = 2;  // ports that may hold a light gun

  CLibretroInputSystem();
  ~CLibretroInputSystem();

  void SetInputStateCallback(retro_input_state_t inputState);
  void SetBitmasksSupported(bool supported);
  void SetPortDevice(unsigned port, unsigned device);

  // Size in pixels of the whole frame rendered by the core (the light gun
  // coordinates span this area)
  void SetFrameSize(unsigned width, unsigned height);

  // Rumble (force feedback) output
  void SetRumbleInterface(retro_set_rumble_state_t setRumbleState);
  void SetRumbleStrength(unsigned percent);
  void ApplyRumble();   // call from the frontend thread once per frame

  // CInputSystem
  int GetNumKeyboards() const override;
  int GetNumMice() const override;
  int GetNumJoysticks() const override;
  const KeyDetails *GetKeyDetails(int kbdNum) override;
  const MouseDetails *GetMouseDetails(int mseNum) override;
  const JoyDetails *GetJoyDetails(int joyNum) override;
  bool Poll() override;
  void SetMouseVisibility(bool visible) override;

protected:
  bool InitializeSystem() override;
  int GetKeyIndex(const char *keyName) override;
  const char *GetKeyName(int keyIndex) override;
  bool IsKeyPressed(int kbdNum, int keyIndex) const override;
  int GetMouseAxisValue(int mseNum, int axisNum) const override;
  int GetMouseWheelDir(int mseNum) const override;
  bool IsMouseButPressed(int mseNum, int butNum) const override;
  int GetJoyAxisValue(int joyNum, int axisNum) const override;
  bool IsJoyPOVInDir(int joyNum, int povNum, int povDir) const override;
  bool IsJoyButPressed(int joyNum, int butNum) const override;
  bool ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd) override;

private:
  struct PadState
  {
    uint16_t buttons = 0;           // bit n = RETRO_DEVICE_ID_JOYPAD_<n>
    int axes[NUM_JOY_AXES] = {};    // X, Y, Z, RX, RY, RZ, S1, S2
  };

  struct GunState
  {
    int x = 0;                      // pixels within the frame
    int y = 0;
    uint8_t buttons = 0;            // bit n = mouse button n
  };

  retro_input_state_t m_inputState = nullptr;
  retro_set_rumble_state_t m_setRumbleState = nullptr;
  bool m_bitmasks = false;
  unsigned m_portDevice[NUM_PORTS];
  unsigned m_frameWidth = 496;
  unsigned m_frameHeight = 384;

  PadState m_pads[NUM_PORTS];
  GunState m_guns[NUM_GUN_PORTS];
  JoyDetails m_joyDetails[NUM_PORTS];
  MouseDetails m_mouseDetails[NUM_GUN_PORTS];

  // Force feedback is requested by the drive board thread and applied by the
  // frontend thread (rumble values 0-65535, strong and weak motor)
  unsigned m_rumbleStrength = 100;
  std::atomic<uint32_t> m_rumbleStrong[NUM_PORTS];
  std::atomic<uint32_t> m_rumbleWeak[NUM_PORTS];
  uint32_t m_appliedStrong[NUM_PORTS];
  uint32_t m_appliedWeak[NUM_PORTS];

  void PollPad(unsigned port);
  void PollGun(unsigned port);
};

#endif  // INCLUDED_LIBRETROINPUTSYSTEM_H
