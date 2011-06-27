#include "Supermodel.h"

#include <stdarg.h>
#include <vector>
#include <string>
#include <iostream>
using namespace std;

CInputs::CInputs(CInputSystem *system) : m_system(system)
{
	// UI Controls  
	uiExit             = AddSwitchInput("UIExit",             "Exit UI",               GAME_INPUT_UI, "KEY_ESCAPE");
	uiReset            = AddSwitchInput("UIReset",            "Reset",                 GAME_INPUT_UI, "KEY_ALT+KEY_R");
	uiPause            = AddSwitchInput("UIPause",            "Pause",                 GAME_INPUT_UI, "KEY_ALT+KEY_P");
	uiSaveState        = AddSwitchInput("UISaveState",        "Save State",            GAME_INPUT_UI, "KEY_F5");
	uiChangeSlot       = AddSwitchInput("UIChangeSlot",       "Change Save Slot",      GAME_INPUT_UI, "KEY_F6");
	uiLoadState        = AddSwitchInput("UILoadState",        "Load State",            GAME_INPUT_UI, "KEY_F7");
	uiDumpInpState     = AddSwitchInput("UIDumpInputState",   "Dump Input State",      GAME_INPUT_UI, "KEY_F8");
	uiClearNVRAM       = AddSwitchInput("UIClearNVRAM",       "Clear NVRAM",           GAME_INPUT_UI, "KEY_ALT+KEY_N");
	uiToggleCursor     = AddSwitchInput("UIToggleCursor",     "Toggle Cursor",         GAME_INPUT_UI, "KEY_ALT+KEY_I");
	uiToggleFrLimit    = AddSwitchInput("UIToggleFrameLimit", "Toggle Frame Limiting", GAME_INPUT_UI, "KEY_ALT+KEY_T");
#ifdef SUPERMODEL_DEBUGGER
	uiEnterDebugger    = AddSwitchInput("UIEnterDebugger",    "Enter Debugger",        GAME_INPUT_UI, "KEY_ALT+KEY_B");
#endif

	// Common Controls
	start[0]           = AddSwitchInput("Start1",   "P1 Start",  GAME_INPUT_COMMON, "KEY_1,JOY1_BUTTON9");
	start[1]           = AddSwitchInput("Start2",   "P2 Start",  GAME_INPUT_COMMON, "KEY_2,JOY2_BUTTON9");
	coin[0]            = AddSwitchInput("Coin1",    "P1 Coin",   GAME_INPUT_COMMON, "KEY_3,JOY1_BUTTON10");
	coin[1]            = AddSwitchInput("Coin2",    "P2 Coin",   GAME_INPUT_COMMON, "KEY_4,JOY2_BUTTON10");
	service[0]         = AddSwitchInput("ServiceA", "Service A", GAME_INPUT_COMMON, "KEY_5");
	service[1]         = AddSwitchInput("ServiceB", "Service B", GAME_INPUT_COMMON, "KEY_7");
	test[0]            = AddSwitchInput("TestA",    "Test A",    GAME_INPUT_COMMON, "KEY_6");
	test[1]            = AddSwitchInput("TestB",    "Test B",    GAME_INPUT_COMMON, "KEY_8");
	
	// 8-Way Joysticks
	up[0]              = AddSwitchInput("JoyUp",     "P1 Joystick Up",    GAME_INPUT_JOYSTICK1, "KEY_UP,JOY1_UP");
	down[0]            = AddSwitchInput("JoyDown",   "P1 Joystick Down",  GAME_INPUT_JOYSTICK1, "KEY_DOWN,JOY1_DOWN");
	left[0]            = AddSwitchInput("JoyLeft",   "P1 Joystick Left",  GAME_INPUT_JOYSTICK1, "KEY_LEFT,JOY1_LEFT");
	right[0]           = AddSwitchInput("JoyRight",  "P1 Joystick Right", GAME_INPUT_JOYSTICK1, "KEY_RIGHT,JOY1_RIGHT");
	up[1]              = AddSwitchInput("JoyUp2",    "P2 Joystick Up",    GAME_INPUT_JOYSTICK2, "JOY2_UP");
	down[1]            = AddSwitchInput("JoyDown2",  "P2 Joystick Down",  GAME_INPUT_JOYSTICK2, "JOY2_DOWN");
	left[1]            = AddSwitchInput("JoyLeft2",  "P2 Joystick Left",  GAME_INPUT_JOYSTICK2, "JOY2_LEFT");
	right[1]           = AddSwitchInput("JoyRight2", "P2 Joystick Right", GAME_INPUT_JOYSTICK2, "JOY2_RIGHT");

	// Fighting Game Buttons
	punch[0]           = AddSwitchInput("Punch",   "P1 Punch",  GAME_INPUT_FIGHTING, "KEY_A,JOY1_BUTTON1");
	kick[0]            = AddSwitchInput("Kick",    "P1 Kick",   GAME_INPUT_FIGHTING, "KEY_S,JOY1_BUTTON2");
	guard[0]           = AddSwitchInput("Guard",   "P1 Guard",  GAME_INPUT_FIGHTING, "KEY_D,JOY1_BUTTON3");
	escape[0]          = AddSwitchInput("Escape",  "P1 Escape", GAME_INPUT_FIGHTING, "KEY_F,JOY1_BUTTON4");
	punch[1]           = AddSwitchInput("Punch2",  "P2 Punch",  GAME_INPUT_FIGHTING, "JOY2_BUTTON1");
	kick[1]            = AddSwitchInput("Kick2",   "P2 Kick",   GAME_INPUT_FIGHTING, "JOY2_BUTTON2");
	guard[1]           = AddSwitchInput("Guard2",  "P2 Guard",  GAME_INPUT_FIGHTING, "JOY2_BUTTON3");
	escape[1]          = AddSwitchInput("Escape2", "P2 Escape", GAME_INPUT_FIGHTING, "JOY2_BUTTON4");

	// Virtua Striker Buttons
	shortPass[0]       = AddSwitchInput("ShortPass",  "P1 Short Pass", GAME_INPUT_SOCCER, "KEY_A,JOY1_BUTTON1");
	longPass[0]        = AddSwitchInput("LongPass",   "P1 Long Pass",  GAME_INPUT_SOCCER, "KEY_S,JOY1_BUTTON2");
	shoot[0]           = AddSwitchInput("Shoot",      "P1 Shoot",      GAME_INPUT_SOCCER, "KEY_D,JOY1_BUTTON3");
	shortPass[1]       = AddSwitchInput("ShortPass1", "P2 Short Pass", GAME_INPUT_SOCCER, "JOY2_BUTTON1");
	longPass[1]        = AddSwitchInput("LongPass1",  "P2 Long Pass",  GAME_INPUT_SOCCER, "JOY2_BUTTON2");
	shoot[1]           = AddSwitchInput("Shoot1",     "P2 Shoot",      GAME_INPUT_SOCCER, "JOY2_BUTTON3");

	// Racing Game Steering Controls
	CAnalogInput *steeringLeft  = AddAnalogInput("SteeringLeft",  "Steer Left",  GAME_INPUT_VEHICLE, "KEY_LEFT");
	CAnalogInput *steeringRight = AddAnalogInput("SteeringRight", "Steer Right", GAME_INPUT_VEHICLE, "KEY_RIGHT");
	
	steering           = AddAxisInput  ("Steering",    "Full Steering",     GAME_INPUT_VEHICLE, "JOY1_XAXIS", steeringLeft, steeringRight);
	accelerator        = AddAnalogInput("Accelerator", "Accelerator Pedal", GAME_INPUT_VEHICLE, "KEY_UP,JOY1_UP");
	brake              = AddAnalogInput("Brake",       "Brake Pedal",       GAME_INPUT_VEHICLE, "KEY_DOWN,JOY1_DOWN");
	
	// Racing Game Gear Shift
	CSwitchInput *shift1    = AddSwitchInput("GearShift1",    "Shift 1/Up",   GAME_INPUT_SHIFT4, "KEY_Q,JOY1_BUTTON5");
	CSwitchInput *shift2    = AddSwitchInput("GearShift2",    "Shift 2/Down", GAME_INPUT_SHIFT4, "KEY_W,JOY1_BUTTON6");
	CSwitchInput *shift3    = AddSwitchInput("GearShift3",    "Shift 3",      GAME_INPUT_SHIFT4, "KEY_E,JOY1_BUTTON7");
	CSwitchInput *shift4    = AddSwitchInput("GearShift4",    "Shift 4",      GAME_INPUT_SHIFT4, "KEY_R,JOY1_BUTTON8");
	CSwitchInput *shiftUp   = AddSwitchInput("GearShiftUp",   "Shift Up",     GAME_INPUT_SHIFT4, "NONE");
	CSwitchInput *shiftDown = AddSwitchInput("GearShiftDown", "Shift Down",   GAME_INPUT_SHIFT4, "NONE");

	gearShift4         = AddGearShift4Input("GearShift", "Gear Shift", GAME_INPUT_SHIFT4, shift1, shift2, shift3, shift4, shiftUp, shiftDown);

	// Racing Game VR View Buttons
	vr[0]              = AddSwitchInput("VR1", "VR1", GAME_INPUT_VR, "KEY_A,JOY1_BUTTON1");
	vr[1]              = AddSwitchInput("VR2", "VR2", GAME_INPUT_VR, "KEY_S,JOY1_BUTTON2");
	vr[2]              = AddSwitchInput("VR3", "VR3", GAME_INPUT_VR, "KEY_D,JOY1_BUTTON3");
	vr[3]              = AddSwitchInput("VR4", "VR4", GAME_INPUT_VR, "KEY_F,JOY1_BUTTON4");

	// Sega Rally Buttons
	viewChange         = AddSwitchInput("ViewChange", "View Change", GAME_INPUT_RALLY, "KEY_A,JOY1_BUTTON1");
	handBrake          = AddSwitchInput("HandBrake",  "Hand Brake",  GAME_INPUT_RALLY, "KEY_S,JOY1_BUTTON2");

	// Virtua On Controls
	twinJoyTurnLeft    = AddSwitchInput("TwinJoyTurnLeft",    "Turn Left",          GAME_INPUT_TWIN_JOYSTICKS, "KEY_Q,JOY1_RXAXIS_NEG");
	twinJoyTurnRight   = AddSwitchInput("TwinJoyTurnRight",   "Turn Right",         GAME_INPUT_TWIN_JOYSTICKS, "KEY_W,JOY1_RXAXIS_POS");
	twinJoyForward     = AddSwitchInput("TwinJoyForward",     "Forward",            GAME_INPUT_TWIN_JOYSTICKS, "KEY_UP,JOY1_YAXIS_NEG");
	twinJoyReverse     = AddSwitchInput("TwinJoyReverse",     "Reverse",            GAME_INPUT_TWIN_JOYSTICKS, "KEY_DOWN,JOY1_YAXIS_POS");
	twinJoyStrafeLeft  = AddSwitchInput("TwinJoyStrafeLeft",  "Strafe Left",        GAME_INPUT_TWIN_JOYSTICKS, "KEY_LEFT,JOY1_XAXIS_NEG");
	twinJoyStrafeRight = AddSwitchInput("TwinJoyStrafeRight", "Strafe Right",       GAME_INPUT_TWIN_JOYSTICKS, "KEY_RIGHT,JOY1_XAXIS_POS");
	twinJoyJump        = AddSwitchInput("TwinJoyJump",        "Jump",               GAME_INPUT_TWIN_JOYSTICKS, "KEY_E,JOY1_BUTTON1");
	twinJoyCrouch      = AddSwitchInput("TwinJoyCrouch",      "Crouch",             GAME_INPUT_TWIN_JOYSTICKS, "KEY_R,JOY1_BUTTON2");
	twinJoyLeftShot    = AddSwitchInput("TwinJoyLeftShot",    "Left Shot Trigger",  GAME_INPUT_TWIN_JOYSTICKS, "KEY_A,JOY1_BUTTON5");
	twinJoyRightShot   = AddSwitchInput("TwinJoyRightShot",   "Right Shot Trigger", GAME_INPUT_TWIN_JOYSTICKS, "KEY_S,JOY1_BUTTON6");
	twinJoyLeftTurbo   = AddSwitchInput("TwinJoyLeftTurbo",   "Left Turbo",         GAME_INPUT_TWIN_JOYSTICKS, "KEY_Z,JOY1_BUTTON7");
	twinJoyRightTurbo  = AddSwitchInput("TwinJoyRightTurbo",  "Right Turbo",        GAME_INPUT_TWIN_JOYSTICKS, "KEY_X,JOY1_BUTTON8");
	
	// Analog Joystick
	CAnalogInput *analogJoyLeft  = AddAnalogInput("AnalogJoyLeft",  "Analog Left",  GAME_INPUT_ANALOG_JOYSTICK, "KEY_LEFT");
	CAnalogInput *analogJoyRight = AddAnalogInput("AnalogJoyRight", "Analog Right", GAME_INPUT_ANALOG_JOYSTICK, "KEY_RIGHT");
	CAnalogInput *analogJoyUp    = AddAnalogInput("AnalogJoyUp",    "Analog Up",    GAME_INPUT_ANALOG_JOYSTICK, "KEY_UP");
	CAnalogInput *analogJoyDown  = AddAnalogInput("AnalogJoyDown",  "Analog Down",  GAME_INPUT_ANALOG_JOYSTICK, "KEY_DOWN");
	
	analogJoyX         = AddAxisInput  ("AnalogJoyX",       "Analog X-Axis",  GAME_INPUT_ANALOG_JOYSTICK, "JOY_XAXIS,MOUSE_XAXIS", analogJoyLeft, analogJoyRight);
	analogJoyY         = AddAxisInput  ("AnalogJoyY",       "Analog Y-Axis",  GAME_INPUT_ANALOG_JOYSTICK, "JOY_YAXIS,MOUSE_YAXIS", analogJoyUp,   analogJoyDown);
	analogJoyTrigger   = AddSwitchInput("AnalogJoyTrigger", "Trigger Button", GAME_INPUT_ANALOG_JOYSTICK, "KEY_A,JOY_BUTTON1,MOUSE_LEFT_BUTTON");
	analogJoyEvent     = AddSwitchInput("AnalogJoyEvent",   "Event Button",   GAME_INPUT_ANALOG_JOYSTICK, "KEY_S,JOY_BUTTON2,MOUSE_RIGHT_BUTTON");

	// Lightguns
	CAnalogInput *gun1Left  = AddAnalogInput("GunLeft",  "P1 Gun Left",  GAME_INPUT_GUN1, "KEY_LEFT");
	CAnalogInput *gun1Right = AddAnalogInput("GunRight", "P1 Gun Right", GAME_INPUT_GUN1, "KEY_RIGHT");
	CAnalogInput *gun1Up    = AddAnalogInput("GunUp",    "P1 Gun Up",    GAME_INPUT_GUN1, "KEY_UP");
	CAnalogInput *gun1Down  = AddAnalogInput("GunDown",  "P1 Gun Down",  GAME_INPUT_GUN1, "KEY_DOWN");

	gunX[0]            = AddAxisInput("GunX", "P1 Gun X-Axis", GAME_INPUT_GUN1, "MOUSE_XAXIS,JOY1_XAXIS", gun1Left, gun1Right, 150, 400, 651); // normalize to [150,651]
	gunY[0]            = AddAxisInput("GunY", "P1 Gun Y-Axis", GAME_INPUT_GUN1, "MOUSE_YAXIS,JOY1_YAXIS", gun1Up,   gun1Down,  80,  272, 465); // normalize to [80,465]
	
	CSwitchInput *gun1Trigger   = AddSwitchInput("Trigger",   "P1 Trigger",          GAME_INPUT_GUN1, "KEY_A,JOY1_BUTTON1,MOUSE_LEFT_BUTTON");
	CSwitchInput *gun1Offscreen = AddSwitchInput("Offscreen", "P1 Point Off-screen", GAME_INPUT_GUN1, "KEY_S,JOY1_BUTTON2,MOUSE_RIGHT_BUTTON");

	trigger[0]         = AddTriggerInput("AutoTrigger", "P1 Auto Trigger", GAME_INPUT_GUN1, gun1Trigger, gun1Offscreen);
	
	CAnalogInput *gun2Left  = AddAnalogInput("GunLeft2",  "P2 Gun Left",  GAME_INPUT_GUN2, "NONE");
	CAnalogInput *gun2Right = AddAnalogInput("GunRight2", "P2 Gun Right", GAME_INPUT_GUN2, "NONE");
	CAnalogInput *gun2Up    = AddAnalogInput("GunUp2",    "P2 Gun Up",    GAME_INPUT_GUN2, "NONE");
	CAnalogInput *gun2Down  = AddAnalogInput("GunDown2",  "P2 Gun Down",  GAME_INPUT_GUN2, "NONE");

	gunX[1]            = AddAxisInput("GunX2", "P2 Gun X-Axis", GAME_INPUT_GUN2, "JOY2_XAXIS", gun2Left, gun2Right, 150, 400, 651); // normalize to [150,651]
	gunY[1]            = AddAxisInput("GunY2", "P2 Gun Y-Axis", GAME_INPUT_GUN2, "JOY2_YAXIS", gun2Up,   gun2Down,  80,  272, 465); // normalize to [80,465]
	
	CSwitchInput *gun2Trigger   = AddSwitchInput("Trigger2",   "P2 Trigger",          GAME_INPUT_GUN2, "JOY2_BUTTON1");
	CSwitchInput *gun2Offscreen = AddSwitchInput("Offscreen2", "P2 Point Off-screen", GAME_INPUT_GUN2, "JOY2_BUTTON2");

	trigger[1]         = AddTriggerInput("AutoTrigger2", "P2 Auto Trigger", GAME_INPUT_GUN2, gun2Trigger, gun2Offscreen);
}

