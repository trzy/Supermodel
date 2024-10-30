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
 * SDLInputSystem.h
 *
 * Header file for SDL input system.
 */

#ifndef INCLUDED_SDLINPUTSYSTEM_H
#define INCLUDED_SDLINPUTSYSTEM_H

#include "Types.h"
#include "Inputs/InputSource.h"
#include "Inputs/InputSystem.h"
#include "SDLIncludes.h"

#include <vector>

#define NUM_SDL_KEYS (sizeof(s_keyMap) / sizeof(SDLKeyMapStruct))

struct SDLKeyMapStruct
{
	const char *keyName;
	SDL_Scancode sdlKey;
};

/*
 * Input system that uses SDL.
 */
class CSDLInputSystem : public CInputSystem
{
private:
	const Util::Config::Node& m_config;

	// Lookup table to map key names to SDLKeys
	static SDLKeyMapStruct s_keyMap[];

	// Vector to keep track of attached joysticks
	std::vector<SDL_Joystick*> m_joysticks;

	// Vector of joystick details
	std::vector<JoyDetails> m_joyDetails;

	// Current key state obtained from SDL
	const Uint8 *m_keyState;

	// Current mouse state obtained from SDL
	int m_mouseX;
	int m_mouseY;
	int m_mouseZ;
	short m_mouseWheelDir;
	Uint8 m_mouseButtons;

	// SDL2 ffb
	SDL_HapticEffect eff;
	unsigned sdlConstForceMax;
	unsigned sdlSelfCenterMax;
	unsigned sdlFrictionMax;
	unsigned sdlVibrateMax;
	struct hapticInfo
	{
		SDL_Haptic* SDLhaptic = NULL;
		int effectConstantForceID = -1;
		int effectVibrationID = -1;
		int effectSpringForceID = -1;
		int effectFrictionForceID = -1;
	};
	std::vector<hapticInfo> m_SDLHapticDatas;

	/*
	 * Opens all attached joysticks.
	 */
	void OpenJoysticks();

	/*
	 * Closes all attached joysticks.
	 */
	void CloseJoysticks();

protected:
	/*
	 * Initializes the SDL input system.
	 */
	bool InitializeSystem();

	int GetKeyIndex(const char *keyName);

	const char *GetKeyName(int keyIndex);

	bool IsKeyPressed(int kbdNum, int keyIndex) const;

	int GetMouseAxisValue(int mseNum, int axisNum) const;

	int GetMouseWheelDir(int mseNum) const;

	bool IsMouseButPressed(int mseNum, int butNum) const;

	int GetJoyAxisValue(int joyNum, int axisNum) const;

	bool IsJoyPOVInDir(int joyNum, int povNum, int povDir) const;

	bool IsJoyButPressed(int joyNum, int butNum) const;

	bool ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd);

	void StopAllEffect(int joyNum);

	void StopConstanteforce(int joyNum);

	void StopVibrationforce(int joyNum);

	void StopSpringforce(int joyNum);

	void StopFrictionforce(int joyNum);

	void VibrationEffect(float strength, int joyNum);

	void ConstantForceEffect(float force, int dir, int length, int joyNum);

	void SpringForceEffect(float force, int joyNum);

	void FrictionForceEffect(float force, int joyNum);

	bool HasBasicForce(SDL_Haptic* hap);

public:
	/*
	 * Constructs an SDL input system.
	 */
	CSDLInputSystem(const Util::Config::Node& config);

	~CSDLInputSystem();

	int GetNumKeyboards() const;

	int GetNumMice() const;

	int GetNumJoysticks() const;

	const KeyDetails *GetKeyDetails(int kbdNum);

	const MouseDetails *GetMouseDetails(int mseNum);

	const JoyDetails *GetJoyDetails(int joyNum);

	bool Poll();

	void SetMouseVisibility(bool visible);
};

#endif	// INCLUDED_SDLINPUTSYSTEM_H
