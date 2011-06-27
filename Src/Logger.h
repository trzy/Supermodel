#ifndef INCLUDED_LOGGER_H
#define INCLUDED_LOGGER_H

#include <stdarg.h>

/*
 * CLogger
 * 
 * Abstract class that receives log messages from Supermodel.
 */
class CLogger
{
public:
	void DebugLog(const char *fmt, ...)
	{
		va_list vl;
		va_start(vl, fmt);
		DebugLog(fmt, vl);
		va_end(vl);
	}

	void InfoLog(const char *fmt, ...)
	{
		va_list vl;
		va_start(vl, fmt);
		InfoLog(fmt, vl);
		va_end(vl);
	}

	void ErrorLog(const char *fmt, ...)
	{
		va_list vl;
		va_start(vl, fmt);
		ErrorLog(fmt, vl);
		va_end(vl);
	}

	virtual void DebugLog(const char *fmt, va_list vl) = 0;

	virtual void InfoLog(const char *fmt, va_list vl) = 0;
	
	virtual void ErrorLog(const char *fmt, va_list vl) = 0;
};

#endif	// INCLUDED_LOGGER_H
