#ifndef INCLUDED_SDLINPUTSYSTEM_H
#define INCLUDED_SDLINPUTSYSTEM_H

#include "Types.h"
#include "Inputs/InputSource.h"
#include "Inputs/InputSystem.h"

#ifdef SUPERMODEL_OSX
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#include <vector>
using namespace std;

#define NUM_SDL_KEYS (sizeof(s_keyMap) / sizeof(SDLKeyMapStruct))

struct SDLKeyMapStruct
{
	const char *keyName;
	SDLKey sdlKey;
};

/*
 * Input system that uses SDL.
 */
class CSDLInputSystem : public CInputSystem
{
private:
	// Lookup table to map key names to SDLKeys
	static SDLKeyMapStruct s_keyMap[];

	// Vector to keep track of attached joysticks
	vector<SDL_Joystick*> m_joysticks;

	vector<JoyDetails*> m_joyDetails;

	// Current key state obtained from SDL
	Uint8 *m_keyState;

	// Current mouse state obtained from SDL
	int m_mouseX;
	int m_mouseY;
	int m_mouseZ;
	short m_mouseWheelDir;
	Uint8 m_mouseButtons;

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

	int GetNumKeyboards();	

	int GetNumMice();
	
	int GetNumJoysticks();

	int GetKeyIndex(const char *keyName);

	const char *GetKeyName(int keyIndex);

	JoyDetails *GetJoyDetails(int joyNum);

	bool IsKeyPressed(int kbdNum, int keyIndex);

	int GetMouseAxisValue(int mseNum, int axisNum);
	
	int GetMouseWheelDir(int mseNum);

	bool IsMouseButPressed(int mseNum, int butNum);

	int GetJoyAxisValue(int joyNum, int axisNum);

	bool IsJoyPOVInDir(int joyNum, int povNum, int povDir);

	bool IsJoyButPressed(int joyNum, int butNum);

	void Wait(int ms);

	void SetMouseVisibility(bool visible);

public:
	/*
	 * Constructs an SDL input system.
	 */
	CSDLInputSystem();

	~CSDLInputSystem();

	bool Poll();
};

#endif	// INCLUDED_SDLINPUTSYSTEM_H
