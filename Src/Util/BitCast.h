#ifndef INCLUDED_UTIL_BITCAST_H
#define INCLUDED_UTIL_BITCAST_H

#include <stdexcept>
#include <cstring>

namespace Util
{
#if __cplusplus >= 202002L
#include <bit>
#define FloatAsInt(x) std::bit_cast<int>(x)
#define IntAsFloat(x) std::bit_cast<float>(x)
#define UintAsFloat(x) std::bit_cast<float>(x)
#elif 1
template <class Dest, class Source>
inline Dest bit_cast(Source const& source) {
   static_assert(sizeof(Dest) == sizeof(Source), "size of destination and source objects must be equal");
   static_assert(std::is_trivially_copyable<Dest>::value, "destination type must be trivially copyable.");
   static_assert(std::is_trivially_copyable<Source>::value, "source type must be trivially copyable");

   Dest dest;
   std::memcpy(&dest, &source, sizeof(dest));
   return dest;
}
#define FloatAsInt(x) bit_cast<int,float>(x)
#define IntAsFloat(x) bit_cast<float,int>(x)
#define UintAsFloat(x) bit_cast<float,unsigned int>(x)
#else
inline int FloatAsInt(const float x)
{
   union {
      float f;
      int i;
   } uc;
   uc.f = x;
   return uc.i;
}

inline float IntAsFloat(const int i)
{
   union {
      int i;
      float f;
   } iaf;
   iaf.i = i;
   return iaf.f;
}

inline float UintAsFloat(const unsigned int i)
{
   union {
      unsigned int u;
      float f;
   } iaf;
   iaf.u = i;
   return iaf.f;
}
#endif
} // Util

#endif  // INCLUDED_UTIL_BITCAST_H
