/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
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
 * Main.cpp
 * 
 * Main program driver for the SDL port.
 *
 * Compile-Time Options
 * --------------------
 * - SUPERMODEL_WIN32: Define this if compiling on Windows.
 * - SUPERMODEL_OSX: Define this if compiling on Mac OS X.
 *
 * TO-DO List
 * ----------
 * - A lot of this code is actually OS-independent! Should it be moved into the
 *   root of the source tree? Might not be worth it; eventually, OS-dependent
 *	 UIs will be introduced.
 */
 
#ifdef SUPERMODEL_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <new>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "Pkgs/glew.h"
#ifdef SUPERMODEL_OSX
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#include "Supermodel.h"

// stricmp() is non-standard, apparently...
#ifdef _MSC_VER	// MS VisualC++
	#define stricmp	_stricmp
#else			// assume GCC
	#define stricmp	strcasecmp
#endif


/******************************************************************************
 Display Management
******************************************************************************/

/*
 * Position and size of rectangular region within OpenGL display to render to
 */
unsigned	xOffset, yOffset;	// offset of renderer output within OpenGL viewport
unsigned 	xRes, yRes;			// renderer output resolution (can be smaller than GL viewport)

/*
 * CreateGLScreen():
 *
 * Creates an OpenGL display surface of the requested size. xOffset and yOffset
 * are used to return a display surface offset (for OpenGL viewport commands)
 * because the actual drawing area may need to be adjusted to preserve the 
 * Model 3 aspect ratio. The new resolution will be passed back as well.
 */
static BOOL CreateGLScreen(const char *caption, unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, BOOL fullScreen)
{
	const SDL_VideoInfo	*VideoInfo;
	GLenum				err;
	float				model3Ratio, ratio;
	float				xRes, yRes;
	
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
    	return ErrorLog("Unable to initialize SDL: %s\n", SDL_GetError());
    
    // Important GL attributes
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,5);	// need at least RGB555 for Model 3 textures
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

  	// Set video mode
  	if (SDL_SetVideoMode(*xResPtr,*yResPtr,0,SDL_OPENGL|(fullScreen?SDL_FULLSCREEN|SDL_HWSURFACE:0)) == NULL)
  	{
    	ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    	SDL_Quit();
    	return FAIL;
  	}
  	
  	if (fullScreen)
  		SDL_ShowCursor(SDL_DISABLE);
  		
  	VideoInfo = SDL_GetVideoInfo();	// what resolution did we actually get?
  	
  	// Fix the aspect ratio of the resolution that the user passed to match Model 3 ratio
  	xRes = (float) *xResPtr;
  	yRes = (float) *yResPtr;
  	model3Ratio = 496.0f/384.0f;
  	ratio = xRes/yRes;
  	if (yRes < (xRes/model3Ratio))
  		xRes = yRes*model3Ratio;
  	if (xRes < (yRes*model3Ratio))
		yRes = xRes/model3Ratio;
		
	// Center the visible area 
  	*xOffsetPtr = (*xResPtr - (unsigned) xRes)/2;
  	*yOffsetPtr = (*yResPtr - (unsigned) yRes)/2;
  	
  	// If the desired resolution is smaller than what we got, re-center again
  	if (*xResPtr < VideoInfo->current_w)
  		*xOffsetPtr += (VideoInfo->current_w - *xResPtr)/2;
  	if (*yResPtr < VideoInfo->current_h)
  		*yOffsetPtr += (VideoInfo->current_h - *yResPtr)/2;
  	
  	// Create window caption
  	SDL_WM_SetCaption(caption,NULL);
  	
  	// Initialize GLEW, allowing us to use features beyond OpenGL 1.2
	err = glewInit();
	if (GLEW_OK != err)
	{
		ErrorLog("OpenGL initialization failed: %s\n", glewGetErrorString(err));
		SDL_Quit();
		return FAIL;
	}
  	
  	// OpenGL initialization
 	glViewport(0,0,*xResPtr,*yResPtr);
 	glClearColor(0.0,0.0,0.0,0.0);
 	glClearDepth(1.0);
 	glDepthFunc(GL_LESS);
 	glEnable(GL_DEPTH_TEST);
 	glShadeModel(GL_SMOOTH);
 	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
 	glDisable(GL_CULL_FACE);
 	glMatrixMode(GL_PROJECTION);
 	glLoadIdentity();
 	gluPerspective(90.0,(GLfloat)xRes/(GLfloat)yRes,0.1,1e5);
 	glMatrixMode(GL_MODELVIEW);
 	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);	// clear at least once to ensure black border
 	
 	// Write back resolution parameters
 	*xResPtr = (unsigned) xRes;
 	*yResPtr = (unsigned) yRes;
 	 	
	return 0;
}

static void DestroyGLScreen(void)
{
	SDL_Quit();
}


/******************************************************************************
 Configuration
******************************************************************************/

#define CONFIG_FILE_PATH	"Config/Supermodel.ini"
#define CONFIG_FILE_COMMENT	";\n" \
							"; Supermodel Configuration File\n" \
							";\n"

