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
#include <dinput.h>

static LPDIRECTINPUT8			dinput;
static LPDIRECTINPUTDEVICE8		keyboard;
static LPDIRECTINPUTDEVICE8		mouse;

static CHAR keyboard_buffer[256];
static DIMOUSESTATE mouse_state;

extern HWND main_window;

static OSD_CONTROLS controls;

void osd_input_init(void)
{
	HINSTANCE hinstance;
	HRESULT hr;

	atexit(osd_input_shutdown);

	hinstance = GetModuleHandle(NULL);
	hr = DirectInput8Create( hinstance, DIRECTINPUT_VERSION, &IID_IDirectInput8,
							 (void**)&dinput, NULL );
	if(FAILED(hr))
		error("DirectInput8Create failed.");

	// Create keyboard device
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

	// Create mouse device
	hr = IDirectInput8_CreateDevice( dinput, &GUID_SysMouse, &mouse, NULL );
	if(FAILED(hr))
		error("IDirectInput8_CreateDevice failed.");

	hr = IDirectInputDevice8_SetDataFormat( mouse, &c_dfDIMouse );
	if(FAILED(hr))
		error("IDirectInputDevice8_SetDataFormat failed.");

	hr = IDirectInputDevice8_SetCooperativeLevel( mouse, main_window, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE );
	if(FAILED(hr))
		error("IDirectInputDevice8_SetCooperativeLevel failed.");
}

void osd_input_shutdown(void)
{
	if(dinput) {
		if(keyboard) {
			IDirectInputDevice8_Unacquire( keyboard );
			IDirectInputDevice8_Release( keyboard );
			keyboard = NULL;
		}
		if(mouse) {
			IDirectInputDevice8_Unacquire( mouse );
			IDirectInputDevice8_Release( mouse );
			mouse = NULL;
		}
		IDirectInput8_Release( dinput );
		dinput = NULL;
	}
}

static void input_update(void)
{
	/* updates the input buffer */
	if(dinput) {
		// Get keyboard state
		IDirectInputDevice8_GetDeviceState( keyboard, sizeof(keyboard_buffer), &keyboard_buffer );
		// Get mouse state
		IDirectInputDevice8_Acquire( mouse );
		IDirectInputDevice8_GetDeviceState( mouse, sizeof(mouse_state), &mouse_state );
	}
}

static BOOL keyboard_get_key(UINT8 key)
{
	if(keyboard_buffer[key] & 0x80)
		return TRUE;
	else
		return FALSE;
}

static BOOL mouse_get_button(UINT8 button)
{
	if(mouse_state.rgbButtons[button] & 0x80)
		return TRUE;
	else
		return FALSE;
}

static void mouse_get_position(INT32* xposition, INT32* yposition)
{
	POINT mouse_pos;

	GetCursorPos( &mouse_pos );
	ScreenToClient( main_window, &mouse_pos );

	*xposition = mouse_pos.x + (400 - (MODEL3_SCREEN_WIDTH / 2));
	*yposition = mouse_pos.y + (272 - (MODEL3_SCREEN_HEIGHT / 2));
}

