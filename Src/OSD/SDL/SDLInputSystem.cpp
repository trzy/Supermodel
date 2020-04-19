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
 * SDLInputSystem.cpp
 *
 * Implementation of SDL input system.
 *
 * TODO:
 * -----
 * - Implement multiple keyboard and mouse support.
 */

#include "SDLInputSystem.h"
#include "Supermodel.h"

#include <vector>
using namespace std;

SDLKeyMapStruct CSDLInputSystem::s_keyMap[] =
{
  // General keys
  { "BACKSPACE",      SDL_SCANCODE_BACKSPACE },
  { "TAB",            SDL_SCANCODE_TAB },
  { "CLEAR",          SDL_SCANCODE_CLEAR },
  { "RETURN",         SDL_SCANCODE_RETURN },
  { "PAUSE",          SDL_SCANCODE_PAUSE },
  { "ESCAPE",         SDL_SCANCODE_ESCAPE },
  { "SPACE",          SDL_SCANCODE_SPACE },
  { "QUOTE",          SDL_SCANCODE_APOSTROPHE },
  { "LEFTPAREN",      SDL_SCANCODE_KP_LEFTPAREN },
  { "RIGHTPAREN",     SDL_SCANCODE_KP_RIGHTPAREN },
  { "COMMA",          SDL_SCANCODE_COMMA },
  { "MINUS",          SDL_SCANCODE_MINUS },
  { "PERIOD",         SDL_SCANCODE_PERIOD },
  { "SLASH",          SDL_SCANCODE_SLASH },
  { "0",              SDL_SCANCODE_0 },
  { "1",              SDL_SCANCODE_1 },
  { "2",              SDL_SCANCODE_2 },
  { "3",              SDL_SCANCODE_3 },
  { "4",              SDL_SCANCODE_4 },
  { "5",              SDL_SCANCODE_5 },
  { "6",              SDL_SCANCODE_6 },
  { "7",              SDL_SCANCODE_7 },
  { "8",              SDL_SCANCODE_8 },
  { "9",              SDL_SCANCODE_9 },
  { "SEMICOLON",      SDL_SCANCODE_SEMICOLON },
  { "EQUALS",         SDL_SCANCODE_EQUALS },
  { "LEFTBRACKET",    SDL_SCANCODE_LEFTBRACKET },
  { "BACKSLASH",      SDL_SCANCODE_BACKSLASH },
  { "RIGHTBRACKET",   SDL_SCANCODE_RIGHTBRACKET },
  { "BACKQUOTE",      SDL_SCANCODE_GRAVE },
  { "A",              SDL_SCANCODE_A },
  { "B",              SDL_SCANCODE_B },
  { "C",              SDL_SCANCODE_C },
  { "D",              SDL_SCANCODE_D },
  { "E",              SDL_SCANCODE_E },
  { "F",              SDL_SCANCODE_F },
  { "G",              SDL_SCANCODE_G },
  { "H",              SDL_SCANCODE_H },
  { "I",              SDL_SCANCODE_I },
  { "J",              SDL_SCANCODE_J },
  { "K",              SDL_SCANCODE_K },
  { "L",              SDL_SCANCODE_L },
  { "M",              SDL_SCANCODE_M },
  { "N",              SDL_SCANCODE_N },
  { "O",              SDL_SCANCODE_O },
  { "P",              SDL_SCANCODE_P },
  { "Q",              SDL_SCANCODE_Q },
  { "R",              SDL_SCANCODE_R },
  { "S",              SDL_SCANCODE_S },
  { "T",              SDL_SCANCODE_T },
  { "U",              SDL_SCANCODE_U },
  { "V",              SDL_SCANCODE_V },
  { "W",              SDL_SCANCODE_W },
  { "X",              SDL_SCANCODE_X },
  { "Y",              SDL_SCANCODE_Y },
  { "Z",              SDL_SCANCODE_Z },
  { "DEL",            SDL_SCANCODE_DELETE },

  // Keypad
  { "KEYPAD0",        SDL_SCANCODE_KP_0 },
  { "KEYPAD1",        SDL_SCANCODE_KP_1 },
  { "KEYPAD2",        SDL_SCANCODE_KP_2 },
  { "KEYPAD3",        SDL_SCANCODE_KP_3 },
  { "KEYPAD4",        SDL_SCANCODE_KP_4 },
  { "KEYPAD5",        SDL_SCANCODE_KP_5 },
  { "KEYPAD6",        SDL_SCANCODE_KP_6 },
  { "KEYPAD7",        SDL_SCANCODE_KP_7 },
  { "KEYPAD8",        SDL_SCANCODE_KP_8 },
  { "KEYPAD9",        SDL_SCANCODE_KP_9 },
  { "KEYPADPERIOD",   SDL_SCANCODE_KP_PERIOD },
  { "KEYPADDIVIDE",   SDL_SCANCODE_KP_DIVIDE },
  { "KEYPADMULTIPLY", SDL_SCANCODE_KP_MULTIPLY },
  { "KEYPADMINUS",    SDL_SCANCODE_KP_MINUS },
  { "KEYPADPLUS",     SDL_SCANCODE_KP_PLUS },
  { "KEYPADENTER",    SDL_SCANCODE_KP_ENTER },
  { "KEYPADEQUALS",   SDL_SCANCODE_KP_EQUALS },

  // Arrows + Home/End Pad
  { "UP",             SDL_SCANCODE_UP },
  { "DOWN",           SDL_SCANCODE_DOWN },
  { "RIGHT",          SDL_SCANCODE_RIGHT },
  { "LEFT",           SDL_SCANCODE_LEFT },
  { "INSERT",         SDL_SCANCODE_INSERT },
  { "HOME",           SDL_SCANCODE_HOME },
  { "END",            SDL_SCANCODE_END },
  { "PGUP",           SDL_SCANCODE_PAGEUP },
  { "PGDN",           SDL_SCANCODE_PAGEDOWN },

  // Function Key
  { "F1",             SDL_SCANCODE_F1 },
  { "F2",             SDL_SCANCODE_F2 },
  { "F3",             SDL_SCANCODE_F3 },
  { "F4",             SDL_SCANCODE_F4 },
  { "F5",             SDL_SCANCODE_F5 },
  { "F6",             SDL_SCANCODE_F6 },
  { "F7",             SDL_SCANCODE_F7 },
  { "F8",             SDL_SCANCODE_F8 },
  { "F9",             SDL_SCANCODE_F9 },
  { "F10",            SDL_SCANCODE_F10 },
  { "F11",            SDL_SCANCODE_F11 },
  { "F12",            SDL_SCANCODE_F12 },
  { "F13",            SDL_SCANCODE_F13 },
  { "F14",            SDL_SCANCODE_F14 },
  { "F15",            SDL_SCANCODE_F15 },

  // Modifier Keys
  // Removed Numlock, Capslock and Scrollock as don't seem to be handled well by SDL
  //{ "NUMLOCK",      SDLK_NUMLOCK },
  //{ "CAPSLOCK",     SDL_SCANCODE_CAPSLOCK },
  //{ "SCROLLLOCK",   SDLK_SCROLLOCK },
  { "RIGHTSHIFT",     SDL_SCANCODE_RSHIFT },
  { "LEFTSHIFT",      SDL_SCANCODE_LSHIFT },
  { "RIGHTCTRL",      SDL_SCANCODE_RCTRL },
  { "LEFTCTRL",       SDL_SCANCODE_LCTRL },
  { "RIGHTALT",       SDL_SCANCODE_RALT },
  { "LEFTALT",        SDL_SCANCODE_LALT },
  { "RIGHTMETA",      SDL_SCANCODE_RGUI },
  { "LEFTMETA",       SDL_SCANCODE_LGUI },
  { "ALTGR",          SDL_SCANCODE_MODE },

  // Other
  { "HELP",           SDL_SCANCODE_HELP },
  { "SYSREQ",         SDL_SCANCODE_SYSREQ },
  { "MENU",           SDL_SCANCODE_MENU },
  { "POWER",          SDL_SCANCODE_POWER },
  { "UNDO",           SDL_SCANCODE_UNDO }
};