// Map keys and buttons to text names
static struct
{
	int				sdlKeyID;		// SDL key identifier (SDLK_*) or mouse button (SDL_BUTTON_*) if negative
	const char		*keyName;		// case-insensitive key name
} InputMap[] =
{
	{ SDLK_UNKNOWN,						"unmapped" },	// this is guaranteed to be at index 0
	{ -SDL_BUTTON(SDL_BUTTON_LEFT),		"MouseLeft" },
	{ -SDL_BUTTON(SDL_BUTTON_MIDDLE),	"MouseMiddle" },
	{ -SDL_BUTTON(SDL_BUTTON_RIGHT),	"MouseRight" },
	{ -SDL_BUTTON(SDL_BUTTON_WHEELUP),	"MouseWheelUp" },
	{ -SDL_BUTTON(SDL_BUTTON_WHEELDOWN),"MouseWheelDown"},
	{ -SDL_BUTTON(SDL_BUTTON_X1),		"MouseX1" },
	{ -SDL_BUTTON(SDL_BUTTON_X2),		"MouseX2" },
	{ SDLK_BACKSPACE,					"Backspace" },
	{ SDLK_TAB,							"Tab" },
	{ SDLK_CLEAR,						"Clear" },
	{ SDLK_RETURN,						"Return" },
	{ SDLK_PAUSE,						"Pause" },
	{ SDLK_ESCAPE,						"Escape" },
	{ SDLK_SPACE,						"Space" },
	{ SDLK_EXCLAIM,						"Exclaim" },
	{ SDLK_QUOTEDBL,					"DblQuote" },
	{ SDLK_HASH,						"Hash" },
	{ SDLK_DOLLAR,						"Dollar" },
	{ SDLK_AMPERSAND,					"Ampersand" },
	{ SDLK_QUOTE,						"Quote" },
	{ SDLK_LEFTPAREN,					"LeftParen" },
	{ SDLK_RIGHTPAREN,					"RightParen" },
	{ SDLK_ASTERISK,					"Asterisk" },
	{ SDLK_PLUS,						"Plus" },
	{ SDLK_COMMA,						"Comma" },
	{ SDLK_MINUS,						"Minus" },
	{ SDLK_PERIOD,						"Period" },
	{ SDLK_SLASH,						"Slash" },
	{ SDLK_0,							"0" },
	{ SDLK_1,							"1" },
	{ SDLK_2,							"2" },
	{ SDLK_3,							"3" },
	{ SDLK_4,							"4" },
	{ SDLK_5,							"5" },
	{ SDLK_6,							"6" },
	{ SDLK_7,							"7" },
	{ SDLK_8,							"8" },
	{ SDLK_9,							"9" },
	{ SDLK_COLON,						"Colon" },
	{ SDLK_SEMICOLON,					"Semicolon" },
	{ SDLK_LESS,						"Less" },
	{ SDLK_EQUALS,						"Equals" },
	{ SDLK_GREATER,						"Greater" },
	{ SDLK_QUESTION,					"Question" },
	{ SDLK_AT,							"At" },
	{ SDLK_LEFTBRACKET,					"LeftBracket" },
	{ SDLK_BACKSLASH,					"BackSlash" },
	{ SDLK_RIGHTBRACKET,				"RightBracket" },
	{ SDLK_CARET,						"Caret" },
	{ SDLK_UNDERSCORE,					"Underscore" },
	{ SDLK_BACKQUOTE,					"BackQuote" },
	{ SDLK_a,							"A" },
	{ SDLK_b,							"B" },
	{ SDLK_c,							"C" },
	{ SDLK_d,							"D" },
	{ SDLK_e,							"E" },
	{ SDLK_f,							"F" },
	{ SDLK_g,							"G" },
	{ SDLK_h,							"H" },
	{ SDLK_i,							"I" },
	{ SDLK_j,							"J" },
	{ SDLK_k,							"K" },
	{ SDLK_l,							"L" },
	{ SDLK_m,							"M" },
	{ SDLK_n,							"N" },
	{ SDLK_o,							"O" },
	{ SDLK_p,							"P" },
	{ SDLK_q,							"Q" },
	{ SDLK_r,							"R" },
	{ SDLK_s,							"S" },
	{ SDLK_t,							"T" },
	{ SDLK_u,							"U" },
	{ SDLK_v,							"V" },
	{ SDLK_w,							"W" },
	{ SDLK_x,							"X" },
	{ SDLK_y,							"Y" },
	{ SDLK_z,							"Z" },
	{ SDLK_DELETE,						"Del" },
	{ SDLK_KP0,							"Keypad0" },
	{ SDLK_KP1,							"Keypad1" },
	{ SDLK_KP2,							"Keypad2" },
	{ SDLK_KP3,							"Keypad3" },
	{ SDLK_KP4,							"Keypad4" },
	{ SDLK_KP5,							"Keypad5" },
	{ SDLK_KP6,							"Keypad6" },
	{ SDLK_KP7,							"Keypad7" },
	{ SDLK_KP8,							"Keypad8" },
	{ SDLK_KP9,							"Keypad9" },
	{ SDLK_KP_PERIOD,					"KeypadPeriod" },
	{ SDLK_KP_DIVIDE,					"KeypadDivide" },
	{ SDLK_KP_MULTIPLY,					"KeypadMultiply" },
	{ SDLK_KP_MINUS,					"KeypadMinus" },
	{ SDLK_KP_PLUS,						"KeypadPlus" },
	{ SDLK_KP_ENTER,					"KeypadEnter" },
	{ SDLK_KP_EQUALS,					"KeypadEquals" },
	// Arrows + Home/End pad
	{ SDLK_UP,							"Up" },
	{ SDLK_DOWN,						"Down" },
	{ SDLK_RIGHT,						"Right" },
	{ SDLK_LEFT,						"Left" },
	{ SDLK_INSERT,						"Insert" },
	{ SDLK_HOME,						"Home" },
	{ SDLK_END,							"End" },
	{ SDLK_PAGEUP,						"Pgup" },
	{ SDLK_PAGEDOWN,					"Pgdn" },
	// F keys
	{ SDLK_F1,							"F1" },
	{ SDLK_F2,							"F2" },
	{ SDLK_F3,							"F3" },
	{ SDLK_F4,							"F4" },
	{ SDLK_F5,							"F5" },
	{ SDLK_F6,							"F6" },
	{ SDLK_F7,							"F7" },
	{ SDLK_F8,							"F8" },
	{ SDLK_F9,							"F9" },
	{ SDLK_F10,							"F10" },
	{ SDLK_F11,							"F11" },
	{ SDLK_F12,							"F12" },
	{ SDLK_F13,							"F13" },
	{ SDLK_F14,							"F14" },
	{ SDLK_F15,							"F15" },
    // Modifier keys  
    { SDLK_NUMLOCK,						"NumLock" },
	{ SDLK_CAPSLOCK,					"CapsLock" },
	{ SDLK_SCROLLOCK,					"ScrollLock" },
	{ SDLK_RSHIFT,						"RightShift" },
	{ SDLK_LSHIFT,						"LeftShift" },
	{ SDLK_RCTRL,						"RightCtrl" },
	{ SDLK_LCTRL,						"LeftCtrl" },
	{ SDLK_RALT,						"RightAlt" },
	{ SDLK_LALT,						"LeftAlt" },
	{ SDLK_RMETA,						"RightMeta" },
	{ SDLK_LMETA,						"LeftMeta" },
	{ SDLK_LSUPER,						"LeftWindows" },
	{ SDLK_RSUPER,						"RightWindows" },
	{ SDLK_MODE,						"AltGr" },
	{ SDLK_COMPOSE,						"Compose" },
    // Other
    { SDLK_HELP,						"Help" },
	{ SDLK_PRINT,						"Print" },
	{ SDLK_SYSREQ,						"SysReq" },
	{ SDLK_BREAK,						"Break" },
	{ SDLK_MENU,						"Menu" },
	{ SDLK_POWER,						"Power" },
	{ SDLK_EURO,						"Euro" },
	{ SDLK_UNDO,						"Undo" },
	{ -1, NULL }
};

// Map Model 3 inputs to keys/buttons in InputMap[]; unmapped/unknown should be set to 0
static int	keyStart1 = 0;
static int	keyStart2 = 0;
static int	keyCoin1 = 0;
static int	keyCoin2 = 0;
static int	keyServiceA = 0;
static int	keyServiceB = 0;
static int	keyTestA = 0;
static int	keyTestB = 0;
static int	keyJoyUp = 0;
static int	keyJoyDown = 0;
static int	keyJoyLeft = 0;
static int	keyJoyRight = 0;
static int	keyPunch = 0;
static int	keyKick = 0;
static int	keyGuard = 0;
static int	keyEscape = 0;
static int	keySteeringLeft = 0;
static int	keySteeringRight = 0;
static int	keyAccelerator = 0;
static int	keyBrake = 0;
static int	keyVR1 = 0;
static int	keyVR2 = 0;
static int	keyVR3 = 0;
static int	keyVR4 = 0;
static int	keyGearShift1 = 0;
static int	keyGearShift2 = 0;
static int	keyGearShift3 = 0;
static int	keyGearShift4 = 0;
static int	keyViewChange = 0;
static int	keyHandBrake = 0;
static int	keyShortPass = 0;
static int	keyLongPass = 0;
static int	keyShoot = 0;
static int	keyTwinJoyTurnLeft = 0;
static int	keyTwinJoyTurnRight = 0;
static int	keyTwinJoyForward = 0;
static int	keyTwinJoyReverse = 0;
static int	keyTwinJoyStrafeLeft = 0;
static int	keyTwinJoyStrafeRight = 0;
static int	keyTwinJoyJump = 0;
static int	keyTwinJoyCrouch = 0;
static int	keyTwinJoyLeftShot = 0;
static int	keyTwinJoyRightShot = 0;
static int	keyTwinJoyLeftTurbo = 0;
static int	keyTwinJoyRightTurbo = 0;
static int	keyAnalogJoyTrigger = 0;
static int	keyAnalogJoyEvent = 0;
static int	keyTrigger = 0;
static int	keyOffscreen = 0;

// Returns index into symbol table or -1 if an error occurs
static int LookUpInputByName(const char *keyName)
{
	for (int i = 0; InputMap[i].keyName != NULL; i++)
	{
		if (stricmp(keyName, InputMap[i].keyName) == 0)
			return i;
	}
	
	// Unable to map, index 0 is reserved for this case
	return 0;
}

// Use the negative of SDL_BUTTON_* for mouse buttons.
static int LookUpInputByKeyID(int sdlKeyID)
{
	for (int i = 0; InputMap[i].keyName != NULL; i++)
	{
		if (sdlKeyID == InputMap[i].sdlKeyID)
			return i;
	}
	
	// Unable to map, index 0 is reserved for this case
	return 0;
}

// Interactively obtains and returns the new mapping for a key
static int RemapKey(const char *inputName, int inputKeyIdx)
{
	SDL_Event	Event;	
	char		name[64];
	int			idx;
	BOOL		done = FALSE;
	
	idx = inputKeyIdx;
	strcpy(name, InputMap[idx].keyName);
	printf("\t%s (%s): ", inputName, name);
	fflush(stdout);	// required on terminals that use buffering
	
	while (!done)
	{
		SDL_WaitEvent(&Event);
		if (Event.type == SDL_KEYDOWN)
 		{
	 		switch (Event.key.keysym.sym)
 			{
 			case SDLK_ESCAPE:	// do not change anything
	 			done = TRUE;
 				break;
 			default:
	 			idx = LookUpInputByKeyID(Event.key.keysym.sym);
	 			if ((InputMap[idx].sdlKeyID < SDLK_F1) || (InputMap[idx].sdlKeyID > SDLK_F12))	// do not allow F-keys
 					done = TRUE;
 				break;
 			}
 		}
 		else if (Event.type == SDL_MOUSEBUTTONDOWN)
 		{
 			idx = LookUpInputByKeyID(-SDL_BUTTON(Event.button.button));
 			done = TRUE;
 		}
 		
 		glClear(GL_COLOR_BUFFER_BIT);
 		SDL_GL_SwapBuffers();
 	}
 	
 	// Update the remapped key and print it
 	inputKeyIdx = idx;
	strcpy(name, InputMap[idx].keyName);
	printf("%s\n", name);
	
	// Return the new key
	return inputKeyIdx;
}