CInputs::~CInputs()
{
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
		delete *it;
	m_inputs.clear();
}

CSwitchInput *CInputs::AddSwitchInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping, 
	UINT16 offVal, UINT16 onVal)
{
	CSwitchInput *input = new CSwitchInput(id, label, gameFlags, defaultMapping, offVal, onVal);
	m_inputs.push_back(input);
	return input;
}

CAnalogInput *CInputs::AddAnalogInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping, 
	UINT16 minVal, UINT16 maxVal)
{
	CAnalogInput *input = new CAnalogInput(id, label, gameFlags, defaultMapping, minVal, maxVal);
	m_inputs.push_back(input);
	return input;
}

CAxisInput *CInputs::AddAxisInput(const char *id, const char *label, unsigned gameFlags, const char *defaultMapping, 
	CAnalogInput *axisNeg, CAnalogInput *axisPos, UINT16 minVal, UINT16 offVal, UINT16 maxVal)
{
	CAxisInput *input = new CAxisInput(id, label, gameFlags, defaultMapping, axisNeg, axisPos, minVal, offVal, maxVal);
	m_inputs.push_back(input);
	return input;
}

CGearShift4Input *CInputs::AddGearShift4Input(const char *id, const char *label, unsigned gameFlags, 
	CSwitchInput *shift1, CSwitchInput *shift2, CSwitchInput *shift3, CSwitchInput *shift4, CSwitchInput *shiftUp, CSwitchInput *shiftDown)
{
	CGearShift4Input *input = new CGearShift4Input(id, label, gameFlags, shift1, shift2, shift3, shift4, shiftUp, shiftDown);
	m_inputs.push_back(input);
	return input;
}

