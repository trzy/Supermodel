#include "Util/BitRegister.h"
#include <cstring>
#include <cctype>

namespace Util
{
  uint8_t BitRegister::GetLeftMost() const
  {
    return m_bits.empty() ? m_no_data : m_bits.front();
  }
  
  uint8_t BitRegister::GetRightMost() const
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
 
  // Shift a bit into the right side, growing the vector by 1
  void BitRegister::ShiftInRight(uint8_t bit)
  {
    m_bits.push_back(!!bit);
  }
  
  // Shift a bit into the left side, growing the vector by 1
  void BitRegister::ShiftInLeft(uint8_t bit)
  {
    m_bits.push_back(0);
    ShiftRight(1);
    m_bits[0] = !!bit;
  }

  // Shift left by 1, returning ejected bit (shrinks vector)
  uint8_t BitRegister::ShiftOutLeft()
  {
    uint8_t ejected = GetLeftMost();
    ShiftOutLeft(1);
    return ejected;
  }
  
  // Shift left and lose bits (shrinks vector)
  void BitRegister::ShiftOutLeft(size_t count)
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
  uint8_t BitRegister::ShiftOutRight()
  {
    uint8_t ejected = GetRightMost();
    ShiftOutRight(1);
    return ejected;
  }

  // Shift right and lose bits (shrinks vector)
  void BitRegister::ShiftOutRight(size_t count)
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
  
  // Shift right and lose right-most bits, shifting in new bits from left to
  // preserve vector size
  void BitRegister::ShiftRight(size_t count)
  {
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
    if (count < m_bits.size())
    {
      // Shift over bits to the left
      memmove(m_bits.data(), m_bits.data() + count, m_bits.size() - count);
    }
    
    // Fill in the right with "no data"
    memset(m_bits.data() + m_bits.size() - count, m_no_data, count);
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

  void BitRegister::SetZeros(size_t count)
  {
    m_bits.resize(count);
    memset(m_bits.data(), 0, count);
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
} // Util