// Configure inputs (this will eventually become a general .ini file processor)
static void ConfigureInputs(BOOL remapKeys)
{	
	CINIFile	INI;
	string		key;
		
	// Open and parse configuration file
	INI.Open(CONFIG_FILE_PATH);	// doesn't matter if it exists or not, will get overwritten
	INI.Parse();
	
	// Assign program defaults for inputs in case they are not specified in config. file
	keyStart1 = LookUpInputByName("1");
	keyStart2 = LookUpInputByName("2");
	keyCoin1 = LookUpInputByName("3");
	keyCoin2 = LookUpInputByName("4");
	keyServiceA = LookUpInputByName("5");
	keyServiceB = LookUpInputByName("7");
	keyTestA = LookUpInputByName("6");
	keyTestB = LookUpInputByName("8");
	keyJoyUp = LookUpInputByName("Up");
	keyJoyDown = LookUpInputByName("Down");
	keyJoyLeft = LookUpInputByName("Left");
	keyJoyRight = LookUpInputByName("Right");
	keyPunch = LookUpInputByName("A");
	keyKick = LookUpInputByName("S");
	keyGuard = LookUpInputByName("D");
	keyEscape = LookUpInputByName("F");
	keySteeringLeft = LookUpInputByName("Left");
	keySteeringRight = LookUpInputByName("Right");
	keyAccelerator = LookUpInputByName("Up");
	keyBrake = LookUpInputByName("Down");
	keyGearShift1 = LookUpInputByName("Q");
	keyGearShift2 = LookUpInputByName("W");
	keyGearShift3 = LookUpInputByName("E");
	keyGearShift4 = LookUpInputByName("R");
	keyVR1 = LookUpInputByName("A");
	keyVR2 = LookUpInputByName("S");
	keyVR3 = LookUpInputByName("D");
	keyVR4 = LookUpInputByName("F");
	keyViewChange = LookUpInputByName("A");
	keyHandBrake = LookUpInputByName("S");
	keyShortPass = LookUpInputByName("A");
	keyLongPass = LookUpInputByName("S");
	keyShoot = LookUpInputByName("D");
	keyTwinJoyTurnLeft = LookUpInputByName("Q");
	keyTwinJoyTurnRight = LookUpInputByName("W");
	keyTwinJoyForward = LookUpInputByName("Up");
	keyTwinJoyReverse = LookUpInputByName("Down");
	keyTwinJoyStrafeLeft = LookUpInputByName("Left");
	keyTwinJoyStrafeRight = LookUpInputByName("Right");	
	keyTwinJoyJump = LookUpInputByName("E");
	keyTwinJoyCrouch = LookUpInputByName("R");
	keyTwinJoyLeftShot = LookUpInputByName("A");
	keyTwinJoyRightShot = LookUpInputByName("S");
	keyTwinJoyLeftTurbo = LookUpInputByName("Z");
	keyTwinJoyRightTurbo = LookUpInputByName("X");
	keyAnalogJoyTrigger = LookUpInputByName("A");
	keyAnalogJoyEvent = LookUpInputByName("S");
	keyTrigger = LookUpInputByName("MouseLeft");
	keyOffscreen = LookUpInputByName("MouseRight");
	
	// Load whatever inputs are specified from file
	if (OKAY == INI.Get("Global","InputStart1",key))
		keyStart1 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputStart2",key))
		keyStart2 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputCoin1",key))
		keyCoin1 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputCoin2",key))
		keyCoin2 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputServiceA",key))
		keyServiceA = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputServiceB",key))
		keyServiceB = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTestA",key))
		keyTestA = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTestB",key))
		keyTestB = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputJoyUp",key))
		keyJoyUp = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputJoyDown",key))
		keyJoyDown = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputJoyLeft",key))
		keyJoyLeft = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputJoyRight",key))
		keyJoyRight = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputPunch",key))
		keyPunch = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputKick",key))
		keyKick = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputGuard",key))
		keyGuard = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputEscape",key))
		keyEscape = LookUpInputByName(key.c_str()) ;
	if (OKAY == INI.Get("Global","InputSteeringLeft",key))
		keySteeringLeft = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputSteeringRight",key))
		keySteeringRight = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputAccelerator",key))
		keyAccelerator = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputBrake",key))
		keyBrake = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputVR1",key))
		keyVR1 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputVR2",key))
		keyVR2 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputVR3",key))
		keyVR3 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputVR4",key))
		keyVR4 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputGearShift1",key))
		keyGearShift1 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputGearShift2",key))
		keyGearShift2 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputGearShift3",key))
		keyGearShift3 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputGearShift4",key))
		keyGearShift4 = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputViewChange",key))
		keyViewChange = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputHandBrake",key))
		keyHandBrake = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputShortPass",key))
		keyShortPass = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputLongPass",key))
		keyLongPass = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputShoot",key))
		keyShoot = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyTurnLeft",key))
		keyTwinJoyTurnLeft = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyTurnRight",key))
		keyTwinJoyTurnRight = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyForward",key))
		keyTwinJoyForward = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyReverse",key))
		keyTwinJoyReverse = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyStrafeLeft",key))
		keyTwinJoyStrafeLeft = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyStrafeRight",key))
		keyTwinJoyStrafeRight = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyJump",key))
		keyTwinJoyJump = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyCrouch",key))
		keyTwinJoyCrouch = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyLeftShot",key))
		keyTwinJoyLeftShot = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyRightShot",key))
		keyTwinJoyRightShot = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyLeftTurbo",key))
		keyTwinJoyLeftTurbo = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTwinJoyRightTurbo",key))
		keyTwinJoyRightTurbo = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputAnalogJoyTrigger",key))
		keyAnalogJoyTrigger = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputAnalogJoyEvent",key))
		keyAnalogJoyEvent = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputTrigger",key))
		keyTrigger = LookUpInputByName(key.c_str());
	if (OKAY == INI.Get("Global","InputOffscreen",key))
		keyOffscreen = LookUpInputByName(key.c_str());
		
	// If the user wants to remap the keys, do that now
	if (remapKeys)
	{
		// Open an SDL window 
		unsigned	xOffset, yOffset, xRes=496, yRes=384;
		if (OKAY != CreateGLScreen("Supermodel - Configuring Inputs...",&xOffset,&yOffset,&xRes,&yRes,FALSE))
		{
			ErrorLog("Unable to start SDL to configure inputs.\n");
			goto QuitConfig;
		}
		
		// Remap the inputs
		puts("Configure Inputs");
		puts("----------------");
		puts("");
		puts("Press Escape if you wish to keep the current setting. Function keys may not be");
		puts("used. Please make sure the configuration window is selected. To assign mouse");
		puts("buttons, the window area must be clicked on.");
		puts("");

		puts("Common Inputs:");
		keyStart1 = RemapKey("Start 1", keyStart1);
		keyStart2 = RemapKey("Start 2", keyStart2);
		keyCoin1 = RemapKey("Coin 1", keyCoin1);
		keyCoin2 = RemapKey("Coin 2", keyCoin2);
		keyServiceA = RemapKey("Service A", keyServiceA);
		keyServiceB = RemapKey("Service B", keyServiceB);
		keyTestA = RemapKey("Test A", keyTestA);
		keyTestB = RemapKey("Test B", keyTestB);
		INI.Set("Global","InputStart1",(string) InputMap[keyStart1].keyName);
		INI.Set("Global","InputStart2",(string) InputMap[keyStart2].keyName);
		INI.Set("Global","InputCoin1",(string) InputMap[keyCoin1].keyName);
		INI.Set("Global","InputCoin2",(string) InputMap[keyCoin2].keyName);
		INI.Set("Global","InputServiceA",(string) InputMap[keyServiceA].keyName);
		INI.Set("Global","InputServiceB",(string) InputMap[keyServiceB].keyName);
		INI.Set("Global","InputTestA",(string) InputMap[keyTestA].keyName);
		INI.Set("Global","InputTestB",(string) InputMap[keyTestB].keyName);
		
		puts("Digital Joystick:");
		keyJoyUp = RemapKey("Joystick Up", keyJoyUp);
		keyJoyDown = RemapKey("Joystick Down", keyJoyDown);
		keyJoyLeft = RemapKey("Joystick Left", keyJoyLeft);
		keyJoyRight = RemapKey("Joystick Right", keyJoyRight);
		INI.Set("Global","InputJoyUp",(string) InputMap[keyJoyUp].keyName);
		INI.Set("Global","InputJoyDown",(string) InputMap[keyJoyDown].keyName);
		INI.Set("Global","InputJoyLeft",(string) InputMap[keyJoyLeft].keyName);
		INI.Set("Global","InputJoyRight",(string) InputMap[keyJoyRight].keyName);
		
		puts("Fighting Games:");	
		keyPunch = RemapKey("Punch", keyPunch);
		keyKick = RemapKey("Kick", keyKick);
		keyGuard = RemapKey("Guard", keyGuard);
		keyEscape = RemapKey("Escape", keyEscape);
		INI.Set("Global","InputPunch",(string) InputMap[keyPunch].keyName);
		INI.Set("Global","InputKick",(string) InputMap[keyKick].keyName);
		INI.Set("Global","InputGuard",(string) InputMap[keyGuard].keyName);
		INI.Set("Global","InputEscape",(string) InputMap[keyEscape].keyName);
	
		puts("Racing Game Steering:");
		keySteeringLeft = RemapKey("Steer Left", keySteeringLeft);
		keySteeringRight = RemapKey("Steer Right", keySteeringRight);
		keyAccelerator = RemapKey("Accelerator Pedal", keyAccelerator);
		keyBrake = RemapKey("Brake Pedal", keyBrake);
		INI.Set("Global","InputSteeringLeft",(string) InputMap[keySteeringLeft].keyName);
		INI.Set("Global","InputSteeringRight",(string) InputMap[keySteeringRight].keyName);
		INI.Set("Global","InputAccelerator",(string) InputMap[keyAccelerator].keyName);
		INI.Set("Global","InputBrake",(string) InputMap[keyBrake].keyName);
	
		puts("Racing Game Gear Shift:");
		keyGearShift1 = RemapKey("Shift 1/Up", keyGearShift1);
		keyGearShift2 = RemapKey("Shift 2/Down", keyGearShift2);
		keyGearShift3 = RemapKey("Shift 3", keyGearShift3);
		keyGearShift4 = RemapKey("Shift 4", keyGearShift4);
		INI.Set("Global","InputGearShift1",(string) InputMap[keyGearShift1].keyName);
		INI.Set("Global","InputGearShift2",(string) InputMap[keyGearShift2].keyName);
		INI.Set("Global","InputGearShift3",(string) InputMap[keyGearShift3].keyName);
		INI.Set("Global","InputGearShift4",(string) InputMap[keyGearShift4].keyName);
	
		puts("Racing Game VR View Buttons:");
		keyVR1 = RemapKey("VR1", keyVR1);
		keyVR2 = RemapKey("VR2", keyVR2);
		keyVR3 = RemapKey("VR3", keyVR3);
		keyVR4 = RemapKey("VR4", keyVR4);
		INI.Set("Global","InputVR1",(string) InputMap[keyVR1].keyName);
		INI.Set("Global","InputVR2",(string) InputMap[keyVR2].keyName);
		INI.Set("Global","InputVR3",(string) InputMap[keyVR3].keyName);
		INI.Set("Global","InputVR4",(string) InputMap[keyVR4].keyName);
	
		puts("Sega Rally Buttons:");
		keyViewChange = RemapKey("View Change", keyViewChange);
		keyHandBrake = RemapKey("Hand Brake", keyHandBrake);
		INI.Set("Global","InputViewChange",(string) InputMap[keyViewChange].keyName);
		INI.Set("Global","InputHandBrake",(string) InputMap[keyHandBrake].keyName);
		
		puts("Virtua Striker Buttons:");
		keyShortPass = RemapKey("Short Pass", keyShortPass);
		keyLongPass = RemapKey("Long Pass", keyLongPass);
		keyShoot = RemapKey("Shoot", keyShoot);
		INI.Set("Global","InputShortPass",(string) InputMap[keyShortPass].keyName);
		INI.Set("Global","InputLongPass",(string) InputMap[keyLongPass].keyName);
		INI.Set("Global","InputShoot",(string) InputMap[keyShoot].keyName);
		
		puts("Virtual On Controls:");
		keyTwinJoyTurnLeft = RemapKey("Turn Left", keyTwinJoyTurnLeft);
		keyTwinJoyTurnRight = RemapKey("Turn Right", keyTwinJoyTurnRight);
		keyTwinJoyForward = RemapKey("Forward", keyTwinJoyForward);
		keyTwinJoyReverse = RemapKey("Reverse", keyTwinJoyReverse);
		keyTwinJoyStrafeLeft = RemapKey("Strafe Left", keyTwinJoyStrafeLeft);
		keyTwinJoyStrafeRight = RemapKey("Strafe Right", keyTwinJoyStrafeRight);
		keyTwinJoyJump = RemapKey("Jump", keyTwinJoyJump);
		keyTwinJoyCrouch = RemapKey("Crouch", keyTwinJoyCrouch);
		keyTwinJoyLeftShot = RemapKey("Left Shot Trigger", keyTwinJoyLeftShot);
		keyTwinJoyRightShot = RemapKey("Right Shot Trigger", keyTwinJoyRightShot);
		keyTwinJoyLeftTurbo = RemapKey("Left Turbo", keyTwinJoyLeftTurbo);
		keyTwinJoyRightTurbo = RemapKey("Right Turbo", keyTwinJoyRightTurbo);
		INI.Set("Global","InputTwinJoyTurnLeft",(string) InputMap[keyTwinJoyTurnLeft].keyName);
		INI.Set("Global","InputTwinJoyTurnRight",(string) InputMap[keyTwinJoyTurnRight].keyName);
		INI.Set("Global","InputTwinJoyForward",(string) InputMap[keyTwinJoyForward].keyName);
		INI.Set("Global","InputTwinJoyReverse",(string) InputMap[keyTwinJoyReverse].keyName);
		INI.Set("Global","InputTwinJoyStrafeLeft",(string) InputMap[keyTwinJoyStrafeLeft].keyName);
		INI.Set("Global","InputTwinJoyStrafeRight",(string) InputMap[keyTwinJoyStrafeRight].keyName);
		INI.Set("Global","InputTwinJoyJump",(string) InputMap[keyTwinJoyJump].keyName);
		INI.Set("Global","InputTwinJoyCrouch",(string) InputMap[keyTwinJoyCrouch].keyName);
		INI.Set("Global","InputTwinJoyLeftShot",(string) InputMap[keyTwinJoyLeftShot].keyName);
		INI.Set("Global","InputTwinJoyRightShot",(string) InputMap[keyTwinJoyRightShot].keyName);
		INI.Set("Global","InputTwinJoyLeftTurbo",(string) InputMap[keyTwinJoyLeftTurbo].keyName);
		INI.Set("Global","InputTwinJoyRightTurbo",(string) InputMap[keyTwinJoyRightTurbo].keyName);
		
		puts("Analog Joystick:");
		puts("Analog joystick motion is controlled by the mouse.");
		keyAnalogJoyTrigger = RemapKey("Trigger Button", keyAnalogJoyTrigger);
		keyAnalogJoyEvent = RemapKey("Event Button", keyAnalogJoyEvent);
		INI.Set("Global","InputAnalogJoyTrigger",(string) InputMap[keyAnalogJoyTrigger].keyName);
		INI.Set("Global","InputAnalogJoyEvent",(string) InputMap[keyAnalogJoyEvent].keyName);
		
		puts("Lightgun:");
		puts("Lightgun aim is controlled by the mouse.");
		keyTrigger = RemapKey("Trigger", keyTrigger);
		keyOffscreen = RemapKey("Point Off-screen", keyOffscreen);
		INI.Set("Global","InputTrigger",(string) InputMap[keyTrigger].keyName);
		INI.Set("Global","InputOffscreen",(string) InputMap[keyOffscreen].keyName);
		
		// Write config file
		printf("\n");
		if (OKAY != INI.Write(CONFIG_FILE_COMMENT))
			ErrorLog("Unable to save configuration to %s.", CONFIG_FILE_PATH);
		else
			printf("Configuration successfully saved to %s.\n", CONFIG_FILE_PATH);
		printf("\n");
	}
		
	// Close and quit
QuitConfig:
	INI.Close();
}


