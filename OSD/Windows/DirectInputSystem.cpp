#include "DirectInputSystem.h"
#include "Supermodel.h"

#include <SDL.h>
#include <SDL_syswm.h>

// TODO - need to double check these all correct and see if can fill in any missing codes (although most just don't exist)
DIKeyMapStruct CDirectInputSystem::s_keyMap[] = 
{
	// General keys
	{ "BACKSPACE",			DIK_BACK },
	{ "TAB",				DIK_TAB },
	//{ "CLEAR",			?? },
	{ "RETURN",				DIK_RETURN },
	{ "PAUSE",				DIK_PAUSE },
	{ "ESCAPE",				DIK_ESCAPE },
	{ "SPACE",				DIK_SPACE },
	//{ "EXCLAIM",			?? },
	//{ "DBLQUOTE",			?? },
	//{ "HASH",				?? }, 
	//{ "DOLLAR",			?? },
	//{ "AMPERSAND",		?? },
	{ "QUOTE",				DIK_APOSTROPHE },
	{ "LEFTPAREN",			DIK_LBRACKET },
	{ "RIGHTPAREN",			DIK_RBRACKET },
	//{ "ASTERISK",			?? },
	//{ "PLUS",				?? },
	{ "COMMA",				DIK_COMMA },
	{ "MINUS",				DIK_MINUS },
	{ "PERIOD",				DIK_PERIOD },
	{ "SLASH",				DIK_SLASH },
	{ "0",					DIK_0 },
	{ "1",					DIK_1 },
	{ "2",					DIK_2 },
	{ "3",					DIK_3 },
	{ "4",					DIK_4 },
	{ "5",					DIK_5 },
	{ "6",					DIK_6 },
	{ "7",					DIK_7 },
	{ "8",					DIK_8 },
	{ "9",					DIK_9 },
	//{ "COLON",			?? },
	{ "SEMICOLON",			DIK_SEMICOLON },
	{ "LESS",				DIK_OEM_102 },
	{ "EQUALS",				DIK_EQUALS },
	//{ "GREATER",			?? },
	//{ "QUESTION",			?? },
	//{ "AT",				?? },
	//{ "LEFTBRACKET",		?? },
	//{ "BACKSLASH",		?? },
	//{ "RIGHTBRACKET",		?? },
	//{ "CARET",			?? },
	//{ "UNDERSCORE",		?? },
	{ "BACKQUOTE",			DIK_GRAVE },
	{ "A",					DIK_A },
	{ "B",					DIK_B },
	{ "C",					DIK_C },
	{ "D",					DIK_D },
	{ "E",					DIK_E },
	{ "F",					DIK_F },
	{ "G",					DIK_G },
	{ "H",					DIK_H },
	{ "I",					DIK_I },
	{ "J",					DIK_J },
	{ "K",					DIK_K },
	{ "L",					DIK_L },
	{ "M",					DIK_M },
	{ "N",					DIK_N },
	{ "O",					DIK_O },
	{ "P",					DIK_P },
	{ "Q",					DIK_Q },
	{ "R",					DIK_R },
	{ "S",					DIK_S },
	{ "T",					DIK_T },
	{ "U",					DIK_U },
	{ "V",					DIK_V },
	{ "W",					DIK_W },
	{ "X",					DIK_X },
	{ "Y",					DIK_Y },
	{ "Z",					DIK_Z },
	{ "DEL",				DIK_DELETE },
	
	// Keypad
	{ "KEYPAD0",			DIK_NUMPAD0 },
	{ "KEYPAD1",			DIK_NUMPAD1 },
	{ "KEYPAD2",			DIK_NUMPAD2 },
	{ "KEYPAD3",			DIK_NUMPAD3 },
	{ "KEYPAD4",			DIK_NUMPAD4 },
	{ "KEYPAD5",			DIK_NUMPAD5 },
	{ "KEYPAD6",			DIK_NUMPAD6 },
	{ "KEYPAD7",			DIK_NUMPAD7 },
	{ "KEYPAD8",			DIK_NUMPAD8 },
	{ "KEYPAD9",			DIK_NUMPAD9 },
	{ "KEYPADPERIOD",		DIK_DECIMAL },
	{ "KEYPADDIVIDE",		DIK_DIVIDE },
	{ "KEYPADMULTIPLY",		DIK_MULTIPLY },
	{ "KEYPADMINUS",		DIK_SUBTRACT },
	{ "KEYPADPLUS",			DIK_ADD },
	{ "KEYPADENTER",		DIK_NUMPADENTER },
	{ "KEYPADEQUALS",		DIK_NUMPADEQUALS },
	
	// Arrows + Home/End Pad
	{ "UP",					DIK_UP },
	{ "DOWN",				DIK_DOWN },
	{ "RIGHT",				DIK_RIGHT },
	{ "LEFT",				DIK_LEFT },
	{ "INSERT",				DIK_INSERT },
	{ "HOME",				DIK_HOME },
	{ "END",				DIK_END },
	{ "PGUP",				DIK_PRIOR },
	{ "PGDN",				DIK_NEXT },

	// Function Key
	{ "F1",					DIK_F1 },
	{ "F2",					DIK_F2 },
	{ "F3",					DIK_F3 },
	{ "F4",					DIK_F4 },
	{ "F5",					DIK_F5 },
	{ "F6",					DIK_F6 },
	{ "F7",					DIK_F7 },
	{ "F8",					DIK_F8 },
	{ "F9",					DIK_F9 },
	{ "F10",				DIK_F10 },
	{ "F11",				DIK_F11 },
	{ "F12",				DIK_F12 },
	{ "F13",				DIK_F13 },
	{ "F14",				DIK_F14 },
	{ "F15",				DIK_F15 },
    
	// Modifier Keys  
	{ "NUMLOCK",			DIK_NUMLOCK },
	{ "CAPSLOCK",			DIK_CAPITAL },
	{ "SCROLLLOCK",			DIK_SCROLL },
	{ "RIGHTSHIFT",			DIK_RSHIFT },
	{ "LEFTSHIFT",			DIK_LSHIFT },
	{ "RIGHTCTRL",			DIK_RCONTROL },
	{ "LEFTCTRL",			DIK_LCONTROL },
	{ "RIGHTALT",			DIK_RMENU },
	{ "LEFTALT",			DIK_LMENU },
	//{ "RIGHTMETA",		?? },
	//{ "LEFTMETA",			?? },
	{ "RIGHTWINDOWS",		DIK_RWIN },
	{ "LEFTWINDOWS",		DIK_LWIN },
	//{ "ALTGR",			?? },
	//{ "COMPOSE",			?? },
    
	// Other
	//{ "HELP",				?? },
	{ "PRINT",				DIK_SYSRQ },
	//{ "SYSREQ",			?? },
	//{ "BREAK",			?? },								
	//{ "MENU",				?? },
	//{ "POWER",			?? },
	//{ "EURO",				?? },
	//{ "UNDO",				?? },
};

