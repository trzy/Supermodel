/*
 * TODO:
 * -----
 * - ParseInteger() should be optimized. It is frequently used throughout the
 *   code base at run-time.
 * - Possible optimization: cache last ValueAs<T> conversion. Return this when
 *   available and when value is Set(), mark dirty. This may require memory
 *   allocation or, alternatively, we could do this only for data types of size
 *   <= 8.
 */

#ifndef INCLUDED_UTIL_GENERICVALUE_H
#define INCLUDED_UTIL_GENERICVALUE_H

#include "Util/Format.h"
#include <typeinfo>
#include <typeindex>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <cctype>

namespace Util
{
  namespace detail
  {
    // Support for hexadecimal conversion for 16-bit or greater integers.
    // Cannot distinguish chars from 8-bit integers, so unsupported there.
    template <typename T>
    struct IntegerEncodableAsHex
    {
      static const bool value = std::is_integral_v<T> && sizeof(T) >= 2 && sizeof(T) <= 8;
    };

    // This case should never actually be called
    template <typename T>
    static typename std::enable_if_t<!IntegerEncodableAsHex<T>::value, T> ParseInteger(const std::string &str)
    {
      return T();
    }

    // This case will be generated for hex encodable integers and executed
    template <typename T>
    static typename std::enable_if_t<IntegerEncodableAsHex<T>::value, T> ParseInteger(const std::string &str)
    {
      T tmp = 0;
      if (str.length() >= 3 && (
          (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))  ||
          ((str[0] == '-' || str[0] == '+') && (str[1] == '0' && (str[2] == 'x' || str[2] == 'X')))
        ))
      {
        bool negative = str[0] == '-';
        size_t start_at = 2 + ((negative || str[0] == '+') ? 1 : 0);
        size_t i;
        for (i = start_at; i < str.size(); i++)
        {
          tmp *= 16;
          char c = str[i];
          if (!isxdigit(c)) // not a hex digit, abort
            goto not_hex;
          if (isdigit(c))
            tmp |= (c - '0');
          else if (isupper(c))
            tmp |= (c - 'A' + 10);
          else if (islower(c))
            tmp |= (c - 'a' + 10);
        }
        if (i == start_at)  // no digits parsed
          goto not_hex;
        if (negative)
          tmp *= -1;
        return tmp;
      }
    not_hex:
      std::stringstream ss;
      ss << str;
      ss >> tmp;
      return tmp;
    }

    // This case should never actually be called
    template <typename T>
    inline T ParseBool(const std::string &str)
    {
      return T();
    }

    // This case will be generated for bools
    template <>
    inline bool ParseBool<bool>(const std::string &str)
    {
      if (!Util::Stricmp(str.c_str(), "true") || !Util::Stricmp(str.c_str(), "on") || !Util::Stricmp(str.c_str(), "yes"))
        return true;
      if (!Util::Stricmp(str.c_str(), "false") || !Util::Stricmp(str.c_str(), "off") || !Util::Stricmp(str.c_str(), "no"))
        return false;
      bool tmp;
      std::stringstream ss;
      ss << str;
      ss >> tmp;
      return tmp;
    }    
  }

  class GenericValue
  {
  private:
    std::type_index m_type;

    virtual void *GetData() = 0;
    virtual const void *GetData() const = 0;

  public:
    template <typename T>
    inline bool Is() const
    {
      return m_type == std::type_index(typeid(T));
    }

    template <typename T>
    const T &Value() const
    {
      if (!Is<T>())
        throw std::logic_error(Util::Format() << "GenericValue::Value(): cannot get value as " << std::type_index(typeid(T)).name() <<" because it is stored as " << m_type.name());
      return *reinterpret_cast<const T *>(GetData());
    }

    template <typename T>
    T ValueAs() const
    {
      if (m_type == std::type_index(typeid(T)))
        return *reinterpret_cast<const T *>(GetData());
      if (m_type == std::type_index(typeid(std::string)) && detail::IntegerEncodableAsHex<T>::value)
        return detail::ParseInteger<T>(Value<std::string>()); // special case string -> integer conversion
      if (m_type == std::type_index(typeid(std::string)) && std::type_index(typeid(T)) == std::type_index(typeid(bool)))
        return detail::ParseBool<T>(Value<std::string>());    // special case string -> bool conversion
      std::stringstream ss;
      Serialize(&ss);
      T tmp;
      ss >> tmp;
      return tmp;
    }

    template <typename T>
    void Set(const T &value)
    {
      if (!Is<T>())
        throw std::logic_error(Util::Format() << "GenericValue::Set(): cannot set value as " << std::type_index(typeid(T)).name() <<" because it is stored as " << m_type.name());
      *reinterpret_cast<T *>(GetData()) = value;
    }

    void Set(const char *value)
    {
      Set<std::string>(value);
    }
   
    virtual void Serialize(std::ostream *os) const = 0;
    virtual std::shared_ptr<GenericValue> MakeCopy() const = 0;
      
    GenericValue(std::type_index type)
      : m_type(type)
    {}

    virtual ~GenericValue()
    {
    }
  };

  template <typename T>
  struct ValueInstance: public GenericValue
  {
  private:
    T m_data;
    
    void *GetData()
    {
      return reinterpret_cast<void *>(&m_data);
    }
    
    const void *GetData() const
    {
      return reinterpret_cast<const void *>(&m_data);
    }

  public:
    void Serialize(std::ostream *os) const
    {
      *os << m_data;
    }

    std::shared_ptr<GenericValue> MakeCopy() const
    {
      return std::make_shared<ValueInstance<T>>(*this);
    }
    
    ValueInstance(const ValueInstance<T> &that)
      : GenericValue(std::type_index(typeid(T))),
        m_data(that.m_data)
    {
    }

    ValueInstance(const T &data)
      : GenericValue(std::type_index(typeid(T))),
        m_data(data)
    {
    }

    ValueInstance()
      : GenericValue(std::type_index(typeid(T)))
    {
    }

    ~ValueInstance()
    {
      //std::cout << "ValueInstance destroyed" << std::endl;
    }
  };
} // Util

#endif  // INCLUDED_UTIL_GENERICVALUE_H