/******************************************************************************
 Input Handling
******************************************************************************/

// "Sticky" gear shifters
static int gearButtons[4];	// indicates which gear shift buttons are pressed

// Analog controls require initialization
static void InitInputs(CModel3Inputs *Inputs)
{
	// Vehicle inputs
	Inputs->steering = 0x80;	// steering wheel center position
	Inputs->accelerator = 0;
	Inputs->brake = 0;
	
	// Analog joystick
	Inputs->analogJoyX = 0x80;		// center
	Inputs->analogJoyY = 0x80;

	// Neutral gear
	memset(gearButtons, 0, sizeof(gearButtons));

	// All other inputs are sampled each frame
}

static int Clamp(int val, int min, int max)
{
	if (val > max)
		val = max;
	else if (val < min)
		val = min;
	return val;
}

// Determines whether a key/button is pressed. Returns 0 (released) or 1 (pressed).
static UINT8 GetKeyState(int keySymIdx, const unsigned char *keyState)
{
	int	x, y, mouseButtons;
	
	if (InputMap[keySymIdx].sdlKeyID < 0)	// mouse button
	{
		mouseButtons = SDL_GetMouseState(&x, &y);
		return !!(mouseButtons&(-InputMap[keySymIdx].sdlKeyID));
	}
	else										// keyboard key
		return !!keyState[InputMap[keySymIdx].sdlKeyID];
}