CTriggerInput *CInputs::AddTriggerInput(const char *id, const char *label, unsigned gameFlags, 
	CSwitchInput *trigger, CSwitchInput *offscreen, UINT16 offVal, UINT16 onVal)
{
	CTriggerInput *input = new CTriggerInput(id, label, gameFlags, trigger, offscreen, offVal, onVal);
	m_inputs.push_back(input);
	return input;
}

void CInputs::PrintHeader(const char *fmt, ...)
{
	char header[1024];
	va_list vl;
	va_start(vl, fmt);
	vsprintf(header, fmt, vl);
	va_end(vl);

	puts(header);
	for (size_t i = 0; i < strlen(header); i++)
		putchar('-');
	printf("\n\n");
}

void CInputs::PrintConfigureInputsHelp()
{
	puts("For each control, use the following keys to map the inputs:");
	puts("");
	puts("  Return  Set current input mapping and move to next control,");
	puts("  c       Clear current input mapping and remain there,");
	puts("  s       Set the current input mapping and remain there,");
	puts("  a       Append to current input mapping (for multiple assignments)");
	puts("          and remain there,");
	puts("  r       Reset current input mapping to default and remain there,");
	puts("  Down    Move onto next control,");
	puts("  Up      Go back to previous control,");
	puts("  i       Display information about input system and attached devices,");
	puts("  h       Display this help again,");
	puts("  q       Finish and save all changes,");
	puts("  Esc     Finish without saving any changes.");
	puts("");
	puts("To assign input(s), simply press the appropriate key(s), mouse button(s)");
	puts("or joystick button(s) or move the mouse along one or more axes or move a");
	puts("joystick's axis or POV hat controller(s).  The input(s) will be accepted");
	puts("as soon as all pressed keys and buttons are released and all moved mouse");
	puts("and joystick controllers are returned to their rest positions.");
	puts("");
	puts("NOTES:");
	puts(" - in order to assign keys the configuration window must on top,");
	puts(" - in order to assign mouse buttons the mouse must be clicked");
	puts("   within the window,");
	puts(" - in order to assign mouse axes, the cursor must first be placed in");
	puts("   the center of the window and then moved in the corresponding");
	puts("   direction to the window's edge and then returned to the center.");
	puts("");
}

