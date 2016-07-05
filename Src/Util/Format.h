#ifndef INCLUDED_FORMAT_H
#define INCLUDED_FORMAT_H

#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <vector>

namespace Util
{
  class Format
  {
  public:
    template <typename T>
    Format &operator<<(const T &data)
    {
      m_stream << data;
      return *this;
    }
  
    operator std::string() const
    {
      return str();
    }

    std::string str() const
    {
      return m_stream.str();
    }

    template <typename T>
    Format &Join(const T &collection)
    {
      std::string separator = m_stream.str();
      clear();
      for (auto it = collection.begin(); it != collection.end(); )
      {
        m_stream << *it;
        ++it;
        if (it != collection.end())
          m_stream << separator;
      }
      return *this;
    }

    std::vector<std::string> Split(char separator)
    {
      // Very inefficient: lots of intermediate string copies!
      std::string str = m_stream.str();
      const char *start = str.c_str();
      const char *end = start;
      std::vector<std::string> parts;
      do
      {
        if (*end == separator || !*end)
        {
          size_t len = end - start;
          if (len)
            parts.emplace_back(start, len);
          else
            parts.emplace_back();
          start = end + 1;
        }
        ++end;
      } while (end[-1]);
      return parts;
    }

    Format(const std::string &str)
      : m_stream(str)
    {
    }
      
    Format()
    {
    }
  private:
    std::stringstream m_stream;
      
    void clear()
    {
      m_stream.str(std::string());
    }
  };
  
  std::string TrimWhiteSpace(const std::string &str);
  std::string Hex(uint32_t n, size_t num_digits);
  std::string Hex(uint32_t n);
  std::string Hex(uint16_t n);
  std::string Hex(uint8_t n);
} // Util

#endif  // INCLUDED_FORMAT_H