static void UpdateInputs(CModel3Inputs *Inputs, const unsigned char *keyState)
{
	int	shift4[4], analog, x, y, mouseButtons;
	
	// Common inputs
	Inputs->coin[0] = GetKeyState(keyCoin1,keyState);		// Coin 1
	Inputs->coin[1] = GetKeyState(keyCoin2,keyState);		// Coin 2
	Inputs->start[0] = GetKeyState(keyStart1,keyState);		// Start 1
	Inputs->start[1] = GetKeyState(keyStart2,keyState);		// Start 2
	Inputs->service[0] = GetKeyState(keyServiceA,keyState);	// Service A
	Inputs->service[1] = GetKeyState(keyServiceB,keyState);	// Service B
	Inputs->test[0] = GetKeyState(keyTestA,keyState);		// Test A
	Inputs->test[1] = GetKeyState(keyTestB,keyState);		// Test B
	
	// VR view buttons
	Inputs->vr[0] = GetKeyState(keyVR1,keyState);	// VR1
	Inputs->vr[1] = GetKeyState(keyVR2,keyState);	// VR2
	Inputs->vr[2] = GetKeyState(keyVR3,keyState);	// VR3
	Inputs->vr[3] = GetKeyState(keyVR4,keyState);	// VR4
	
	// Rally car controls
	Inputs->viewChange = GetKeyState(keyViewChange,keyState);	// View change
	Inputs->handBrake = GetKeyState(keyHandBrake,keyState);;		// Hand brake
	
	// Soccer controls
	Inputs->shortPass = GetKeyState(keyShortPass,keyState);	// Short Pass
	Inputs->longPass = GetKeyState(keyLongPass,keyState);	// Long Pass
	Inputs->shoot = GetKeyState(keyShoot,keyState);			// Shoot

	// Joystick 1
	Inputs->up[0] = GetKeyState(keyJoyUp,keyState);
	Inputs->down[0] = GetKeyState(keyJoyDown,keyState);
	Inputs->left[0] = GetKeyState(keyJoyLeft,keyState);
	Inputs->right[0] = GetKeyState(keyJoyRight,keyState);
	
	// Fighting game buttons
	Inputs->punch[0] = GetKeyState(keyPunch,keyState);	// Punch
	Inputs->kick[0] = GetKeyState(keyKick,keyState);	// Kick
	Inputs->guard[0] = GetKeyState(keyGuard,keyState);	// Guard
	Inputs->escape[0] = GetKeyState(keyEscape,keyState);// Escape
	
	// Twin joysticks
	Inputs->twinJoyTurnLeft = GetKeyState(keyTwinJoyTurnLeft,keyState);
	Inputs->twinJoyTurnRight = GetKeyState(keyTwinJoyTurnRight,keyState);
	Inputs->twinJoyForward = GetKeyState(keyTwinJoyForward,keyState);
	Inputs->twinJoyReverse = GetKeyState(keyTwinJoyReverse,keyState);
	Inputs->twinJoyStrafeLeft = GetKeyState(keyTwinJoyStrafeLeft,keyState);
	Inputs->twinJoyStrafeRight = GetKeyState(keyTwinJoyStrafeRight,keyState);
	Inputs->twinJoyJump = GetKeyState(keyTwinJoyJump,keyState);
	Inputs->twinJoyCrouch = GetKeyState(keyTwinJoyCrouch,keyState);
	Inputs->twinJoyLeftShot = GetKeyState(keyTwinJoyLeftShot,keyState);
	Inputs->twinJoyRightShot = GetKeyState(keyTwinJoyRightShot,keyState);
	Inputs->twinJoyLeftTurbo = GetKeyState(keyTwinJoyLeftTurbo,keyState);
	Inputs->twinJoyRightTurbo = GetKeyState(keyTwinJoyRightTurbo,keyState);

	/*
	 * 4-speed gear shift.
 	 *
	 * Neutral is when all gear buttons are released. Here, shifting is 
 	 * implemented as follows: gears are set by pressing a key and "stick"
	 * until a shift to another gear or until the button is pressed again, at
	 * which point we return to the neutral position.
	 */

	shift4[0] = GetKeyState(keyGearShift1,keyState);	// Shift 1
	shift4[1] = GetKeyState(keyGearShift2,keyState);	// Shift 2
	shift4[2] = GetKeyState(keyGearShift3,keyState);	// Shift 3
	shift4[3] = GetKeyState(keyGearShift4,keyState);	// Shift 4
	
	// Change gear status, clear all others (lower gears have higher priority here)
	if (!gearButtons[0] && shift4[0])
	{
		Inputs->gearShift4[0] ^= 1;
		Inputs->gearShift4[1] = 0;
		Inputs->gearShift4[2] = 0;
		Inputs->gearShift4[3] = 0;
	}
	else if (!gearButtons[1] && shift4[1])
	{
		Inputs->gearShift4[0] = 0;
		Inputs->gearShift4[1] ^= 1;
		Inputs->gearShift4[2] = 0;
		Inputs->gearShift4[3] = 0;
	}
	else if (!gearButtons[2] && shift4[2])
	{
		Inputs->gearShift4[0] = 0;
		Inputs->gearShift4[1] = 0;
		Inputs->gearShift4[2] ^= 1;
		Inputs->gearShift4[3] = 0;
	}
	else if (!gearButtons[3] && shift4[3])
	{
		Inputs->gearShift4[0] = 0;
		Inputs->gearShift4[1] = 0;
		Inputs->gearShift4[2] = 0;
		Inputs->gearShift4[3] ^= 1;
	}

	// Save current gear buttons
	memcpy(gearButtons, shift4, sizeof(gearButtons));

	/*
	 * Vehicle controls: steering wheel, pedals
	 */
	 
	// If steering key pressed, change wheel position
	analog = Inputs->steering;
	if (GetKeyState(keySteeringLeft,keyState))
		analog -= 32;
	if (GetKeyState(keySteeringRight,keyState))
		analog += 32;
		
	// Let it drift back to center
	if (analog > 0x80)
	{
		analog -= 16;
		if (analog < 0x80)
			analog = 0x80;
	}
	else
	{
		analog += 16;
		if (analog > 0x80)
			analog = 0x80;
	}
	
	// Clamp and store
	Inputs->steering = Clamp(analog,0x00,0xFF);
	
	// Accelerator
	analog = Inputs->accelerator;
	if (GetKeyState(keyAccelerator,keyState))
		analog += 32;
	else
		analog -= 16;
	Inputs->accelerator = Clamp(analog,0x00,0xFF);
	
	// Brake
	analog = Inputs->brake;
	if (GetKeyState(keyBrake,keyState))
		analog += 32;
	else
		analog -= 16;
	Inputs->brake = Clamp(analog,0x00,0xFF);

	/*
	 * Analog joystick
	 */
	
	// Axes
	mouseButtons = SDL_GetMouseState(&x, &y);
	x = Clamp(x,xOffset,xOffset+xRes-1)-xOffset;	// clamp to within rendering field
	y = Clamp(y,yOffset,yOffset+yRes-1)-yOffset;
	Inputs->analogJoyX = (UINT8) ((float)x*(255.0f/(float)xRes));
	Inputs->analogJoyY = (UINT8) ((float)y*(255.0f/(float)yRes));
	
	// Buttons
	Inputs->analogJoyTrigger = GetKeyState(keyAnalogJoyTrigger,keyState);
	Inputs->analogJoyEvent = GetKeyState(keyAnalogJoyEvent,keyState);

	/*
	 * Gun
	 */
	mouseButtons = SDL_GetMouseState(&x, &y);
	x = Clamp(x,xOffset,xOffset+xRes-1)-xOffset;	// clamp to within rendering field
	y = Clamp(y,yOffset,yOffset+yRes-1)-yOffset;
	Inputs->gunX = (int) ((float)(651-150)*(float)x/(float)xRes);	// normalize to [150,651]
	Inputs->gunY = (int) ((float)(465-80)*(float)y/(float)yRes);	// normalize to [80,465]
	Inputs->gunX += 150;
	Inputs->gunY += 80;
	Inputs->trigger = GetKeyState(keyTrigger,keyState);
	Inputs->offscreen = GetKeyState(keyOffscreen,keyState);
}
	

/******************************************************************************
 Save States and NVRAM
 
 Save states and NVRAM use the same basic format.
 
 Header block name: "Supermodel Save State" or "Supermodel NVRAM State"
 Data: Save state file version (4-byte integer), ROM set ID (up to 9 bytes, 
 including terminating \0).
 
 Different subsystems output their own blocks.
******************************************************************************/

