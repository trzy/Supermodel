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

#include "Util/Format.h"
#include <algorithm>
#include <cctype>

namespace Util
{
  std::ostream &operator<<(std::ostream &os, const Format &format)
  {
    format.Write(os);
    return os;
  }

  std::string ToLower(const std::string &str)
  {
    std::string tmp(str);
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
    return tmp;
  }

  std::string TrimWhiteSpace(const std::string &str)
  {
    if (str.empty())
      return std::string();
    size_t first = 0;
    for (; first < str.length(); ++first)
    {
      if (!isspace(str[first]))
        break;
    }
    if (first >= str.length())
      return std::string();
    size_t last = str.length() - 1;
    for (; last > first; --last)
    {
      if (!isspace(str[last]))
        break;
    }
    ++last;
    return std::string(str.c_str() + first, last - first);
  }

  static constexpr char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

  std::string Hex(uint64_t n, size_t num_digits)
  {
    Util::Format f;
    f << "0x";
    for (size_t b = num_digits * 4; b; )
    {
      b -= 4;
      f << hex_digits[(n >> b) & 0xf];
    }
    return f;
  }

  std::string Hex(uint64_t n)
  {
    return Hex(n, 16);
  }

  std::string Hex(uint32_t n)
  {
    return Hex(n, 8);
  }
  
  std::string Hex(uint16_t n)
  {
    return Hex(n, 4);
  }
  
  std::string Hex(uint8_t n)
  {
    return Hex(n, 2);
  }

  int Stricmp(const char *s1, const char *s2)
  {
    int cmp;
    char c1;
    char c2;
    do
    {
      c1 = *s1++;
      c2 = *s2++;
      cmp = unsigned(tolower(c1)) - unsigned(tolower(c2));
    } while ((cmp == 0) && (c1 != '\0') && (c2 != '\0'));
    return cmp;
  }
} // Util
