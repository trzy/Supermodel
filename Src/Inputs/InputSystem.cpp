#include "Supermodel.h"

#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
using namespace std;

const char *CInputSystem::s_validKeyNames[] = 
{
	// General keys
	"BACKSPACE",
	"TAB",
	"CLEAR",
	"RETURN",
	"PAUSE",
	"ESCAPE",
	"SPACE",
	"EXCLAIM",
	"DBLQUOTE",
	"HASH",
	"DOLLAR",
	"AMPERSAND",
	"QUOTE",
	"LEFTPAREN",
	"RIGHTPAREN",
	"ASTERISK",
	"PLUS",
	"COMMA",
	"MINUS",
	"PERIOD",
	"SLASH",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"COLON",
	"SEMICOLON",
	"LESS",
	"EQUALS",
	"GREATER",
	"QUESTION",
	"AT",
	"LEFTBRACKET",
	"BACKSLASH",
	"RIGHTBRACKET",
	"CARET",
	"UNDERSCORE",
	"BACKQUOTE",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"DEL",
	
	// Keypad
	"KEYPAD0",
	"KEYPAD1",
	"KEYPAD2",
	"KEYPAD3",
	"KEYPAD4",
	"KEYPAD5",
	"KEYPAD6",
	"KEYPAD7",
	"KEYPAD8",
	"KEYPAD9",
	"KEYPADPERIOD",
	"KEYPADDIVIDE",
	"KEYPADMULTIPLY",
	"KEYPADMINUS",
	"KEYPADPLUS",
	"KEYPADENTER",
	"KEYPADEQUALS",
	
	// Arrows + Home/End Pad
	"UP",
	"DOWN",
	"RIGHT",
	"LEFT",
	"INSERT",
	"HOME",
	"END",
	"PGUP",
	"PGDN",

	// Function Key
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
	"F13",
	"F14",
	"F15",
    
	// Modifier Keys  
	"NUMLOCK",
	"CAPSLOCK",
	"SCROLLLOCK",
	"SHIFT",
	"RIGHTSHIFT",
	"LEFTSHIFT",
	"CTRL",
	"RIGHTCTRL",
	"LEFTCTRL",
	"ALT",
	"RIGHTALT",
	"LEFTALT",
	"RIGHTMETA",
	"LEFTMETA",
	"RIGHTWINDOWS",
	"LEFTWINDOWS",
	"ALTGR",
	"COMPOSE",
    
	// Other
	"HELP",
	"PRINT",
	"SYSREQ",
	"BREAK",
	"MENU",
	"POWER",
	"EURO",
	"UNDO"
};

MousePartsStruct CInputSystem::s_mseParts[] = 
{
	// X Axis (Axis 0)
	{ "XAXIS",         MouseXAxis },
	{ "XAXIS_INV",     MouseXAxisInv },
	{ "XAXIS_POS",     MouseXAxisPos },
	{ "XAXIS_NEG",     MouseXAxisNeg },
	{ "RIGHT",         MouseXAxisPos },
	{ "LEFT",          MouseXAxisNeg },
	{ "AXIS1",         MouseXAxis },
	{ "AXIS1_INV",     MouseXAxisInv },
	{ "AXIS1_POS",     MouseXAxisPos },
	{ "AXIS1_NEG",     MouseXAxisNeg },
	
	// Y Axis (Axis 1)
	{ "YAXIS",         MouseYAxis },
	{ "YAXIS_INV",     MouseYAxisInv },
	{ "YAXIS_POS",     MouseYAxisPos },
	{ "YAXIS_NEG",     MouseYAxisNeg },
	{ "DOWN",          MouseYAxisPos },
	{ "UP",            MouseYAxisNeg },
	{ "AXIS2",         MouseYAxis },
	{ "AXIS2_INV",     MouseYAxisInv },
	{ "AXIS2_POS",     MouseYAxisPos },
	{ "AXIS2_NEG",     MouseYAxisNeg },

	// Z/Wheel Axis (Axis 2)
	{ "ZAXIS",         MouseZAxis },
	{ "ZAXIS_INV",     MouseZAxisInv },
	{ "ZAXIS_POS",     MouseZAxisPos },
	{ "ZAXIS_NEG",     MouseZAxisNeg },
	{ "WHEEL_UP",      MouseZAxisPos },
	{ "WHEEL_DOWN",    MouseZAxisNeg },
	{ "AXIS3",         MouseZAxis },
	{ "AXIS3_INV",     MouseZAxisInv },
	{ "AXIS3_POS",     MouseZAxisPos },
	{ "AXIS3_NEG",     MouseZAxisNeg },

	// Left/Middle/Right Buttons (Buttons 0-2)
	{ "LEFT_BUTTON",   MouseButtonLeft },
	{ "BUTTON1",       MouseButtonLeft },
	{ "MIDDLE_BUTTON", MouseButtonMiddle },
	{ "BUTTON2",       MouseButtonMiddle },
	{ "RIGHT_BUTTON",  MouseButtonRight },
	{ "BUTTON3",       MouseButtonRight },

	// Extra Buttons (Buttons 3 & 4)
	{ "BUTTONX1",      MouseButtonX1 },
	{ "BUTTON4",       MouseButtonX1 },
	{ "BUTTONX2",      MouseButtonX2 },
	{ "BUTTON5",       MouseButtonX2 },

	// List terminator
	{ NULL,            MouseUnknown }
};

JoyPartsStruct CInputSystem::s_joyParts[] = 
{
	// X-Axis (Axis 0)
	{ "XAXIS",         JoyXAxis },
	{ "XAXIS_INV",     JoyXAxisInv },
	{ "XAXIS_POS",     JoyXAxisPos },
	{ "XAXIS_NEG",     JoyXAxisNeg },
	{ "RIGHT",         JoyXAxisPos },
	{ "LEFT",          JoyXAxisNeg },
	{ "AXIS1",         JoyXAxis },
	{ "AXIS1_INV",     JoyXAxisInv },
	{ "AXIS1_POS",     JoyXAxisPos },
	{ "AXIS1_NEG",     JoyXAxisNeg },
	
	// Y-Axis (Axis 1)
	{ "YAXIS",         JoyYAxis },
	{ "YAXIS_INV",     JoyYAxisInv },
	{ "YAXIS_POS",     JoyYAxisPos },
	{ "YAXIS_NEG",     JoyYAxisNeg },
	{ "DOWN",          JoyYAxisPos },
	{ "UP",            JoyYAxisNeg },
	{ "AXIS2",         JoyYAxis },
	{ "AXIS2_INV",     JoyYAxisInv },
	{ "AXIS2_POS",     JoyYAxisPos },
	{ "AXIS2_NEG",     JoyYAxisNeg },
	
	// Z-Axis (Axis 2)
	{ "ZAXIS",         JoyZAxis },
	{ "ZAXIS_INV",     JoyZAxisInv },
	{ "ZAXIS_POS",     JoyZAxisPos },
	{ "ZAXIS_NEG",     JoyZAxisNeg },
	{ "AXIS3",         JoyZAxis },
	{ "AXIS3_INV",     JoyZAxisInv },
	{ "AXIS3_POS",     JoyZAxisPos },
	{ "AXIS3_NEG",     JoyZAxisNeg },

	// RX-Axis (Axis 3)
	{ "RXAXIS",        JoyRXAxis },
	{ "RXAXIS_INV",    JoyRXAxisInv },
	{ "RXAXIS_POS",    JoyRXAxisPos },
	{ "RXAXIS_NEG",    JoyRXAxisNeg },
	{ "AXIS4",         JoyRXAxis },
	{ "AXIS4_INV",     JoyRXAxisInv },
	{ "AXIS4_POS",     JoyRXAxisPos },
	{ "AXIS4_NEG",     JoyRXAxisNeg },

	// RY-Axis (Axis 4)
	{ "RYAXIS",        JoyRYAxis },
	{ "RYAXIS_INV",    JoyRYAxisInv },
	{ "RYAXIS_POS",    JoyRYAxisPos },
	{ "RYAXIS_NEG",    JoyRYAxisNeg },
	{ "AXIS5",         JoyRYAxis },
	{ "AXIS5_INV",     JoyRYAxisInv },
	{ "AXIS5_POS",     JoyRYAxisPos },
	{ "AXIS5_NEG",     JoyRYAxisNeg },
	
	// RZ-Axis (Axis 5)
	{ "RZAXIS",        JoyRZAxis },
	{ "RZAXIS_INV",    JoyRZAxisInv },
	{ "RZAXIS_POS",    JoyRZAxisPos },
	{ "RZAXIS_NEG",    JoyRZAxisNeg },
	{ "AXIS6",         JoyRZAxis },
	{ "AXIS6_INV",     JoyRZAxisInv },
	{ "AXIS6_POS",     JoyRZAxisPos },
	{ "AXIS6_NEG",     JoyRZAxisNeg },

	// POV Hat 0
	{ "POV1_UP",       JoyPOV0Up },
	{ "POV1_DOWN",     JoyPOV0Down },
	{ "POV1_LEFT",     JoyPOV0Left },
	{ "POV1_RIGHT",    JoyPOV0Right },

	// POV Hat 1
	{ "POV2_UP",       JoyPOV1Up },
	{ "POV2_DOWN",     JoyPOV1Down },
	{ "POV2_LEFT",     JoyPOV1Left },
	{ "POV2_RIGHT",    JoyPOV1Right },

	// POV Hat 2
	{ "POV3_UP",       JoyPOV2Up },
	{ "POV3_DOWN",     JoyPOV2Down },
	{ "POV3_LEFT",     JoyPOV2Left },
	{ "POV3_RIGHT",    JoyPOV2Right },

	// POV Hat 3
	{ "POV4_UP",       JoyPOV3Up },
	{ "POV4_DOWN",     JoyPOV3Down },
	{ "POV4_LEFT",     JoyPOV3Left },
	{ "POV4_RIGHT",    JoyPOV3Right },

	// Buttons 0-31
	{ "BUTTON1",       JoyButton0 },
	{ "BUTTON2",       JoyButton1 },
	{ "BUTTON3",       JoyButton2 },
	{ "BUTTON4",       JoyButton3 },
	{ "BUTTON5",       JoyButton4 },
	{ "BUTTON6",       JoyButton5 },
	{ "BUTTON7",       JoyButton6 },
	{ "BUTTON8",       JoyButton7 },
	{ "BUTTON9",       JoyButton8 },
	{ "BUTTON10",      JoyButton9 },
	{ "BUTTON11",      JoyButton10 },
	{ "BUTTON12",      JoyButton11 },
	{ "BUTTON13",      JoyButton12 },
	{ "BUTTON14",      JoyButton13 },
	{ "BUTTON15",      JoyButton14 },
	{ "BUTTON16",      JoyButton15 },
	{ "BUTTON17",      JoyButton16 },
	{ "BUTTON18",      JoyButton17 },
	{ "BUTTON19",      JoyButton18 },
	{ "BUTTON20",      JoyButton19 },
	{ "BUTTON21",      JoyButton20 },
	{ "BUTTON22",      JoyButton21 },
	{ "BUTTON23",      JoyButton22 },
	{ "BUTTON24",      JoyButton23 },
	{ "BUTTON25",      JoyButton24 },
	{ "BUTTON26",      JoyButton25 },
	{ "BUTTON27",      JoyButton26 },
	{ "BUTTON28",      JoyButton27 },
	{ "BUTTON29",      JoyButton28 },
	{ "BUTTON30",      JoyButton29 },
	{ "BUTTON31",      JoyButton30 },
	{ "BUTTON32",      JoyButton31 },

	// List terminator
	{ NULL,            JoyUnknown }
};