#define STATE_FILE_VERSION	0	// save state file version

static unsigned	saveSlot = 0;	// save state slot #

static void SaveState(CModel3 *Model3)
{
	CBlockFile	SaveState;
	char		filePath[24];
	int			fileVersion = STATE_FILE_VERSION;
	
	sprintf(filePath, "Saves/%s.st%d", Model3->GetGameInfo()->id, saveSlot);
	if (OKAY != SaveState.Create(filePath, "Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION))
	{
		ErrorLog("Unable to save state to %s.", filePath);
		return;
	}
	
	// Write file format version and ROM set ID to header block 
	SaveState.Write(&fileVersion, sizeof(fileVersion));
	SaveState.Write(Model3->GetGameInfo()->id, strlen(Model3->GetGameInfo()->id)+1);
	
	// Save state
	Model3->SaveState(&SaveState);
	SaveState.Close();
	printf("Saved state to %s.\n", filePath);
	DebugLog("Saved state to %s.\n", filePath);
}

static void LoadState(CModel3 *Model3)
{
	CBlockFile	SaveState;
	char		filePath[24];
	int			fileVersion;
	
	// Generate file path
	sprintf(filePath, "Saves/%s.st%d", Model3->GetGameInfo()->id, saveSlot);
	
	// Open and check to make sure format is correct
	if (OKAY != SaveState.Load(filePath))
	{
		ErrorLog("Unable to load state from %s.", filePath);
		return;
	}
	
	if (OKAY != SaveState.FindBlock("Supermodel Save State"))
	{
		ErrorLog("%s does not appear to be a valid save state file.", filePath);
		return;
	}
	
	SaveState.Read(&fileVersion, sizeof(fileVersion));
	if (fileVersion != STATE_FILE_VERSION)
	{
		ErrorLog("Format of %s is incompatible with this version of Supermodel.", filePath);
		return;
	}
	
	// Load
	Model3->LoadState(&SaveState);
	SaveState.Close();
	printf("Loaded state from %s.\n", filePath);
	DebugLog("Loaded state from %s.\n", filePath);
}

static void SaveNVRAM(CModel3 *Model3)
{
	CBlockFile	NVRAM;
	char		filePath[24];
	int			fileVersion = STATE_FILE_VERSION;
	
	sprintf(filePath, "NVRAM/%s.nv", Model3->GetGameInfo()->id);
	if (OKAY != NVRAM.Create(filePath, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
	{
		ErrorLog("Unable to save NVRAM to %s. Make sure directory exists!", filePath);
		return;
	}
	
	// Write file format version and ROM set ID to header block 
	NVRAM.Write(&fileVersion, sizeof(fileVersion));
	NVRAM.Write(Model3->GetGameInfo()->id, strlen(Model3->GetGameInfo()->id)+1);
	
	// Save NVRAM
	Model3->SaveNVRAM(&NVRAM);
	NVRAM.Close();
	DebugLog("Saved NVRAM to %s.\n", filePath);
}

static void LoadNVRAM(CModel3 *Model3)
{
	CBlockFile	NVRAM;
	char		filePath[24];
	int			fileVersion;
	
	// Generate file path
	sprintf(filePath, "NVRAM/%s.nv", Model3->GetGameInfo()->id);
	
	// Open and check to make sure format is correct
	if (OKAY != NVRAM.Load(filePath))
	{
		//ErrorLog("Unable to restore NVRAM from %s.", filePath);
		return;
	}
	
	if (OKAY != NVRAM.FindBlock("Supermodel NVRAM State"))
	{
		ErrorLog("%s does not appear to be a valid NVRAM file.", filePath);
		return;
	}
	
	NVRAM.Read(&fileVersion, sizeof(fileVersion));
	if (fileVersion != STATE_FILE_VERSION)
	{
		ErrorLog("Format of %s is incompatible with this version of Supermodel.", filePath);
		return;
	}
	
	// Load
	Model3->LoadNVRAM(&NVRAM);
	NVRAM.Close();
	DebugLog("Loaded NVRAM from %s.\n", filePath);
}


/******************************************************************************
 Main Program Driver
 
 All configuration management is done prior to calling Supermodel().
******************************************************************************/

static int Supermodel(const char *zipFile, unsigned ppcFrequency, unsigned xResParam, unsigned yResParam, BOOL fullScreen, BOOL noThrottle, BOOL showFPS, const char *vsFile, const char *fsFile)
{
	char			titleStr[128], titleFPSStr[128];
	CModel3Inputs	Inputs;
	CModel3			*Model3 = new CModel3();
	CRender2D		*Render2D = new CRender2D();
	CRender3D		*Render3D = new CRender3D();
	SDL_Event		Event;
	unsigned char	*keyState;
	unsigned		prevFPSTicks, currentFPSTicks, currentTicks, targetTicks, startTicks;
	unsigned		fpsFramesElapsed, framesElapsed;
	BOOL			showCursor = FALSE;	// show cursor in full screen mode?
	BOOL			quit = 0;
	
	// Info log user options
	InfoLog("PowerPC frequency: %d Hz", ppcFrequency); 	
	InfoLog("Resolution: %dx%d (%s)", xResParam, yResParam, fullScreen?"full screen":"windowed");
	InfoLog("Frame rate limiting: %s", noThrottle?"Disabled":"Enabled");

	// Initialize and load ROMs
	Model3->Init(ppcFrequency);
	if (OKAY != Model3->LoadROMSet(Model3GameList, zipFile))
		return 1;
		
	// Load NVRAM
	LoadNVRAM(Model3);
		
	// Start up SDL and open a GL window
  	xRes = xResParam;
  	yRes = yResParam;
  	sprintf(titleStr, "Supermodel - %s", Model3->GetGameInfo()->title);
	if (OKAY != CreateGLScreen(titleStr,&xOffset,&yOffset,&xRes,&yRes,fullScreen))
		return 1;
		
	// Initialize inputs
	keyState = SDL_GetKeyState(NULL);
	InitInputs(&Inputs);
	Model3->AttachInputs(&Inputs);
	
	// Initialize the renderer
	if (OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes))
		goto QuitError;
	if (OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes, vsFile, fsFile))
		goto QuitError;
	Model3->AttachRenderers(Render2D,Render3D);
	
	// Emulate!
	Model3->Reset();
	fpsFramesElapsed = 0;
	framesElapsed = 0;
	prevFPSTicks = SDL_GetTicks();
	startTicks = prevFPSTicks;
	while (!quit)
	{
		// Run one frame
		Model3->RunFrame();
		
		// Swap the buffers
		SDL_GL_SwapBuffers();
		
		// SDL event pump
 		while (SDL_PollEvent(&Event))
 		{
 			if (Event.type == SDL_QUIT)
 				quit = 1; 			
 			else if (Event.type == SDL_KEYDOWN)
 			{
 				switch (Event.key.keysym.sym)
 				{
 				default:
 					break;
 				/*
 				 * Single keys
 				 */
 				case SDLK_ESCAPE:	// Quit
 					quit = 1;
 					break;
 				case SDLK_F5:		// Save state
 					SaveState(Model3);
 					break;
 				case SDLK_F6:		// Change save slot
 					++saveSlot;
 					saveSlot %= 10;	// clamp to [0,9]
 					printf("Save slot: %d\n", saveSlot);
 					break;
 				case SDLK_F7:		// Load state
 					LoadState(Model3);
 					break;
 				/*
 				 * Alt+<key>
 				 */
 				case SDLK_i:		// Toggle cursor in full screen mode
					if ((Event.key.keysym.mod&KMOD_ALT))
					{
						if (fullScreen)
						{
							showCursor = !showCursor;
							SDL_ShowCursor(showCursor?SDL_ENABLE:SDL_DISABLE);
						}
					}
 					break;
 				case SDLK_r:		// Reset
 					if ((Event.key.keysym.mod&KMOD_ALT))
 					{
 						Model3->Reset();
 						printf("Model 3 reset.\n");
 					}
 					break;
 				case SDLK_n:		// Clear NVRAM
 					if ((Event.key.keysym.mod&KMOD_ALT))
 					{
 						Model3->ClearNVRAM();
 						printf("NVRAM cleared.\n");
 					}
 					break;
 				case SDLK_t:		// Toggle frame limiting
 					if ((Event.key.keysym.mod&KMOD_ALT))
 					{
 						noThrottle = !noThrottle;
 						printf("Frame limiting: %s\n", noThrottle?"Off":"On");
 					}
 					break;
 				}
 			}	
 		}
 		
 		// Inputs
 		UpdateInputs(&Inputs, keyState);
 		
 		// FPS and frame rate
 		currentFPSTicks = SDL_GetTicks();
 		currentTicks = currentFPSTicks;
 		
 		// FPS
 		if (showFPS)
 		{
 			++fpsFramesElapsed;
			if((currentFPSTicks-prevFPSTicks) >= 1000)	// update FPS every 1 second (each tick is 1 ms)
			{
				sprintf(titleFPSStr, "%s - %1.1f FPS", titleStr, (float)fpsFramesElapsed*(float)(currentFPSTicks-prevFPSTicks)/1000.0f);
				SDL_WM_SetCaption(titleFPSStr,NULL);
				prevFPSTicks = currentFPSTicks;			// reset tick count
				fpsFramesElapsed = 0;					// reset frame count
			}
		}
		
		// Frame limiting
		if (!noThrottle)
		{
			++framesElapsed;
			targetTicks = startTicks + (unsigned) ((float)framesElapsed * 1000.0f/60.0f);
			if (currentTicks <= targetTicks)	// add a delay until we reach the next (target) frame time
				SDL_Delay(targetTicks-currentTicks);
			else								// begin a new frame
			{
				framesElapsed = 0;
				startTicks = currentTicks;
			}
		}
	}
	
	// Save NVRAM
	SaveNVRAM(Model3);
	
	// Shut down
	delete Model3;
	delete Render2D;
	delete Render3D;
	DestroyGLScreen();
	
	// Dump PowerPC registers
#ifdef DEBUG
	for (int i = 0; i < 32; i += 4)
        printf("R%d=%08X\tR%d=%08X\tR%d=%08X\tR%d=%08X\n",
        i + 0, ppc_get_gpr(i + 0),
        i + 1, ppc_get_gpr(i + 1),
        i + 2, ppc_get_gpr(i + 2),
        i + 3, ppc_get_gpr(i + 3));
	printf("PC =%08X\n", ppc_get_pc());
  	printf("LR =%08X\n", ppc_get_lr());
	printf("DBAT0U=%08X\tIBAT0U=%08X\n", ppc_read_spr(SPR603E_DBAT0U), ppc_read_spr(SPR603E_IBAT0U));
	printf("DBAT0L=%08X\tIBAT0L=%08X\n", ppc_read_spr(SPR603E_DBAT0L), ppc_read_spr(SPR603E_IBAT0L));
	printf("DBAT1U=%08X\tIBAT1U=%08X\n", ppc_read_spr(SPR603E_DBAT1U), ppc_read_spr(SPR603E_IBAT1U));
	printf("DBAT1L=%08X\tIBAT1L=%08X\n", ppc_read_spr(SPR603E_DBAT1L), ppc_read_spr(SPR603E_IBAT1L));
	printf("DBAT2U=%08X\tIBAT2U=%08X\n", ppc_read_spr(SPR603E_DBAT2U), ppc_read_spr(SPR603E_IBAT2U));
	printf("DBAT2L=%08X\tIBAT2L=%08X\n", ppc_read_spr(SPR603E_DBAT2L), ppc_read_spr(SPR603E_IBAT2L));
	printf("DBAT3U=%08X\tIBAT3U=%08X\n", ppc_read_spr(SPR603E_DBAT3U), ppc_read_spr(SPR603E_IBAT3U));
	printf("DBAT3L=%08X\tIBAT3L=%08X\n", ppc_read_spr(SPR603E_DBAT3L), ppc_read_spr(SPR603E_IBAT3L));
	for (int i = 0; i < 10; i++)
		printf("SR%d =%08X\n", i, ppc_read_sr(i));
	for (int i = 10; i < 16; i++)
		printf("SR%d=%08X\n", i, ppc_read_sr(i));
	printf("SDR1=%08X\n", ppc_read_spr(SPR603E_SDR1));
#endif
	
	return 0;

	// Quit with an error
QuitError:
	delete Model3;
	delete Render2D;
	delete Render3D;
	DestroyGLScreen();
	return 1;
}


/******************************************************************************
 Error and Debug Logging
******************************************************************************/

#define DEBUG_LOG_FILE	"debug.log"
#define ERROR_LOG_FILE	"error.log"

/*
 * DebugLog(fmt, ...):
 *
 * Prints to debug log. The file is opened and closed each time so that its 
 * contents are preserved even if the program crashes.
 *
 * Parameters:
 *		fmt		Format string (same as printf()).
 *		...		Variable number of arguments as required by format string.
 */
void DebugLog(const char *fmt, ...)
{
#ifdef DEBUG
	char	string[1024];
	va_list	vl;
	FILE 	*fp;

	fp = fopen(DEBUG_LOG_FILE, "ab");
	if (NULL != fp)
	{
		va_start(vl, fmt);
		vsprintf(string, fmt, vl);
		va_end(vl);
		fprintf(fp, string);
		fclose(fp);
	}
#endif
}

/*
 * InfoLog(fmt, ...);
 *
 * Prints information to the error log file but does not print to stderr. This
 * is useful for logging non-error information.
 *
 * Parameters:
 *		fmt		Format string (same as printf()).
 *		...		Variable number of arguments as required by format string.
 */
void InfoLog(const char *fmt, ...)
{
	char	string[4096];
	va_list	vl;
	FILE 	*fp;

	va_start(vl, fmt);
	vsprintf(string, fmt, vl);
	va_end(vl);
	
	fp = fopen(ERROR_LOG_FILE, "ab");
	if (NULL != fp)
	{
		fprintf(fp, "%s\n", string);
		fclose(fp);
	}
	
	DebugLog("Info: ");
	DebugLog(string);
	DebugLog("\n");
}

/*
 * ErrorLog(fmt, ...):
 *
 * Prints an error to stderr and the error log file.
 *
 * Parameters:
 *		fmt		Format string (same as printf()).
 *		...		Variable number of arguments as required by format string.
 *
 * Returns:
 *		Always returns FAIL.
 */
BOOL ErrorLog(const char *fmt, ...)
{
	char	string[4096];
	va_list	vl;
	FILE 	*fp;

	va_start(vl, fmt);
	vsprintf(string, fmt, vl);
	va_end(vl);
	fprintf(stderr, "Error: %s\n", string);
	
	fp = fopen(ERROR_LOG_FILE, "ab");
	if (NULL != fp)
	{
		fprintf(fp, "%s\n", string);
		fclose(fp);
	}
	
	DebugLog("Error: ");
	DebugLog(string);
	DebugLog("\n");
	
	return FAIL;
}
 
// Clear log file
static void ClearLog(const char *file, const char *title)
{
	FILE *fp = fopen(file, "w");
	if (NULL != fp)
	{
		unsigned	i;
		fprintf(fp, "%s\n", title);
		for (i = 0; i < strlen(title); i++)
			fputc('-', fp);
		fprintf(fp, "\n\n");
		fclose(fp);
	}
}


/******************************************************************************
 Diagnostic Commands
******************************************************************************/

// Disassemble instructions from CROM
static int DisassembleCROM(const char *zipFile, UINT32 addr, unsigned n)
{
	const struct GameInfo	*Game;
	UINT8					*crom;
	struct ROMMap Map[] =
	{
		{ "CROM", 	NULL },
		{ "CROMxx",	NULL },
		{ NULL, 	NULL }
	};
	char	mnem[16], oprs[48];
	UINT32	op;
	
	// Do we have a valid CROM address?
	if (addr < 0xFF800000)
		return ErrorLog("Valid CROM address range is FF800000-FFFFFFFF.");
		
	// Allocate memory and set ROM region
	crom = new(std::nothrow) UINT8[0x8800000];
	if (NULL == crom)
		return ErrorLog("Insufficient memory to load CROM (need %d MB).", (0x8800000/8));
	Map[0].ptr = crom;
	Map[1].ptr = &crom[0x800000];
	
	// Load ROM set
	Game = LoadROMSetFromZIPFile(Map, Model3GameList, zipFile, FALSE);
	if (NULL == Game)
		return ErrorLog("Failed to load ROM set.");	
		
	// Mirror CROM if necessary
	if (Game->cromSize < 0x800000)	// high part of fixed CROM region contains CROM0
		CopyRegion(crom, 0, 0x800000-0x200000, &crom[0x800000], 0x800000);
		
	// Disassemble!
	addr -= 0xFF800000;
	while ((n > 0) && ((addr+4) <= 0x800000))
	{
		op = (crom[addr+0]<<24) | (crom[addr+1]<<16) | (crom[addr+2]<<8) | crom[addr+3];
        
        printf("%08X: ", addr+0xFF800000);
        if (DisassemblePowerPC(op, addr+0xFF800000, mnem, oprs, 1))
        {
            if (mnem[0] != '\0')    // invalid form
                printf("%08X %s*\t%s\n", op, mnem, oprs);
            else
                printf("%08X ?\n", op);
        }
        else
            printf("%08X %s\t%s\n", op, mnem, oprs);
		
		addr += 4;
		--n;
	}
	
	delete [] crom;
	return OKAY;
}

/*
 * PrintGLInfo():
 *
 * Queries and prints OpenGL information. A full list of extensions can
 * optionally be printed.
 */
static void PrintGLInfo(BOOL printExtensions)
{
	const GLubyte	*str;
	char			*strLocal;
	GLint			value;
	unsigned		xOffset, yOffset, xRes=496, yRes=384;
	
	if (OKAY != CreateGLScreen("Supermodel - Querying OpenGL Information...",&xOffset,&yOffset,&xRes,&yRes,FALSE))
	{
		ErrorLog("Unable to query OpenGL.\n");
		return;
	}
	
	puts("OpenGL information:\n");

	str = glGetString(GL_VENDOR);
	printf("                   Vendor: %s\n", str);
	
	str = glGetString(GL_RENDERER);
	printf("                 Renderer: %s\n", str);
	
	str = glGetString(GL_VERSION);
	printf("                  Version: %s\n", str);
	
	str = glGetString(GL_SHADING_LANGUAGE_VERSION);
	printf(" Shading Language Version: %s\n", str);
	
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
	printf("Maximum Vertex Array Size: %d vertices\n", value);
	
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
	printf("     Maximum Texture Size: %d texels\n", value);
	
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
	printf("Maximum Vertex Attributes: %d\n", value);
	
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
	printf("  Maximum Vertex Uniforms: %d\n", value);
	
	if (printExtensions)
	{
		str = glGetString(GL_EXTENSIONS);
		strLocal = (char *) malloc((strlen((char *) str)+1)*sizeof(char));
		if (NULL == strLocal)
			printf("     Supported Extensions: %s\n", str);
		else
		{
			strcpy(strLocal, (char *) str);
			printf("     Supported Extensions: %s\n", (strLocal = strtok(strLocal, " \t\n")));
			while ((strLocal = strtok(NULL, " \t\n")) != NULL)
				printf("                           %s\n", strLocal);
		}
	}
	
	printf("\n");
	
	DestroyGLScreen();
}


/******************************************************************************
 Entry Point and Command Line Procesing
******************************************************************************/

// Print Supermodel title and copyright information
static void Title(void)
{
	puts("Supermodel: A Sega Model 3 Arcade Emulator (Version "SUPERMODEL_VERSION")");
	puts("Copyright (C) 2011 by Bart Trzynadlowski");
	puts("");
}

// Print usage information
static void Help(void)
{
	puts("Usage: Supermodel <romset> [options]");
	puts("ROM set must be a valid ZIP file containing a single game.");
	puts("");
	puts("General Options:");
	puts("    -?, -h                 Print this help text");
	puts("    -print-games           List supported games");
	puts("");
	puts("Emulation Options:");
	puts("    -ppc-frequency=<f>     Set PowerPC frequency in MHz [Default: 25]");
	puts("");
	puts("Video Options:");
	puts("    -res=<x>,<y>           Resolution");
	puts("    -fullscreen            Full screen mode");
	puts("    -no-throttle           Disable 60 Hz frame rate limit");
	puts("    -show-fps              Display frame rate in window title bar");
#ifdef DEBUG	// ordinary users do not need to know about these, but they are always available
	puts("    -vert-shader=<file>    Load 3D vertex shader from external file");
	puts("    -frag-shader=<file>    Load 3D fragment shader from external file");
#endif
	puts("");
	puts("Input Options:");
	puts("    -config-inputs         Configure keys and mouse buttons");
	puts("");
	puts("Diagnostic Options:");
#ifdef DEBUG
	puts("    -dis=<addr>[,n]        Disassemble PowerPC code from CROM");
#endif
	puts("    -print-gl-info         Print extensive OpenGL information\n");
}

// Print game list
static void PrintGameList(void)
{
	int	i, j;
	
	puts("Supported games:");
	puts("");
	puts("    ROM Set        Title");
	puts("    -------        -----");
	for (i = 0; Model3GameList[i].title != NULL; i++)
	{
		printf("    %s", Model3GameList[i].id);
		for (j = strlen(Model3GameList[i].id); j < 8; j++)	// pad for alignment (no game ID is more than 8 letters)
			printf(" ");
		printf("       %s\n", Model3GameList[i].title);
	}
}

/*
 * main(argc, argv):
 *
 * Program entry point.
 */
int main(int argc, char **argv)
{
	int			i, ret;
	int			cmd=0, fileIdx=0, cmdFullScreen=0, cmdNoThrottle=0, cmdShowFPS=0, cmdConfigInputs=0, cmdPrintGames=0, cmdDis=0, cmdPrintGLInfo=0;
	unsigned	n, xRes=496, yRes=384, ppcFrequency=25000000;
	char		*vsFile = NULL, *fsFile = NULL;
	UINT32		addr;

	Title();
	if (argc <= 1)
	{
		Help();
		return 0;
	}
	
#ifdef DEBUG
	ClearLog(DEBUG_LOG_FILE, "Supermodel v"SUPERMODEL_VERSION" Debug Log");
#endif
	ClearLog(ERROR_LOG_FILE, "Supermodel v"SUPERMODEL_VERSION" Error Log");	

	// Parse command line
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i],"-h") || !strcmp(argv[i],"-?"))
		{
			Help();
			return 0;
		}
		else if (!strcmp(argv[i],"-print-games"))
			cmd = cmdPrintGames = 1;
		else if (!strncmp(argv[i],"-ppc-frequency",14))
		{
			int	f;
			ret = sscanf(&argv[i][14],"=%d",&f);
			if (ret != 1)
				ErrorLog("-ppc-frequency requires a frequency.");
			else
			{
				if ((f<1) || (f>1000))	// limit to 1-1000MHz
					ErrorLog("PowerPC frequency must be between 1 and 1000 MHz. Ignoring.");
				else
					ppcFrequency = f*1000000;
			}
		}
		else if (!strncmp(argv[i],"-res",4))
		{
			unsigned	x, y;
			
			ret = sscanf(&argv[i][4],"=%d,%d",&x,&y);
			if (ret != 2)
				ErrorLog("-res requires both a width and a height.");
			else
			{
				xRes = x;
				yRes = y;
			}
		}
		else if (!strcmp(argv[i],"-fullscreen"))
			cmd = cmdFullScreen = 1;
		else if (!strcmp(argv[i],"-no-throttle"))
			cmd = cmdNoThrottle = 1;
		else if (!strcmp(argv[i],"-show-fps"))
			cmd = cmdShowFPS = 1;
		else if (!strncmp(argv[i],"-vert-shader=",13))
		{
			if (argv[i][13] == '\0')
				ErrorLog("-vert-shader requires a file path.");
			else
				vsFile = &argv[i][13];
		}
		else if (!strncmp(argv[i],"-frag-shader=",13))
		{
			if (argv[i][13] == '\0')
				ErrorLog("-frag-shader requires a file path.");
			else
				fsFile = &argv[i][13];
		}
		else if (!strcmp(argv[i],"-config-inputs"))
			cmd = cmdConfigInputs = 1;
		else if (!strncmp(argv[i],"-dis",4))
		{
			ret = sscanf(&argv[i][4],"=%X,%X",&addr,&n);
			if (ret == 1)
			{
				n = 16;
				cmd = cmdDis = 1;
			}
			else if (ret == 2)
				cmd = cmdDis = 1;
			else
				ErrorLog("-dis requires address and, optionally, number of instructions.");
		}
		else if (!strcmp(argv[i],"-print-gl-info"))
			cmd = cmdPrintGLInfo = 1;
		else if (argv[i][0] == '-')
			ErrorLog("Ignoring invalid option: %s.", argv[i]);
		else
		{
			if (fileIdx)		// already specified a file
 				ErrorLog("Multiple files specified. Using %s, ignoring %s.", argv[i], argv[fileIdx]);
 			else
 				fileIdx = i;
 		}
	}
	
	// Assign inputs
	ConfigureInputs(cmdConfigInputs);
	
	// Process commands that don't require ROM set
	if (cmd)
	{	
		if (cmdPrintGames)
		{
			PrintGameList();
			return 0;
		}
		
		if (cmdPrintGLInfo)
		{
			PrintGLInfo(FALSE);
			return 0;
		}
	}
	
	if (fileIdx == 0)
	{
		ErrorLog("No ROM set specified.");
		return 1;
	}
	
	// Process commands that require ROMs
	if (cmd)
	{				
		if (cmdDis)
		{
			if (OKAY != DisassembleCROM(argv[fileIdx], addr, n))
				return 1;
			return 0;
		}
	}

	return Supermodel(argv[fileIdx],ppcFrequency,xRes,yRes,cmdFullScreen,cmdNoThrottle,cmdShowFPS,vsFile,fsFile);
}
