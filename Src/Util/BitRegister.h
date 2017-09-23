#ifndef INCLUDED_UTIL_BITREGISTER_H
#define INCLUDED_UTIL_BITREGISTER_H

#include <cstdint>
#include <vector>
#include <ostream>

namespace Util
{
  class BitRegister
  {
  public:
    inline bool Empty() const
    {
      return m_bits.empty();
    }

    inline size_t Size() const
    {
      return m_bits.size();
    }

    // Functions that grow/shrink the bit register
    void AddToRight(uint8_t bit);
    void AddToLeft(uint8_t bit);
    uint8_t RemoveFromLeft();
    void RemoveFromLeft(size_t count);
    uint8_t RemoveFromRight();
    void RemoveFromRight(size_t count);
    
    // Functions that preserve the current register size, shifting in the "no
    // data" value as needed
    void ShiftRight(size_t count);
    void ShiftLeft(size_t count);
    
    // Functions that preserve the register size, shifting a bit in one side
    // and ejecting one out the other
    uint8_t ShiftOutRight(uint8_t bit);
    uint8_t ShiftOutLeft(uint8_t bit);
    
    // Functions that insert bits, clipped against current register size
    void SetBit(size_t bitPos, uint8_t value);
    void Insert(size_t bitPos, const std::string &value);
    
    // Functions that reset the contents (and size) of the register
    void Set(const std::string &value);
    void SetZeros(size_t count);
    void SetOnes(size_t count);
    void Reset();
    
    // Configuration state
    void SetNoBitValue(uint8_t bit);

    // String serialization
    std::string ToBinaryString() const;
    std::string ToHexString() const;
    friend std::ostream &operator<<(std::ostream &os, const BitRegister &reg);
      
    // Constructors
    BitRegister();
    BitRegister(size_t count);
    BitRegister(size_t count, uint8_t bit);

  private:
    /*
     * Vector layout:
     *
     *       Index:   0   1   2   3   ...   N
     *              +---+---+---+---+-...-+---+
     * <-- left --  | 1 | 0 | 1 | 1 | ... | 1 | -- right -->
     *              +---+---+---+---+-...-+---+
     *
     * "Left" means lower indices in the array. To remain flexible and agnostic
     * about MSB vs. LSB and bit numbers, all functions are explicit about 
     * whether they are shifting in/out of the left/right side. It is up to the
     * user to establish a consistent convention according to their use case.
     */
    std::vector<uint8_t> m_bits;
    uint8_t m_no_data = 0;  // by default, assume non-existent bits are 0
      
    uint8_t GetLeftMost() const;
    uint8_t GetRightMost() const;
    static size_t HexStart(const std::string &value);
    static size_t BinStart(const std::string &value);
    static size_t CountBitsHex(const std::string &value, size_t startPos);
    static size_t CountBitsBin(const std::string &value, size_t startPos);
  };
} // Util

#endif  // INCLUDED_UTIL_BITREGISTER_H