CMultiInputSource *CInputSystem::s_emptySource = new CMultiInputSource();

CInputSystem::CInputSystem(const char *systemName) : name(systemName), m_dispX(0), m_dispY(0), m_dispW(0), m_dispH(0), m_isConfiguring(false)
{
	//
}

CInputSystem::~CInputSystem()
{
	DeleteSourceCache();
	ClearSettings();
}

void CInputSystem::CreateSourceCache()
{
	// Create cache for key sources
	m_anyKeySources = new CInputSource*[NUM_VALID_KEYS];
	memset(m_anyKeySources, NULL, sizeof(CInputSource*) * NUM_VALID_KEYS);
	if (m_numKbds != ANY_KEYBOARD)
	{
		m_keySources = new CInputSource**[m_numKbds];
		for (int kbdNum = 0; kbdNum < m_numKbds; kbdNum++)
		{
			m_keySources[kbdNum] = new CInputSource*[NUM_VALID_KEYS];
			memset(m_keySources[kbdNum], NULL, sizeof(CInputSource*) * NUM_VALID_KEYS);
		}
	}

	// Create cache for mouse sources
	m_anyMseSources = new CInputSource*[NUM_MOUSE_PARTS];
	memset(m_anyMseSources, NULL, sizeof(CInputSource*) * NUM_MOUSE_PARTS);
	if (m_numMice != ANY_MOUSE)
	{
		m_mseSources = new CInputSource**[m_numMice];
		for (int mseNum = 0; mseNum < m_numMice; mseNum++)
		{
			m_mseSources[mseNum] = new CInputSource*[NUM_MOUSE_PARTS];
			memset(m_mseSources[mseNum], NULL, sizeof(CInputSource*) * NUM_MOUSE_PARTS);
		}
	}

	// Create cache for joystick sources
	m_anyJoySources = new CInputSource*[NUM_JOY_PARTS];
	memset(m_anyJoySources, NULL, sizeof(CInputSource*) * NUM_JOY_PARTS);
	if (m_numJoys != ANY_JOYSTICK)
	{
		m_joySources = new CInputSource**[m_numJoys];
		for (int joyNum = 0; joyNum < m_numJoys; joyNum++)
		{
			m_joySources[joyNum] = new CInputSource*[NUM_JOY_PARTS];
			memset(m_joySources[joyNum], NULL, sizeof(CInputSource*) * NUM_JOY_PARTS);
		}
	}
}	

void CInputSystem::DeleteSourceCache()
{
	// Delete cache for keyboard sources
	if (m_anyKeySources != NULL)
	{
		for (int keyIndex = 0; keyIndex < NUM_VALID_KEYS; keyIndex++)
			DeleteSource(m_anyKeySources[keyIndex]);
		delete[] m_anyKeySources;
		m_anyKeySources = NULL;
		if (m_numKbds != ANY_KEYBOARD)
		{
			for (int kbdNum = 0; kbdNum < m_numKbds; kbdNum++)
			{
				for (int keyIndex = 0; keyIndex < NUM_VALID_KEYS; keyIndex++)
					DeleteSource(m_keySources[kbdNum][keyIndex]);
				delete[] m_keySources[kbdNum];
			}
			delete[] m_keySources;
			m_keySources = NULL;
		}
	}

	// Delete cache for mouse sources
	if (m_anyMseSources != NULL)
	{
		for (int mseIndex = 0; mseIndex < NUM_MOUSE_PARTS; mseIndex++)
			DeleteSource(m_anyMseSources[mseIndex]);
		delete[] m_anyMseSources;
		m_anyMseSources = NULL;
		if (m_numMice != ANY_MOUSE)
		{
			for (int mseNum = 0; mseNum < m_numMice; mseNum++)
			{
				for (int mseIndex = 0; mseIndex < NUM_MOUSE_PARTS; mseIndex++)
					DeleteSource(m_mseSources[mseNum][mseIndex]);
				delete[] m_mseSources[mseNum];
			}
			delete[] m_mseSources;
			m_mseSources = NULL;
		}
	}

	// Delete cache for joystick sources
	if (m_anyJoySources != NULL)
	{
		for (int joyIndex = 0; joyIndex < NUM_JOY_PARTS; joyIndex++)
			DeleteSource(m_anyJoySources[joyIndex]);
		delete[] m_anyJoySources;
		m_anyJoySources = NULL;
		if (m_numJoys != ANY_JOYSTICK)
		{
			for (int joyNum = 0; joyNum < m_numJoys; joyNum++)
			{
				for (int joyIndex = 0; joyIndex < NUM_JOY_PARTS; joyIndex++)
					DeleteSource(m_joySources[joyNum][joyIndex]);
				delete[] m_joySources[joyNum];
			}
			delete[] m_joySources;
			m_joySources = NULL;
		}
	}
}

void CInputSystem::DeleteSource(CInputSource *source)
{
	if (source != NULL && source != s_emptySource)
		delete source;
}
	
CInputSource *CInputSystem::GetKeySource(int kbdNum, int keyIndex)
{
	if (kbdNum == ANY_KEYBOARD)
	{
		// Check keyboard source cache first
		if (m_anyKeySources[keyIndex] == NULL)
		{
			if (m_numKbds == ANY_KEYBOARD)
				m_anyKeySources[keyIndex] = CreateKeySource(ANY_KEYBOARD, keyIndex);
			else
				m_anyKeySources[keyIndex] = CreateAnyKeySource(keyIndex);
		}
		return m_anyKeySources[keyIndex];
	}
	else if (kbdNum < m_numKbds)
	{
		// Check keyboard source cache first
		if (m_keySources[kbdNum][keyIndex] == NULL)
			m_keySources[kbdNum][keyIndex] = CreateKeySource(kbdNum, keyIndex);
		return m_keySources[kbdNum][keyIndex];
	}
	else
		return s_emptySource;
}

CInputSource *CInputSystem::GetMouseSource(int mseNum, EMousePart msePart)
{
	int mseIndex = (int)msePart;
	if (mseNum == ANY_MOUSE)
	{
		// Check mouse source cache first
		if (m_anyMseSources[mseIndex] == NULL)
		{
			if (m_numMice == ANY_MOUSE)
				m_anyMseSources[mseIndex] = CreateMouseSource(ANY_MOUSE, msePart);
			else
				m_anyMseSources[mseIndex] = CreateAnyMouseSource(msePart);
		}
		return m_anyMseSources[mseIndex];
	}
	else if (mseNum < m_numMice)
	{
		// Check mouse source cache first
		if (m_mseSources[mseNum][mseIndex] == NULL)
			m_mseSources[mseNum][mseIndex] = CreateMouseSource(mseNum, msePart);
		return m_mseSources[mseNum][mseIndex];
	}
	else
		return s_emptySource;
}

