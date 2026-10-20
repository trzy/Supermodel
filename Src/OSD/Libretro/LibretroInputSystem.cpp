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
 * LibretroInputSystem.cpp
 *
 * Implementation of CLibretroInputSystem. See LibretroInputSystem.h for how
 * libretro devices are presented to Supermodel's input layer.
 */

#include "LibretroInputSystem.h"

#include "Inputs/Input.h"
#include "Supermodel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

CLibretroInputSystem::CLibretroInputSystem()
  : CInputSystem("libretro")
{
  for (int port = 0; port < NUM_PORTS; port++)
  {
    m_portDevice[port] = RETRO_DEVICE_JOYPAD;
    m_rumbleStrong[port] = 0;
    m_rumbleWeak[port] = 0;
    m_appliedStrong[port] = 0;
    m_appliedWeak[port] = 0;

    JoyDetails &details = m_joyDetails[port];
    memset(&details, 0, sizeof(details));
    snprintf(details.name, sizeof(details.name), "RetroPad %d", port + 1);
    details.numAxes = 6;
    details.numPOVs = 1;
    details.numButtons = 16;
    details.hasFFeedback = true;
    static const char *axisNames[] = { "Left Stick X", "Left Stick Y", "L2 Trigger", "Right Stick X", "Right Stick Y", "R2 Trigger" };
    for (int axis = 0; axis < 6; axis++)
    {
      details.hasAxis[axis] = true;
      snprintf(details.axisName[axis], sizeof(details.axisName[axis]), "%s", axisNames[axis]);
    }
    details.axisHasFF[AXIS_X] = true;
  }

  for (int port = 0; port < NUM_GUN_PORTS; port++)
  {
    MouseDetails &details = m_mouseDetails[port];
    memset(&details, 0, sizeof(details));
    snprintf(details.name, sizeof(details.name), "Light Gun %d", port + 1);
    details.isAbsolute = true;
  }
}

CLibretroInputSystem::~CLibretroInputSystem()
{
}

void CLibretroInputSystem::SetInputStateCallback(retro_input_state_t inputState)
{
  m_inputState = inputState;
}

void CLibretroInputSystem::SetBitmasksSupported(bool supported)
{
  m_bitmasks = supported;
}

void CLibretroInputSystem::SetPortDevice(unsigned port, unsigned device)
{
  if (port < NUM_PORTS)
    m_portDevice[port] = device;
}

void CLibretroInputSystem::SetFrameSize(unsigned width, unsigned height)
{
  m_frameWidth = std::max(1u, width);
  m_frameHeight = std::max(1u, height);
}

void CLibretroInputSystem::SetRumbleInterface(retro_set_rumble_state_t setRumbleState)
{
  m_setRumbleState = setRumbleState;
}

void CLibretroInputSystem::SetRumbleStrength(unsigned percent)
{
  m_rumbleStrength = std::min(100u, percent);
}

void CLibretroInputSystem::ApplyRumble()
{
  if (!m_setRumbleState)
    return;
  for (unsigned port = 0; port < NUM_PORTS; port++)
  {
    uint32_t strong = m_rumbleStrong[port].load() * m_rumbleStrength / 100;
    uint32_t weak = m_rumbleWeak[port].load() * m_rumbleStrength / 100;
    if (strong != m_appliedStrong[port])
    {
      m_setRumbleState(port, RETRO_RUMBLE_STRONG, (uint16_t) strong);
      m_appliedStrong[port] = strong;
    }
    if (weak != m_appliedWeak[port])
    {
      m_setRumbleState(port, RETRO_RUMBLE_WEAK, (uint16_t) weak);
      m_appliedWeak[port] = weak;
    }
  }
}

/******************************************************************************
 Polling
******************************************************************************/

