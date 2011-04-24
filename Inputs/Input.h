#ifndef INCLUDED_INPUT_H
#define INCLUDED_INPUT_H

#include "Types.h"

class CInputSource;
class CInputSystem;

// Flags for inputs
#define INPUT_FLAGS_SWITCH  0x0001
#define INPUT_FLAGS_ANALOG  0x0002
#define INPUT_FLAGS_AXIS    0x0004
#define INPUT_FLAGS_VIRTUAL 0x0008

#define MAX_MAPPING_LENGTH 255

/*
 * Base class for any type of Model3 input control.
 */
class CInput
{
private:
	// Current mapping(s) for input, eg JOY1_XAXIS_POS
	char m_mapping[MAX_MAPPING_LENGTH];
	
	// Default mapping for input
	const char *m_defaultMapping;
	
	// Assigned input system
	CInputSystem *m_system;

	/*
	 * Creates an input source using the current input system and assigns it to this input.
	 */
	void CreateSource();

protected:
	// Current input source
	CInputSource *m_source;

public:
	// Input identifier
	const char *id;

	// Input label
	const char *label;

	// Input flags
	unsigned flags;

	// Input game flags
	unsigned gameFlags;

	// Current input value
	UINT16 value;

	// Previous input value
	UINT16 prevValue;

	/*
	 * Constructs an input with the given identifier, label, flags, game flags, default mapping and initial value.
	 */ 
	CInput(const char *inputId, const char *inputLabel, unsigned inputFlags, unsigned inputGameFlags, 
		const char *defaultMapping = "NONE", UINT16 initValue = 0);
	
	/*
	 * Initializes this input with the given input system.  Must be called before any other methods are used.
	 */
	void Initialize(CInputSystem *system);

	/*
	 * Returns the name of the group of controls that this input belongs to.
	 */
	const char* GetInputGroup();

	/*
	 * Returns the current mapping(s) assigned to this input.
	 */
	const char* GetMapping();

	/*
	 * Clears the current mapping(s) assigned to this input.
	 */
	void ClearMapping();

	/*
	 * Sets the current mapping(s) assigned to this input.  Multiple mapping assignments are comma-separated, eg KEY_RIGHT,JOY1_XAXIS_POS
	 */
	void SetMapping(const char *mapping);

	/*
	 * Appends a mapping to this input to create a multiple mapping assignment.
	 */
	void AppendMapping(const char *mapping);

	/*
	 * Resets the mapping(s) assigned to this input to the input's default.
	 */
	void ResetToDefaultMapping();
	
	/*
	 * Configures the current mapping(s) assigned to this input by asking the user for input.
	 * If append is true, then the user's selected mapping is appended.  Otherwise, it overwrites the existing mapping(s).
	 * escapeMapping holds the input mapping used to exit the configuration without applying the changes.
	 */
	bool Configure(bool append, const char *escapeMapping = "KEY_ESCAPE");

	/*
	 * Polls (updates) this input, updating its value from the input source
	 */
	virtual void Poll() = 0;
};

#endif	// INCLUDED_INPUT_H