CInputSource *CInputSystem::GetJoySource(int joyNum, EJoyPart joyPart)
{
	int joyIndex = (int)joyPart;
	if (joyNum == ANY_JOYSTICK)
	{
		// Check joystick source cache first
		if (m_anyJoySources[joyIndex] == NULL)
		{
			if (m_numJoys == ANY_JOYSTICK)
				m_anyJoySources[joyIndex] = CreateJoySource(ANY_JOYSTICK, joyPart);
			else
				m_anyJoySources[joyIndex] = CreateAnyJoySource(joyPart);
		}
		return m_anyJoySources[joyIndex];
	}
	else if (joyNum < m_numJoys)
	{
		// Check joystick source cache first
		if (m_joySources[joyNum][joyIndex] == NULL)
			m_joySources[joyNum][joyIndex] = CreateJoySource(joyNum, joyPart);
		return m_joySources[joyNum][joyIndex];
	}
	else
		return s_emptySource;
}

void CInputSystem::CheckKeySources(int kbdNum, bool fullAxisOnly, vector<CInputSource*> &sources, string &mapping)
{
	// Loop through all valid keys
	for (int i = 0; i < NUM_VALID_KEYS; i++)
	{
		const char *keyName = s_validKeyNames[i];
		int keyIndex = GetKeyIndex(keyName);
		if (keyIndex < 0)
			continue;
		// Get key source for keyboard number and key and test it to see if it is active (but was not previously)
		CInputSource *source = GetKeySource(kbdNum, keyIndex);
		if (source != NULL && source->IsActive() && find(sources.begin(), sources.end(), source) == sources.end())
		{
			// Update mapping string and add source to list
			if (sources.size() == 0)
				mapping.assign("KEY");
			else
				mapping.append("+KEY");
			if (kbdNum >= 0)
				mapping.append(IntToString(kbdNum + 1));
			mapping.append("_");
			mapping.append(keyName);
			sources.push_back(source);
		}
	}
}

void CInputSystem::CheckMouseSources(int mseNum, bool fullAxisOnly, bool mseCentered, vector<CInputSource*> &sources, string &mapping)
{
	// Loop through all mouse parts
	for (int mseIndex = 0; mseIndex < NUM_MOUSE_PARTS; mseIndex++)
	{
		EMousePart msePart = (EMousePart)mseIndex;
		// Get axis details
		int axisNum;
		int axisDir;
		bool isAxis = GetAxisDetails(msePart, axisNum, axisDir);
		bool isXYAxis = isAxis && (axisNum == AXIS_X || axisNum == AXIS_Y);
		// Ignore X- & Y-axes if mouse hasn't been centered yet and filter axes according to fullAxisOnly
		if (isXYAxis && !mseCentered || isAxis && (IsFullAxis(msePart) && !fullAxisOnly || !IsFullAxis(msePart) && fullAxisOnly))
			continue;
		// Get mouse source for mouse number and part and test it to see if it is active (but was not previously)
		CInputSource *source = GetMouseSource(mseNum, msePart);
		if (source != NULL && source->IsActive() && find(sources.begin(), sources.end(), source) == sources.end())
		{
			// Otherwise, update mapping string and add source to list
			const char *partName = LookupName(msePart);
			if (sources.size() == 0)
				mapping.assign("MOUSE");
			else
				mapping.append("+MOUSE");
			if (mseNum >= 0)
				mapping.append(IntToString(mseNum + 1));
			mapping.append("_");
			mapping.append(partName);
			sources.push_back(source);
		}
	}
}

void CInputSystem::CheckJoySources(int joyNum, bool fullAxisOnly, vector<CInputSource*> &sources, string &mapping)
{
	// Loop through all joystick parts
	for (int joyIndex = 0; joyIndex < NUM_JOY_PARTS; joyIndex++)
	{
		EJoyPart joyPart = (EJoyPart)joyIndex;
		// Filter axes according to fullAxisOnly
		if (IsAxis(joyPart) && (IsFullAxis(joyPart) && !fullAxisOnly || !IsFullAxis(joyPart) && fullAxisOnly))
			continue;
		// Get joystick source for joystick number and part and test it to see if it is active (but was not previously)
		CInputSource *source = GetJoySource(joyNum, joyPart);
		if (source != NULL && source->IsActive() && find(sources.begin(), sources.end(), source) == sources.end())
		{
			// Otherwise, update mapping string and add source to list
			const char *partName = LookupName(joyPart);
			if (sources.size() == 0)
				mapping.assign("JOY");
			else
				mapping.append("+JOY");
			if (joyNum >= 0)
				mapping.append(IntToString(joyNum + 1));
			mapping.append("_");
			mapping.append(partName);
			sources.push_back(source);
		}
	}
}

bool CInputSystem::ParseInt(string str, int &num)
{
	stringstream ss(str);
	return !(ss >> num).fail();
}

string CInputSystem::IntToString(int num)
{
	stringstream ss;
	ss << num;
	return ss.str();
}

bool CInputSystem::EqualsIgnoreCase(string str1, const char *str2)
{
	for (string::const_iterator ci = str1.begin(); ci < str1.end(); ci++) 
	{
		if (*str2 == '\0' || tolower(*ci) != tolower(*str2))
			return false;
		str2++;
	}
	return *str2 == '\0';
}

bool CInputSystem::StartsWithIgnoreCase(string str1, const char *str2)
{
	for (string::const_iterator ci = str1.begin(); ci < str1.end(); ci++) 
	{
		if (*str2 == '\0')
			return true;
		if (tolower(*ci) != tolower(*str2))
			return false;
		str2++;
	}
	return *str2 == '\0'; 
}

bool CInputSystem::IsValidKeyName(string str)
{
	for (int i = 0; i < NUM_VALID_KEYS; i++)
	{
		if (EqualsIgnoreCase(str, s_validKeyNames[i]))
			return true;
	}
	return false;
}

EMousePart CInputSystem::LookupMousePart(string str)
{
	for (int i = 0; s_mseParts[i].id != NULL; i++)
	{
		if (EqualsIgnoreCase(str, s_mseParts[i].id))
			return s_mseParts[i].msePart;
	}
	return MouseUnknown;
}

const char *CInputSystem::LookupName(EMousePart msePart)
{
	for (int i = 0; s_mseParts[i].id != NULL; i++)
	{
		if (msePart == s_mseParts[i].msePart)
			return s_mseParts[i].id;
	}
	return NULL;
}

EJoyPart CInputSystem::LookupJoyPart(string str)
{
	for (int i = 0; s_joyParts[i].id != NULL; i++)
	{
		if (EqualsIgnoreCase(str, s_joyParts[i].id))
			return s_joyParts[i].joyPart;
	}
	return JoyUnknown;
}

const char *CInputSystem::LookupName(EJoyPart joyPart)
{
	for (int i = 0; s_joyParts[i].id != NULL; i++)
	{
		if (joyPart == s_joyParts[i].joyPart)
			return s_joyParts[i].id;
	}
	return NULL;
}

size_t CInputSystem::ParseDevMapping(string str, const char *devType, int &devNum)
{
	if (!StartsWithIgnoreCase(str, devType))
		return -1;
	size_t size = str.size();
	size_t devLen = strlen(devType);

	// Parse optional device number
	devNum = -1;
	size_t i = devLen;
	while (i < size && isdigit(str[i]))
		i++;
	if (i > devLen && i < size)
	{
		if (!ParseInt(str.substr(devLen, i), devNum))
			return -1;
		devNum--;
	}

	// Check hyphen preceeds device part
	if (i < size - 1 && str[i] == '_')
		return i + 1;
	else
		return -1;
}

CInputSource* CInputSystem::ParseMultiSource(string str, bool fullAxisOnly, bool isOr)
{
	// Check for empty or NONE mapping
	size_t size = str.size();
	if (size == 0 || EqualsIgnoreCase(str, "NONE"))
		return NULL;

	CInputSource *source = NULL;
	bool isMulti = false;
	vector<CInputSource*> sources;

	size_t start = 0;
	size_t end;
	do
	{
		// Remove leading whitespace/quotes and see if have any plusses/commas in mapping (to specify and/or multiple assignments)
		while ((isspace(str[start]) || str[start] == '"') && start < size - 1)
			start++;
		char delim = (isOr ? ',' : '+');
		end = str.find(delim, start);
		if (end == string::npos)
			end = size;

		// Remove tailing whitespace/quotes
		size_t subEnd = end;
		while (subEnd > 0 && (isspace(str[subEnd - 1]) || str[start] == '"'))
			subEnd--;
		string subStr = str.substr(start, subEnd - start);
		start = end + 1;

		// Parse the multi/single source in substring
		CInputSource *parsed;
		if (isOr)
			parsed = ParseMultiSource(subStr, fullAxisOnly, false);
		else
			parsed = ParseSingleSource(subStr);

		// Check the result is valid
		if (parsed != NULL && parsed->type != SourceInvalid &&
			(parsed->type == SourceEmpty || parsed->type == SourceFullAxis && fullAxisOnly || parsed->type != SourceFullAxis && !fullAxisOnly))
		{
			// Keep track of all sources parsed
			if (isMulti)
				sources.push_back(parsed);
			else if (source != NULL)
			{
				sources.push_back(source);
				sources.push_back(parsed);
				isMulti = true;
			}
			else
				source = parsed;
		}
	}
	while (start < size);

	// If only parsed a single source, return that, otherwise return a CMultiInputSource combining all the sources
	return (isMulti ? new CMultiInputSource(isOr, sources) : source);
}

