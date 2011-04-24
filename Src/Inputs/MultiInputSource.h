#ifndef INCLUDED_MULTIINPUTSOURCE_H
#define INCLUDED_MULTIINPUTSOURCE_H

#include "InputSource.h"

#include <vector>
using namespace std;
/*
 * Represents a collection of input sources and combines their values into a single value.
 * When multiple mappings are assigned to an input, this is the input source that is created.
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

	bool GetValueAsSwitch(bool &val);

	bool GetValueAsAnalog(int &val, int minVal, int offVal, int maxVal);
};

#endif	// INCLUDED_MULTIINPUTSOURCE_H