unsigned CInputs::Count()
{
	return (unsigned)m_inputs.size();
}

CInput *CInputs::operator[](const unsigned index)
{
	return m_inputs[index];
}

CInput *CInputs::operator[](const char *idOrLabel)
{
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
	{
		if (stricmp((*it)->id, idOrLabel) == 0 || stricmp((*it)->label, idOrLabel) == 0)
			return *it;
	}
	return NULL;
}

CInputSystem *CInputs::GetInputSystem()
{
	return m_system;
}

bool CInputs::Initialize()
{
	// Make sure the input system is initialized too
	if (!m_system->Initialize())
		return false;
	
	// Initialize all the inputs
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
		(*it)->Initialize(m_system);
	return true;
}

void CInputs::ReadFromINIFile(CINIFile *ini, const char *section)
{
	m_system->ReadFromINIFile(ini, section);

	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
		(*it)->ReadFromINIFile(ini, section);
}

void CInputs::WriteToINIFile(CINIFile *ini, const char *section)
{
	m_system->WriteToINIFile(ini, section);

	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
		(*it)->WriteToINIFile(ini, section);
}

bool CInputs::ConfigureInputs(const GameInfo *game, unsigned dispX, unsigned dispY, unsigned dispW, unsigned dispH)
{
	m_system->UngrabMouse();

	// Let the input system know the display geometry
	m_system->SetDisplayGeom(dispX, dispY, dispW, dispH);

	// Print header and help message
	int gameFlags;
	if (game != NULL)
	{
		PrintHeader("Configure Inputs for %s", game->title);
		gameFlags = game->inputFlags;
	}
	else
	{
		PrintHeader("Configure Inputs");
		gameFlags = GAME_INPUT_ALL;
	}
	PrintConfigureInputsHelp();

	// Get all inputs to be configured
	vector<CInput*> toConfigure;
	vector<CInput*>::iterator it;
	for (it = m_inputs.begin(); it != m_inputs.end(); it++)
	{
		if ((*it)->IsConfigurable() && ((*it)->gameFlags & gameFlags))
			toConfigure.push_back(*it);
	}

	// Remember current mappings for each input in case changes need to be undone later
	vector<string> oldMappings(toConfigure.size());
	size_t index = 0;
	for (it = toConfigure.begin(); it != toConfigure.end(); it++)
		oldMappings[index++] = (*it)->GetMapping();

	const char *groupLabel = NULL;

	// Loop through all the inputs to be configured
	index = 0;
	while (index < toConfigure.size())
	{
		// Get the current input
		CInput *input = toConfigure[index];

		// If have moved to a new input group, print the group heading
		const char *itGroupLabel = input->GetInputGroup();
		if (groupLabel == NULL || stricmp(groupLabel, itGroupLabel) != 0)
		{
			groupLabel = itGroupLabel;
			printf("%s:\n", groupLabel);
		}

Redisplay:
		// Print the input label, current input mapping and available options
		if (index > 0)
			printf(" %s [%s]: Ret/c/s/a/r/Up/Down/h/q/Esc? ", input->label, input->GetMapping());
		else
			printf(" %s [%s]: Ret/c/s/a/r/Down/h/q/Esc? ", input->label, input->GetMapping());
		fflush(stdout);	// required on terminals that use buffering

		// Loop until user has selected a valid option
		bool done = false;
		char mapping[50];
		while (!done)
		{
			// Wait for input from user
			if (!m_system->ReadMapping(mapping, 50, false, READ_KEYBOARD|READ_MERGE, uiExit->GetMapping()))
			{
				// If user pressed aborted input, then undo all changes and finish configuration
				index = 0;
				for (it = toConfigure.begin(); it != toConfigure.end(); it++)
				{
					(*it)->SetMapping(oldMappings[index].c_str());
					index++;
				}	
				puts("");

				m_system->GrabMouse();
				return false;
			}

			if (stricmp(mapping, "KEY_RETURN") == 0 || stricmp(mapping, "KEY_S") == 0)
			{
				// Set the input mapping
				printf("Setting...");
				fflush(stdout);	// required on terminals that use buffering
				if (input->Configure(false, uiExit->GetMapping()))
				{
					printf(" %s\n", input->GetMapping());
					if (stricmp(mapping, "KEY_RETURN") == 0)
						index++;
					done = true;
				}
				else
				{
					printf(" [Cancelled]\n");
					goto Redisplay;
				}
			}
			else if (stricmp(mapping, "KEY_A") == 0)
			{
				// Append to the input mapping(s)
				printf("Appending...");
				fflush(stdout);	// required on terminals that use buffering
				if (input->Configure(true, uiExit->GetMapping()))
					printf(" %s\n", input->GetMapping());
				else
					printf(" [Cancelled]\n");
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_C") == 0)
			{
				// Clear the input mapping(s)
				input->SetMapping("NONE");
				printf("Cleared\n");
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_R") == 0)
			{
				// Reset the input mapping(s) to the default
				input->ResetToDefaultMapping();
				printf("Reset\n");
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_DOWN") == 0)
			{
				// Move forward to the next mapping
				puts("");
				index++;
				done = true;
			}
			else if (stricmp(mapping, "KEY_UP") == 0)
			{
				// Move back to the previous mapping
				if (index > 0)
				{
					puts("");
					index--;
					done = true;
				}
			}
			else if (stricmp(mapping, "KEY_HOME") == 0)
			{
				// Move to first input
				puts("");
				index = 0;
				done = true;
			}
			else if (stricmp(mapping, "KEY_I") == 0)
			{
				// Print info about input system
				printf("\n\n");
				m_system->PrintSettings();
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_H") == 0)
			{
				// Print the help message again
				printf("\n\n");
				PrintConfigureInputsHelp();
				goto Redisplay;
			}
			else if (stricmp(mapping, "KEY_Q") == 0)
			{
				// Finish configuration
				puts("");

				m_system->GrabMouse();
				return true;
			}
		}
	}

	// All inputs set, finish configuration
	puts("");

	m_system->GrabMouse();
	return true;
}

void CInputs::PrintInputs(const GameInfo *game)
{
	// Print header
	int gameFlags;
	if (game != NULL)
	{
		PrintHeader("Input Assignments for %s", game->title);
		gameFlags = game->inputFlags;
	}
	else
	{
		PrintHeader("Input Assignments");
		gameFlags = GAME_INPUT_ALL;
	}

	const char *groupLabel = NULL;
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
	{
		if (!(*it)->IsConfigurable() || !((*it)->gameFlags & gameFlags))
			continue;

		const char *itGroupLabel = (*it)->GetInputGroup();
		if (groupLabel == NULL || stricmp(groupLabel, itGroupLabel) != 0)
		{
			groupLabel = itGroupLabel;
			printf("%s:\n", groupLabel);
		}

		printf(" %s = %s\n", (*it)->label, (*it)->GetMapping());
	}

	puts("");
}

bool CInputs::Poll(const GameInfo *game, unsigned dispX, unsigned dispY, unsigned dispW, unsigned dispH)
{
	// Update the input system with the current display geometry
	m_system->SetDisplayGeom(dispX, dispY, dispW, dispH);

	// Poll the input system
	if (!m_system->Poll())
		return false;

	// Poll all UI inputs and all the inputs used by the current game, or all inputs if game is NULL
	int gameFlags = (game != NULL ? game->inputFlags : GAME_INPUT_ALL);
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
	{
		if ((*it)->IsUIInput() || ((*it)->gameFlags & gameFlags))
			(*it)->Poll();
	}
	return true;
}

void CInputs::DumpState(const GameInfo *game)
{
	// Print header
	int gameFlags;
	if (game != NULL)
	{
		PrintHeader("Input States for %s", game->title);
		gameFlags = game->inputFlags;
	}
	else
	{
		PrintHeader("Input States");
		gameFlags = GAME_INPUT_ALL;
	}

	// Loop through the inputs used by the current game, or all inputs if game is NULL, and dump their values to stdout
	for (vector<CInput*>::iterator it = m_inputs.begin(); it != m_inputs.end(); it++)
	{
		if (!(*it)->IsUIInput() && !((*it)->gameFlags & gameFlags))
			continue;

		if ((*it)->IsVirtual())
			printf("%s = (%d)\n", (*it)->id, (*it)->value);
		else
			printf("%s [%s] = (%d)\n", (*it)->id, (*it)->GetMapping(), (*it)->value);
	}
}