CInputSource *CInputSystem::ParseSingleSource(string str)
{
	// Try parsing a key mapping
	int kbdNum;
	int keyNameIndex = ParseDevMapping(str, "KEY", kbdNum);
	if (keyNameIndex >= 0)
	{
		string keyName = str.substr(keyNameIndex);
		if (IsValidKeyName(keyName))
		{
			// Lookup key index and map to key source
			int keyIndex = GetKeyIndex(keyName.c_str());
			if (keyIndex >= 0)
				return GetKeySource(kbdNum, keyIndex);
			else if (EqualsIgnoreCase(keyName, "SHIFT") || EqualsIgnoreCase(keyName, "CTRL") || EqualsIgnoreCase(keyName, "ALT"))
			{
				// Handle special cases if not handled by subclass: SHIFT, CTRL and ALT
				string leftName = "LEFT" + keyName;
				string rightName = "RIGHT" + keyName;
				int leftIndex = GetKeyIndex(leftName.c_str());
				int rightIndex = GetKeyIndex(rightName.c_str());
				vector<CInputSource*> sources;
				if (leftIndex >= 0)
				{
					CInputSource *leftSource = GetKeySource(kbdNum, leftIndex);
					if (leftSource != NULL)
						sources.push_back(leftSource);
				}
				if (rightIndex >= 0)
				{
					CInputSource *rightSource = GetKeySource(kbdNum, rightIndex);
					if (rightSource != NULL)
						sources.push_back(rightSource);
				}
				if (sources.size() > 0)
					return new CMultiInputSource(true, sources);
			}
			return s_emptySource;
		}
	}
	
	// Try parsing a mouse mapping
	int mseNum;
	int msePartIndex = ParseDevMapping(str, "MOUSE", mseNum);
	if (msePartIndex >= 0)
	{
		// Lookup mouse part and map to mouse source
		EMousePart msePart = LookupMousePart(str.substr(msePartIndex));
		if (msePart != MouseUnknown)
			return GetMouseSource(mseNum, msePart);
	}

	// Try parsing a joystick mapping
	int joyNum;
	int joyPartIndex = ParseDevMapping(str, "JOY", joyNum);
	if (joyPartIndex >= 0)
	{
		// Lookup joystick part and map to joystick source
		EJoyPart joyPart = LookupJoyPart(str.substr(joyPartIndex));
		if (joyPart != JoyUnknown)
			return GetJoySource(joyNum, joyPart);
	}

	// As last option, assume mapping is just a key name and try creating keyboard source from it (to retain compatibility with previous 
	// versions of Supermodel)
	if (IsValidKeyName(str))
	{
		// Lookup key index and map to key source
		int keyIndex = GetKeyIndex(str.c_str());
		if (keyIndex >= 0)
			return GetKeySource(ANY_KEYBOARD, keyIndex);
		else
			return s_emptySource;
	}

	// If got here, it was not possible to parse mapping string so return NULL
	return NULL;
}

void CInputSystem::PrintKeySettings(KeySettings *settings)
{
	// Get common key settings and print header
	KeySettings *common;
	if (settings->kbdNum == ANY_KEYBOARD)
	{
		puts("Common Keyboard Settings:");
		common = NULL;
	}
	else
	{
		printf("Keyboard %d Settings:\n", settings->kbdNum + 1);
		common = GetKeySettings(ANY_KEYBOARD, true);
	}

	// Print all common settings and any settings that are different to common settings
	if (common == NULL || settings->sensitivity != common->sensitivity) printf(" Sensitivity = %d %%\n", settings->sensitivity);
	if (common == NULL || settings->decaySpeed != common->decaySpeed)   printf(" Decay Speed = %d %%\n", settings->decaySpeed);
}

KeySettings *CInputSystem::ReadKeySettings(CINIFile *ini, const char *section, int kbdNum)
{
	// Get common key settings and create new key settings based on that
	KeySettings *common = (kbdNum != ANY_KEYBOARD ? GetKeySettings(ANY_KEYBOARD, true) : &m_defKeySettings);
	KeySettings *settings = new KeySettings(*common);
	settings->kbdNum = kbdNum;

	// Read settings from ini file
	string baseKey("InputKey");
	if (kbdNum != ANY_KEYBOARD)
		baseKey.append(IntToString(kbdNum + 1));
	bool read = false;
	read |= ini->Get(section, baseKey + "Sensitivity", settings->sensitivity) == OKAY;
	read |= ini->Get(section, baseKey + "DecaySpeed", settings->decaySpeed) == OKAY;
	if (read)
		return settings;
	delete settings;
	return NULL;
}

void CInputSystem::WriteKeySettings(CINIFile *ini, const char *section, KeySettings *settings)
{
	// Get common key settings
	KeySettings *common = (settings->kbdNum != ANY_KEYBOARD ? GetKeySettings(ANY_KEYBOARD, true) : &m_defKeySettings);

	// Write to ini file any settings that are different to common settings
	string baseKey("InputKey");
	if (settings->kbdNum != ANY_KEYBOARD)
		baseKey.append(IntToString(settings->kbdNum + 1));
	if (settings->sensitivity != common->sensitivity) ini->Set(section, baseKey + "Sensitivity", settings->sensitivity);
	if (settings->decaySpeed != common->decaySpeed) ini->Set(section, baseKey + "DecaySpeed", settings->decaySpeed);
}

void CInputSystem::PrintMouseSettings(MouseSettings *settings)
{
	// Get common mouse settings and print header
	MouseSettings *common;
	if (settings->mseNum == ANY_MOUSE)
	{
		puts("Common Mouse Settings:");
		common = NULL;
	}
	else
	{
		printf("Mouse %d Settings:\n", settings->mseNum + 1);
		common = GetMouseSettings(ANY_MOUSE, true);
	}

	// Print all common settings and any settings that are different to common/default settings
	if (common == NULL || settings->xDeadZone != common->xDeadZone) printf(" X Dead Zone = %d %%\n", settings->xDeadZone);
	if (common == NULL || settings->yDeadZone != common->yDeadZone) printf(" Y Dead Zone = %d %%\n", settings->yDeadZone);
	if (common == NULL || settings->zDeadZone != common->zDeadZone) printf(" Z Dead Zone = %d %%\n", settings->zDeadZone);
}

MouseSettings *CInputSystem::ReadMouseSettings(CINIFile *ini, const char *section, int mseNum)
{
	// Get common mouse settings and create new mouse settings based on that
	MouseSettings *common = (mseNum != ANY_MOUSE ? GetMouseSettings(ANY_MOUSE, true) : &m_defMseSettings);
	MouseSettings *settings = new MouseSettings(*common);
	settings->mseNum = mseNum;

	// Read settings from ini file
	string baseKey("InputMouse");
	if (mseNum != ANY_MOUSE)
		baseKey.append(IntToString(mseNum + 1));
	bool read = false;
	read |= ini->Get(section, baseKey + "XDeadZone", settings->xDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "YDeadZone", settings->yDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "ZDeadZone", settings->zDeadZone) == OKAY;
	if (read)
		return settings;
	delete settings;
	return NULL;
}

void CInputSystem::WriteMouseSettings(CINIFile *ini, const char *section, MouseSettings *settings)
{
	// Get common mouse settings
	MouseSettings *common = (settings->mseNum != ANY_MOUSE ? GetMouseSettings(ANY_MOUSE, true) : &m_defMseSettings);
	
	// Write to ini file any settings that are different to common/default settings
	string baseKey("InputMouse");
	if (settings->mseNum != ANY_MOUSE)
		baseKey.append(IntToString(settings->mseNum + 1));
	if (settings->xDeadZone != common->xDeadZone) ini->Set(section, baseKey + "XDeadZone", settings->xDeadZone);
	if (settings->yDeadZone != common->yDeadZone) ini->Set(section, baseKey + "YDeadZone", settings->yDeadZone);
	if (settings->zDeadZone != common->zDeadZone) ini->Set(section, baseKey + "ZDeadZone", settings->zDeadZone);
}