CSDLInputSystem::CSDLInputSystem()
  : CInputSystem("SDL"),
    m_keyState(nullptr),
    m_mouseX(0),
    m_mouseY(0),
    m_mouseZ(0),
    m_mouseButtons(0)
{
  //
}

CSDLInputSystem::~CSDLInputSystem()
{
  CloseJoysticks();
}

void CSDLInputSystem::OpenJoysticks()
{
  // Open all available joysticks
  int numJoys = SDL_NumJoysticks();
  for (int joyNum = 0; joyNum < numJoys; joyNum++)
  {
    SDL_Joystick *joystick = SDL_JoystickOpen(joyNum);
    if (joystick == nullptr)
    {
      ErrorLog("Unable to open joystick device %d with SDL - skipping joystick.\n", joyNum + 1);
      continue;
    }

    // Gather joystick details (name, num POVs & buttons and which axes are available)
    JoyDetails joyDetails;
    const char *pName = SDL_JoystickName(joystick);
    strncpy(joyDetails.name, pName, MAX_NAME_LENGTH);
    joyDetails.name[MAX_NAME_LENGTH] = '\0';
    joyDetails.numAxes = SDL_JoystickNumAxes(joystick);
    for (int axisNum = 0; axisNum < NUM_JOY_AXES; axisNum++)
    {
      joyDetails.hasAxis[axisNum] = joyDetails.numAxes > axisNum;
      joyDetails.axisHasFF[axisNum] = false; // SDL 1.2 does not support force feedback
      char *axisName = joyDetails.axisName[axisNum];
      strcpy(axisName, CInputSystem::GetDefaultAxisName(axisNum)); // SDL 1.2 does not support axis names
    }
    joyDetails.numPOVs = SDL_JoystickNumHats(joystick);
    joyDetails.numButtons = SDL_JoystickNumButtons(joystick);
    joyDetails.hasFFeedback = false; // SDL 1.2 does not support force feedback

    m_joysticks.push_back(joystick);
    m_joyDetails.push_back(joyDetails);
  }
}

