#ifndef INCLUDED_DIRECTINPUTSYSTEM_H
#define INCLUDED_DIRECTINPUTSYSTEM_H

#include "Types.h"
#include "Inputs/Input.h"
#include "Inputs/InputSource.h"
#include "Inputs/InputSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dinput.h>
#include <XInput.h>

#include <vector>
using namespace std;

#define NUM_DI_KEYS (sizeof(s_keyMap) / sizeof(DIKeyMapStruct))

struct DIKeyMapStruct
{
	const char *keyName;
	int diKey;
};

struct RawMseState
{
	LONG x;
	LONG y;
	LONG z;
	LONG wheelDelta;
	SHORT wheelDir;
	USHORT buttons;
};

struct DIMseState
{
	LONG x;
	LONG y;
	LONG z;
	SHORT wheelDir;
	BYTE buttons[5];
};

struct DIJoyInfo
{
	GUID guid;
	bool isXInput;
	int dInputNum;
	LPDIRECTINPUTEFFECT dInputEffects[NUM_JOY_AXES][NUM_FF_EFFECTS];
	int xInputNum;
};

struct DIEnumDevsContext
{
	vector<DIJoyInfo> *infos;
	bool useXInput;
};

// RawInput API
typedef /*WINUSERAPI*/ INT (WINAPI *GetRawInputDeviceListPtr)(OUT PRAWINPUTDEVICELIST pRawInputDeviceList, IN OUT PUINT puiNumDevices, IN UINT cbSize);
typedef /*WINUSERAPI*/ INT (WINAPI *GetRawInputDeviceInfoPtr)(IN HANDLE hDevice, IN UINT uiCommand, OUT LPVOID pData, IN OUT PUINT pcbSize);
typedef /*WINUSERAPI*/ BOOL (WINAPI *RegisterRawInputDevicesPtr)(IN PCRAWINPUTDEVICE pRawInputDevices, IN UINT uiNumDevices, IN UINT cbSize);
typedef /*WINUSERAPI*/ INT (WINAPI *GetRawInputDataPtr)(IN HRAWINPUT hRawInput, IN UINT uiCommand, OUT LPVOID pData, IN OUT PUINT pcbSize, IN UINT cbSizeHeader);

// XInput API
typedef /*WINUSERAPI*/ DWORD (WINAPI *XInputGetCapabilitiesPtr)(IN DWORD dwUserIndex, IN DWORD dwFlags, OUT PXINPUT_CAPABILITIES pCapabilities);
typedef /*WINUSERAPI*/ DWORD (WINAPI *XInputGetStatePtr)(IN DWORD dwUserIndex, OUT PXINPUT_STATE pState);
typedef /*WINUSERAPI*/ DWORD (WINAPI *XInputSetStatePtr)(IN DWORD dwUserIndex, IN PXINPUT_VIBRATION pVibration);

// DirectInput callbacks
static bool IsXInputDevice(const GUID &devProdGUID);

static BOOL CALLBACK DI8EnumDevicesCallback(LPCDIDEVICEINSTANCE instance, LPVOID context);

static BOOL CALLBACK DI8EnumAxesCallback(LPDIDEVICEOBJECTINSTANCE instance, LPVOID context);

static BOOL CALLBACK DI8EnumEffectsCallback(LPCDIEFFECTINFO effectInfo, LPVOID context);

/*
 * Input system that uses combination of DirectInput, XInput and RawInput APIs.
 */
class CDirectInputSystem : public CInputSystem
{
private:
	// Lookup table to map key names to DirectInput keycodes and Virtual keycodes
	static DIKeyMapStruct s_keyMap[];

	static const char *ConstructName(bool useRawInput, bool useXInput, bool enableFFeedback);

	bool m_useRawInput;
	bool m_useXInput;
	bool m_enableFFeedback;
	
	HWND m_hwnd;
	DWORD m_screenW;
	DWORD m_screenH;

	bool m_initializedCOM;
	bool m_activated;
	
	// Function pointers for RawInput API
	GetRawInputDeviceListPtr m_getRIDevListPtr;
	GetRawInputDeviceInfoPtr m_getRIDevInfoPtr;
	RegisterRawInputDevicesPtr m_regRIDevsPtr;
	GetRawInputDataPtr m_getRIDataPtr;
	
	// Keyboard, mouse and joystick details
	vector<KeyDetails> m_keyDetails;
	vector<MouseDetails> m_mseDetails;
	vector<JoyDetails> m_joyDetails;
	