void CInputSystem::PrintJoySettings(JoySettings *settings)
{
	// Get common mouse settings and print header
	JoySettings *common;
	if (settings->joyNum == ANY_JOYSTICK)
	{
		puts("Common Joystick Settings:");
		common = NULL;
	}
	else
	{
		printf("Joystick %d Settings:\n", settings->joyNum + 1);
		common = GetJoySettings(ANY_JOYSTICK, true);
	}

	// Print all common settings and any settings that are different to common/default settings
	if (common == NULL || settings->xDeadZone != common->xDeadZone) printf(" X-Axis Dead Zone = %d %%\n", settings->xDeadZone);
	if (common == NULL || settings->xSaturation != common->xSaturation) printf(" X-Axis Saturation = %d %%\n", settings->xSaturation);
	if (common == NULL || settings->yDeadZone != common->yDeadZone) printf(" Y-Axis Dead Zone = %d %%\n", settings->yDeadZone);
	if (common == NULL || settings->ySaturation != common->ySaturation) printf(" Y-Axis Saturation = %d %%\n", settings->ySaturation);
	if (common == NULL || settings->zDeadZone != common->zDeadZone) printf(" Z-Axis Dead Zone = %d %%\n", settings->zDeadZone);
	if (common == NULL || settings->zSaturation != common->zSaturation) printf(" Z-Axis Saturation = %d %%\n", settings->zSaturation);
	if (common == NULL || settings->rxDeadZone != common->rxDeadZone) printf(" RX-Axis Dead Zone = %d %%\n", settings->rxDeadZone);
	if (common == NULL || settings->rxSaturation != common->rxSaturation) printf(" RX-Axis Saturation = %d %%\n", settings->rxSaturation);
	if (common == NULL || settings->ryDeadZone != common->ryDeadZone) printf(" RY-Axis Dead Zone = %d %%\n", settings->ryDeadZone);
	if (common == NULL || settings->rySaturation != common->rySaturation) printf(" RY-Axis Saturation = %d %%\n", settings->rySaturation);
	if (common == NULL || settings->rzDeadZone != common->rzDeadZone) printf(" RZ-Axis Dead Zone = %d %%\n", settings->rzDeadZone);
	if (common == NULL || settings->rzSaturation != common->rzSaturation) printf(" RZ-Axis Saturation = %d %%\n", settings->rzSaturation);
}

JoySettings *CInputSystem::ReadJoySettings(CINIFile *ini, const char *section, int joyNum)
{
	// Get common/default joystick settings and create new joystick settings based on that
	JoySettings *common = (joyNum != ANY_JOYSTICK ? GetJoySettings(ANY_JOYSTICK, true) : &m_defJoySettings);
	JoySettings *settings = new JoySettings(*common);
	settings->joyNum = joyNum;

	// Read settings from ini file
	string baseKey("InputJoy");
	if (joyNum != ANY_JOYSTICK)
		baseKey.append(IntToString(joyNum + 1));
	bool read = false;
	read |= ini->Get(section, baseKey + "XDeadZone", settings->xDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "XSaturation", settings->xSaturation) == OKAY;
	read |= ini->Get(section, baseKey + "YDeadZone", settings->yDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "YSaturation", settings->ySaturation) == OKAY;
	read |= ini->Get(section, baseKey + "ZDeadZone", settings->zDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "ZSaturation", settings->zSaturation) == OKAY;
	read |= ini->Get(section, baseKey + "RXDeadZone", settings->rxDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "RXSaturation", settings->rxSaturation) == OKAY;
	read |= ini->Get(section, baseKey + "RYDeadZone", settings->ryDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "RYSaturation", settings->rySaturation) == OKAY;
	read |= ini->Get(section, baseKey + "RZDeadZone", settings->rzDeadZone) == OKAY;
	read |= ini->Get(section, baseKey + "RZSaturation", settings->rzSaturation) == OKAY;
	if (read)
		return settings;
	delete settings;
	return NULL;
}

void CInputSystem::WriteJoySettings(CINIFile *ini, const char *section, JoySettings *settings)
{
	// Get common/default joystick settings
	JoySettings *common = (settings->joyNum != ANY_JOYSTICK ? GetJoySettings(ANY_JOYSTICK, true) : &m_defJoySettings);

	// Write to ini file any settings that are different to common/default settings
	string baseKey("InputJoy");
	if (settings->joyNum != ANY_JOYSTICK)
		baseKey.append(IntToString(settings->joyNum + 1));
	if (settings->xDeadZone != common->xDeadZone) ini->Set(section, baseKey + "XDeadZone", settings->xDeadZone);
	if (settings->xSaturation != common->xSaturation) ini->Set(section, baseKey + "XSaturation", settings->xSaturation);
	if (settings->yDeadZone != common->yDeadZone) ini->Set(section, baseKey + "YDeadZone", settings->yDeadZone);
	if (settings->ySaturation != common->ySaturation) ini->Set(section, baseKey + "YSaturation", settings->ySaturation);
	if (settings->zDeadZone != common->zDeadZone) ini->Set(section, baseKey + "ZDeadZone", settings->zDeadZone);
	if (settings->zSaturation != common->zSaturation) ini->Set(section, baseKey + "ZSaturation", settings->zSaturation);
	if (settings->rxDeadZone != common->rxDeadZone) ini->Set(section, baseKey + "RXDeadZone", settings->rxDeadZone);
	if (settings->rxSaturation != common->rxSaturation) ini->Set(section, baseKey + "RXSaturation", settings->rxSaturation);
	if (settings->ryDeadZone != common->ryDeadZone) ini->Set(section, baseKey + "RYDeadZone", settings->ryDeadZone);
	if (settings->rySaturation != common->rySaturation) ini->Set(section, baseKey + "RYSaturation", settings->rySaturation);
	if (settings->rzDeadZone != common->rzDeadZone) ini->Set(section, baseKey + "RZDeadZone", settings->rzDeadZone);
	if (settings->rzSaturation != common->rzSaturation) ini->Set(section, baseKey + "RZSaturation", settings->rzSaturation);
}

KeySettings *CInputSystem::GetKeySettings(int kbdNum, bool useDefault)
{
	KeySettings *common = NULL;
	for (vector<KeySettings*>::iterator it = m_keySettings.begin(); it < m_keySettings.end(); it++)
	{
		if ((*it)->kbdNum == kbdNum)
			return *it;
		else if ((*it)->kbdNum == ANY_KEYBOARD)
			common = *it;
	}
	if (!useDefault)
		return NULL;
	return (common != NULL ? common : &m_defKeySettings);
}

MouseSettings *CInputSystem::GetMouseSettings(int mseNum, bool useDefault)
{
	MouseSettings *common = NULL;
	for (vector<MouseSettings*>::iterator it = m_mseSettings.begin(); it < m_mseSettings.end(); it++)
	{
		if ((*it)->mseNum == mseNum)
			return *it;
		else if ((*it)->mseNum == ANY_MOUSE)
			common = *it;
	}
	if (!useDefault)
		return NULL;
	return (common != NULL ? common : &m_defMseSettings);
}

JoySettings *CInputSystem::GetJoySettings(int joyNum, bool useDefault)
{
	JoySettings *common = NULL;
	for (vector<JoySettings*>::iterator it = m_joySettings.begin(); it < m_joySettings.end(); it++)
	{
		if ((*it)->joyNum == joyNum)
			return *it;
		else if ((*it)->joyNum == ANY_JOYSTICK)
			common = *it;
	}
	if (!useDefault)
		return NULL;
	return (common != NULL ? common : &m_defJoySettings);
}

bool CInputSystem::IsAxis(EMousePart msePart)
{
	return msePart >= MouseXAxis && msePart <= MouseZAxisNeg;
}

bool CInputSystem::IsFullAxis(EMousePart msePart)
{
	return IsAxis(msePart) && (((msePart - MouseXAxis) % 4) == AXIS_FULL || ((msePart - MouseXAxis) % 4) == AXIS_INVERTED);
}

bool CInputSystem::GetAxisDetails(EMousePart msePart, int &axisNum, int &axisDir)
{
	if (!IsAxis(msePart))
		return false;
	axisNum = (msePart - MouseXAxis) / 4;
	axisDir = (msePart - MouseXAxis) % 4;
	return true;
}

bool CInputSystem::IsButton(EMousePart msePart)
{
	return msePart >= MouseButtonLeft && msePart <= MouseButtonX2;
}

int CInputSystem::GetButtonNumber(EMousePart msePart)
{
	if (!IsButton(msePart))
		return -1;
	return msePart - MouseButtonLeft;
}

EMousePart CInputSystem::GetMouseAxis(int axisNum, int axisDir)
{
	if (axisNum > 0 || axisNum >= NUM_MOUSE_AXES || axisDir < 0 || axisDir > 3)
		return MouseUnknown;
	return (EMousePart)(MouseXAxis + 4 * axisNum + axisDir);
}

EMousePart CInputSystem::GetMouseButton(int butNum)
{
	if (butNum < 0 || butNum >= NUM_MOUSE_BUTTONS)
		return MouseUnknown;
	return (EMousePart)(MouseButtonLeft + butNum);	
}

bool CInputSystem::IsAxis(EJoyPart joyPart)
{
	return joyPart >= JoyXAxis && joyPart <= JoyRZAxisNeg;
}

bool CInputSystem::IsFullAxis(EJoyPart joyPart)
{
	return IsAxis(joyPart) && (((joyPart - JoyXAxis) % 4) == AXIS_FULL || ((joyPart - JoyXAxis) % 4) == AXIS_INVERTED);
}

bool CInputSystem::GetAxisDetails(EJoyPart joyPart, int &axisNum, int &axisDir)
{
	if (!IsAxis(joyPart))
		return false;
	axisNum = (joyPart - JoyXAxis) / 4;
	axisDir = (joyPart - JoyXAxis) % 4;
	return true;
}

