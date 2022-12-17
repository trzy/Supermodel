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
#include "Util/Format.h"
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

namespace FileSystemPath
{
    // Checks if a directory exists (returns true if exists, false if it doesn't)
    bool PathExists(std::string fileSystemPath)
    {
        bool pathExists = false;
        struct stat pathInfo;

        if (stat(fileSystemPath.c_str(), &pathInfo) == 0 && S_ISDIR(pathInfo.st_mode))
        {
            pathExists = true;
        }

        return pathExists;

    }

    // Generates a path to be used by Supermodel files
    std::string GetPath(PathType pathType)
    {
        std::string finalPath = "";
        std::string homePath = "";
        std::string strPathType = "";
        struct passwd* pwd = getpwuid(getuid());

        // Resolve pathType to string for later use
        switch (pathType)
        {
        case Analysis:
            strPathType = "Analysis";
            break;
        case Config:
            strPathType = "Config";
            break;
        case Log:
            strPathType = "Log";
            break;
        case NVRAM:
            strPathType = "NVRAM";
            break;
        case Saves:
            strPathType = "Saves";
            break;
        case Screenshots:
            strPathType = "Screenshots";
            break;
        }

        // Get user's HOME directory
        if (pwd)
        {
            homePath = pwd->pw_dir;
        }
        else
        {
            homePath = getenv("HOME");
        }

        // If Config path exists in current directory or the user doesn't have a HOME directory use current directory
        if (FileSystemPath::PathExists("Config") || homePath.empty())
        {
            // Use current directory
            if (pathType == Screenshots || pathType == Log)
            {
                finalPath = "";
            }
            else
            {
                // If directory doesn't exist, create it
                if (!FileSystemPath::PathExists(strPathType))
                {
                    mkdir(strPathType.c_str(), 0775);
                }
                finalPath = strPathType;
            }
        }
        // Check if $HOME/.supermodel exists
        else if (FileSystemPath::PathExists(Util::Format() << homePath << "/.supermodel"))
        {
            // Use $HOME/.supermodel
            finalPath = Util::Format() << homePath << "/.supermodel/" << strPathType;
            // If directory doesn't exist, create it
            if (!FileSystemPath::PathExists(finalPath))
            {
                mkdir(finalPath.c_str(), 0775);
            }
        }
        // On Linux one may want to follow the XDG base directory specs (https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html)
        else
        {
            // Use $HOME/.config/supermodel or $HOME/.local/share/supermodel depending on the file type
            if (pathType == Config)
            {
                finalPath = Util::Format() << homePath << "/.config/supermodel";
                // If directory doesn't exist, create it
                if (!FileSystemPath::PathExists(finalPath))
                {
                    mkdir(finalPath.c_str(), 0775);
                }
                // If directory doesn't exist, create it
                finalPath = Util::Format() << homePath << "/.config/supermodel/Config";
                if (!FileSystemPath::PathExists(finalPath))
                {
                    mkdir(finalPath.c_str(), 0775);
                }
            }
            else
            {
                finalPath = Util::Format() << homePath << "/.local/share/supermodel";
                // If directory doesn't exist, create it
                if (!FileSystemPath::PathExists(finalPath))
                {
                    mkdir(finalPath.c_str(), 0775);
                }
                // If directory doesn't exist, create it
                finalPath = Util::Format() << homePath << "/.local/share/supermodel/" << strPathType;
                if (!FileSystemPath::PathExists(finalPath))
                {
                    mkdir(finalPath.c_str(), 0775);
                }
            }
            
        }
        
        if (finalPath != "")
        {
            finalPath = Util::Format() << finalPath << "/";
        }

        return finalPath;

    }
}
