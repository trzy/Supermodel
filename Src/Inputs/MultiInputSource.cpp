#include "Supermodel.h"

#include <vector>
using namespace std;

ESourceType CMultiInputSource::GetCombinedType(vector<CInputSource*> &sources)
{
	// Check if vector is empty
	if (sources.size() == 0)
		return SourceEmpty;
	// Otherwise, see whether all sources are switches, or if have a full- or half-axis present
	bool allSwitches = true;
	bool hasFullAxis = false;
	bool hasHalfAxis = false;
	for (vector<CInputSource*>::iterator it = sources.begin(); it < sources.end(); it++)
	{
		if ((*it)->type == SourceInvalid)
			return SourceInvalid;  // An invalid source makes the whole lot invalid
		else if ((*it)->type == SourceSwitch)
			continue;
		allSwitches = false;
		if ((*it)->type == SourceFullAxis)
		{
			if (hasHalfAxis)
				return SourceInvalid;  // A half-axis and full-axis combined makes the whole lot invalid
			hasFullAxis = true;
		}
		else if ((*it)->type == SourceHalfAxis)
		{
			if (hasFullAxis)
				return SourceInvalid;  // A half-axis and full-axis combined makes the whole lot invalid
			hasHalfAxis = true;
		}
	}
	// Return resulting combined type
	if      (allSwitches) return SourceSwitch;
	else if (hasFullAxis) return SourceFullAxis;
	else if (hasHalfAxis) return SourceHalfAxis;
	else                  return SourceEmpty;
}

CMultiInputSource::CMultiInputSource() : CInputSource(SourceEmpty), m_isOr(true), m_numSrcs(0), m_srcArray(NULL) { }

CMultiInputSource::CMultiInputSource(bool isOr, vector<CInputSource*> &sources) : 
	CInputSource(GetCombinedType(sources)), m_isOr(isOr), m_numSrcs(sources.size())
{
	m_srcArray = new CInputSource*[m_numSrcs];
	copy(sources.begin(), sources.end(), m_srcArray);
}

CMultiInputSource::~CMultiInputSource()
{
	if (m_srcArray != NULL)
		delete m_srcArray;
}

bool CMultiInputSource::GetValueAsSwitch(bool &val)
{
	if (m_isOr)
	{
		// Return value for first input that is active
		for (int i = 0; i < m_numSrcs; i++)
		{
			if (m_srcArray[i]->GetValueAsSwitch(val))
				return true;
		}
		return false;
	}
	else
	{
		// Check all switch inputs are active
		for (int i = 0; i < m_numSrcs; i++)
		{
			if (m_srcArray[i]->type == SourceSwitch && !m_srcArray[i]->GetValueAsSwitch(val))
				return false;
		}
		// Then return value for first non-switch input that is active
		for (int i = 0; i < m_numSrcs; i++)
		{
			if (m_srcArray[i]->type != SourceSwitch && m_srcArray[i]->GetValueAsSwitch(val))
				return true;
		}
		// Otherwise, value is only valid if not empty and all inputs are switches 
		return m_numSrcs > 0 && type == SourceSwitch;
	}
}

bool CMultiInputSource::GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal)
{
	if (m_isOr)
	{
		// Return value for first input that is active
		for (int i = 0; i < m_numSrcs; i++)
		{
			if (m_srcArray[i]->GetValueAsAnalog(val, minVal, offVal, maxVal))
				return true;
		}
		return false;
	}
	else
	{
		// Check all switch inputs are active
		for (int i = 0; i < m_numSrcs; i++)
		{
			if (m_srcArray[i]->type == SourceSwitch && !m_srcArray[i]->GetValueAsAnalog(val, minVal, offVal, maxVal))
				return false;
		}
		// Then return value for first non-switch input that is active
		for (int i = 0; i < m_numSrcs; i++)
		{
			if (m_srcArray[i]->type != SourceSwitch && m_srcArray[i]->GetValueAsAnalog(val, minVal, offVal, maxVal))
				return true;
		}
		// Otherwise, value is only valid if not empty and all inputs are switches 
		return m_numSrcs > 0 && type == SourceSwitch;
	}
}