OSD_CONTROLS* osd_input_update_controls(void)
{
	INT32 mouse_x, mouse_y;
	input_update();

    controls.game_controls[0] = 0xFF;
    controls.game_controls[1] = 0xFF;
    controls.system_controls[0] = 0xFF;
    controls.system_controls[1] = 0xFF;

	// Lightgun
	if(mouse_get_button(0))
		controls.game_controls[0] &= ~0x01;

	if(mouse_get_button(1))
		controls.gun_acquired[0] = TRUE;
	else
		controls.gun_acquired[0] = FALSE;

	mouse_get_position(&mouse_x, &mouse_y);
	controls.gun_x[0] = mouse_x;
	controls.gun_y[0] = mouse_y;

	// Game controls
	if(keyboard_get_key(DIK_A))
		controls.game_controls[0] &= ~0x01;
	if(keyboard_get_key(DIK_S))
		controls.game_controls[0] &= ~0x02;
	if(keyboard_get_key(DIK_D))
		controls.game_controls[0] &= ~0x04;
	if(keyboard_get_key(DIK_F))
		controls.game_controls[0] &= ~0x08;
	if(keyboard_get_key(DIK_G))
		controls.game_controls[0] &= ~0x80;
	if(keyboard_get_key(DIK_H))
		controls.game_controls[0] &= ~0x40;

	if(keyboard_get_key(DIK_Q))
		controls.game_controls[0] &= ~0x80;
	if(keyboard_get_key(DIK_W))
		controls.game_controls[0] &= ~0x40;
	if(keyboard_get_key(DIK_E))
		controls.game_controls[0] &= ~0x20;
	if(keyboard_get_key(DIK_R))
		controls.game_controls[0] &= ~0x10;
	if(keyboard_get_key(DIK_T))
		controls.game_controls[1] &= ~0x80;
	if(keyboard_get_key(DIK_Y))
		controls.game_controls[1] &= ~0x40;
	if(keyboard_get_key(DIK_U))
		controls.game_controls[1] &= ~0x20;
	if(keyboard_get_key(DIK_I))
		controls.game_controls[1] &= ~0x10;

	if(keyboard_get_key(DIK_Z))		// VON2, shot trigger 1
		controls.game_controls[0] &= ~0x01;
	if(keyboard_get_key(DIK_X))		// VON2, shot trigger 2
		controls.game_controls[1] &= ~0x01;
	if(keyboard_get_key(DIK_C))		// VON2, turbo 1
		controls.game_controls[0] &= ~0x02;
	if(keyboard_get_key(DIK_V))		// VON2, turbo 2
		controls.game_controls[1] &= ~0x02;

    if(keyboard_get_key(DIK_UP) || keyboard_get_key(DIK_NUMPAD8)) {
        controls.game_controls[0] &= ~0x20; // VON2, forward
		controls.game_controls[1] &= ~0x20;
	}
    if(keyboard_get_key(DIK_DOWN) || keyboard_get_key(DIK_NUMPAD2)) {
        controls.game_controls[0] &= ~0x10; // VON2, backward
		controls.game_controls[1] &= ~0x10;
	}
    if(keyboard_get_key(DIK_LEFT) || keyboard_get_key(DIK_NUMPAD4)) {
        controls.game_controls[0] &= ~0x10; // VON2, turn left
		controls.game_controls[1] &= ~0x20;
	}
    if(keyboard_get_key(DIK_RIGHT) || keyboard_get_key(DIK_NUMPAD6)) {
        controls.game_controls[0] &= ~0x20; // VON2, turn right
		controls.game_controls[1] &= ~0x10;
	}
	if(keyboard_get_key(DIK_NUMPAD1)) {// VON2, strafe left
		controls.game_controls[0] &= ~0x80;
		controls.game_controls[1] &= ~0x80;
	}
	if(keyboard_get_key(DIK_NUMPAD3)) {// VON2, strafe right
		controls.game_controls[0] &= ~0x40;
		controls.game_controls[1] &= ~0x40;
	}
    if(keyboard_get_key(DIK_NUMPAD5)) {// VON2, jump
        controls.game_controls[0] &= ~0x80;
        controls.game_controls[1] &= ~0x40;
	}

	// System controls
	if(keyboard_get_key(DIK_F1)) {	// Test button
		controls.system_controls[0] &= ~0x04;
		controls.system_controls[1] &= ~0x04;
	}
	if(keyboard_get_key(DIK_F2)) {	// Service button
		controls.system_controls[0] &= ~0x08;
		controls.system_controls[1] &= ~0x80;
	}
	if(keyboard_get_key(DIK_F3)) {	// Start button
		controls.system_controls[0] &= ~0x10;
	}
	if(keyboard_get_key(DIK_F4)) {	// Coin #1
		controls.system_controls[0] &= ~0x01;
	}

	// Steering Wheel
	if(keyboard_get_key(DIK_LEFT))
		controls.steering -= 32;
	if(keyboard_get_key(DIK_RIGHT))
		controls.steering += 32;

	if(controls.steering > 0) {
		controls.steering -= 16;
		if(controls.steering < 0)
			controls.steering = 0;
	} else {
		controls.steering += 16;
		if(controls.steering > 0)
			controls.steering = 0;
	}

	if(controls.steering < -128)
		controls.steering = -128;
	if(controls.steering > 127)
		controls.steering = 127;

	if(keyboard_get_key(DIK_UP))
		controls.acceleration += 32;
	else
		controls.acceleration -= 16;

	if(controls.acceleration < 0)
		controls.acceleration = 0;
	if(controls.acceleration > 0xFF)
		controls.acceleration = 0xFF;

	if(keyboard_get_key(DIK_DOWN))
		controls.brake += 32;
	else
		controls.brake -= 16;

	if(controls.brake < 0)
		controls.brake = 0;
	if(controls.brake > 0xFF)
		controls.brake = 0xFF;


	return &controls;
}
