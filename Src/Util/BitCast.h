#ifndef INCLUDED_UTIL_BITCAST_H
#define INCLUDED_UTIL_BITCAST_H

#include <cstdint>

#if __cplusplus >= 202002L
#include <bit>
namespace Util
{
#define FloatAsInt32(x) std::bit_cast<int32_t>(x)
#define Int32AsFloat(x) std::bit_cast<float>(x)
#define Uint32AsFloat(x) std::bit_cast<float>(x)
#else
#include <stdexcept>
#include <cstring>
namespace Util
{
template <class Dest, class Source>
inline Dest bit_cast(Source const& source) {
   static_assert(sizeof(Dest) == sizeof(Source), "size of destination and source objects must be equal");
   static_assert(std::is_trivially_copyable_v<Dest>, "destination type must be trivially copyable.");
   static_assert(std::is_trivially_copyable_v<Source>, "source type must be trivially copyable");

   Dest dest;
   std::memcpy(&dest, &source, sizeof(dest));
   return dest;
}
#define FloatAsInt32(x) bit_cast<int32_t,float>(x)
#define Int32AsFloat(x) bit_cast<float,int32_t>(x)
#define Uint32AsFloat(x) bit_cast<float,uint32_t>(x)
#endif
} // Util

#endif  // INCLUDED_UTIL_BITCAST_H