void CLibretroInputSystem::PollPad(unsigned port)
{
  PadState &pad = m_pads[port];
  pad = PadState();

  if (m_bitmasks)
    pad.buttons = (uint16_t) m_inputState(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
  else
  {
    for (unsigned id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
    {
      if (m_inputState(port, RETRO_DEVICE_JOYPAD, 0, id))
        pad.buttons |= (uint16_t) (1u << id);
    }
  }

  // Analog sticks: libretro range is -0x8000..0x7FFF, positive is right/down,
  // which is what Supermodel expects from a joystick
  pad.axes[AXIS_X]  = m_inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
  pad.axes[AXIS_Y]  = m_inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
  pad.axes[AXIS_RX] = m_inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
  pad.axes[AXIS_RY] = m_inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

  // Analog triggers: 0..0x7FFF. Frontends without analog triggers return 0;
  // the digital L2/R2 buttons are mapped as well, so they still work.
  pad.axes[AXIS_Z]  = m_inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_L2);
  pad.axes[AXIS_RZ] = m_inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2);
  for (int axis = 0; axis < NUM_JOY_AXES; axis++)
    pad.axes[axis] = std::max(-32768, std::min(32767, pad.axes[axis]));
}

void CLibretroInputSystem::PollGun(unsigned port)
{
  GunState &gun = m_guns[port];
  gun.buttons = 0;
  if (m_portDevice[port] != RETRO_DEVICE_LIGHTGUN)
    return;

  bool offscreen = m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) != 0;
  bool trigger = m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) != 0;
  bool reload = m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD) != 0;

  if (!offscreen)
  {
    // Screen coordinates are -0x7FFF..0x7FFF across the frame; keep the last
    // position while the gun points off screen
    int sx = m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
    int sy = m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
    gun.x = (int) (((int64_t) sx + 0x7FFF) * (int64_t) (m_frameWidth - 1) / 0xFFFE);
    gun.y = (int) (((int64_t) sy + 0x7FFF) * (int64_t) (m_frameHeight - 1) / 0xFFFE);
  }

  if (trigger && !offscreen)
    gun.buttons |= 1 << 0;  // left: trigger
  if (reload || (trigger && offscreen))
    gun.buttons |= 1 << 2;  // right: point off screen (reload)
  if (m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A))
    gun.buttons |= 1 << 1;  // middle
  if (m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_B))
    gun.buttons |= 1 << 3;
  if (m_inputState(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_C))
    gun.buttons |= 1 << 4;
}

bool CLibretroInputSystem::Poll()
{
  if (!m_inputState)
    return true;
  for (unsigned port = 0; port < NUM_PORTS; port++)
    PollPad(port);
  for (unsigned port = 0; port < NUM_GUN_PORTS; port++)
    PollGun(port);
  return true;
}

/******************************************************************************
 CInputSystem Implementation
******************************************************************************/

bool CLibretroInputSystem::InitializeSystem()
{
  return true;
}

int CLibretroInputSystem::GetKeyIndex(const char *keyName)
{
  (void) keyName;
  return -1;  // no keyboard
}

const char *CLibretroInputSystem::GetKeyName(int keyIndex)
{
  (void) keyIndex;
  return nullptr;
}

bool CLibretroInputSystem::IsKeyPressed(int kbdNum, int keyIndex) const
{
  (void) kbdNum;
  (void) keyIndex;
  return false;
}

int CLibretroInputSystem::GetMouseAxisValue(int mseNum, int axisNum) const
{
  if (mseNum < 0)
    mseNum = 0;
  if (mseNum >= NUM_GUN_PORTS)
    return 0;
  // A port without a light gun reports the centre of the display, which the
  // input layer treats as "no input", so that other sources mapped to the same
  // control (e.g. the analog stick) take over
  bool gun = m_portDevice[mseNum] == RETRO_DEVICE_LIGHTGUN;
  switch (axisNum)
  {
  case AXIS_X: return gun ? m_guns[mseNum].x : (int) (m_dispX + m_dispW / 2);
  case AXIS_Y: return gun ? m_guns[mseNum].y : (int) (m_dispY + m_dispH / 2);
  default:     return 0;
  }
}

int CLibretroInputSystem::GetMouseWheelDir(int mseNum) const
{
  (void) mseNum;
  return 0;
}

