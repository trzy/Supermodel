#ifndef INCLUDED_UTIL_GENERICVALUE_H
#define INCLUDED_UTIL_GENERICVALUE_H

#include "Util/Format.h"
#include <typeinfo>
#include <typeindex>
#include <sstream>
#include <stdexcept>
#include <memory>

namespace Util
{
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
