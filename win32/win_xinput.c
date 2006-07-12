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
/* XInput for Xbox360 controller								  */
/******************************************************************/

#include "MODEL3.H"
#include <dinput.h>
#include <xinput.h>

static LPDIRECTINPUT8			dinput;
static LPDIRECTINPUTDEVICE8		keyboard;
static LPDIRECTINPUTDEVICE8		mouse;

static CHAR keyboard_buffer[256];
static DIMOUSESTATE mouse_state;

extern HWND main_window;

static OSD_CONTROLS controls;

static int xbox_controllers[4];


static int analog_axis[8];
static BOOL button_state[16];

static struct
{
	BOOL up, down, left, right;
} joystick[4];

BOOL osd_input_init(void)
{
	HINSTANCE hinstance;
	HRESULT hr;
	
	DWORD result;
	XINPUT_STATE state;

	atexit(osd_input_shutdown);

	hinstance = GetModuleHandle(NULL);
	hr = DirectInput8Create( hinstance, DIRECTINPUT_VERSION, &IID_IDirectInput8,
							 (void**)&dinput, NULL );
	if (FAILED(hr))
	{
		message(0, "DirectInput8Create failed.");
		return FALSE;
	}

	// Create keyboard device
	hr = IDirectInput8_CreateDevice( dinput, &GUID_SysKeyboard, &keyboard, NULL );
	if (FAILED(hr))
	{
		message(0, "IDirectInput8_CreateDevice failed.");
		return FALSE;
	}

	hr = IDirectInputDevice8_SetDataFormat( keyboard, &c_dfDIKeyboard );
	if (FAILED(hr))
	{
		message(0, "IDirectInputDevice8_SetDataFormat failed.");
		return FALSE;
	}

	hr = IDirectInputDevice8_SetCooperativeLevel( keyboard, main_window, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE );
	if (FAILED(hr))
	{
		message(0, "IDirectInputDevice8_SetCooperativeLevel failed.");
		return FALSE;
	}

	if (keyboard)
	{
		IDirectInputDevice8_Acquire( keyboard );
	}

	// Create mouse device
	hr = IDirectInput8_CreateDevice( dinput, &GUID_SysMouse, &mouse, NULL );
	if (FAILED(hr))
	{
		message(0, "IDirectInput8_CreateDevice failed.");
		return FALSE;
	}

	hr = IDirectInputDevice8_SetDataFormat( mouse, &c_dfDIMouse );
	if (FAILED(hr))
	{
		message(0, "IDirectInputDevice8_SetDataFormat failed.");
		return FALSE;
	}

	hr = IDirectInputDevice8_SetCooperativeLevel( mouse, main_window, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE );
	if (FAILED(hr))
	{
		message(0, "IDirectInputDevice8_SetCooperativeLevel failed.");
		return FALSE;
	}

	// Init Xbox360 controllers
	ZeroMemory(&state, sizeof(XINPUT_STATE));

	// Simply get the state of the controller from XInput.
	result = XInputGetState(0, &state);

	if (result == ERROR_SUCCESS)
	{ 
		// Controller is connected
		xbox_controllers[0] = 1;
		message(0, "Xbox360 controller found!");
	}
	else
	{
		// Controller is not connected
		xbox_controllers[0] = 0;
	}

	return TRUE;
}