bool CInputSystem::IsPOV(EJoyPart joyPart)
{
	return joyPart >= JoyPOV0Up && joyPart <= JoyPOV3Right;
}

bool CInputSystem::GetPOVDetails(EJoyPart joyPart, int &povNum, int &povDir)
{
	if (!IsPOV(joyPart))
		return false;
	povNum = (joyPart - JoyPOV0Up) / 4;
	povDir = (joyPart - JoyPOV0Up) % 4;
	return true;
}

bool CInputSystem::IsButton(EJoyPart joyPart)
{
	return joyPart >= JoyButton0 && joyPart <= JoyButton31;
}

int CInputSystem::GetButtonNumber(EJoyPart joyPart)
{
	if (!IsButton(joyPart))
		return -1;
	return joyPart - JoyButton0;
}

EJoyPart CInputSystem::GetJoyAxis(int axisNum, int axisDir)
{
	if (axisNum < 0 || axisNum >= NUM_JOY_AXES || axisDir < 0 || axisDir > 3)
		return JoyUnknown;
	return (EJoyPart)(JoyXAxis + 4 * axisNum + axisDir);
}

EJoyPart CInputSystem::GetJoyPOV(int povNum, int povDir)
{
	if (povNum < 0 || povNum >= NUM_JOY_POVS)
		return JoyUnknown;
	return (EJoyPart)(JoyPOV0Up + 4 * povNum + povDir);
}

EJoyPart CInputSystem::GetJoyButton(int butNum)
{
	if (butNum < 0 || butNum >= NUM_JOY_BUTTONS)
		return JoyUnknown;
	return (EJoyPart)(JoyButton0 + butNum);	
}

bool CInputSystem::ConfigMouseCentered()
{
	// Get mouse X & Y
	int mx = GetMouseAxisValue(ANY_MOUSE, AXIS_X);
	int my = GetMouseAxisValue(ANY_MOUSE, AXIS_Y);
	
	// See if mouse in center of display
	unsigned lx = m_dispX + m_dispW / 4;
	unsigned ly = m_dispY + m_dispH / 4;
	return mx >= lx && mx <= lx + m_dispW / 2 && my >= ly && my <= ly + m_dispH / 2;
}	

CInputSource *CInputSystem::CreateAnyKeySource(int keyIndex)
{
	// Default ANY_KEYBOARD source is to use CMultiInputSource to combine all individual sources
	vector<CInputSource*> keySrcs;
	for (int kbdNum = 0; kbdNum < m_numKbds; kbdNum++)
	{
		CInputSource *keySrc = CreateKeySource(kbdNum, keyIndex);
		if (keySrc != NULL)
			keySrcs.push_back(keySrc);
	}
	return new CMultiInputSource(true, keySrcs);
}

CInputSource *CInputSystem::CreateAnyMouseSource(EMousePart msePart)
{
	// Default ANY_MOUSE source is to use CMultiInputSource to combine all individual sources
	vector<CInputSource*> mseSrcs;
	for (int mseNum = 0; mseNum < m_numMice; mseNum++)
	{
		CInputSource *mseSrc = CreateMouseSource(mseNum, msePart);
		if (mseSrc != NULL)
			mseSrcs.push_back(mseSrc);
	}
	return new CMultiInputSource(true, mseSrcs);
}

CInputSource *CInputSystem::CreateAnyJoySource(EJoyPart joyPart)
{
	// Default ANY_JOYSTICK source is to use CMultiInputSource to combine all individual sources
	vector<CInputSource*> joySrcs;
	for (int joyNum = 0; joyNum < m_numJoys; joyNum++)
	{
		CInputSource *joySrc = CreateJoySource(joyNum, joyPart);
		if (joySrc != NULL)
			joySrcs.push_back(joySrc);
	}
	return new CMultiInputSource(true, joySrcs);
}

CInputSource *CInputSystem::CreateKeySource(int kbdNum, int keyIndex)
{
	// Get key settings
	KeySettings *settings = GetKeySettings(kbdNum, true);

	// Create source for given key index
	return new CKeyInputSource(this, kbdNum, keyIndex, settings->sensitivity, settings->decaySpeed);
}

CInputSource *CInputSystem::CreateMouseSource(int mseNum, EMousePart msePart)
{
	// Get mouse settings
	MouseSettings *settings = GetMouseSettings(mseNum, true);

	// Create source according to given mouse part
	int axisNum;
	int axisDir;
	if (GetAxisDetails(msePart, axisNum, axisDir))
	{
		// Part is mouse axis so get the deadzone setting for it
		unsigned deadZone;
		switch (axisNum)
		{
			case AXIS_X: deadZone = settings->xDeadZone; break;
			case AXIS_Y: deadZone = settings->yDeadZone; break;
			case AXIS_Z: deadZone = settings->zDeadZone; break;
			default:     return NULL;  // Any other axis numbers are invalid
		}
		return new CMseAxisInputSource(this, mseNum, axisNum, axisDir, deadZone);
	}
	else if (IsButton(msePart))
	{
		// Part is mouse button so map it to button number
		int butNum = GetButtonNumber(msePart);
		if (butNum < 0)
			return NULL;  // Buttons out of range are invalid
		return new CMseButInputSource(this, mseNum, butNum);
	}

	// If got here, then mouse part is invalid
	return NULL;
}

CInputSource *CInputSystem::CreateJoySource(int joyNum, EJoyPart joyPart)
{
	// Get joystick details and settings
	JoyDetails *joyDetails = GetJoyDetails(joyNum);
	JoySettings *settings = GetJoySettings(joyNum, true);
	
	// Create source according to given joystick part
	int axisNum;
	int axisDir;
	int povNum;
	int povDir;
	if (GetAxisDetails(joyPart, axisNum, axisDir))
	{
		// Part is joystick axis so see whether joystick has this axis and get the deadzone and saturation settings for it
		bool hasAxis;
		unsigned axisDZone;
		unsigned axisSat;
		switch (axisNum)
		{
			case AXIS_X:  hasAxis = joyDetails->hasXAxis; axisDZone = settings->xDeadZone; axisSat = settings->xSaturation; break;
			case AXIS_Y:  hasAxis = joyDetails->hasYAxis; axisDZone = settings->yDeadZone; axisSat = settings->ySaturation; break;
			case AXIS_Z:  hasAxis = joyDetails->hasZAxis; axisDZone = settings->zDeadZone; axisSat = settings->zSaturation; break;
			case AXIS_RX: hasAxis = joyDetails->hasRXAxis; axisDZone = settings->rxDeadZone; axisSat = settings->rxSaturation; break;
			case AXIS_RY: hasAxis = joyDetails->hasRYAxis; axisDZone = settings->ryDeadZone; axisSat = settings->rySaturation; break;
			case AXIS_RZ: hasAxis = joyDetails->hasRZAxis; axisDZone = settings->rzDeadZone; axisSat = settings->rzSaturation; break;
			default:      return NULL;  // Any other axis numbers are invalid
		}
		if (!hasAxis)
			return s_emptySource;  // If joystick doesn't have axis, then return empty source rather than NULL as not really an error
		return new CJoyAxisInputSource(this, joyNum, axisNum, axisDir, axisDZone, axisSat);
	}
	else if (GetPOVDetails(joyPart, povNum, povDir))
	{
		// Part is joystick POV hat controller so see whether joystick has this POV
		if (povNum >= joyDetails->numPOVs)
			return s_emptySource;  // If joystick doesn't have POV, then return empty source rather than NULL as not really an error
		return new CJoyPOVInputSource(this, joyNum, povNum, povDir);
	}
	else if (IsButton(joyPart))
	{	
		// Part is joystick button so map it to a button number
		int butNum = GetButtonNumber(joyPart);
		if (butNum < 0)
			return NULL;  // Buttons out of range are invalid
		if (butNum >= joyDetails->numButtons)
			return s_emptySource;  // If joystick doesn't have button, then return empty source rather than NULL as not really an error
		return new CJoyButInputSource(this, joyNum, butNum);
	}
	
	// If got here, then joystick part is invalid
	return NULL;
}

bool CInputSystem::Initialize()
{
	// Initialize subclass
	if (!InitializeSystem())
		return false;

	// Get number of keyboard, mice and joysticks
	m_numKbds = GetNumKeyboards();
	m_numMice = GetNumMice();
	m_numJoys = GetNumJoysticks();

	// Create cache to hold input sources
	CreateSourceCache();
	return true;
}

void CInputSystem::SetDisplayGeom(unsigned dispX, unsigned dispY, unsigned dispW, unsigned dispH)
{
	// Remember display geometry
	m_dispX = dispX;
	m_dispY = dispY;
	m_dispW = dispW;
	m_dispH = dispH;
}

CInputSource* CInputSystem::ParseSource(const char *mapping, bool fullAxisOnly)
{
	return ParseMultiSource(mapping, fullAxisOnly, true);
}

