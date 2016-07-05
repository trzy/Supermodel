#include "OSD/Logger.h"

// Logger object is used to redirect log messages appropriately
static CLogger *s_Logger = NULL;

CLogger *GetLogger()
{
  return s_Logger;
}

void SetLogger(CLogger *Logger)
{
  s_Logger = Logger;
}

void DebugLog(const char *fmt, ...)
{
  if (s_Logger == NULL)
    return;
  va_list vl;
  va_start(vl, fmt);
  s_Logger->DebugLog(fmt, vl);
  va_end(vl);
}

void InfoLog(const char *fmt, ...)
{
  if (s_Logger == NULL)
    return;
  va_list vl;
  va_start(vl, fmt);
  s_Logger->InfoLog(fmt, vl);
  va_end(vl);
}

bool ErrorLog(const char *fmt, ...)
{
  if (s_Logger == NULL)
    return FAIL;
  va_list vl;
  va_start(vl, fmt);
  s_Logger->ErrorLog(fmt, vl);
  va_end(vl);
  return FAIL;
}
