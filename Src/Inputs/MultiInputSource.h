#ifndef INCLUDED_MULTIINPUTSOURCE_H
#define INCLUDED_MULTIINPUTSOURCE_H

#include "InputSource.h"

#include <vector>
using namespace std;

/*
 * Represents a collection of input sources and combines their values into a single value.
 * When multiple mappings are assigned to an input, this is the input source that is created.
 * Can represent either a combination of multiple assignments that all map to the same input, eg KEY_ALT,JOY1_BUTTON1 or to
 * specify that controls must be combined together, eg KEY_ALT+KEY_P.
 */
class CMultiInputSource : public CInputSource
{
private:
	// Controls how the inputs sources are combined
	bool m_isOr;

	// Number of input sources (if zero then represents an 'empty' source)
	int m_numSrcs;

	// Array of the input sources
	CInputSource **m_srcArray;

public:
	/*
	 * Returns the combined source type of the given vector of sources.
	 */ 
	static ESourceType GetCombinedType(vector<CInputSource*> &sources);

	/*
	 * Constructs an 'empty' source (ie one which is always 'off').
	 */
	CMultiInputSource();

	/*
	 * Constructs a multiple input source from the given vector of sources.
	 * If isOr is true, then the value of this input will always be the value of the first active input found.  If false, then all
	 * switch inputs must be active for this input to have a value (which will be the value of the first non-switch input in the list,
	 * or the first switch input if there are none).
	 */
	CMultiInputSource(bool isOr, vector<CInputSource*> &sources);

	~CMultiInputSource();
	
	void Acquire();

	void Release();

	bool GetValueAsSwitch(bool &val);

	bool GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal);	

	bool SendForceFeedbackCmd(ForceFeedbackCmd ffCmd);
};

/*
 * Represents a negation of an input source - ie is active when the given source is inactive and vice-versa.
 * Can be used to specify that a particular input must not be active when used in multiple assignments, eg !KEY_ALT+KEY_P.  This helps
 * to get rid of collisions that might otherwise occur.
 */
class CNegInputSource : public CInputSource
{
private:
	// Input source being negated
	CInputSource *m_source;

public:
	CNegInputSource(CInputSource *source);

	void Acquire();

	void Release();

	bool GetValueAsSwitch(bool &val);

	bool GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal);
};

#endif	// INCLUDED_MULTIINPUTSOURCE_H