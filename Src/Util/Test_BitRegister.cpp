#include "Util/BitRegister.h"
#include <iostream>

int main(int argc, char **argv)
{
  std::vector<std::string> expected;
  std::vector<std::string> results;
  
  Util::BitRegister reg;
  
  // Test
  reg.SetZeros(1);
  expected.push_back("0");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.SetZeros(2);
  expected.push_back("00");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.SetZeros(3);
  expected.push_back("000");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.SetOnes(1);
  expected.push_back("1");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.SetOnes(2);
  expected.push_back("11");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.SetOnes(3);
  expected.push_back("111");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  expected.push_back("");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("101000110101111100101");
  expected.push_back("101000110101111100101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("%101000110101111100101");
  expected.push_back("101000110101111100101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0b101000110101111100101");
  expected.push_back("101000110101111100101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0B101000110101111100101");
  expected.push_back("101000110101111100101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("$1d");
  expected.push_back("00011101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0x1d");
  expected.push_back("00011101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0X1d");
  expected.push_back("00011101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("110010101");
  expected.push_back("1");
  results.push_back(reg.RemoveFromRight() == 1 ? "1" : "0");
  expected.push_back("11001010");
  results.push_back(reg.ToBinaryString());
  
  expected.push_back("0");
  results.push_back(reg.RemoveFromRight() == 1 ? "1" : "0");
  expected.push_back("1100101");
  results.push_back(reg.ToBinaryString());
  
  expected.push_back("1");
  results.push_back(reg.RemoveFromRight() == 1 ? "1" : "0");
  expected.push_back("110010");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("110010101");
  expected.push_back("1");
  results.push_back(reg.RemoveFromLeft() == 1 ? "1" : "0");
  expected.push_back("10010101");
  results.push_back(reg.ToBinaryString());
  
  expected.push_back("1");
  results.push_back(reg.RemoveFromLeft() == 1 ? "1" : "0");
  expected.push_back("0010101");
  results.push_back(reg.ToBinaryString());
  
  expected.push_back("0");
  results.push_back(reg.RemoveFromLeft() == 1 ? "1" : "0");
  expected.push_back("010101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0x12345");
  reg.RemoveFromLeft(1);
  expected.push_back("0010010001101000101");
  results.push_back(reg.ToBinaryString());
  
  reg.RemoveFromLeft(2);
  expected.push_back("10010001101000101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0x12345");
  reg.ShiftLeft(1);
  expected.push_back("00100100011010001010");
  results.push_back(reg.ToBinaryString());
  
  reg.ShiftLeft(2);
  expected.push_back("10010001101000101000");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0x12345");
  reg.RemoveFromRight(1);
  expected.push_back("0001001000110100010");
  results.push_back(reg.ToBinaryString());
  
  reg.RemoveFromRight(2);
  expected.push_back("00010010001101000");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.Set("0x12345");
  reg.ShiftRight(1);
  expected.push_back("00001001000110100010");
  results.push_back(reg.ToBinaryString());

  reg.ShiftRight(2);
  expected.push_back("00000010010001101000");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.AddToRight(1);
  reg.AddToRight(1);
  reg.AddToRight(0);
  reg.AddToRight(1);
  reg.AddToRight(0);
  expected.push_back("11010");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.AddToRight(1);
  reg.AddToRight(1);
  reg.AddToRight(0);
  reg.AddToRight(1);
  reg.AddToRight(0);
  reg.RemoveFromRight();
  expected.push_back("1101");
  results.push_back(reg.ToBinaryString());
  
  reg.RemoveFromLeft();
  expected.push_back("101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Reset();
  reg.AddToLeft(1);
  reg.AddToLeft(0);
  reg.AddToLeft(1);
  reg.AddToLeft(1);
  reg.AddToLeft(0);
  reg.AddToLeft(1);
  reg.AddToLeft(0);
  expected.push_back("0101101");
  results.push_back(reg.ToBinaryString());
  
  reg.RemoveFromLeft();
  expected.push_back("101101");
  results.push_back(reg.ToBinaryString());
  
  // Test
  reg.Set("11000");
  results.push_back(reg.ShiftOutLeft(0) == 0 ? "0" : "1");
  results.push_back(reg.ShiftOutLeft(1) == 0 ? "0" : "1");
  results.push_back(reg.ShiftOutLeft(0) == 0 ? "0" : "1");
  results.push_back(reg.ToBinaryString());
  expected.push_back("1");
  expected.push_back("1");
  expected.push_back("0");
  expected.push_back("00010");
  
  // Test
  reg.Set("11001");
  results.push_back(reg.ShiftOutRight(0) == 0 ? "0" : "1");
  results.push_back(reg.ShiftOutRight(1) == 0 ? "0" : "1");
  results.push_back(reg.ShiftOutRight(0) == 0 ? "0" : "1");
  results.push_back(reg.ToBinaryString());
  expected.push_back("1");
  expected.push_back("0");
  expected.push_back("0");
  expected.push_back("01011");  
  
  // Test
  reg.Reset();
  reg.SetZeros(32);
  reg.Insert(31, "0xffff");
  expected.push_back("00000000000000000000000000000001");
  results.push_back(reg.ToBinaryString());
  
  reg.Insert(16, "$1234");
  expected.push_back("00000000000000000001001000110100");
  results.push_back(reg.ToBinaryString());
  
  reg.Insert(14, "101");
  expected.push_back("00000000000000101001001000110100");
  results.push_back(reg.ToBinaryString());
  
  reg.Insert(29, "0110");
  expected.push_back("00000000000000101001001000110011");
  results.push_back(reg.ToBinaryString());
  
  // Test hex printing
  reg.SetOnes(1);
  expected.push_back("0x1");
  results.push_back(reg.ToHexString());
  
  reg.SetZeros(1);
  expected.push_back("0x0");
  results.push_back(reg.ToHexString());
    
  reg.SetZeros(2);
  expected.push_back("0x0");
  results.push_back(reg.ToHexString());
    
  reg.SetZeros(3);
  expected.push_back("0x0");
  results.push_back(reg.ToHexString());
  
  reg.SetZeros(4);
  expected.push_back("0x0");
  results.push_back(reg.ToHexString());
    
  reg.SetZeros(5);
  expected.push_back("0x00");
  results.push_back(reg.ToHexString());

  reg.Set("11001");
  expected.push_back("0x19");
  results.push_back(reg.ToHexString());
    
  reg.Set("111001");
  expected.push_back("0x39");
  results.push_back(reg.ToHexString());
  
  reg.Set("1111001");
  expected.push_back("0x79");
  results.push_back(reg.ToHexString());
  
  reg.Set("01111001");
  expected.push_back("0x79");
  results.push_back(reg.ToHexString());
    
  reg.Set("101111001");
  expected.push_back("0x179");
  results.push_back(reg.ToHexString());
  
  // Test
  reg.Set("0x12345678");
  expected.push_back("ok");
  results.push_back(reg.GetBits() == 0x12345678 ? "ok" : "bad");
  expected.push_back("ok");
  results.push_back(reg.GetBits(0, 8) == 0x12 ? "ok" : "bad");
  expected.push_back("ok");
  results.push_back(reg.GetBits(26, 6) == 0x38 ? "ok" : "bad");

  // Check results
  size_t num_failed = 0;
  for (size_t i = 0; i < expected.size(); i++)
  {
    if (expected[i] != results[i])
    {
      std::cout << "Test #" << i << " FAILED. Expected \"" << expected[i] << "\" but got \"" << results[i] << '\"' << std::endl;
      num_failed++;
    }
  }
  
  if (num_failed == 0)
    std::cout << "All tests passed!" << std::endl;
  return 0;
}