BOOL CALLBACK DI8EnumDevicesCallback(LPCDIDEVICEINSTANCE instance, LPVOID context)
{
	// Keep track of all joystick device GUIDs
	vector<GUID> *guids = (vector<GUID>*)context;
	guids->push_back(instance->guidInstance);
	return DIENUM_CONTINUE;
}

BOOL CALLBACK DI8EnumAxesCallback(LPCDIDEVICEOBJECTINSTANCE instance, LPVOID context)
{
	// Find out which joystick axes are present
	// TODO - is usage/usage page best method or is better to use something like:
	//  int axisNum = (instance->dwOfs - offsetof(DIJOYSTATE2, lX)) / sizeof(LONG)?
	JoyDetails *joyDetails = (JoyDetails*)context;
	if      (MATCHES_USAGE(instance->wUsage, instance->wUsagePage, X_AXIS_USAGE))  joyDetails->hasXAxis = true;
	else if (MATCHES_USAGE(instance->wUsage, instance->wUsagePage, Y_AXIS_USAGE))  joyDetails->hasYAxis = true;
	else if (MATCHES_USAGE(instance->wUsage, instance->wUsagePage, Z_AXIS_USAGE))  joyDetails->hasZAxis = true;
	else if (MATCHES_USAGE(instance->wUsage, instance->wUsagePage, RX_AXIS_USAGE)) joyDetails->hasRXAxis = true;
	else if (MATCHES_USAGE(instance->wUsage, instance->wUsagePage, RY_AXIS_USAGE)) joyDetails->hasRYAxis = true;
	else if (MATCHES_USAGE(instance->wUsage, instance->wUsagePage, RZ_AXIS_USAGE)) joyDetails->hasRZAxis = true;
	return DIENUM_CONTINUE;
}

CDirectInputSystem::CDirectInputSystem(bool useRawInput) : CInputSystem("DirectInput/RawInput"), m_useRawInput(useRawInput), 
	m_activated(false), m_hwnd(NULL), m_getRIDevListPtr(NULL), m_getRIDevInfoPtr(NULL), m_regRIDevsPtr(NULL), m_getRIDataPtr(NULL),
	m_di8(NULL), m_di8Keyboard(NULL), m_di8Mouse(NULL)
{
	// Reset initial states
	memset(&m_combRawMseState, 0, sizeof(RawMseState));
	memset(&m_diKeyState, 0, sizeof(LPDIRECTINPUTDEVICE8));
	memset(&m_diMseState, 0, sizeof(LPDIRECTINPUTDEVICE8));
}

CDirectInputSystem::~CDirectInputSystem()
{
	CloseKeyboardsAndMice();
	CloseJoysticks();

	if (m_di8 != NULL)
	{
		m_di8->Release();
		m_di8 = NULL;
	}
}

