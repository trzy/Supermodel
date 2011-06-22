#include "Supermodel.h"

#include <vector>
using namespace std;

CInputSource::CInputSource(ESourceType sourceType) : type(sourceType), m_acquired(0)
{
	//
}

void CInputSource::Acquire()
{
	m_acquired++;

#ifdef DEBUG
	CInputSystem::totalSrcsAcquired++;
#endif
}

void CInputSource::Release()
{
	if (--m_acquired == 0)
		delete this;

#ifdef DEBUG
	CInputSystem::totalSrcsReleased++;
#endif
}

int CInputSource::Clamp(int val, int minVal, int maxVal)
{
	if      (val > maxVal) return maxVal;
	else if (val < minVal) return minVal;
	else                   return val;
}

int CInputSource::Scale(int val, int fromMinVal, int fromMaxVal, int toMinVal, int toMaxVal)
{
	return Scale(val, fromMinVal, fromMinVal, fromMaxVal, toMinVal, toMinVal, toMaxVal);
}

int CInputSource::Scale(int val, int fromMinVal, int fromOffVal, int fromMaxVal, int toMinVal, int toOffVal, int toMaxVal)
{
	double fromRange;
	double frac;
	if (fromMaxVal > fromMinVal)
	{
		val = Clamp(val, fromMinVal, fromMaxVal);
		if (val > fromOffVal)
		{
			fromRange = (double)(fromMaxVal - fromOffVal);
			frac = (double)(val - fromOffVal) / fromRange;
		}
		else if (val < fromOffVal)
		{
			fromRange = (double)(fromOffVal - fromMinVal);
			frac = (double)(val - fromOffVal) / fromRange;
		}
		else
			return toOffVal;
	}
	else if (fromMinVal > fromMaxVal)
	{
		val = Clamp(val, fromMaxVal, fromMinVal);
		if (val > fromOffVal)
		{
			fromRange = (double)(fromMinVal - fromOffVal);
			frac = (double)(fromOffVal - val) / fromRange;
		}
		else if (val < fromOffVal)
		{
			fromRange = (double)(fromOffVal - fromMaxVal);
			frac = (double)(fromOffVal - val) / fromRange;
		}
		else
			return toOffVal;
	}
	else
		return toOffVal;
	double toRange;
	if (toMaxVal > toMinVal)
	{
		if (frac >= 0)
			toRange = (double)(toMaxVal - toOffVal);
		else
			toRange = (double)(toOffVal - toMinVal);
		return toOffVal + (int)(toRange * frac); 
	}
	else
	{
		if (frac >= 0)
			toRange = (double)(toOffVal - toMaxVal);
		else
			toRange = (double)(toMinVal - toOffVal);
		return toOffVal - (int)(toRange * frac); 
	}
}

bool CInputSource::IsActive()
{
	bool boolVal;
	return GetValueAsSwitch(boolVal);
}

bool CInputSource::SendForceFeedbackCmd(ForceFeedbackCmd ffCmd)
{
	return false;
}