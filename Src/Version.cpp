/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * Version.cpp
 *
 * Supermodel version string. The build systems must recompile this file on
 * every build so that __DATE__ is the date of the build.
 */

#include "Version.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Official builds pass SUPERMODEL_GIT_REVISION (commit date and hash), giving,
// e.g., 0.4-20261106-git-2d833a0. Other builds get the build date from the
// compiler's __DATE__ ("Mmm dd yyyy"), giving, e.g., 0.4-dev-20261106.
std::string SupermodelVersion()
{
  const std::string series_version = "0.4";
#ifdef SUPERMODEL_GIT_REVISION
  return series_version + "-" SUPERMODEL_GIT_REVISION;
#else
  const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *p = strstr(months, std::string(__DATE__, 3).c_str());
  const int month = p ? int(p - months) / 3 + 1 : 0;
  const int day = atoi(&__DATE__[4]);
  char date[9];
  snprintf(date, sizeof(date), "%.4s%02d%02d", &__DATE__[7], month, day);
  return series_version + "-dev-" + date;
#endif
}
