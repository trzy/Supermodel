/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2020 Bart Trzynadlowski, Nik Henson, Ian Curtis,
 **                     Harry Tuttle, and Spindizzi
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
 * Logger.h
 *
 * Header file for message logging. The OSD code is expected to set up a
 * default logger (CFileLogger).
 */

#ifndef INCLUDED_LOGGER_H
#define INCLUDED_LOGGER_H

#include "Types.h"
#include "Version.h"
#include "Util/NewConfig.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>


/******************************************************************************
 Class Definitions
******************************************************************************/

/*
 * CLogger
 *
 * Abstract class that receives log messages from Supermodel's log functions.
 * The logger object handles actual output of messages. Use the function-based
 * message logging interface to generate messages.
 */
class CLogger
{
public:
	// Log level in ascending order
	enum LogLevel: int
	{
		All = 0,
		Debug,
		Info,
		Error
	};

	virtual ~CLogger()
  	{
  	}

	/*
	 * DebugLog(fmt, ...):
	 * DebugLog(fmt, vl):
	 *
	 * Prints to debug log. If DEBUG is not defined, will end up doing nothing.
	 *
	 * Parameters:
	 *		fmt		printf()-style format string.
	 *		...		Variable number of parameters, corresponding to format
	 *				string.
	 *		vl		Variable arguments already stored in a list.
	 */
	void DebugLog(const char *fmt, ...)
	{
		va_list vl;
		va_start(vl, fmt);
		DebugLog(fmt, vl);
		va_end(vl);
	}

	virtual void DebugLog(const char *fmt, va_list vl) = 0;

	/*
	 * InfoLog(fmt, ...):
	 * InfoLog(fmt, vl):
	 *
	 * Prints to error log but does not output an error to stderr. This is
	 * useful for printing session information to the error log.
	 *
	 * Parameters:
	 *		fmt		printf()-style format string.
	 *		...		Variable number of parameters, corresponding to format
	 *				string.
	 *		vl		Variable arguments already stored in a list.
	 */
	void InfoLog(const char *fmt, ...)
	{
		va_list vl;
		va_start(vl, fmt);
		InfoLog(fmt, vl);
		va_end(vl);
	}

	virtual void InfoLog(const char *fmt, va_list vl) = 0;

	/*
	 * ErrorLog(fmt, ...):
	 * ErrorLog(fmt, vl):
	 *
	 * Prints to error log and outputs an error message to stderr.
	 *
	 * Parameters:
	 *		fmt		printf()-style format string.
	 *		...		Variable number of parameters, corresponding to format
	 *				string.
	 *		vl		Variable arguments already stored in a list.
	 */
	void ErrorLog(const char *fmt, ...)
	{
		va_list vl;
		va_start(vl, fmt);
		ErrorLog(fmt, vl);
		va_end(vl);
	}

	virtual void ErrorLog(const char *fmt, va_list vl) = 0;
};

/*
 * CMultiLogger:
 *
 * Redirects to multiple loggers.
 */
class CMultiLogger: public CLogger
{
public:
  void DebugLog(const char *fmt, va_list vl);
  void InfoLog(const char *fmt, va_list vl);
  void ErrorLog(const char *fmt, va_list vl);
  CMultiLogger(std::vector<std::shared_ptr<CLogger>> loggers);

private:
  std::vector<std::shared_ptr<CLogger>> m_loggers;
};

/*
 * CConsoleErrorLogger:
 *
 * Logger that prints only essential error messages, formatted appropriately,
 * to the console (stderr).
 *
 * In the future, logging and console output need to be separated. This is
 * intended to be an interrim solution.
 */
class CConsoleErrorLogger: public CLogger
{
  void DebugLog(const char *fmt, va_list vl);
  void InfoLog(const char *fmt, va_list vl);
  void ErrorLog(const char *fmt, va_list vl);
};

/*
 * CFileLogger:
 *
 * Default logger that logs to debug and error log files. Files are opened and
 * closed for each message in order to preserve contents in case of program
 * crash.
 */
class CFileLogger: public CLogger
{
public:
  void DebugLog(const char *fmt, va_list vl);
  void InfoLog(const char *fmt, va_list vl);
  void ErrorLog(const char *fmt, va_list vl);
	CFileLogger(LogLevel level, std::vector<std::string> filenames);
  CFileLogger(LogLevel level, std::vector<std::string> filenames, std::vector<FILE *> systemFiles);

private:
	std::mutex m_mtx; // needed because we may close/reopen files and logging must be thread-safe
  LogLevel m_logLevel;
	const std::vector<std::string> m_logFilenames;
  std::vector<std::ofstream> m_logFiles;
  std::vector<FILE *> m_systemFiles;

  void ReopenFiles(std::ios_base::openmode mode);
  void WriteToFiles(const char *str);
};

/*
 * CSystemLogger:
 *
 * Logs to the system log.
 */
class CSystemLogger: public CLogger
{
public:
  void DebugLog(const char *fmt, va_list vl);
  void InfoLog(const char *fmt, va_list vl);
  void ErrorLog(const char *fmt, va_list vl);
  CSystemLogger(LogLevel level);

private:
  LogLevel m_logLevel;
};


/******************************************************************************
 Log Functions

 Message logging interface. All messages are passed to the currently active
 logger object.
******************************************************************************/

/*
 * DebugLog(fmt, ...):
 *
 * Prints debugging information. The OSD layer may choose to print this to a
 * log file, the screen, neither, or both. Newlines and other formatting codes
 * must be explicitly included.
 *
 * Parameters:
 *		fmt		A format string (the same as printf()).
 *		...		Variable number of arguments, as required by format string.
 */
extern void	DebugLog(const char *fmt, ...);

/*
 * ErrorLog(fmt, ...):
 *
 * Prints error information. Errors need not require program termination and
 * may simply be informative warnings to the user. Newlines should not be
 * included in the format string -- they are automatically added at the end of
 * a line.
 *
 * Parameters:
 *		fmt		A format string (the same as printf()).
 *		...		Variable number of arguments, as required by format string.
 *
 * Returns:
 *		Must always return FAIL.
 */
extern bool	ErrorLog(const char *fmt, ...);

/*
 * InfoLog(fmt, ...);
 *
 * Prints information to the error log file but does not print to stderr. This
 * is useful for logging non-error information. Newlines are automatically
 * appended.
 *
 * Parameters:
 *		fmt		Format string (same as printf()).
 *		...		Variable number of arguments as required by format string.
 */
extern void InfoLog(const char *fmt, ...);

/*
 * SetLogger(Logger):
 *
 * Sets the logger object to use.
 *
 * Parameters:
 *		Logger	Unique pointer to a new logger object. If null pointer, log
 *            messages will not be output.
 */
extern void SetLogger(std::shared_ptr<CLogger> logger);

/*
 * GetLogger(void):
 *
 * Returns:
 *		Current logger object (null pointer if not set).
 */
extern std::shared_ptr<CLogger> GetLogger(void);

/*
 * CreateLogger(config):
 *
 * Returns:
 *    A logger object that satisfies the requirements specified in the passed
 *    configuration or an empty pointer if an unrecoverable error occurred.
 */
std::shared_ptr<CLogger> CreateLogger(const Util::Config::Node &config);


#endif	// INCLUDED_LOGGER_H
