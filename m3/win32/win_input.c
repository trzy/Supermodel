/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/******************************************************************/
/* DirectX 9 Input                                                */
/******************************************************************/

#include "MODEL3.H"

static LPDIRECTINPUT8			dinput;
static LPDIRECTINPUTDEVICE8		keyboard;
static LPDIRECTINPUTDEVICE8		mouse;

static CHAR keyboard_buffer[256];

extern HWND main_window;

void input_init(void)
{
	HINSTANCE hinstance;
	HRESULT hr;

	hinstance = GetModuleHandle(NULL);
	hr = DirectInput8Create( hinstance, DIRECTINPUT_VERSION, &IID_IDirectInput8,
							 (void**)&dinput, NULL );
	if(FAILED(hr))
		error("DirectInput8Create failed.");

	hr = IDirectInput8_CreateDevice( dinput, &GUID_SysKeyboard, &keyboard, NULL );
	if(FAILED(hr))
		error("IDirectInput8_CreateDevice failed.");

	hr = IDirectInputDevice8_SetDataFormat( keyboard, &c_dfDIKeyboard );
	if(FAILED(hr))
		error("IDirectInputDevice8_SetDataFormat failed.");

	hr = IDirectInputDevice8_SetCooperativeLevel( keyboard, main_window, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE );
	if(FAILED(hr))
		error("IDirectInputDevice8_SetCooperativeLevel failed.");

	if(keyboard)
		IDirectInputDevice8_Acquire( keyboard );
}

void input_shutdown(void)
{
	if(dinput) {
		if(keyboard) {
			IDirectInputDevice8_Unacquire( keyboard );
			IDirectInputDevice8_Release( keyboard );
			keyboard = NULL;
		}
		IDirectInput8_Release( dinput );
		dinput = NULL;
	}
}

void input_reset(void)
{

}

void input_update(void)
{
	/* updates the input buffer */
	if(dinput) {
		IDirectInputDevice8_GetDeviceState( keyboard, sizeof(keyboard_buffer), &keyboard_buffer );
	}
}

BOOL keyboard_get_key(UINT8 key)
{
	if(keyboard_buffer[key] & 0x80)
		return TRUE;
	else
		return FALSE;
}

BOOL input_get_key(UINT8 key)
{
	/* returns TRUE if the key is pressed, else FALSE */
	/* NOTE: this approach only works if we're handling */
	/* the keyboard alone. If we will be using the mouse */
	/* and eventually a joypad, we'll need another method, */
	/* more flexible, to allow the user the maximum choice. */

	/* we'll need a convention for key names to be constant */
	/* on all platforms. */
	return keyboard_get_key(key);
}

