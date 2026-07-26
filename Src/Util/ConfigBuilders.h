/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
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

#ifndef INCLUDED_UTIL_CONFIGBUILDERS_H
#define INCLUDED_UTIL_CONFIGBUILDERS_H

#include <string>

namespace Util
{
  namespace Config
  {
    class Node;

    bool FromXML(Node *config, const std::string &text);
    bool FromXMLFile(Node *config, const std::string &filename);
    bool FromINIFile(Node *config, const std::string &filename);
    void MergeINISections(Node *merged, const Node &x, const Node &y);
    void WriteINIFile(const std::string &filename, const Node &config, const std::string &header_comment);
  }
}

#endif  // INCLUDED_UTIL_CONFIGBUILDERS_H
