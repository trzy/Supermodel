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

#include "OSD/Logger.h"
#include <set>
#ifdef _WIN32
#include <windows.h>
#else
#include <syslog.h>
#endif


// Logger object is used to redirect log messages appropriately
static std::shared_ptr<CLogger> s_Logger;


std::shared_ptr<CLogger> GetLogger()
{
  return s_Logger;
}

void SetLogger(std::shared_ptr<CLogger> logger)
{
  s_Logger = logger;
}

void DebugLog(const char *fmt, ...)
{
  if (!s_Logger)
    return;
  va_list vl;
  va_start(vl, fmt);
  s_Logger->DebugLog(fmt, vl);
  va_end(vl);
}

void InfoLog(const char *fmt, ...)
{
  if (!s_Logger)
    return;
  va_list vl;
  va_start(vl, fmt);
  s_Logger->InfoLog(fmt, vl);
  va_end(vl);
}

bool ErrorLog(const char *fmt, ...)
{
  if (!s_Logger)
    return FAIL;
  va_list vl;
  va_start(vl, fmt);
  s_Logger->ErrorLog(fmt, vl);
  va_end(vl);
  return FAIL;
}

static std::pair<bool, CLogger::LogLevel> GetLogLevel(const Util::Config::Node &config)
{
  const std::map<std::string, CLogger::LogLevel> logLevelByString
  {
    { "debug", CLogger::LogLevel::Debug },
    { "info", CLogger::LogLevel::Info },
    { "error", CLogger::LogLevel::Error },
    { "all", CLogger::LogLevel::All },
  };

  std::string logLevel = Util::ToLower(config["LogLevel"].ValueAsDefault<std::string>("info"));

  auto it = logLevelByString.find(logLevel);
  if (it != logLevelByString.end())
  {
    return std::pair<bool, CLogger::LogLevel>(OKAY, it->second);
  }

  ErrorLog("Invalid log level: %s", logLevel.c_str());
  return std::pair<bool, CLogger::LogLevel>(FAIL, CLogger::LogLevel::Info);
}

std::shared_ptr<CLogger> CreateLogger(const Util::Config::Node &config)
{
  std::vector<std::shared_ptr<CLogger>> loggers;

  // Get log level
  auto logLevelResult = GetLogLevel(config);
  if (logLevelResult.first != OKAY)
  {
    return std::shared_ptr<CLogger>();
  }
  CLogger::LogLevel logLevel = logLevelResult.second;

  // Console message logger always required
  loggers.push_back(std::make_shared<CConsoleErrorLogger>());

  // Parse other log outputs
  std::string logOutputs = config["LogOutput"].ValueAsDefault<std::string>(std::string());
  std::vector<std::string> outputs = Util::Format(logOutputs).Split(',');

  std::set<std::string> supportedDestinations { "stdout", "stderr", "syslog" };
  std::set<std::string> destinations; // log output destinations
  std::set<std::string> filenames;    // anything that is not a known destination is assumed to be a file
  for (auto output: outputs)
  {
    // Is this a known destination or a file?
    std::string canonicalizedOutput = Util::TrimWhiteSpace(Util::ToLower(output));
    if (supportedDestinations.count(canonicalizedOutput) > 0)
    {
      destinations.insert(canonicalizedOutput);
    }
    else if (!canonicalizedOutput.empty())
    {
      filenames.insert(Util::TrimWhiteSpace(output)); // trim whitespace but preserve capitalization of filenames
    }
  }

  // File logger (if any files were specified)
  std::vector<std::string> logFilenames(filenames.begin(), filenames.end());
  std::vector<FILE *> systemFiles;

  if (destinations.count("stdout") > 0)
  {
    systemFiles.push_back(stdout);
  }

  if (destinations.count("stderr") > 0)
  {
    systemFiles.push_back(stderr);
  }

  if (!logFilenames.empty() || !systemFiles.empty())
  {
    loggers.push_back(std::make_shared<CFileLogger>(logLevel, logFilenames, systemFiles));
  }

  // System logger
  if (destinations.count("syslog") > 0)
  {
    loggers.push_back(std::make_shared<CSystemLogger>(logLevel));
  }

  return std::make_shared<CMultiLogger>(loggers);
}

/*
 * CMultiLogger
 */

void CMultiLogger::DebugLog(const char *fmt, va_list vl)
{
  for (auto &logger: m_loggers)
  {
    va_list vl_tmp;
    va_copy(vl_tmp, vl);
    logger->DebugLog(fmt, vl_tmp);
    va_end(vl_tmp);
  }
}

void CMultiLogger::InfoLog(const char *fmt, va_list vl)
{
  for (auto &logger: m_loggers)
  {
    va_list vl_tmp;
    va_copy(vl_tmp, vl);
    logger->InfoLog(fmt, vl_tmp);
    va_end(vl_tmp);
  }
}

void CMultiLogger::ErrorLog(const char *fmt, va_list vl)
{
  for (auto &logger: m_loggers)
  {
    va_list vl_tmp;
    va_copy(vl_tmp, vl);
    logger->ErrorLog(fmt, vl_tmp);
    va_end(vl_tmp);
  }
}