void CDirectInputSystem::OpenKeyboardsAndMice()
{
	if (m_useRawInput)
	{
		// If RawInput enabled, get list of available devices
		UINT nDevices;
		if (m_getRIDevListPtr(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) == 0 && nDevices > 0)
		{
			PRAWINPUTDEVICELIST pDeviceList = new RAWINPUTDEVICELIST[nDevices];
			if (pDeviceList != NULL && m_getRIDevListPtr(pDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST)) != (UINT)-1)
			{
				// Loop through devices backwards (since new devices are usually added at beginning)
				for (int devNum = nDevices - 1; devNum >= 0; devNum--)
				{
					RAWINPUTDEVICELIST device = pDeviceList[devNum];
					
					// Get device name
					UINT nLength;
					if (m_getRIDevInfoPtr(device.hDevice, RIDI_DEVICENAME, NULL, &nLength) != 0)
						continue;
					char *name = new char[nLength];
					if (m_getRIDevInfoPtr(device.hDevice, RIDI_DEVICENAME, name, &nLength) == -1)
						continue;

					// Ignore any RDP devices
					if (strstr(name, "Root#RDP_") != NULL)
						continue;

					// Store device handles for attached keyboards and mice
					if (device.dwType == RIM_TYPEKEYBOARD)
					{
						m_rawKeyboards.push_back(device.hDevice);

						BOOL *keyState = new BOOL[255];
						memset(keyState, 0, sizeof(BOOL) * 255);
						m_rawKeyStates.push_back(keyState);
					}
					else if (device.dwType == RIM_TYPEMOUSE)
					{
						m_rawMice.push_back(device.hDevice);

						RawMseState *mseState = new RawMseState();
						memset(mseState, 0, sizeof(RawMseState));
						m_rawMseStates.push_back(mseState);
					}
				}

				DebugLog("RawInput - found %d keyboards and %d mice", m_rawKeyboards.size(), m_rawMice.size());

				// Check some devices were actually found
				m_useRawInput = m_rawKeyboards.size() > 0 && m_rawMice.size() > 0;
			}
			else
			{
				ErrorLog("Unable to query RawInput API for attached devices (error %d) - switching to DirectInput.\n", GetLastError());

				m_useRawInput = false;
			}

			if (pDeviceList != NULL)
				delete[] pDeviceList;
		}
		else
		{
			ErrorLog("Unable to query RawInput API for attached devices (error %d) - switching to DirectInput.\n", GetLastError());

			m_useRawInput = false;
		}

		if (m_useRawInput)
			return;
	}

	// If get here then either RawInput disabled or getting its devices failed so default to DirectInput.
	// Open DirectInput system keyboard and set its data format
	HRESULT hr;
	if (FAILED(hr = m_di8->CreateDevice(GUID_SysKeyboard, &m_di8Keyboard, NULL)))
	{
		ErrorLog("Unable to create DirectInput keyboard device (error %d) - key input will be unavailable.\n", hr);

		m_di8Keyboard = NULL;	
	}
	else if (FAILED(hr = m_di8Keyboard->SetDataFormat(&c_dfDIKeyboard)))
	{
		ErrorLog("Unable to set data format for DirectInput keyboard (error %d) - key input will be unavailable.\n", hr);

		m_di8Keyboard->Release();
		m_di8Keyboard = NULL;
	}
	
	// Open DirectInput system mouse and set its data format
	if (FAILED(hr = m_di8->CreateDevice(GUID_SysMouse, &m_di8Mouse, NULL)))
	{
		ErrorLog("Unable to create DirectInput mouse device (error %d) - mouse input will be unavailable.\n", hr);

		m_di8Mouse = NULL;	
		return;
	}
	if (FAILED(hr = m_di8Mouse->SetDataFormat(&c_dfDIMouse2)))
	{
		ErrorLog("Unable to set data format for DirectInput mouse (error %d) - mouse input will be unavailable.\n", hr);

		m_di8Mouse->Release();
		m_di8Mouse = NULL;
		return;
	}

	// Set mouse axis mode to relative
	DIPROPDWORD dipdw;
	dipdw.diph.dwSize = sizeof(DIPROPDWORD); 
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.diph.dwObj = 0;
	dipdw.dwData = DIPROPAXISMODE_REL;
	if (FAILED(hr = m_di8Mouse->SetProperty(DIPROP_AXISMODE, &dipdw.diph)))
	{
		ErrorLog("Unable to set axis mode for DirectInput mouse (error %d) - mouse input will be unavailable.\n", hr);

		m_di8Mouse->Release();
		m_di8Mouse = NULL;
	}
}

