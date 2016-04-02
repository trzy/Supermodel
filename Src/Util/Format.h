#ifndef INCLUDED_FORMAT_H
#define INCLUDED_FORMAT_H

#include <string>
#include <sstream>
#include <iomanip>

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
  
  const std::string Hex(uint32_t n, size_t num_digits);
  const std::string Hex(uint32_t n);
  const std::string Hex(uint16_t n);
  const std::string Hex(uint8_t n);
} // Util

#endif  // INCLUDED_FORMAT_H
