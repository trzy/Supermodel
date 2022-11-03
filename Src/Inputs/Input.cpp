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
  * Input.cpp
  *
  * Implementation of CInput, the base input class. Input types are derived
  * from this.
  */

#include "Input.h"

#include "Supermodel.h"
#include "InputSystem.h"

CInput::CInput(const char *inputId, const char *inputLabel, unsigned inputFlags, unsigned inputGameFlags, const char *defaultMapping, UINT16 initValue) : 
	id(inputId), label(inputLabel), flags(inputFlags), gameFlags(inputGameFlags), m_defaultMapping(defaultMapping), value(initValue), prevValue(initValue),
	m_system(NULL), m_source(NULL)
{
	ResetToDefaultMapping();
}

CInput::~CInput()
{
	// Release source, if any
	if (m_source != NULL)
		m_source->Release();
}

void CInput::CreateSource()
{
	// If already have a source, then release it now
	if (m_source != NULL)
		m_source->Release();

	// If no system set yet or mapping is empty or NONE, then set source to NULL
	if (m_system == NULL || m_mapping[0] == '\0' || stricmp(m_mapping, "NONE") == 0)
		m_source = NULL;
	else
	{
		// Otherwise, ask system to parse mapping into appropriate input source
		m_source = m_system->ParseSource(m_mapping, !!(flags & INPUT_FLAGS_AXIS));

		// Check that mapping was parsed okay and if so acquire it
		if (m_source != NULL)
			m_source->Acquire();
		else
		{
			// Otherwise, fall back to default mapping
			if (stricmp(m_mapping, m_defaultMapping) != 0)
			{
				ErrorLog("Unable to map input %s to [%s] - switching to default [%s].\n", id, m_mapping, m_defaultMapping);

				ResetToDefaultMapping();
			}
		}
	}
}

void CInput::Initialize(CInputSystem *system)
{
	m_system = system;

	CreateSource();
}

const char* CInput::GetInputGroup()
{
	switch (gameFlags)
	{
		case Game::INPUT_UI:              return "User Interface Controls";
		case Game::INPUT_COMMON:          return "Common Controls";
		case Game::INPUT_JOYSTICK1:       // Fall through to below
		case Game::INPUT_JOYSTICK2:       return "4-Way Joysticks";
		case Game::INPUT_FIGHTING:        return "Fighting Game Buttons";
		case Game::INPUT_SPIKEOUT:        return "Spikeout Buttons";
		case Game::INPUT_SOCCER:          return "Virtua Striker Buttons";
		case Game::INPUT_VEHICLE:         return "Racing Game Steering Controls";
		case Game::INPUT_SHIFT4:          return "Racing Game Gear 4-Way Shift";
		case Game::INPUT_SHIFTUPDOWN:     return "Racing Game Gear Up/Down Shift";
		case Game::INPUT_VR4:             return "Racing Game 4 VR View Buttons";
		case Game::INPUT_VIEWCHANGE:      return "Racing Game View Change";
		case Game::INPUT_HANDBRAKE:       return "Racing Game Handbrake";
		case Game::INPUT_HARLEY:          return "Harley Davidson Controls";
		case Game::INPUT_TWIN_JOYSTICKS:  return "Virtual On Controls";
		case Game::INPUT_ANALOG_JOYSTICK: return "Analog Joystick";
		case Game::INPUT_GUN1:            // Fall through to below
		case Game::INPUT_GUN2:            return "Light Guns";
		case Game::INPUT_ANALOG_GUN1:     // Fall through to below
		case Game::INPUT_ANALOG_GUN2:     return "Analog Guns";
		case Game::INPUT_SKI:             return "Ski Controls";
		case Game::INPUT_MAGTRUCK:        return "Magical Truck Controls";
		case Game::INPUT_FISHING:         return "Fishing Controls";
		default:                          return "Misc";
	}
}

const char *CInput::GetMapping()
{
	return m_mapping;
}

void CInput::ClearMapping()
{
	SetMapping("NONE");
}

void CInput::SetMapping(const char *mapping)
{
	strncpy(m_mapping, mapping, MAX_MAPPING_LENGTH);
	m_mapping[MAX_MAPPING_LENGTH] = '\0';
	CreateSource();
}

void CInput::AppendMapping(const char *mapping)
{
	// If mapping is empty or NONE, then simply set mapping
	if (m_mapping[0] == '\0' || stricmp(m_mapping, "NONE") == 0)
		SetMapping(mapping);
	else
	{
		// Otherwise, append to mapping string and recreate source from new mapping string
		size_t size = MAX_MAPPING_LENGTH - strlen(m_mapping);
		strncat(m_mapping, ",", size--);
		strncat(m_mapping, mapping, size);
		CreateSource();
	}
}

void CInput::ResetToDefaultMapping()
{
	SetMapping(m_defaultMapping);
}

void CInput::LoadFromConfig(const Util::Config::Node &config)
{
	// See if input is configurable
	if (IsConfigurable())
	{
		// If so, check INI file for mapping string
		std::string key("Input");
		key.append(id);
		std::string mapping;
		auto *node = config.TryGet(key);
		if (node)
		{
			// If found, then set mapping string
			mapping = node->ValueAs<std::string>();
			SetMapping(mapping.c_str());
			return;
		}
	}

	// If input has not been configured, then force recreation of source anyway since input system settings may have changed 
	CreateSource();
}

void CInput::StoreToConfig(Util::Config::Node *config)
{
	if (!IsConfigurable())
		return;
	std::string key("Input");
	key.append(id);
  config->Set(key, m_mapping);
}

void CInput::InputSystemChanged()
{
	// If input system or its settings have changed, then force recreation of source
	CreateSource();
}

bool CInput::Configure(bool append, const char *escapeMapping)
{
	char mapping[MAX_MAPPING_LENGTH];
	if (!m_system->ReadMapping(mapping, MAX_MAPPING_LENGTH, !!(flags & INPUT_FLAGS_AXIS), READ_ALL, escapeMapping))
		return false;
	if (append)
		AppendMapping(mapping);
	else
		SetMapping(mapping);
	return true;
}

bool CInput::Changed()
{
	return value != prevValue;
}

bool CInput::SendForceFeedbackCmd(ForceFeedbackCmd ffCmd)
{
	if (m_source == NULL)
		return false;
	return m_source->SendForceFeedbackCmd(ffCmd);
}