bool CLibretroInputSystem::IsMouseButPressed(int mseNum, int butNum) const
{
  if (mseNum == ANY_MOUSE)
  {
    for (int i = 0; i < NUM_GUN_PORTS; i++)
    {
      if (IsMouseButPressed(i, butNum))
        return true;
    }
    return false;
  }
  if (mseNum >= NUM_GUN_PORTS || butNum < 0 || butNum >= NUM_MOUSE_BUTTONS)
    return false;
  return (m_guns[mseNum].buttons >> butNum) & 1;
}

int CLibretroInputSystem::GetJoyAxisValue(int joyNum, int axisNum) const
{
  if (joyNum < 0 || joyNum >= NUM_PORTS || axisNum < 0 || axisNum >= NUM_JOY_AXES)
    return 0;
  return m_pads[joyNum].axes[axisNum];
}

bool CLibretroInputSystem::IsJoyPOVInDir(int joyNum, int povNum, int povDir) const
{
  if (joyNum < 0 || joyNum >= NUM_PORTS || povNum != 0)
    return false;
  uint16_t buttons = m_pads[joyNum].buttons;
  switch (povDir)
  {
  case POV_UP:    return (buttons >> RETRO_DEVICE_ID_JOYPAD_UP) & 1;
  case POV_DOWN:  return (buttons >> RETRO_DEVICE_ID_JOYPAD_DOWN) & 1;
  case POV_LEFT:  return (buttons >> RETRO_DEVICE_ID_JOYPAD_LEFT) & 1;
  case POV_RIGHT: return (buttons >> RETRO_DEVICE_ID_JOYPAD_RIGHT) & 1;
  default:        return false;
  }
}

bool CLibretroInputSystem::IsJoyButPressed(int joyNum, int butNum) const
{
  if (joyNum < 0 || joyNum >= NUM_PORTS || butNum < 0 || butNum > RETRO_DEVICE_ID_JOYPAD_R3)
    return false;
  // The D-pad is reported as a POV hat, not as buttons
  if (butNum >= RETRO_DEVICE_ID_JOYPAD_UP && butNum <= RETRO_DEVICE_ID_JOYPAD_RIGHT)
    return false;
  return (m_pads[joyNum].buttons >> butNum) & 1;
}

bool CLibretroInputSystem::ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd)
{
  (void) axisNum;
  if (joyNum < 0)
    joyNum = 0;
  if (joyNum >= NUM_PORTS)
    return false;

  uint32_t level = (uint32_t) std::lround(std::min(1.0f, std::fabs(ffCmd.force)) * 65535.0f);
  switch (ffCmd.id)
  {
  case FFStop:
    m_rumbleStrong[joyNum] = 0;
    m_rumbleWeak[joyNum] = 0;
    break;
  case FFConstantForce:
    // Steering wheel torque: the closest a gamepad gets is the strong motor
    m_rumbleStrong[joyNum] = level;
    break;
  case FFVibrate:
    m_rumbleWeak[joyNum] = level;
    break;
  case FFSelfCenter:
  case FFFriction:
  default:
    // Spring and friction effects have no rumble equivalent
    return false;
  }
  return true;
}

int CLibretroInputSystem::GetNumKeyboards() const
{
  return 0;
}

int CLibretroInputSystem::GetNumMice() const
{
  return NUM_GUN_PORTS;
}

int CLibretroInputSystem::GetNumJoysticks() const
{
  return NUM_PORTS;
}

const KeyDetails *CLibretroInputSystem::GetKeyDetails(int kbdNum)
{
  (void) kbdNum;
  return nullptr;
}

const MouseDetails *CLibretroInputSystem::GetMouseDetails(int mseNum)
{
  if (mseNum < 0 || mseNum >= NUM_GUN_PORTS)
    return nullptr;
  return &m_mouseDetails[mseNum];
}

const JoyDetails *CLibretroInputSystem::GetJoyDetails(int joyNum)
{
  if (joyNum < 0 || joyNum >= NUM_PORTS)
    return nullptr;
  return &m_joyDetails[joyNum];
}

void CLibretroInputSystem::SetMouseVisibility(bool visible)
{
  (void) visible;
}