void CInputSystem::ClearSettings()
{
	// Delete all key settings
	for (vector<KeySettings*>::iterator it = m_keySettings.begin(); it < m_keySettings.end(); it++)
		delete *it;
	m_keySettings.clear();

	// Delete all mouse settings
	for (vector<MouseSettings*>::iterator it = m_mseSettings.begin(); it < m_mseSettings.end(); it++)
		delete *it;
	m_mseSettings.clear();

	// Delete all joystick settings
	for (vector<JoySettings*>::iterator it = m_joySettings.begin(); it < m_joySettings.end(); it++)
		delete *it;
	m_joySettings.clear();
}

void CInputSystem::PrintSettings()
{
	puts("Input System Settings");
	puts("---------------------");
	puts("");

	printf("Input System: %s\n", name);

	puts("");

	// Print all key settings for attached keyboards
	KeySettings *keySettings = GetKeySettings(ANY_KEYBOARD, true);
	PrintKeySettings(keySettings);
	for (int kbdNum = 0; kbdNum < m_numKbds; kbdNum++)
	{
		keySettings = GetKeySettings(kbdNum, false);
		if (keySettings != NULL)
			PrintKeySettings(keySettings);
	}

	// Print all mouse settings for attached mice
	MouseSettings *mseSettings = GetMouseSettings(ANY_MOUSE, true);
	PrintMouseSettings(mseSettings);
	for (int mseNum = 0; mseNum < m_numMice; mseNum++)
	{
		mseSettings = GetMouseSettings(mseNum, false);
		if (mseSettings != NULL)
			PrintMouseSettings(mseSettings);
	}

	// Print all joystick settings for attached joysticks
	JoySettings *joySettings = GetJoySettings(ANY_JOYSTICK, true);
	PrintJoySettings(joySettings);
	for (int joyNum = 0; joyNum < m_numJoys; joyNum++)
	{
		joySettings = GetJoySettings(joyNum, false);
		if (joySettings != NULL)
			PrintJoySettings(joySettings);
	}

	puts("");
}

void CInputSystem::ReadFromINIFile(CINIFile *ini, const char *section)
{
	ClearSettings();

	// Read all key settings for attached keyboards
	KeySettings *keySettings = ReadKeySettings(ini, section, ANY_KEYBOARD);
	if (keySettings != NULL)
		m_keySettings.push_back(keySettings);
	for (int kbdNum = 0; kbdNum < m_numKbds; kbdNum++)
	{
		keySettings = ReadKeySettings(ini, section, kbdNum);
		if (keySettings != NULL)
			m_keySettings.push_back(keySettings);
	}

	// Read all mouse settings for attached mice
	MouseSettings *mseSettings = ReadMouseSettings(ini, section, ANY_MOUSE);
	if (mseSettings != NULL)
		m_mseSettings.push_back(mseSettings);
	for (int mseNum = 0; mseNum < m_numMice; mseNum++)
	{
		mseSettings = ReadMouseSettings(ini, section, mseNum);
		if (mseSettings != NULL)
			m_mseSettings.push_back(mseSettings);
	}

	// Read all joystick settings for attached joysticks
	JoySettings *joySettings = ReadJoySettings(ini, section, ANY_JOYSTICK);
	if (joySettings != NULL)
		m_joySettings.push_back(joySettings);
	for (int joyNum = 0; joyNum < m_numJoys; joyNum++)
	{
		joySettings = ReadJoySettings(ini, section, joyNum);
		if (joySettings != NULL)
			m_joySettings.push_back(joySettings);
	}
}

void CInputSystem::WriteToINIFile(CINIFile *ini, const char *section)
{
	// Write all key settings
	for (vector<KeySettings*>::iterator it = m_keySettings.begin(); it < m_keySettings.end(); it++)
		WriteKeySettings(ini, section, *it);

	// Write all mouse settings
	for (vector<MouseSettings*>::iterator it = m_mseSettings.begin(); it < m_mseSettings.end(); it++)
		WriteMouseSettings(ini, section, *it);

	// Write all joystick settings
	for (vector<JoySettings*>::iterator it = m_joySettings.begin(); it < m_joySettings.end(); it++)
		WriteJoySettings(ini, section, *it);
}

bool CInputSystem::ReadMapping(char *buffer, unsigned bufSize, bool fullAxisOnly, unsigned readFlags, const char *escapeMapping)
{
	// Map given escape mapping to an input source
	CInputSource *escape = ParseSource(escapeMapping);
	
	string mapping;
	vector<CInputSource*> sources;
	bool mseCentered = false;

	// Loop until have received meaningful inputs
	while (true)
	{
		// Poll inputs
		if (!Poll())
			return false;

		// Check if escape source was triggered
		if (escape != NULL && escape->IsActive())
		{
			// If so, wait until source no longer active and then exit
			while (escape->IsActive())
			{
				if (!Poll())
					return false; 
				Wait(1000/60);
			}
			return false;
		}

		// See if should read keyboards
		if (readFlags & READ_KEYBOARD)
		{
			// Check all keyboard sources for inputs, merging devices if required
			if ((readFlags & READ_MERGE_KEYBOARD) || m_numKbds == ANY_KEYBOARD)
				CheckKeySources(ANY_KEYBOARD, fullAxisOnly, sources, mapping);
			else
			{
				for (int kbdNum = 0; kbdNum < m_numKbds; kbdNum++)
					CheckKeySources(kbdNum, fullAxisOnly, sources, mapping);
			}
		}

		// See if should read mice
		if (readFlags & READ_MOUSE)
		{
			// For mouse input, wait until mouse is in centre of display before parsing X- and Y-axis movements
			if (!mseCentered)
				mseCentered = ConfigMouseCentered();
		
			// Check all mouse sources for input, merging devices if required
			if ((readFlags & READ_MERGE_MOUSE) || m_numMice == ANY_MOUSE)
				CheckMouseSources(ANY_MOUSE, fullAxisOnly, mseCentered, sources, mapping);
			else
			{
				for (int mseNum = 0; mseNum < m_numMice; mseNum++)
					CheckMouseSources(mseNum, fullAxisOnly, mseCentered, sources, mapping);
			}
		}
		
		// See if should read joysticks
		if (readFlags & READ_JOYSTICK)
		{
			// Check all joystick sources, merging devices if required
			if ((readFlags & READ_MERGE_JOYSTICK) || m_numJoys == ANY_JOYSTICK)
				CheckJoySources(ANY_JOYSTICK, fullAxisOnly, sources, mapping);
			else
			{
				for (int joyNum = 0; joyNum < m_numJoys; joyNum++)
					CheckJoySources(joyNum, fullAxisOnly, sources, mapping);
			}
		}

		// When some inputs have been activated, keep lopping until they have all been released again.
		if (sources.size() > 0)
		{
			// Check each source is no longer active
			bool active = false;
			for (vector<CInputSource*>::iterator it = sources.begin(); it < sources.end(); it++)
			{
				if ((*it)->IsActive())
				{
					active = true;
					break;
				}
			}
			if (!active)
			{
				// If so, get combined type of sources and if is valid then return
				ESourceType type = CMultiInputSource::GetCombinedType(sources);
				if (type != SourceInvalid && (type == SourceFullAxis && fullAxisOnly || type != SourceFullAxis && !fullAxisOnly))
					break;

				mapping.clear();
				sources.clear();
				mseCentered = false;
			}	
		}

		// Don't poll continuously
		Wait(1000/60);
	}

	// Copy mapping to buffer and return
	strncpy(buffer, mapping.c_str(), bufSize - 1);
	buffer[bufSize - 1] = '\0';
	return true;
}

void CInputSystem::ConfigStart()
{
	m_isConfiguring = true;

	// Make sure mouse is visible
	SetMouseVisibility(true);
}

void CInputSystem::ConfigEnd()
{
	m_isConfiguring = false;
}

/*
 * CInputSystem::CKeyInputSource
 */
CInputSystem::CKeyInputSource::CKeyInputSource(CInputSystem *system, int kbdNum, int keyIndex, unsigned sensitivity, unsigned decaySpeed) : 
	CInputSource(SourceSwitch), m_system(system), m_kbdNum(kbdNum), m_keyIndex(keyIndex), m_val(0) 
{
	// Calculate max value and incr and decr values (sensitivity is given as percentage 1-100, with 100 being most sensitive, and
	// decay speed given as percentage 1-200 of attack speed)
	int s = Clamp((int)sensitivity, 1, 100);
	int d = Clamp((int)decaySpeed, 1, 200);
	m_incr = 100 * s;
	m_decr = d * s;
	m_maxVal = 10000;
}

bool CInputSystem::CKeyInputSource::GetValueAsSwitch(bool &val)
{
	if (!m_system->IsKeyPressed(m_kbdNum, m_keyIndex))
		return false;
	val = true;
	return true;
}

bool CInputSystem::CKeyInputSource::GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal)
{
	if (m_system->IsKeyPressed(m_kbdNum, m_keyIndex))
		m_val = min<int>(m_maxVal, m_val + m_incr);
	else
		m_val = max<int>(0, m_val - m_decr);
	if (m_val == 0)
		return false;
	val = Scale(m_val, 0, 0, m_maxVal, minVal, offVal, maxVal);
	return true;
}

/*
 * CInputSystem::CMseAxisInputSource
 */