void osd_input_shutdown(void)
{
	if(dinput)
	{
		if(keyboard)
		{
			IDirectInputDevice8_Unacquire( keyboard );
			IDirectInputDevice8_Release( keyboard );
			keyboard = NULL;
		}
		if(mouse)
		{
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
	if(dinput)
	{
		// Get keyboard state
		IDirectInputDevice8_GetDeviceState( keyboard, sizeof(keyboard_buffer), &keyboard_buffer );
		// Get mouse state
		IDirectInputDevice8_Acquire( mouse );
		IDirectInputDevice8_GetDeviceState( mouse, sizeof(mouse_state), &mouse_state );
	}
}

static BOOL keyboard_get_key(UINT8 key)
{
	if (keyboard_buffer[key] & 0x80)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static BOOL mouse_get_button(UINT8 button)
{
	if (mouse_state.rgbButtons[button] & 0x80)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static void mouse_get_position(INT32* xposition, INT32* yposition)
{
	POINT mouse_pos;

	GetCursorPos( &mouse_pos );
	ScreenToClient( main_window, &mouse_pos );

	*xposition = mouse_pos.x;
	*yposition = mouse_pos.y;
}

static void update_controls_keyboard(void)
{
	memset(button_state, 0, sizeof(button_state));
	memset(joystick, 0, sizeof(joystick));

	if (keyboard_get_key(DIK_A))		button_state[0] = 1;
	if (keyboard_get_key(DIK_S))		button_state[1] = 1;
	if (keyboard_get_key(DIK_D))		button_state[2] = 1;
	if (keyboard_get_key(DIK_F))		button_state[3] = 1;
	if (keyboard_get_key(DIK_Z))		button_state[4] = 1;
	if (keyboard_get_key(DIK_X))		button_state[5] = 1;
	if (keyboard_get_key(DIK_C))		button_state[6] = 1;
	if (keyboard_get_key(DIK_V))		button_state[7] = 1;

	if (keyboard_get_key(DIK_LEFT))		joystick[0].left = TRUE;
	if (keyboard_get_key(DIK_RIGHT))	joystick[0].right = TRUE;
	if (keyboard_get_key(DIK_UP))		joystick[0].up = TRUE;
	if (keyboard_get_key(DIK_DOWN))		joystick[0].down = TRUE;

	if (keyboard_get_key(DIK_LEFT))		analog_axis[0] -= 16;
	if (keyboard_get_key(DIK_RIGHT))	analog_axis[0] += 16;
	if (keyboard_get_key(DIK_UP))		analog_axis[1] -= 16;
	if (keyboard_get_key(DIK_DOWN))		analog_axis[1] += 16;

	if (analog_axis[0] < -128)	analog_axis[0] = -128;
	if (analog_axis[0] > 127)	analog_axis[0] = 127;
	if (analog_axis[1] < -128)	analog_axis[1] = -128;
	if (analog_axis[1] > 127)	analog_axis[1] = 127;
}

static void update_controls_xbox(void)
{
	XINPUT_STATE state;
	DWORD res;

	memset(button_state, 0, sizeof(button_state));
	memset(analog_axis, 0, sizeof(analog_axis));
	memset(joystick, 0, sizeof(joystick));

	res = XInputGetState(0, &state);
	if (res == ERROR_SUCCESS)
	{
		// Zero value if thumbsticks are within the dead zone 
		if ((state.Gamepad.sThumbLX < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE && 
			 state.Gamepad.sThumbLX > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE))
		{
			state.Gamepad.sThumbLX = 0;
		}
		if ((state.Gamepad.sThumbLY < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE && 
			 state.Gamepad.sThumbLY > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE))
		{
			
			state.Gamepad.sThumbLY = 0;
		}
		
		if ((state.Gamepad.sThumbRX < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE && 
			 state.Gamepad.sThumbRX > -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE))
		{
			state.Gamepad.sThumbRX = 0;
		}
		if ((state.Gamepad.sThumbRY < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE && 
			 state.Gamepad.sThumbRY > -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)) 
		{
			state.Gamepad.sThumbRY = 0;
		}


		if (state.Gamepad.wButtons & XINPUT_GAMEPAD_A)		button_state[0] = TRUE;
		if (state.Gamepad.wButtons & XINPUT_GAMEPAD_B)		button_state[1] = TRUE;
		if (state.Gamepad.wButtons & XINPUT_GAMEPAD_X)		button_state[2] = TRUE;
		if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)		button_state[3] = TRUE;

		if (state.Gamepad.sThumbLX < 0)		joystick[0].left = TRUE;
		if (state.Gamepad.sThumbLX > 0)		joystick[0].right = TRUE;
		if (state.Gamepad.sThumbLY < 0)		joystick[0].up = TRUE;
		if (state.Gamepad.sThumbLY > 0)		joystick[0].down = TRUE;

		analog_axis[0] = state.Gamepad.sThumbLX / 256;
		analog_axis[1] = state.Gamepad.sThumbLY / 256;
		analog_axis[2] = state.Gamepad.bRightTrigger;
		analog_axis[3] = state.Gamepad.bLeftTrigger;
		analog_axis[4] = state.Gamepad.sThumbRX;
		analog_axis[5] = state.Gamepad.sThumbRY;
	}
}

static int get_analog_axis(GAME_ANALOG axis)
{
	switch (axis)
	{
		case ANALOG_AXIS_1:		return analog_axis[0];
		case ANALOG_AXIS_2:		return analog_axis[1];
		case ANALOG_AXIS_3:		return analog_axis[2];
		case ANALOG_AXIS_4:		return analog_axis[3];
		case ANALOG_AXIS_5:		return analog_axis[4];
		case ANALOG_AXIS_6:		return analog_axis[5];
		case ANALOG_AXIS_7:		return analog_axis[6];
		case ANALOG_AXIS_8:		return analog_axis[7];
	}

	return 0;
}

static BOOL is_button_pressed(GAME_BUTTON button)
{
	switch (button)
	{
		case P1_BUTTON_1:			return button_state[0];
		case P1_BUTTON_2:			return button_state[1];
		case P1_BUTTON_3:			return button_state[2];
		case P1_BUTTON_4:			return button_state[3];
		case P1_BUTTON_5:			return button_state[4];
		case P1_BUTTON_6:			return button_state[5];
		case P1_BUTTON_7:			return button_state[6];
		case P1_BUTTON_8:			return button_state[7];
		case P2_BUTTON_1:			return button_state[8];
		case P2_BUTTON_2:			return button_state[9];
		case P2_BUTTON_3:			return button_state[10];
		case P2_BUTTON_4:			return button_state[11];
		case P2_BUTTON_5:			return button_state[12];
		case P2_BUTTON_6:			return button_state[13];
		case P2_BUTTON_7:			return button_state[14];
		case P2_BUTTON_8:			return button_state[15];
		case P1_JOYSTICK_UP:		return joystick[0].up;
		case P1_JOYSTICK_DOWN:		return joystick[0].down;
		case P1_JOYSTICK_LEFT:		return joystick[0].left;
		case P1_JOYSTICK_RIGHT:		return joystick[0].right;
	}

	return FALSE;
}

OSD_CONTROLS* osd_input_update_controls(void)
{
	int i;
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
	if (!m3_config.fullscreen)
	{
		controls.gun_x[0] = mouse_x + (400 - (MODEL3_SCREEN_WIDTH / 2));
		controls.gun_y[0] = mouse_y + (272 - (MODEL3_SCREEN_HEIGHT / 2));
	}
	else
	{
		if (m3_config.stretch)
		{
			mouse_x = (mouse_x * 496) / m3_config.width;
			mouse_y = (mouse_y * 384) / m3_config.height;
		}

		controls.gun_x[0] = mouse_x + (400 - (MODEL3_SCREEN_WIDTH / 2));
		controls.gun_y[0] = mouse_y + (272 - (MODEL3_SCREEN_HEIGHT / 2));
	}

	// Game controls
	if (xbox_controllers[0])
	{
		update_controls_xbox();
	}
	else
	{
		update_controls_keyboard();
	}

	// update button states
	for (i=0; i < 16; i++)
	{
		if (m3_config.controls.button[i].enabled)
		{
			if (is_button_pressed(m3_config.controls.button[i].mapping))
			{
				int set = m3_config.controls.button[i].control_set;
				controls.game_controls[set] &= ~m3_config.controls.button[i].control_bit;
			}
		}
	}

	// update analog controls
	for (i=0; i < 8; i++)
	{
		if (m3_config.controls.analog_axis[i].enabled)
		{
			int value = get_analog_axis(m3_config.controls.analog_axis[i].mapping);
			value += m3_config.controls.analog_axis[i].center;
			controls.analog_axis[i] = value;
		}
	}

	// Lightgun hack for Star Wars Trilogy
	if (stricmp(m3_config.game_id, "swtrilgy") == 0)
	{
		mouse_get_position(&mouse_x, &mouse_y);

		mouse_x = (mouse_x * 256) / ((m3_config.fullscreen) ? m3_config.width : 496);
		mouse_y = (mouse_y * 256) / ((m3_config.fullscreen) ? m3_config.height : 384);

		controls.analog_axis[0] = mouse_y;
		controls.analog_axis[1] = mouse_x;

		if (mouse_get_button(1))
			controls.game_controls[0] &= ~0x01;
		if (mouse_get_button(0))
			controls.game_controls[0] &= ~0x20;
	}

	// System controls
	if (keyboard_get_key(DIK_F1))	// Service button
	{
		controls.system_controls[0] &= ~0x08;
	}
	if (keyboard_get_key(DIK_F2))	// Test button
	{
		controls.system_controls[0] &= ~0x04;
	}
	if (keyboard_get_key(DIK_F3))	// Service button B
	{
		controls.system_controls[1] &= ~0x40;
	}
	if (keyboard_get_key(DIK_F4))	// Test button B
	{
		controls.system_controls[1] &= ~0x80;
	}
	if (keyboard_get_key(DIK_1))	// Start button 1
	{
		controls.system_controls[0] &= ~0x10;
	}
	if (keyboard_get_key(DIK_2))	// Start button 2
	{
		controls.system_controls[0] &= ~0x20;
	}
	if (keyboard_get_key(DIK_5))	// Coin #1
	{
		controls.system_controls[0] &= ~0x01;
	}
	if (keyboard_get_key(DIK_6))	// Coin #2
	{
		controls.system_controls[0] &= ~0x02;
	}

	return &controls;
}
