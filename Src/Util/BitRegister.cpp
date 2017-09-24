#include "Util/BitRegister.h"
#include <cstring>
#include <cctype>
#include <string>

namespace Util
{
  uint8_t BitRegister::GetLeftmost() const
  {
    return m_bits.empty() ? m_no_data : m_bits.front();
  }
  
  uint8_t BitRegister::GetRightmost() const
  {
    return m_bits.empty() ? m_no_data : m_bits.back();
  }
  
  size_t BitRegister::HexStart(const std::string &value)
  {
    if (value.length() > 1 && value[0] == '$')
      return 1;
    if (value.length() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
      return 2;
    return std::string::npos;
  }
  
  size_t BitRegister::BinStart(const std::string &value)
  {
    if (value.length() > 1 && value[0] == '%')
      return 1;
    if (value.length() > 2 && value[0] == '0' && (value[1] == 'b' || value[1] == 'B'))
      return 2;
    return 0;
  }
  
  size_t BitRegister::CountBitsHex(const std::string &value, size_t startPos)
  {
    return 4 * (value.length() - startPos);
  }
  
  size_t BitRegister::CountBitsBin(const std::string &value, size_t startPos)
  {
    return value.length() - startPos;
  }
 
  uint8_t BitRegister::GetBit(size_t pos) const
  {
    if (pos < Size())
      return m_bits[pos];
    return m_no_data;
  }

  // Convert entire bit vector into a 64-bit integer. If bit register is larger
  // than 64 bits, only least significant bits will be present.
  uint64_t BitRegister::GetBits() const
  {
    return GetBits(0, Size());
  }
  
  // Convert a subset of the bit vector into a 64-bit integer. If the data does
  // not fit into 64 bits, only the least significant (rightmost) bits. Result
  // undefined if range falls outside of vector.
  uint64_t BitRegister::GetBits(size_t from, size_t count) const
  {
    uint64_t value = 0;
    if (from + count > Size())
      return value;
    for (size_t i = from; i < from + count; i++)
    {
      value <<= 1;
      value |= GetBit(i);
    }
    return value;
  }
    
  // Shift a bit into the right side, growing the vector by 1
  void BitRegister::AddToRight(uint8_t bit)
  {
    m_bits.push_back(!!bit);
  }
  
  // Shift a bit into the left side, growing the vector by 1
  void BitRegister::AddToLeft(uint8_t bit)
  {
    m_bits.push_back(0);
    ShiftRight(1);
    m_bits[0] = !!bit;
  }

  // Shift left by 1, returning ejected bit (shrinks vector)
  uint8_t BitRegister::RemoveFromLeft()
  {
    uint8_t ejected = GetLeftmost();
    RemoveFromLeft(1);
    return ejected;
  }
  
  // Shift left and lose bits (shrinks vector)
  void BitRegister::RemoveFromLeft(size_t count)
  {
    if (count >= m_bits.size())
    {
      // All bits shifted out
      m_bits.clear();
      return;
    }
    
    // Shift and resize
    memmove(m_bits.data(), m_bits.data() + count, m_bits.size() - count);
    m_bits.resize(m_bits.size() - count);
  }
  
  // Shift right by 1, returning ejected bit (shrinks vector)
  uint8_t BitRegister::RemoveFromRight()
  {
    uint8_t ejected = GetRightmost();
    RemoveFromRight(1);
    return ejected;
  }

  // Shift right and lose bits (shrinks vector)
  void BitRegister::RemoveFromRight(size_t count)
  {
    // Shifting right means we lose lower bits (which are higher in the
    // vector), which means we just trim the vector from the right side
    if (count >= m_bits.size())
    {
      m_bits.clear();
      return;
    }
    m_bits.resize(m_bits.size() - count);
  }
  
  // Shift right and lose Rightmost bits, shifting in new bits from left to
  // preserve vector size
  void BitRegister::ShiftRight(size_t count)
  {
    if (Empty())
      return;

    if (count < m_bits.size())
    {
      // Shift over bits to the right
      memmove(m_bits.data() + count, m_bits.data(), m_bits.size() - count);
    }

    // Fill in the left with "no data"
    memset(m_bits.data(), m_no_data, count);
  }
  
  // Shift left and lose left-most bits, shifting in new bits from right to
  // preserve vector size
  void BitRegister::ShiftLeft(size_t count)
  {
    if (Empty())
      return;

    if (count < m_bits.size())
    {
      // Shift over bits to the left
      memmove(m_bits.data(), m_bits.data() + count, m_bits.size() - count);
    }
    
    // Fill in the right with "no data"
    memset(m_bits.data() + m_bits.size() - count, m_no_data, count);
  }
  
  // Shift right and eject Rightmost bit, shifting new bit into left side to
  // preserve vector size
  uint8_t BitRegister::ShiftOutRight(uint8_t bit)
  {
    if (Empty())
      return m_no_data;

    uint8_t ejected = GetRightmost();
    ShiftRight(1);
    m_bits[0] = !!bit;
    return ejected;
  }
  
  // Shift left and eject left-most bit, shifting new bit into right side to
  // preserve vector size
  uint8_t BitRegister::ShiftOutLeft(uint8_t bit)
  {
    if (Empty())
      return m_no_data;

    uint8_t ejected = GetLeftmost();
    ShiftLeft(1);
    m_bits[m_bits.size() - 1] = !!bit;
    return ejected;
  }

  void BitRegister::Reset()
  {
    m_bits.clear();
  }
  
  // Set single bit, indexed from left, without expanding vector
  void BitRegister::SetBit(size_t bitPos, uint8_t value)
  {
    if (bitPos < m_bits.size())
      m_bits[bitPos] = !!value;
  }
  
  // Insert value, indexed from left, without expanding vector
  void BitRegister::Insert(size_t bitPos, const std::string &value)
  {
    size_t hexStart = HexStart(value);
    if (hexStart != std::string::npos)
    {
      for (size_t i = hexStart; i < value.length(); i++)
      {
        char digit = tolower(value[i]);
        uint8_t nibble = 0;
        if (isxdigit(digit))
           nibble = (digit >= 'a') ? (digit - 'a' + 10) : (digit - '0');
        SetBit(bitPos++, nibble & 8);
        SetBit(bitPos++, nibble & 4);
        SetBit(bitPos++, nibble & 2);
        SetBit(bitPos++, nibble & 1);
      }
    }
    else
    {
      for (size_t i = BinStart(value); i < value.length(); i++)
      {
        SetBit(bitPos++, value[i] == '0' ? 0 : 1);
      }
    }
  }
  
  void BitRegister::Set(const std::string &value)
  {
    size_t hexStart = HexStart(value);
    size_t binStart = BinStart(value);
    if (hexStart != std::string::npos)
      m_bits.resize(CountBitsHex(value, hexStart));
    else
      m_bits.resize(CountBitsBin(value, binStart));
    Insert(0, value);
  }

  void BitRegister::SetZeros()
  {
    if (Empty())
      return;
    memset(m_bits.data(), 0, m_bits.size());
  }

  void BitRegister::SetZeros(size_t count)
  {
    m_bits.resize(count);
    memset(m_bits.data(), 0, count);
  }
  
  void BitRegister::SetOnes()
  {
    if (Empty())
      return;
    memset(m_bits.data(), 1, m_bits.size());
  }

  void BitRegister::SetOnes(size_t count)
  {
    m_bits.resize(count);
    memset(m_bits.data(), 1, count);
  }

  
  void BitRegister::SetNoBitValue(uint8_t bit)
  {
    m_no_data = bit == 0 ? 0 : 1;
  }
  
  std::string BitRegister::ToBinaryString() const
  {
    if (Empty())
      return std::string("");
    std::string out(Size(), '0');
    for (size_t i = 0; i < Size(); i++)
    {
      out[i] = m_bits[i] == 0 ? '0' : '1';
    }
    return out;
  }
  
  std::string BitRegister::ToHexString() const
  {
    const char digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    if (Empty())
      return std::string("");
    size_t partial = Size() & 3;
    size_t num_digits = Size() / 4 + (partial != 0 ? 1 : 0);
    std::string out(num_digits + 2, '0');
    out[0] = '0';
    out[1] = 'x';
    size_t idx = 0;
    size_t digit_idx = 2;
    size_t digit = 0;
    if (partial != 0)
    {
      for (idx = 0; idx < partial; idx++)
      {
        digit <<= 1;
        digit |= m_bits[idx];
      }
      out[digit_idx++] = digits[digit];
    }
    while (idx < Size())
    {
      digit  = m_bits[idx++] << 3;
      digit |= m_bits[idx++] << 2;
      digit |= m_bits[idx++] << 1;
      digit |= m_bits[idx++] << 0;
      out[digit_idx++] = digits[digit];
    }
    return out;
  }

  std::ostream &operator<<(std::ostream &os, const BitRegister &reg)
  {
    if (reg.Empty())
    {
      os << "[ empty ]";
      return os;
    }
    os << "[ " << reg.Size() << ": " << reg.ToHexString() << " ]";
    return os;
  }
  
  BitRegister::BitRegister()
  {
  }

  BitRegister::BitRegister(size_t count)
  {
    SetZeros(count);
  }
  
  BitRegister::BitRegister(size_t count, uint8_t bit)
  {
    if (bit == 0)
      SetZeros(count);
    else
      SetOnes(count);
  }
} // Util