void CDirectInputSystem::ActivateKeyboardsAndMice()
{
	// Sync up all mice with current cursor position
	ResetMice();

	if (m_useRawInput)
	{
		// Register for RawInput
		RAWINPUTDEVICE rid[2];
		
		// Register for keyboard input
		rid[0].usUsagePage = 0x01;
		rid[0].usUsage = 0x06;
		rid[0].dwFlags = (m_isConfiguring ? RIDEV_INPUTSINK : RIDEV_CAPTUREMOUSE) | RIDEV_NOLEGACY;
		rid[0].hwndTarget = m_hwnd;

		// Register for mouse input
		rid[1].usUsagePage = 0x01;
		rid[1].usUsage = 0x02;
		rid[1].dwFlags = (m_isConfiguring ? RIDEV_INPUTSINK : RIDEV_CAPTUREMOUSE) | RIDEV_NOLEGACY;
		rid[1].hwndTarget = m_hwnd;

		if (!m_regRIDevsPtr(rid, 2, sizeof(RAWINPUTDEVICE)))
			ErrorLog("Unable to register for keyboard and mouse input with RawInput API (error %d) - keyboard and mouse input will be unavailable.\n", GetLastError());
		return;
	}
	
	// Set DirectInput cooperative level of keyboard and mouse
	if (m_di8Keyboard != NULL)
		m_di8Keyboard->SetCooperativeLevel(m_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
	if (m_di8Mouse != NULL)
		m_di8Mouse->SetCooperativeLevel(m_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
}

void CDirectInputSystem::PollKeyboardsAndMice()
{
	if (m_useRawInput)
	{
		// For RawInput, only thing to do is update wheelDir from wheelData for each mouse state.  Everything else is updated via WM events.
		for (vector<RawMseState*>::iterator it = m_rawMseStates.begin(); it < m_rawMseStates.end(); it++)
		{
			if ((*it)->wheelDelta != 0)
			{
				(*it)->wheelDir = ((*it)->wheelDelta > 0 ? 1 : -1);
				(*it)->wheelDelta = 0;
			}
			else
				(*it)->wheelDir = 0;
		}
		if (m_combRawMseState.wheelDelta != 0)
		{
			m_combRawMseState.wheelDir = (m_combRawMseState.wheelDelta > 0 ? 1 : -1);
			m_combRawMseState.wheelDelta = 0;
		}
		else
			m_combRawMseState.wheelDir = 0;
		return;
	}

	// Get current keyboard state from DirectInput
	HRESULT hr;
	if (m_di8Keyboard != NULL)
	{
		if (FAILED(hr = m_di8Keyboard->Poll()))
		{
			hr = m_di8Keyboard->Acquire();
			while (hr == DIERR_INPUTLOST)
				hr = m_di8Keyboard->Acquire();

			if (hr == DIERR_OTHERAPPHASPRIO || hr == DIERR_INVALIDPARAM || hr == DIERR_NOTINITIALIZED)
				return;
		}

		// Keep track of keyboard state
		m_di8Keyboard->GetDeviceState(sizeof(m_diKeyState), m_diKeyState);
	}

	// Get current mouse state from DirectInput
	if (m_di8Mouse != NULL)
	{
		if (FAILED(hr = m_di8Mouse->Poll()))
		{
			hr = m_di8Mouse->Acquire();
			while (hr == DIERR_INPUTLOST)
				hr = m_di8Mouse->Acquire();

			if (hr == DIERR_OTHERAPPHASPRIO || hr == DIERR_INVALIDPARAM || hr == DIERR_NOTINITIALIZED)
				return;
		}

		// Keep track of mouse absolute axis values, clamping them at display edges, aswell as wheel direction and buttons
		DIMOUSESTATE2 mseState;
		m_di8Mouse->GetDeviceState(sizeof(mseState), &mseState);
		m_diMseState.x = CInputSource::Clamp(m_diMseState.x + mseState.lX, m_dispX, m_dispX + m_dispW);
		m_diMseState.y = CInputSource::Clamp(m_diMseState.y + mseState.lY, m_dispY, m_dispY + m_dispH);
		if (mseState.lZ != 0)
		{
			// Z-axis is clamped to range -100 to 100 (DirectInput returns +120 & -120 for wheel delta which are scaled to +5 & -5)
			LONG wheelDelta = 5 * mseState.lZ / 120;
			m_diMseState.z = CInputSource::Clamp(m_diMseState.z + wheelDelta, -100, 100);
			m_diMseState.wheelDir = (wheelDelta > 0 ? 1 : -1);
		}
		else
			m_diMseState.wheelDir = 0;
		memcpy(&m_diMseState.buttons, mseState.rgbButtons, sizeof(m_diMseState.buttons));
	}
}

void CDirectInputSystem::CloseKeyboardsAndMice()
{
	if (m_useRawInput)
	{
		if (m_activated)
		{
			// If RawInput was registered, then unregister now
			RAWINPUTDEVICE rid[2];
			
			// Unregister from keyboard input
			rid[0].usUsagePage = 0x01;
			rid[0].usUsage = 0x06;
			rid[0].dwFlags = RIDEV_REMOVE;
			rid[0].hwndTarget = m_hwnd;

			// Unregister from mouse input
			rid[1].usUsagePage = 0x01;
			rid[1].usUsage = 0x02;
			rid[1].dwFlags = RIDEV_REMOVE;
			rid[1].hwndTarget = m_hwnd;

			m_regRIDevsPtr(rid, 2, sizeof(RAWINPUTDEVICE));
		}

		for (vector<BOOL*>::iterator it = m_rawKeyStates.begin(); it < m_rawKeyStates.end(); it++)
			delete[] *it;
		for (vector<RawMseState*>::iterator it = m_rawMseStates.begin(); it < m_rawMseStates.end(); it++)
			delete *it;
	}

	// If DirectInput keyboard and mouse were created, then release them now
	if (m_di8Keyboard != NULL)
	{
		m_di8Keyboard->Unacquire();
		m_di8Keyboard->Release();
		m_di8Keyboard = NULL;
	}
	if (m_di8Mouse != NULL)
	{
		m_di8Mouse->Unacquire();
		m_di8Mouse->Release();
		m_di8Mouse = NULL;
	}
}

void CDirectInputSystem::ResetMice()
{
	// Get current mouse cursor position in window
	POINT p;
	if (!GetCursorPos(&p) || !ScreenToClient(m_hwnd, &p))
		return;

	// Set all mice coords to current cursor position
	if (m_useRawInput)
	{
		m_combRawMseState.x = p.x;
		m_combRawMseState.y = p.y;
		m_combRawMseState.z = 0;
		for (vector<RawMseState*>::iterator it = m_rawMseStates.begin(); it < m_rawMseStates.end(); it++)
		{
			(*it)->x = p.x;
			(*it)->y = p.y;
			(*it)->z = 0;
		}
	}

	m_diMseState.x = p.x;
	m_diMseState.y = p.y;
	m_diMseState.z = 0;
}

void CDirectInputSystem::ProcessRawInput(HRAWINPUT hInput)
{
	// RawInput data event
	BYTE buffer[4096];
	LPBYTE pBuf = buffer;

	// Get size of data structure to receive
	UINT dwSize;
	if (m_getRIDataPtr(hInput, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER)) != 0)
		return;
	if (dwSize > sizeof(buffer))
	{
		pBuf = new BYTE[dwSize];
		if (pBuf == NULL)
			return;
	}

	// Get data
	if (m_getRIDataPtr(hInput, RID_INPUT, pBuf, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
	{
		RAWINPUT *pData = (RAWINPUT*)pBuf;
		if (pData->header.dwType == RIM_TYPEKEYBOARD)
		{
			// Keyboard event, so identify which keyboard produced event
			BOOL *keyState = NULL;
			size_t kbdNum;
			for (kbdNum = 0; kbdNum < m_rawKeyboards.size(); kbdNum++)
			{
				if (m_rawKeyboards[kbdNum] == pData->header.hDevice)
				{
					keyState = m_rawKeyStates[kbdNum];
					break;
				}
			}
			
			// Check is a valid keyboard
			if (keyState != NULL)
			{
				// Get scancode of key and whether key was pressed or released
				BOOL isRight = pData->data.keyboard.Flags & RI_KEY_E0;
				UINT8 scanCode = (pData->data.keyboard.MakeCode & 0x7f) | (isRight ? 0x80 : 0x00);
				BOOL pressed = !(pData->data.keyboard.Flags & RI_KEY_BREAK);

				// Store current state for key 
				if (scanCode != 0xAA)
					keyState[scanCode] = pressed;
			}
		}
		else if (pData->header.dwType == RIM_TYPEMOUSE)
		{
			// Mouse event, so identify which mouse produced event
			RawMseState *mseState = NULL;
			size_t mseNum;
			for (mseNum = 0; mseNum < m_rawMice.size(); mseNum++)
			{
				if (m_rawMice[mseNum] == pData->header.hDevice)
				{
					mseState = m_rawMseStates[mseNum];
					break;
				}
			}

			// Check is a valid mouse
			if (mseState != NULL)
			{
				// Get X- & Y-axis data
				LONG lx = pData->data.mouse.lLastX;
				LONG ly = pData->data.mouse.lLastY;
				if (pData->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
				{
					// If data is absolute, then scale source values (0 to 65535) to display dimensions
					mseState->x = CInputSource::Scale(lx, 0, 0xFFFF, m_dispX, m_dispX + m_dispW);
					mseState->y = CInputSource::Scale(ly, 0, 0xFFFF, m_dispY, m_dispY + m_dispH);

					// Also update combined state
					m_combRawMseState.x = mseState->x;
					m_combRawMseState.y = mseState->y;
				}
				else
				{
					// If data is relative, then keep track of absolute position, clamping it at display edges
					mseState->x = CInputSource::Clamp(mseState->x + lx, m_dispX, m_dispX + m_dispW);
					mseState->y = CInputSource::Clamp(mseState->y + ly, m_dispY, m_dispY + m_dispH);

					// Also update combined state
					m_combRawMseState.x = CInputSource::Clamp(m_combRawMseState.x + lx, m_dispX, m_dispX + m_dispW);
					m_combRawMseState.y = CInputSource::Clamp(m_combRawMseState.y + ly, m_dispY, m_dispY + m_dispH);
				}

				// Get button flags and wheel delta (RawInput returns +120 & -120 for the latter which are scaled to +5 & -5)
				USHORT butFlags = pData->data.mouse.usButtonFlags;
				LONG wheelDelta = 5 * (SHORT)pData->data.mouse.usButtonData / 120;
				
				// Update Z-axis (wheel) value
				if (butFlags & RI_MOUSE_WHEEL)
				{
					// Z-axis is clamped to range -100 to 100
					mseState->z = CInputSource::Clamp(mseState->z + wheelDelta, -100, 100);
					mseState->wheelDelta += wheelDelta;
				}

				// Keep track of buttons pressed/released
				if      (butFlags & RI_MOUSE_LEFT_BUTTON_DOWN)   mseState->buttons |= 1;
				else if (butFlags & RI_MOUSE_LEFT_BUTTON_UP)     mseState->buttons ^= 1;
				if      (butFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) mseState->buttons |= 2;
				else if (butFlags & RI_MOUSE_MIDDLE_BUTTON_UP)   mseState->buttons ^= 2;
				if      (butFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)  mseState->buttons |= 4;
				else if (butFlags & RI_MOUSE_RIGHT_BUTTON_UP)    mseState->buttons ^= 4;
				if      (butFlags & RI_MOUSE_BUTTON_4_DOWN)      mseState->buttons |= 8;
				else if (butFlags & RI_MOUSE_BUTTON_4_UP)        mseState->buttons ^= 8;
				if      (butFlags & RI_MOUSE_BUTTON_5_DOWN)      mseState->buttons |= 16;
				else if (butFlags & RI_MOUSE_BUTTON_5_UP)        mseState->buttons ^= 16;

				// Also update combined state for wheel axis and buttons
				if (butFlags & RI_MOUSE_WHEEL)
				{
					// Z-axis is clamped to range -100 to 100
					m_combRawMseState.z = CInputSource::Clamp(m_combRawMseState.z + wheelDelta, -100, 100);
					m_combRawMseState.wheelDelta += wheelDelta;
				}

				m_combRawMseState.buttons = 0;
				for (vector<RawMseState*>::iterator it = m_rawMseStates.begin(); it < m_rawMseStates.end(); it++)
					m_combRawMseState.buttons |= (*it)->buttons;
			}
		}
	}

	if (pBuf != buffer)
		delete[] pBuf;
}

void CDirectInputSystem::OpenJoysticks()
{
	// Get the GUIDs of all attached joystick devices
	vector<GUID> guids;
	HRESULT hr;
	if (FAILED(hr = m_di8->EnumDevices(DI8DEVCLASS_GAMECTRL, DI8EnumDevicesCallback, &guids, DIEDFL_ATTACHEDONLY)))
		return;

	// Loop through those found
	int joyNum = 0;
	for (vector<GUID>::iterator it = guids.begin(); it < guids.end(); it++)
	{
		joyNum++;

		// Open joystick with DirectInput for given GUID and set its data format
		LPDIRECTINPUTDEVICE8 di8Joystick;
		if (FAILED(hr = m_di8->CreateDevice((*it), &di8Joystick, NULL)))
		{
			ErrorLog("Unable to create DirectInput joystick device %d (error %d) - skipping joystick.\n", joyNum, hr);
			continue;
		}
		if (FAILED(hr = di8Joystick->SetDataFormat(&c_dfDIJoystick2)))
		{
			ErrorLog("Unable to set data format for DirectInput joystick %d (error %d) - skipping joystick.\n", joyNum, hr);
			
			di8Joystick->Release();
			continue;
		}

		// Get joystick's capabilities
		DIDEVCAPS devCaps;
		devCaps.dwSize = sizeof(DIDEVCAPS);
		if (FAILED(hr = di8Joystick->GetCapabilities(&devCaps)))
		{
			ErrorLog("Unable to query capabilities of DirectInput joystick %d (error %d) - skipping joystick.\n", joyNum, hr);
			
			di8Joystick->Release();
			continue;
		}

		// Gather joystick details (num POVs & buttons and which axes are available)
		JoyDetails *joyDetails = new JoyDetails();
		joyDetails->numPOVs    = devCaps.dwPOVs;
		joyDetails->numButtons = devCaps.dwButtons;
		if (FAILED(hr = di8Joystick->EnumObjects(DI8EnumAxesCallback, joyDetails, DIDFT_AXIS)))
		{
			ErrorLog("Unable to enumerate axes of DirectInput joystick %d (error %d) - skipping joystick.\n", joyNum, hr);

			di8Joystick->Release();
			continue;
		}
		joyDetails->numAxes = 
			joyDetails->hasXAxis + joyDetails->hasYAxis + joyDetails->hasZAxis +
			joyDetails->hasRXAxis + joyDetails->hasRYAxis + joyDetails->hasRZAxis;

		// Configure axes, if any
		if (joyDetails->numAxes > 0)
		{
			// Set axis range to be from -32768 to 32767
			DIPROPRANGE didpr;
			didpr.diph.dwSize = sizeof(DIPROPRANGE); 
			didpr.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
			didpr.diph.dwHow = DIPH_DEVICE;
			didpr.diph.dwObj = 0;
			didpr.lMin = -32768;
			didpr.lMax = 32767;
			if (FAILED(hr = di8Joystick->SetProperty(DIPROP_RANGE, &didpr.diph)))
			{
				ErrorLog("Unable to set axis range of DirectInput joystick %d (error %d) - skipping joystick.\n", joyNum, hr);

				di8Joystick->Release();
				continue;
			}

			// Set axis mode to absolute
			DIPROPDWORD dipdw;
			dipdw.diph.dwSize = sizeof(DIPROPDWORD); 
			dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
			dipdw.diph.dwHow = DIPH_DEVICE;
			dipdw.diph.dwObj = 0;
			dipdw.dwData = DIPROPAXISMODE_ABS;
			if (FAILED(hr = di8Joystick->SetProperty(DIPROP_AXISMODE, &dipdw.diph)))
			{
				ErrorLog("Unable to set axis mode of DirectInput joystick %d (error %d) - skipping joystick.\n", joyNum, hr);

				di8Joystick->Release();
				continue;
			}

			// Turn off deadzone as handled by this class
			dipdw.dwData = 0;
			if (FAILED(hr = di8Joystick->SetProperty(DIPROP_DEADZONE, &dipdw.diph)))
			{
				ErrorLog("Unable to set deadzone of DirectInput joystick %d (error %d) - skipping joystick.\n", joyNum, hr);

				di8Joystick->Release();
				continue;
			}

			// Turn off saturation as handle by this class
			dipdw.dwData = 10000;
			if (FAILED(hr = di8Joystick->SetProperty(DIPROP_SATURATION, &dipdw.diph)))
			{
				ErrorLog("Unable to set saturation of DirectInput joystick %d (error %d) - skipping joystick.\n", joyNum, hr);

				di8Joystick->Release();
				continue;
			}
		}

		// Create initial blank joystick state
		LPDIJOYSTATE2 joyState = new DIJOYSTATE2();
		memset(joyState, 0, sizeof(DIJOYSTATE2));
		for (int povNum = 0; povNum < 4; povNum++)
			joyState->rgdwPOV[povNum] = -1;
		
		m_di8Joysticks.push_back(di8Joystick);
		m_joyDetails.push_back(joyDetails);
		m_diJoyStates.push_back(joyState);
	}
}

void CDirectInputSystem::ActivateJoysticks()
{
	// Set DirectInput cooperative level of joysticks
	for (vector<LPDIRECTINPUTDEVICE8>::iterator it = m_di8Joysticks.begin(); it < m_di8Joysticks.end(); it++)
		(*it)->SetCooperativeLevel(m_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
}

void CDirectInputSystem::PollJoysticks()
{
	// Get current joystick states from DirectInput
	for (size_t i = 0; i < m_di8Joysticks.size(); i++)
	{
		LPDIRECTINPUTDEVICE8 joystick = m_di8Joysticks[i];
		LPDIJOYSTATE2 state = m_diJoyStates[i];

		HRESULT hr;
		if (FAILED(hr = joystick->Poll()))
		{
			hr = joystick->Acquire();
			while (hr == DIERR_INPUTLOST)
				hr = joystick->Acquire();

			if (hr == DIERR_OTHERAPPHASPRIO || hr == DIERR_INVALIDPARAM || hr == DIERR_NOTINITIALIZED)
				continue;
		}

		// Keep tack of each joystick state
		joystick->GetDeviceState(sizeof(DIJOYSTATE2), state);
	}
}

void CDirectInputSystem::CloseJoysticks()
{
	// Release each DirectInput joystick
	for (size_t joyNum = 0; joyNum < m_di8Joysticks.size(); joyNum++)
	{
		LPDIRECTINPUTDEVICE8 joystick = m_di8Joysticks[joyNum];
		JoyDetails *joyDetails = m_joyDetails[joyNum];
		LPDIJOYSTATE2 diJoyState = m_diJoyStates[joyNum];

		joystick->Unacquire();
		joystick->Release();

		delete joyDetails;
		delete diJoyState;
	}

	m_di8Joysticks.clear();
	m_joyDetails.clear();
	m_diJoyStates.clear();
}

bool CDirectInputSystem::InitializeSystem()
{
	if (m_useRawInput)
	{
		// Dynamically load RawInput API
		HMODULE user32 = LoadLibrary(TEXT("user32.dll"));
		if (user32 != NULL)
		{
			m_getRIDevListPtr = (GetRawInputDeviceListPtr)GetProcAddress(user32, "GetRawInputDeviceList");
			m_getRIDevInfoPtr = (GetRawInputDeviceInfoPtr)GetProcAddress(user32, "GetRawInputDeviceInfoA");
			m_regRIDevsPtr = (RegisterRawInputDevicesPtr)GetProcAddress(user32, "RegisterRawInputDevices");
			m_getRIDataPtr = (GetRawInputDataPtr)GetProcAddress(user32, "GetRawInputData");
			m_useRawInput = m_getRIDevListPtr != NULL && m_getRIDevInfoPtr != NULL && m_regRIDevsPtr != NULL && m_getRIDataPtr != NULL;
		}
		else
			m_useRawInput = false;

		if (!m_useRawInput)
			ErrorLog("Unable to initialize RawInput API (library hooks are not available) - switching to DirectInput.\n");
	}

	// Dynamically create DirectInput8 via COM, rather than statically linking to dinput8.dll
	// TODO - if fails, try older versions of DirectInput
	HRESULT hr;
	if (FAILED(hr = CoInitialize(NULL)))
	{
		ErrorLog("Unable to initialize COM (error %d).\n", hr);

		return false;
	}
	if (FAILED(hr = CoCreateInstance(CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectInput8A, (LPVOID*)&m_di8)))
	{
		ErrorLog("Unable to initialize DirectInput API (error %d) - is DirectX 8 or later installed?\n", hr);
		
		CoUninitialize();
		return false;
	}
	if (FAILED(hr = m_di8->Initialize(GetModuleHandle(NULL), DIRECTINPUT_VERSION)))
	{
		ErrorLog("Unable to initialize DirectInput API (error %d) - is DirectX 8 or later installed?\n", hr);

		m_di8->Release();
		return false;
	}

	// Open all devices
	OpenKeyboardsAndMice();
	OpenJoysticks();
		
	return true;
}

int CDirectInputSystem::GetNumKeyboards()
{
	// If RawInput enabled, then return number of keyboards found.  Otherwise, return ANY_KEYBOARD as DirectInput cannot handle multiple keyboards
	return (m_useRawInput ? m_rawKeyboards.size() : ANY_KEYBOARD);
}
	
int CDirectInputSystem::GetNumMice()
{
	// If RawInput enabled, then return number of mice found.  Otherwise, return ANY_MOUSE as DirectInput cannot handle multiple keyboards
	return (m_useRawInput ? m_rawMice.size() : ANY_MOUSE);
}
	
int CDirectInputSystem::GetNumJoysticks()
{
	// Return number of joysticks found
	return m_di8Joysticks.size();
}

int CDirectInputSystem::GetKeyIndex(const char *keyName)
{
	for (int i = 0; i < NUM_DI_KEYS; i++)
	{
		if (stricmp(keyName, s_keyMap[i].keyName) == 0)
			return i;
	}
	return -1;
}

const char *CDirectInputSystem::GetKeyName(int keyIndex)
{
	if (keyIndex < 0 || keyIndex >= NUM_DI_KEYS)
		return NULL;
	return s_keyMap[keyIndex].keyName;
}

JoyDetails *CDirectInputSystem::GetJoyDetails(int joyNum)
{
	return m_joyDetails[joyNum];
}

bool CDirectInputSystem::IsKeyPressed(int kbdNum, int keyIndex)
{
	// Get DI key code (scancode) for given key index
	int diKey = s_keyMap[keyIndex].diKey;
	
	if (m_useRawInput)
	{
		// For RawInput, check if key is currently pressed for given keyboard number
		BOOL *keyState = m_rawKeyStates[kbdNum];
		return !!keyState[diKey];
	}

	// For DirectInput, just check common keyboard state
	return !!(m_diKeyState[diKey] & 0x80);
}

int CDirectInputSystem::GetMouseAxisValue(int mseNum, int axisNum)
{
	if (m_useRawInput)
	{
		// For RawInput, get combined or individual mouse state and return value for given axis
		// The cursor is always hidden when using RawInput, so it does not matter if these values don't match with the cursor (with multiple
		// mice the cursor is irrelevant anyway)
		RawMseState *mseState = (mseNum == ANY_MOUSE ? &m_combRawMseState : m_rawMseStates[mseNum]);
		switch (axisNum)
		{
			case AXIS_X: return mseState->x;
			case AXIS_Y: return mseState->y;
			case AXIS_Z: return mseState->z;
			default:     return 0;
		}
	}
	
	// For DirectInput, for X- and Y-axes just use cursor position within window if available (so that mouse movements sync with the cursor)
	if (axisNum == AXIS_X || axisNum == AXIS_Y)
	{
		POINT p;
		if (GetCursorPos(&p) && ScreenToClient(m_hwnd, &p))
			return (axisNum == AXIS_X ? p.x : p.y);
	}

	// Otherwise, return the raw DirectInput axis values
	switch (axisNum)
	{
		case AXIS_X: return m_diMseState.x;
		case AXIS_Y: return m_diMseState.y;
		case AXIS_Z: return m_diMseState.z;
		default:     return 0;
	}
}

int CDirectInputSystem::GetMouseWheelDir(int mseNum)
{
	if (m_useRawInput)
	{
		// For RawInput, get combined or individual mouse state and return the wheel value
		RawMseState *mseState = (mseNum == ANY_MOUSE ? &m_combRawMseState : m_rawMseStates[mseNum]);
		return mseState->wheelDir;
	}

	// For DirectInput just return the common wheel value
	return m_diMseState.wheelDir;
}

bool CDirectInputSystem::IsMouseButPressed(int mseNum, int butNum)
{
	if (m_useRawInput)
	{
		// For RawInput, get combined or individual mouse state and return the button state
		RawMseState *mseState = (mseNum == ANY_MOUSE ? &m_combRawMseState : m_rawMseStates[mseNum]);
		return !!(mseState->buttons & (1<<butNum));
	}
	
	// For DirectInput just return the common button state (taking care with the middle and right mouse buttons
	// which DirectInput numbers 2 and 1 respectively, rather than 1 and 2)
	if      (butNum == 1) butNum = 2;
	else if (butNum == 2) butNum = 1;
	return (butNum < 5 ? !!(m_diMseState.buttons[butNum] & 0x80) : false);
}

int CDirectInputSystem::GetJoyAxisValue(int joyNum, int axisNum)
{
	// Get joystick state for given joystick and return raw joystick axis value (values range from -32768 to 32767)
	LPDIJOYSTATE2 diJoyState = m_diJoyStates[joyNum];
	switch (axisNum)
	{
		case AXIS_X:  return (int)diJoyState->lX;
		case AXIS_Y:  return (int)diJoyState->lY;
		case AXIS_Z:  return (int)diJoyState->lZ;
		case AXIS_RX: return (int)diJoyState->lRx;
		case AXIS_RY: return (int)diJoyState->lRy;
		case AXIS_RZ: return (int)diJoyState->lRz;
		default:      return 0;
	}
}

bool CDirectInputSystem::IsJoyPOVInDir(int joyNum, int povNum, int povDir)
{
	// Get joystick state for given joystick and check if POV-hat value for given POV number is pointing in required direction
	LPDIJOYSTATE2 diJoyState = m_diJoyStates[joyNum];
	int povVal = diJoyState->rgdwPOV[povNum] / 100;   // DirectInput value is angle of POV-hat in 100ths of a degree
	switch (povDir)
	{
		case POV_UP:    return povVal == 315 || povVal == 0 || povVal == 45;
		case POV_DOWN:  return povVal == 135 || povVal == 180 || povVal == 225;
		case POV_RIGHT: return povVal == 45 || povVal == 90 || povVal == 135;
		case POV_LEFT:  return povVal == 225 || povVal == 270 || povVal == 315;
		default:        return false;
	}
}

bool CDirectInputSystem::IsJoyButPressed(int joyNum, int butNum)
{
	// Get joystick state for given joystick and return current button value for given button number
	LPDIJOYSTATE2 diJoyState = m_diJoyStates[joyNum];
	return !!diJoyState->rgbButtons[butNum];
}

void CDirectInputSystem::Wait(int ms)
{
	Sleep(ms);
}

bool CDirectInputSystem::ConfigMouseCentered()
{
	// When checking if mouse centered, use system cursor rather than raw values (otherwise user's mouse movements won't match up
	// with onscreen cursor during configuration)
	POINT p;
	if (!GetCursorPos(&p) || !ScreenToClient(m_hwnd, &p))
		return false;
	
	// See if mouse in center of display
	unsigned lx = m_dispX + m_dispW / 4;
	unsigned ly = m_dispY + m_dispH / 4;
	if (p.x < lx || p.x > lx + m_dispW / 2 || p.y < ly || p.y > ly + m_dispH / 2)
		return false;

	// Once mouse has been centered, sync up mice raw values with current cursor position so that movements are detected correctly
	ResetMice();
	return true;
}

CInputSource *CDirectInputSystem::CreateAnyMouseSource(EMousePart msePart)
{
	// If using RawInput, create a mouse source that uses the combined mouse state m_combRawState, rather than combining all the individual mouse
	// sources in the default manner
	if (m_useRawInput)
		return CreateMouseSource(ANY_MOUSE, msePart);

	return CInputSystem::CreateAnyMouseSource(msePart);
}

bool CDirectInputSystem::Poll()
{
	// See if keyboard, mice and joysticks have been activated yet
	if (!m_activated)
	{
		// If not, then get Window handle of SDL window
		SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (SDL_GetWMInfo(&info)) 
			m_hwnd = info.window;
		
		// Tell SDL to pass on all Windows events
		// Removed - see below
		//SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

		// Activate the devices now that a Window handle is available
		ActivateKeyboardsAndMice();
		ActivateJoysticks();

		m_activated = true;
	}

	// Wait or poll for event from SDL
	// Removed - see below
	/*
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT)
			return false; 	
		else if (e.type == SDL_SYSWMEVENT)
		{
			SDL_SysWMmsg *wmMsg = e.syswm.msg;
			ProcessWMEvent(wmMsg->hwnd, wmMsg->msg, wmMsg->wParam, wmMsg->lParam);
		}
	}*/

	// Wait or poll for event on Windows message queue (done this way instead of using SDL_PollEvent as above because
	// for some reason this causes RawInput HRAWINPUT handles to arrive stale.  Not sure what SDL_PollEvent is doing to cause this
	// but the following code can replace it without any problems as it is effectively what SDL_PollEvent does anyway)
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) 
	{
		int ret = GetMessage(&msg, NULL, 0, 0);
		if (ret == 0)
			return false;
		else if (ret > 0)
		{
			TranslateMessage(&msg);

			// Handle RawInput messages here
			if (m_useRawInput && msg.message == WM_INPUT)
				ProcessRawInput((HRAWINPUT)msg.lParam);

			// Propagate all messages to default (SDL) handlers
			DispatchMessage(&msg);
		}
	}

	// Poll keyboards, mice and joysticks
	PollKeyboardsAndMice();
	PollJoysticks();

	return true;
}

void CDirectInputSystem::SetMouseVisibility(bool visible)
{
	if (m_useRawInput)
		ShowCursor(m_isConfiguring && visible ? TRUE : FALSE);
	else
		ShowCursor(visible ? TRUE : FALSE);
}

void CDirectInputSystem::ConfigEnd()
{
	CInputSystem::ConfigEnd();

	// When configuration has taken place, make sure devices get re-activated
	m_activated = false;
}