void CSDLInputSystem::CloseJoysticks()
{
  // Close all previously opened joysticks
  for (size_t i = 0; i < m_joysticks.size(); i++)
  {
    SDL_Joystick *joystick = m_joysticks[i];
    SDL_JoystickClose(joystick);
  }

  m_joysticks.clear();
  m_joyDetails.clear();
}

bool CSDLInputSystem::InitializeSystem()
{
  // Make sure joystick subsystem is initialized and joystick events are enabled
  if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0)
  {
    ErrorLog("Unable to initialize SDL joystick subsystem (%s).\n", SDL_GetError());

    return false;
  }
  SDL_JoystickEventState(SDL_ENABLE);

  // Open attached joysticks
  OpenJoysticks();
  return true;
}

int CSDLInputSystem::GetKeyIndex(const char *keyName)
{
  for (int i = 0; i < NUM_SDL_KEYS; i++)
  {
    if (stricmp(keyName, s_keyMap[i].keyName) == 0)
      return i;
  }
  return -1;
}

const char *CSDLInputSystem::GetKeyName(int keyIndex)
{
  if (keyIndex < 0 || keyIndex >= NUM_SDL_KEYS)
    return nullptr;
  return s_keyMap[keyIndex].keyName;
}

bool CSDLInputSystem::IsKeyPressed(int kbdNum, int keyIndex)
{
  // Get SDL key for given index and check if currently pressed
  SDL_Keycode sdlKey = s_keyMap[keyIndex].sdlKey;
  return !!m_keyState[sdlKey];
}

int CSDLInputSystem::GetMouseAxisValue(int mseNum, int axisNum)
{
  // Return value for given mouse axis
  switch (axisNum)
  {
    case AXIS_X: return m_mouseX;
    case AXIS_Y: return m_mouseY;
    case AXIS_Z: return m_mouseZ;
    default:     return 0;
  }
}

int CSDLInputSystem::GetMouseWheelDir(int mseNum)
{
  // Return wheel value
  return m_mouseWheelDir;
}

