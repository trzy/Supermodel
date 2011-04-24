#ifndef INCLUDED_DIRECTINPUTSYSTEM_H
#define INCLUDED_DIRECTINPUTSYSTEM_H

#include "Types.h"
#include "Inputs/InputSource.h"
#include "Inputs/InputSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dinput.h>

#include <vector>
using namespace std;

#define NUM_DI_KEYS (sizeof(s_keyMap) / sizeof(DIKeyMapStruct))

#define MATCHES_USAGE(wUsage, wUsagePage, word) wUsage == LOWORD(word) && wUsagePage == HIWORD(word)
#define X_AXIS_USAGE  0x00010030
#define Y_AXIS_USAGE  0x00010031
#define Z_AXIS_USAGE  0x00010032
#define RX_AXIS_USAGE 0x00010033
#define RY_AXIS_USAGE 0x00010034
#define RZ_AXIS_USAGE 0x00010035

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

typedef /*WINUSERAPI*/ INT (WINAPI *GetRawInputDeviceListPtr)(OUT PRAWINPUTDEVICELIST pRawInputDeviceList, IN OUT PUINT puiNumDevices, IN UINT cbSize);
typedef /*WINUSERAPI*/ INT (WINAPI *GetRawInputDeviceInfoPtr)(IN HANDLE hDevice, IN UINT uiCommand, OUT LPVOID pData, IN OUT PUINT pcbSize);
typedef /*WINUSERAPI*/ BOOL (WINAPI *RegisterRawInputDevicesPtr)(IN PCRAWINPUTDEVICE pRawInputDevices, IN UINT uiNumDevices, IN UINT cbSize);
typedef /*WINUSERAPI*/ INT (WINAPI *GetRawInputDataPtr)(IN HRAWINPUT hRawInput, IN UINT uiCommand, OUT LPVOID pData, IN OUT PUINT pcbSize, IN UINT cbSizeHeader);

LRESULT WINAPI CallWndProc(int nCode, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK DI8EnumDevicesCallback(LPCDIDEVICEINSTANCE instance, LPVOID context);

BOOL CALLBACK DI8EnumAxesCallback(LPDIDEVICEOBJECTINSTANCE instance, LPVOID context);

/*
 * Input system that uses DirectInput and RawInput APIs.
 */
class CDirectInputSystem : public CInputSystem
{
private:
	// Lookup table to map key names to DirectInput keycodes and Virtual keycodes
	static DIKeyMapStruct s_keyMap[];

	bool m_useRawInput;
	
	HWND m_hwnd;

	bool m_activated;
	
	GetRawInputDeviceListPtr m_getRIDevListPtr;
	GetRawInputDeviceInfoPtr m_getRIDevInfoPtr;
	RegisterRawInputDevicesPtr m_regRIDevsPtr;
	GetRawInputDataPtr m_getRIDataPtr;
	
	vector<HANDLE> m_rawKeyboards;
	vector<BOOL*> m_rawKeyStates;
	vector<HANDLE> m_rawMice;
	RawMseState m_combRawMseState;
	vector<RawMseState*> m_rawMseStates;

	LPDIRECTINPUT8 m_di8;
	LPDIRECTINPUTDEVICE8 m_di8Keyboard;
	LPDIRECTINPUTDEVICE8 m_di8Mouse;	
	vector<LPDIRECTINPUTDEVICE8> m_di8Joysticks;

	vector<JoyDetails*> m_joyDetails;
	
	BYTE m_diKeyState[256];
	DIMseState m_diMseState;
	vector<LPDIJOYSTATE2> m_diJoyStates;

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

protected:
	/*
	 * Initializes the DirectInput input system.
	 */
	bool InitializeSystem();

	int GetNumKeyboards();	

	int GetNumMice();
	
	int GetNumJoysticks();

	int GetKeyIndex(const char *keyName);

	const char *GetKeyName(int keyIndex);

	JoyDetails *GetJoyDetails(int joyNum);

	bool IsKeyPressed(int joyNum, int keyIndex);

	int GetMouseAxisValue(int mseNum, int axisNum);

	int GetMouseWheelDir(int mseNum);

	bool IsMouseButPressed(int mseNum, int butNum);

	int GetJoyAxisValue(int joyNum, int axisNum);

	bool IsJoyPOVInDir(int joyNum, int povNum, int povDir);

	bool IsJoyButPressed(int joyNum, int butNum);

	void Wait(int ms);

	bool ConfigMouseCentered();

	CInputSource *CreateAnyMouseSource(EMousePart msePart);

public:
	/*
	 * Constructs a DirectInput/RawInput input system.  
	 * If useRawInput is true then RawInput is used for keyboard and mice movements (allowing multiple devices).  If false
	 * then DirectInput is used instead (which doesn't allow multiple devices).
	 * In both cases, DirectInput is used for reading joysticks.
	 */
	CDirectInputSystem(bool useRawInput);

	~CDirectInputSystem();

	bool Poll();

	void SetMouseVisibility(bool visible);

	void ConfigEnd();
};

#endif	// INCLUDED_DIRECTINPUTSYSTEM_H