CInputSystem::CMseAxisInputSource::CMseAxisInputSource(CInputSystem *system, int mseNum, int axisNum, int axisDir, unsigned deadZone) : 
	CInputSource(axisDir == AXIS_FULL || axisDir == AXIS_INVERTED ? SourceFullAxis : SourceHalfAxis), 
	m_system(system), m_mseNum(mseNum), m_axisNum(axisNum), m_axisDir(axisDir)
{
	// If X- or Y-axis then calculate size of dead pixels region in centre of display (deadzone is given as a percentage 0-99)
	if (m_axisNum == AXIS_X || m_axisNum == AXIS_Y)
	{
		double dDeadZone = (double)Clamp((int)deadZone, 0, 99) / 100.0;
		m_deadPixels = (int)(dDeadZone * (double)(m_axisNum == AXIS_X ? m_system->m_dispW : m_system->m_dispH));
	}
	else
		m_deadPixels = Clamp((int)deadZone, 0, 99);
}

int CInputSystem::CMseAxisInputSource::ScaleAxisValue(int minVal, int offVal, int maxVal)
{
	int mseVal = m_system->GetMouseAxisValue(m_mseNum, m_axisNum);
	// If X- or Y-axis then convert to value centered around zero (ie relative to centre of display)
	int mseMin, mseMax;
	if (m_axisNum == AXIS_X || m_axisNum == AXIS_Y)
	{
		int dispExtent = (int)(m_axisNum == AXIS_X ? m_system->m_dispW : m_system->m_dispH);
		if (dispExtent == 0)
			return offVal;
		mseMin = -dispExtent / 2;
		mseMax = mseMin + dispExtent - 1;
		mseVal = (m_axisNum == AXIS_X ? mseVal - m_system->m_dispX : mseVal - m_system->m_dispY) - dispExtent / 2;
	}
	else
	{
		// Z-axis (wheel) is always between -100 and 100
		mseMin = -100;
		mseMax = 100;
	}
	// Check value is not zero
	if (mseVal == 0)
		return offVal;
	// Scale values from dead zone to display edge, taking positive or negative values only or using the whole axis range as required
	int dZone = (mseVal > 0 ? m_deadPixels / 2 : m_deadPixels - m_deadPixels / 2);
	if      (m_axisDir == AXIS_POS)  return Scale(mseVal, dZone, dZone, mseMax, minVal, offVal, maxVal);
	else if (m_axisDir == AXIS_NEG)  return Scale(mseVal, -dZone, -dZone, mseMin, minVal, offVal, maxVal);
	else if (m_axisDir == AXIS_FULL)
	{
		// Full axis range
		if (mseVal > 0) return Scale(mseVal, dZone, dZone, mseMax, minVal, offVal, maxVal);
		else            return Scale(mseVal, mseMin, -dZone, -dZone, minVal, offVal, maxVal);
	}
	else
	{
		// Full axis range, but inverted
		if (mseVal > 0) return Scale(mseVal, dZone, dZone, mseMax, maxVal, offVal, minVal);
		else            return Scale(mseVal, mseMin, -dZone, -dZone, maxVal, offVal, minVal);
	}
}

bool CInputSystem::CMseAxisInputSource::GetValueAsSwitch(bool &val)
{
	// For Z-axis (wheel), switch value is handled slightly differently
	if (m_axisNum == AXIS_Z)
	{
		int wheelDir = m_system->GetMouseWheelDir(m_mseNum);
		if ((m_axisDir == AXIS_POS || m_axisDir == AXIS_FULL)     && wheelDir <= 0 ||
			(m_axisDir == AXIS_NEG || m_axisDir == AXIS_INVERTED) && wheelDir >= 0)
			return false;
	}
	else
	{
		if (ScaleAxisValue(0, 0, 3) < 2)
			return false;
	}
	val = true;
	return true;
}

bool CInputSystem::CMseAxisInputSource::GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal)
{
	int axisVal = ScaleAxisValue(minVal, offVal, maxVal);
	if (axisVal == offVal)
		return false;
	val = axisVal;
	return true;
}

/*
 * CInputSystem::CMseButInputSource
 */
CInputSystem::CMseButInputSource::CMseButInputSource(CInputSystem *system, int mseNum, int butNum) :
	CInputSource(SourceSwitch), m_system(system), m_mseNum(mseNum), m_butNum(butNum)
{
	//
}

bool CInputSystem::CMseButInputSource::GetValueAsSwitch(bool &val)
{
	if (!m_system->IsMouseButPressed(m_mseNum, m_butNum))
		return false;
	val = true;
	return true;
}

bool CInputSystem::CMseButInputSource::GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal)
{
	if (!m_system->IsMouseButPressed(m_mseNum, m_butNum))
		return false;
	val = maxVal;
	return true;
}

/*
 * CInputSystem::CJoyAxisInputSource
 */
CInputSystem::CJoyAxisInputSource::CJoyAxisInputSource(CInputSystem *system, int joyNum, int axisNum, int axisDir, unsigned deadZone, unsigned saturation) : 
	CInputSource(axisDir == AXIS_FULL || axisDir == AXIS_INVERTED ? SourceFullAxis : SourceHalfAxis),
	m_system(system), m_joyNum(joyNum), m_axisNum(axisNum), m_axisDir(axisDir)
{
	// Calculate pos/neg deadzone and saturation points (joystick raw values range from -32768 to 32767, deadzone given as 
	// percentage 0-99 and saturation given as percentage 1-100)
	double dDeadZone = (double)Clamp((int)deadZone, 0, 99) / 100.0;
	double dSaturation = (double)Clamp((int)saturation, (int)deadZone + 1, 100) / 100.0;
	m_posDZone = (int)(dDeadZone * 32767.0);
	m_negDZone = (int)(dDeadZone * -32768.0);
	m_posSat = (int)(dSaturation * 32767.0);
	m_negSat = (int)(dSaturation * -32868.0);
}

int CInputSystem::CJoyAxisInputSource::ScaleAxisValue(int minVal, int offVal, int maxVal)
{
	// Get raw axis value from input system (values range from -32768 to 32767)
	int joyVal = m_system->GetJoyAxisValue(m_joyNum, m_axisNum);
	// Check value is not zero
	if (joyVal == 0)
		return offVal;
	// Scale value between deadzone and saturation points, taking positive or negative values only or using the whole axis range as required
	if      (m_axisDir == AXIS_POS)  return Scale(joyVal, m_posDZone, m_posDZone, m_posSat, minVal, offVal, maxVal);
	else if (m_axisDir == AXIS_NEG)  return Scale(joyVal, m_negDZone, m_negDZone, m_negSat, minVal, offVal, maxVal);
	else if (m_axisDir == AXIS_FULL)
	{
		// Full axis range
		if (joyVal > 0) return Scale(joyVal, m_posDZone, m_posDZone, m_posSat, minVal, offVal, maxVal);
		else            return Scale(joyVal, m_negSat, m_negDZone, m_negDZone, minVal, offVal, maxVal);
	}
	else
	{
		// Full axis range, but inverted
		if (joyVal > 0) return Scale(joyVal, m_posDZone, m_posDZone, m_posSat, maxVal, offVal, minVal);
		else            return Scale(joyVal, m_negSat, m_negDZone, m_negDZone, maxVal, offVal, minVal);
	}
}

bool CInputSystem::CJoyAxisInputSource::GetValueAsSwitch(bool &val)
{
	if (ScaleAxisValue(0, 0, 3) < 2)
		return false;
	val = true;
	return true;
}

bool CInputSystem::CJoyAxisInputSource::GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal)
{
	int axisVal = ScaleAxisValue(minVal, offVal, maxVal);
	if (axisVal == offVal)
		return false;
	val = axisVal;
	return true;
}

/*
 * CInputSystem::CJoyPOVInputSource
 */
CInputSystem::CJoyPOVInputSource::CJoyPOVInputSource(CInputSystem *system, int joyNum, int povNum, int povDir) :
	CInputSource(SourceSwitch), m_system(system), m_joyNum(joyNum), m_povNum(povNum), m_povDir(povDir)
{
	//
}

bool CInputSystem::CJoyPOVInputSource::GetValueAsSwitch(bool &val)
{
	if (!m_system->IsJoyPOVInDir(m_joyNum, m_povNum, m_povDir))
		return false;
	val = true;
	return true;
}

bool CInputSystem::CJoyPOVInputSource::GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal)
{
	if (!m_system->IsJoyPOVInDir(m_joyNum, m_povNum, m_povDir))
		return false;
	val = maxVal;
	return true;
}
	
/*
 * CInputSystem::CJoyButInputSource
 */
CInputSystem::CJoyButInputSource::CJoyButInputSource(CInputSystem *system, int joyNum, int butNum) :
	CInputSource(SourceSwitch), m_system(system), m_joyNum(joyNum), m_butNum(butNum)
{
	//
}

bool CInputSystem::CJoyButInputSource::GetValueAsSwitch(bool &val)
{
	if (!m_system->IsJoyButPressed(m_joyNum, m_butNum))
		return false;
	val = true;
	return true;
}

bool CInputSystem::CJoyButInputSource::GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal)
{
	if (!m_system->IsJoyButPressed(m_joyNum, m_butNum))
		return false;
	val = maxVal;
	return true;
}