bool CSDLInputSystem::IsMouseButPressed(int mseNum, int butNum)
{
  // Return value for given mouse button
  switch (butNum)
  {
    case 0:  return !!(m_mouseButtons & SDL_BUTTON_LMASK);
    case 1:  return !!(m_mouseButtons & SDL_BUTTON_MMASK);
    case 2:  return !!(m_mouseButtons & SDL_BUTTON_RMASK);
    case 3:  return !!(m_mouseButtons & SDL_BUTTON_X1MASK);
    case 4:  return !!(m_mouseButtons & SDL_BUTTON_X2MASK);
    default: return false;
  }
}

int CSDLInputSystem::GetJoyAxisValue(int joyNum, int axisNum)
{
  // Get raw joystick axis value for given joystick from SDL (values range from -32768 to 32767)
  SDL_Joystick *joystick = m_joysticks[joyNum];
  return SDL_JoystickGetAxis(joystick, axisNum);
}

bool CSDLInputSystem::IsJoyPOVInDir(int joyNum, int povNum, int povDir)
{
  // Get current joystick POV-hat value for given joystick and POV number from SDL and check if pointing in required direction
  SDL_Joystick *joystick = m_joysticks[joyNum];
  int hatVal = SDL_JoystickGetHat(joystick, povNum);
  switch (povDir)
  {
    case POV_UP:    return !!(hatVal & SDL_HAT_UP);
    case POV_DOWN:  return !!(hatVal & SDL_HAT_DOWN);
    case POV_LEFT:  return !!(hatVal & SDL_HAT_LEFT);
    case POV_RIGHT: return !!(hatVal & SDL_HAT_RIGHT);
    default:        return false;
  }
  return false;
}

bool CSDLInputSystem::IsJoyButPressed(int joyNum, int butNum)
{
  // Get current joystick button state for given joystick and button number from SDL
  SDL_Joystick *joystick = m_joysticks[joyNum];
  return !!SDL_JoystickGetButton(joystick, butNum);
}

bool CSDLInputSystem::ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd)
{
  // SDL 1.2 does not support force feedback
  return false;
}

int CSDLInputSystem::GetNumKeyboards()
{
  // Return ANY_KEYBOARD as SDL 1.2 cannot handle multiple keyboards
  return ANY_KEYBOARD;
}

int CSDLInputSystem::GetNumMice()
{
  // Return ANY_MOUSE as SDL 1.2 cannot handle multiple mice
  return ANY_MOUSE;
}

int CSDLInputSystem::GetNumJoysticks()
{
  // Return number of joysticks found
  return (int)m_joysticks.size();
}

const KeyDetails *CSDLInputSystem::GetKeyDetails(int kbdNum)
{
  // Return nullptr as SDL 1.2 cannot handle multiple keyboards
  return nullptr;
}

const MouseDetails *CSDLInputSystem::GetMouseDetails(int mseNum)
{
  // Return nullptr as SDL 1.2 cannot handle multiple mice
  return nullptr;
}

const JoyDetails *CSDLInputSystem::GetJoyDetails(int joyNum)
{
  return &m_joyDetails[joyNum];
}

bool CSDLInputSystem::Poll()
{
  // Reset mouse wheel direction
  m_mouseWheelDir = 0;

  // Poll for event from SDL
  SDL_Event e;
  while (SDL_PollEvent(&e))
  {
    switch (e.type)
		{
	  default:
	    break;
		case SDL_QUIT:
			return false; 
		case SDL_MOUSEWHEEL:
			if (e.button.y > 0)
			{
				m_mouseZ += 5;
				m_mouseWheelDir = 1;
			}
			else if (e.button.y < 0)
			{
				m_mouseZ -= 5;
				m_mouseWheelDir = -1;
			}
			break;
		}
  }

  // Get key state from SDL
  m_keyState = SDL_GetKeyboardState(nullptr);

  // Get mouse state from SDL (except mouse wheel which was handled earlier)
  m_mouseButtons = SDL_GetMouseState(&m_mouseX, &m_mouseY);

  // Update joystick state (not required as called implicitly by SDL_PollEvent above)
  //SDL_JoystickUpdate();
  return true;
}

void CSDLInputSystem::SetMouseVisibility(bool visible)
{
  SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}
