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
 * Outputs.cpp
 */

#include "Outputs.h"

#include "Supermodel.h"

const char *COutputs::s_outputNames[] =
	{ 
		"pause",
		"LampStart", 
		"LampView1",
		"LampView2",
		"LampView3",
		"LampView4",
		"LampLeader",
		"RawDrive",
		"RawLamps",
		"BillDigit1",
		"BillDigit2",
		"BillDigit3",
		"BillDigit4",
		"BillDigit5"
	};

const char *COutputs::GetOutputName(EOutputs output)
{
	int idx = (int)output;
	if (idx < 0 || idx >= NUM_OUTPUTS)
		return NULL;
	return s_outputNames[idx];
}

EOutputs COutputs::GetOutputByName(const char *name)
{
	for (unsigned i = 0; i < NUM_OUTPUTS; i++)
	{
		if (stricmp(name, s_outputNames[i]) == 0)
			return (EOutputs)i;
	}
	return OutputUnknown;
}

COutputs::COutputs()
{
	memset(m_first, true, sizeof(m_first));
	memset(m_values, 0, sizeof(m_values));
}

COutputs::~COutputs()
{
	//
}

const Game &COutputs::GetGame() const
{
	return m_game;
}

void COutputs::SetGame(const Game &game)
{
	m_game = game;
}

UINT8 COutputs::GetValue(EOutputs output) const
{
	int idx = (unsigned)output;
	if (idx < 0 || idx >= NUM_OUTPUTS)
		return 0;
	return m_values[idx];
}

void COutputs::SetValue(EOutputs output, UINT8 value)
{
	int idx = (unsigned)output;
	if (idx < 0 || idx >= NUM_OUTPUTS)
		return;
	bool firstSet = m_first[idx];
	UINT8 prevValue = m_values[idx];
	m_first[idx] = false;
	m_values[idx] = value;
	if (firstSet || value != prevValue)
		SendOutput(output, prevValue, value);
}
