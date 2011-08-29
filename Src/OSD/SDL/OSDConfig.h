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
 * OSDConfig.h
 * 
 * Header file defining the COSDConfig class: OSD configuration settings,
 * inherited by CConfig.
 */

#ifndef INCLUDED_OSDCONFIG_H
#define INCLUDED_OSDCONFIG_H


#include <string>
#include "Supermodel.h"
using namespace std;


/*
 * COSDConfig:
 *
 * Settings used by COSDConfig.
 */
class COSDConfig
{
public:
	unsigned	xRes, yRes;		// X and Y resolution, in pixels
	bool 		fullScreen;		// Full screen mode (if TRUE)
	bool 		throttle;		// 60 Hz frame limiting
	bool		showFPS;		// Show frame rate

#ifdef SUPERMODEL_DEBUGGER
	bool		disableDebugger;	// disables the debugger (not stored in the config. file)
#endif
	
	// Input system
	inline void SetInputSystem(const char *inpSysName)
	{
		if (inpSysName == NULL)
		{
			inputSystem = "sdl";
			return;
		}
		
		if (stricmp(inpSysName,"sdl") 
#ifdef SUPERMODEL_WIN32
			&& stricmp(inpSysName,"dinput") && stricmp(inpSysName,"xinput") && stricmp(inpSysName,"rawinput")
#endif
			)
		{
			ErrorLog("Unknown input system '%s', defaulting to SDL.", inpSysName);
			inputSystem = "sdl";
			return;
		}
		
		inputSystem = inpSysName;
	}
	
	inline const char *GetInputSystem(void)
	{
		return inputSystem.c_str();
	}
		
	// Defaults
	COSDConfig(void)
	{
		xRes = 496;
		yRes = 384;
		fullScreen = false;
		throttle = true;
		showFPS = false;
#ifdef SUPERMODEL_DEBUGGER
		disableDebugger = false;
#endif
		inputSystem = "sdl";
	}
	
private:
	string	inputSystem;
};


#endif	// INCLUDED_OSDCONFIG_H