CMultiLogger::CMultiLogger(std::vector<std::shared_ptr<CLogger>> loggers)
  : m_loggers(loggers)
{
}

/*
 * CConsoleErrorLogger
 */

void CConsoleErrorLogger::DebugLog(const char *fmt, va_list vl)
{
  // To view debug-level logging on the console, use a file logger writing
  // to stdout
}

void CConsoleErrorLogger::InfoLog(const char *fmt, va_list vl)
{
  // To view info-level logging on the console, use a file logger writing
  // to stdout
}

void CConsoleErrorLogger::ErrorLog(const char *fmt, va_list vl)
{
  char  string[4096];
  vsprintf(string, fmt, vl);
  fprintf(stderr, "Error: %s\n", string);
}

/*
 * CFileLogger
 */

void CFileLogger::DebugLog(const char *fmt, va_list vl)
{
  if (m_logLevel > LogLevel::Debug)
  {
    return;
  }

  char string1[4096];
  char string2[4096];

  vsprintf(string1, fmt, vl);
  sprintf(string2, "[Debug] %s", string1);

  // Debug logging is so copious that we don't bother to guarantee it is saved
  std::unique_lock<std::mutex> lock(m_mtx);
  WriteToFiles(string2);
}

void CFileLogger::InfoLog(const char *fmt, va_list vl)
{
  if (m_logLevel > LogLevel::Info)
  {
    return;
  }

  char string1[4096];
  char string2[4096];

  vsprintf(string1, fmt, vl);
  sprintf(string2, "[Info]  %s\n", string1);

  // Write to file, close, and reopen to ensure it was saved
  std::unique_lock<std::mutex> lock(m_mtx);
  WriteToFiles(string2);
  ReopenFiles(std::ios::app);
}

void CFileLogger::ErrorLog(const char *fmt, va_list vl)
{
  if (m_logLevel > LogLevel::Error)
  {
    return;
  }

  char string1[4096];
  char string2[4096];

  vsprintf(string1, fmt, vl);
  sprintf(string2, "[Error] %s\n", string1);

  // Write to file, close, and reopen to ensure it was saved
  std::unique_lock<std::mutex> lock(m_mtx);
  WriteToFiles(string2);
  ReopenFiles(std::ios::app);
}

void CFileLogger::ReopenFiles(std::ios_base::openmode mode)
{
  // Close existing
  for (std::ofstream &ofs: m_logFiles)
  {
    ofs.close();
  }
  m_logFiles.clear();

  // (Re-)Open
  for (auto filename: m_logFilenames)
  {
    std::ofstream ofs(filename.c_str(), mode);
    if (ofs.is_open() && ofs.good())
    {
      m_logFiles.emplace_back(std::move(ofs));
    }
  }
}

void CFileLogger::WriteToFiles(const char *str)
{
  for (std::ofstream &ofs: m_logFiles)
  {
    ofs << str;
  }

  for (FILE *fp: m_systemFiles)
  {
    fputs(str, fp);
  }
}

CFileLogger::CFileLogger(CLogger::LogLevel level, std::vector<std::string> filenames)
  : m_logLevel(level),
    m_logFilenames(filenames)
{
  ReopenFiles(std::ios::out);
}

CFileLogger::CFileLogger(CLogger::LogLevel level, std::vector<std::string> filenames, std::vector<FILE *> systemFiles)
  : m_logLevel(level),
    m_logFilenames(filenames),
    m_systemFiles(systemFiles)
{
  ReopenFiles(std::ios::out);
}

/*
 * CSystemLogger
 */

void CSystemLogger::DebugLog(const char *fmt, va_list vl)
{
  if (m_logLevel > LogLevel::Debug)
  {
    return;
  }

  char string1[4096];
  char string2[4096];

  vsprintf(string1, fmt, vl);
  sprintf(string2, "[Debug] %s", string1);

#ifdef _WIN32
  OutputDebugString(string2);
#else
  syslog(LOG_DEBUG, string2);
#endif
}

void CSystemLogger::InfoLog(const char *fmt, va_list vl)
{
  if (m_logLevel > LogLevel::Info)
  {
    return;
  }

  char string1[4096];
  char string2[4096];

  vsprintf(string1, fmt, vl);
  sprintf(string2, "[Info]  %s\n", string1);

#ifdef _WIN32
  OutputDebugString(string2);
#else
  syslog(LOG_INFO, string2);
#endif
}

void CSystemLogger::ErrorLog(const char *fmt, va_list vl)
{
  if (m_logLevel > LogLevel::Error)
  {
    return;
  }

  char string1[4096];
  char string2[4096];

  vsprintf(string1, fmt, vl);
  sprintf(string2, "[Error] %s\n", string1);

 #ifdef _WIN32
  OutputDebugString(string2);
#else
  syslog(LOG_ERR, string2);
#endif
}

CSystemLogger::CSystemLogger(CLogger::LogLevel level)
  : m_logLevel(level)
{
}
