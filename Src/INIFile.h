/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
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
 * INIFile.h
 * 
 * Header file for INI file management.
 */

#ifndef INCLUDED_INIFILE_H
#define INCLUDED_INIFILE_H

// Standard C++ and STL headers
#include <fstream>
#include <string>
#include <vector>
using namespace std;

/*
 * CINIFile:
 *
 * INI file parser.
 */
class CINIFile
{
public:
	/*
	 * Get(SectionName, SettingName, value):
	 * Get(SectionName, SettingName, String):
	 *
	 * Obtains the value of a setting associated with a particular section. It
	 * is assumed that the caller knows whether the setting should be a string
	 * or an integer value.  If the setting was specified as a string in the
	 * INI file, the value will be set to 0. Otherwise, the string will be set
	 * to "".
	 *
	 * Parameters:
	 *		SectionName		String defining the section name.
	 *		SettingName		String defining the setting name.
	 *		value			Reference to where the setting value will be
	 *						copied.
	 *		String			Reference to where the string will be copied.
	 *
	 * Returns:
	 *		OKAY if the setting was found, FAIL otherwise. The type is not 
	 *		checked.
	 */
	BOOL	Get(string SectionName, string SettingName, int& value);
	BOOL	Get(string SectionName, string SettingName, unsigned& value);
	BOOL	Get(string SectionName, string SettingName, string& String);
	
	/*
	 * Set(SectionName, SettingName, value):
	 * Set(SectionName, SettingName, String):
	 *
	 * Sets a setting associated with a particular section. For each overload
	 * type (string or integer), the opposite type is cleard (0 is written to
	 * values when a string is set, "" is written to strings when a value is
	 * set). If the setting does not exist, it is created. If the section does
	 * not exist, it will be added as well.
	 *
	 * Parameters:
	 *		SectionName		String defining the section name.
	 *		SettingName		String defining the setting name.
	 *		value			Value to write. String will be set to "".
	 *		String			String to write. Value will be set to 0.
	 */
	void 	Set(string SectionName, string SettingName, int value);
	void 	Set(string SectionName, string SettingName, string String);
	
	/*
	 * Write(comment):
	 *
	 * Outputs the parse tree to the INI file. The current contents of the file
	 * on the disk are discarded and overwritten. This means that comments are
	 * lost.
	 *
	 * Parameters:
	 *		comment		An optional C-string containing a comment to insert at
	 *					the beginning of the file, otherwise NULL. The comment
	 *					lines must include semicolons at the beginning, they
	 *					will not be inserted automatically. The string is
	 *					output directly, as-is.
	 *
	 * Returns:
	 *		OKAY if successful. FAIL if an error occurred at any point. In
	 * 		order to truncate the original contents of the file, this function
	 *		closes the file, reopends it as write-only, then reopens it again
	 *		as a read/write file. If an error occurs during the reopening 
	 *		procedure, it is possible that nothing will be output and the
	 *		previous contents will be lost.
	 */
	BOOL	Write(const char *comment);
	
	/*
	 * Parse(void):
	 *
	 * Parses the contents of the file, building the parse tree. Settings 
	 * already present in the parse tree will continue to exist and will be
	 * overwritten if new, matching settings are found in the file.
	 *
	 * Returns:
	 */
	BOOL	Parse(void);
	
	/*
	 * Open(fileNameStr):
	 *
	 * Opens an INI file.
	 *
	 * Parameters:
	 *		fileNameStr		File path (C string).
	 *
	 * Returns:
	 *		OKAY if successful, FAIL if unable to open for reading and writing.
	 */
	BOOL	Open(const char *fileNameStr);
	
	/*
	 * Close(void):
	 *
	 * Closes the INI file and clears the parse tree.
	 */
	void	Close(void);
	
private:
	// Token
	class CToken
	{
	public:
		int			type;	// token type (defined privately in INIFile.cpp)
		int         number;	// numbers and bools
		string		String;	// strings and identifiers
	
		// Constructor (initialize to null token)
		CToken(void);
	};

	// Searching/modification of settings
	BOOL	LookUpSection(unsigned *idx, string SectionName);
	BOOL 	LookUpSetting(unsigned *idx, unsigned sectionIdx, string SettingName);
	
	// Tokenizer
	CToken	GetString(void);
	CToken	GetNumber(void);
	CToken	GetIdentifier(void);
	CToken	GetToken(void);
	
	// File state
	fstream		File;
	string		FileName;		// name of last file opened
	
	// Parser state
	char		lineBuf[2048];	// holds current line
	char		*linePtr;		// points to current position within line (for tokenization)
	unsigned	lineNum;		// line number
	
	// Parse tree: a list of sections each of which is a list of settings for that section
	struct Setting	// it is up to caller to determine whether to use value or string
	{
		string		Name;		// setting name
		BOOL		isNumber;	// internal flag: true if the setting is a number, false if it is a string
		int      	value;		// value of number
		string		String;		// string
		
		Setting(void)
		{
			value = 0;			// initialize value to 0
			isNumber = TRUE;	// indicate the setting is initially a number
		}
	};
	struct Section
	{
		string					Name;		// section name
		vector<struct Setting>	Settings;	// list of settings associated w/ this section
	};
	vector<struct Section>		Sections;	// a list of sections
};


#endif	// INCLUDED_INIFILE_H
