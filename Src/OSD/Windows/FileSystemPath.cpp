/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2022 The Supermodel Team
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

#include "FileSystemPath.h"
#include <string>

namespace FileSystemPath
{
    // Generates a path to be used by Supermodel files
    std::string GetPath(PathType pathType)
    {
        switch (pathType)
        {
        case Analysis:
            return "Analysis/";
        case Config:
            return "Config/";
        case Log:
            return "";
        case NVRAM:
            return "NVRAM/";
        case Saves:
            return "Saves/";
        case Screenshots:
            return "";
        }

        return "";
    }
}