	// RawInput keyboard and mice handles and states
	vector<HANDLE> m_rawKeyboards;
	vector<BOOL*> m_rawKeyStates;
	vector<HANDLE> m_rawMice;
	RawMseState m_combRawMseState;
	vector<RawMseState> m_rawMseStates;

	// Function pointers for XInput API
	XInputGetCapabilitiesPtr m_xiGetCapabilitiesPtr;
	XInputGetStatePtr m_xiGetStatePtr;
	XInputSetStatePtr m_xiSetStatePtr;

	// DirectInput pointers and details
	LPDIRECTINPUT8 m_di8;
	LPDIRECTINPUTDEVICE8 m_di8Keyboard;
	LPDIRECTINPUTDEVICE8 m_di8Mouse;	
	vector<LPDIRECTINPUTDEVICE8> m_di8Joysticks;
	
	// DirectInput keyboard and mouse states
	BYTE m_diKeyState[256];
	DIMseState m_diMseState;
	
	// DirectInput joystick infos and states
	vector<DIJoyInfo> m_diJoyInfos;
	vector<DIJOYSTATE2> m_diJoyStates;

	bool GetRegString(HKEY regKey, const char *regPath, string &str);

	bool GetRegDeviceName(const char *rawDevName, char *name);

	void OpenKeyboardsAndMice();

	void ActivateKeyboardsAndMice();

	void PollKeyboardsAndMice();

	void CloseKeyboardsAndMice();

	void ResetMice();

	void ProcessRawInput(HRAWINPUT hInput);

	void OpenJoysticks();

	void ActivateJoysticks();

	void PollJoysticks();

	void CloseJoysticks();

	HRESULT CreateJoystickEffect(LPDIRECTINPUTDEVICE8 di8Joystick, int axisNum, ForceFeedbackCmd ffCmd, LPDIRECTINPUTEFFECT *di8Effect);

protected:
	/*
	 * Initializes the DirectInput input system.
	 */
	bool InitializeSystem();

	int GetKeyIndex(const char *keyName);

	const char *GetKeyName(int keyIndex);

	bool IsKeyPressed(int joyNum, int keyIndex);

	int GetMouseAxisValue(int mseNum, int axisNum);

	int GetMouseWheelDir(int mseNum);

	bool IsMouseButPressed(int mseNum, int butNum);

	int GetJoyAxisValue(int joyNum, int axisNum);

	bool IsJoyPOVInDir(int joyNum, int povNum, int povDir);

	bool IsJoyButPressed(int joyNum, int butNum);
	
	bool ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd);

	void Wait(int ms);

	bool ConfigMouseCentered();

	CInputSource *CreateAnyMouseSource(EMousePart msePart);

public:
	/*
	 * Constructs a DirectInput/XInput/RawInput input system.  
	 * If useRawInput is true then RawInput is used for keyboard and mice movements (allowing multiple devices, eg for dual lightguns in gun
	 * games such as Lost World).  If false then DirectInput is used instead (which doesn't allow multiple devices).  In both cases,
	 * DirectInput/XInput is used for reading joysticks.
	 * If useXInput is true then XInput is used for reading XBox 360 game controllers (and/or XInput compatible joysticks) and DirectInput is used
	 * for all other types of joystick.  If false, then DirectInput is used for reading all joysticks (including XBox 360 ones).
	 * The advantage of using XInput for XBox 360 game controllers is that it allows the left and right triggers to be used simultaneously
	 * (ie to brake and accelerate at the same time in order to power slide the car in Daytona USA 2).  Under DirectInput the triggers get mapped
	 * to the same shared axis and so cannot be distinguished when pressed together.
	 * If enableFFeedback is true then force feedback is enabled (for those joysticks which are force feedback capable).
	 */
	CDirectInputSystem(bool useRawInput, bool useXInput, bool enableFFeedback);

	~CDirectInputSystem();

	int GetNumKeyboards();	

	int GetNumMice();
	
	int GetNumJoysticks();

	const KeyDetails *GetKeyDetails(int kbdNum);

	const MouseDetails *GetMouseDetails(int mseNum);

	const JoyDetails *GetJoyDetails(int joyNum);

	bool Poll();

	void GrabMouse();

	void UngrabMouse();

	void SetMouseVisibility(bool visible);
};

#endif	// INCLUDED_DIRECTINPUTSYSTEM_H
