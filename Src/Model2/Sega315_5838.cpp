/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * Sega315_5838.cpp
 *
 * The Sega 315-5838 / 317-0229 compression and encryption chip, ported from
 * MAME's sega/315-5838_317-0229_comp.cpp (see the header).
 *
 * Based on original work by David Haywood, Samuel Neves, Peter Wilhelmsen and
 * Morten Shearman Kirkegaard in MAME. Portions Copyright (c) David Haywood,
 * Samuel Neves, Peter Wilhelmsen and Morten Shearman Kirkegaard, used under
 * the BSD 3-Clause License: see Docs/README.txt.
 */

#include "Sega315_5838.h"
#include "BlockFile.h"
#include <algorithm>
#include <iterator>

namespace Model2
{
  static inline unsigned Bit(unsigned v, unsigned b) { return (v >> b) & 1; }
  static inline unsigned Bits(unsigned v, unsigned b, unsigned n) { return (v >> b) & ((1u << n) - 1); }

  void C315_5838::Init(Variant variant, std::function<uint16_t(uint32_t)> source)
  {
    m_variant = variant;
    m_source_cb = std::move(source);
    Reset();
  }

  void C315_5838::Reset()
  {
    std::fill(std::begin(m_tree), std::end(m_tree), 0);
    std::fill(std::begin(m_dictionary), std::end(m_dictionary), 0);
    m_tree_words = 0;
    m_dictionary_bytes = 0;
    m_upload_dictionary = false;
    m_source = 0;
    m_word = 0;
    m_bits = 0;
    m_output = 0xffff;
    m_abort = false;
  }

  uint16_t C315_5838::Decipher(uint16_t ciphertext) const
  {
    struct CipherParameters
    {
      uint8_t routing[4];
      uint8_t sboxes[4][8];
      uint8_t affine[4];
    };
    static constexpr CipherParameters CIPHERS[] =
    {
      { // 315-5838
        { 0, 1, 2, 3 },
        { { 6, 7, 2, 0, 5, 1, 3, 4 }, { 5, 3, 7, 2, 1, 6, 4, 0 },
          { 2, 0, 6, 1, 3, 7, 4, 5 }, { 6, 5, 7, 4, 1, 3, 0, 2 } },
        { 0x14, 0x18, 0x02, 0x15 }
      },
      { // 317-0229
        { 3, 2, 0, 1 },
        { { 6, 0, 5, 4, 7, 2, 1, 3 }, { 5, 6, 1, 3, 7, 0, 4, 2 },
          { 2, 6, 0, 1, 3, 4, 7, 5 }, { 6, 5, 7, 4, 1, 3, 0, 2 } },
        { 0x12, 0x11, 0x04, 0x1a }
      },
      { // 317-0230
        { 2, 3, 1, 0 },
        { { 6, 2, 7, 0, 5, 3, 1, 4 }, { 5, 3, 7, 2, 1, 6, 4, 0 },
          { 2, 1, 3, 5, 6, 0, 4, 7 }, { 6, 1, 3, 5, 7, 0, 2, 4 } },
        { 0x14, 0x18, 0x02, 0x15 }
      },
      { // 317-0231
        { 1, 0, 3, 2 },
        { { 6, 0, 5, 4, 7, 2, 1, 3 }, { 5, 1, 6, 3, 7, 4, 0, 2 },
          { 2, 1, 3, 5, 6, 0, 4, 7 }, { 6, 3, 1, 5, 7, 2, 0, 4 } },
        { 0x12, 0x11, 0x04, 0x1a }
      }
    };
    static constexpr uint8_t OUTPUT_BITS[4][3] = { { 0, 1, 7 }, { 2, 5, 9 }, { 4, 10, 13 }, { 6, 11, 15 } };
    static constexpr uint8_t AFFINE_BITS[4] = { 3, 8, 12, 14 };

    auto const x = [ciphertext](unsigned bit) { return Bit(ciphertext, bit); };
    uint8_t const lanes[4] =
    {
      uint8_t((x(5) ^ x(12)) | ((x(7) ^ x(9)) << 1) | (x(14) << 2)),
      uint8_t(x(6) | (x(8) << 1) | (x(15) << 2)),
      uint8_t((x(1) ^ x(8)) | (x(7) << 1) | (x(12) << 2)),
      uint8_t(x(3) | (x(4) << 1) | (x(10) << 2))
    };
    uint8_t const affine = uint8_t(x(0) | ((x(0) ^ x(2)) << 1) | ((x(11) ^ x(14)) << 2) | ((x(4) ^ x(13)) << 3) | 16);
    CipherParameters const &cipher = CIPHERS[unsigned(m_variant)];
    uint16_t result = 0;
    for (unsigned group = 0; group < 4; ++group)
    {
      uint8_t const value = cipher.sboxes[group][lanes[cipher.routing[group]]];
      for (unsigned bit = 0; bit < 3; ++bit)
        result |= uint16_t(Bit(value, bit) << OUTPUT_BITS[group][bit]);

      uint8_t parity = affine & cipher.affine[group];
      parity ^= parity >> 4;
      parity ^= parity >> 2;
      parity ^= parity >> 1;
      result |= uint16_t(Bit(parity, 0) << AFFINE_BITS[group]);
    }
    return result;
  }

  void C315_5838::SourceWrite(uint32_t data, uint32_t mask)
  {
    m_source = (m_source & ~mask) | (data & mask);
    m_source &= 0x007fffff;
    m_word = 0;
    m_bits = 0;
    m_abort = false;
  }

  void C315_5838::TableWrite(unsigned offset, uint16_t data)
  {
    if (!Bit(offset, 0))
    {
      m_upload_dictionary = Bit(data, 7) != 0;
      if (m_upload_dictionary)
        m_dictionary_bytes = 0;
      else
        m_tree_words = 0;
    }
    else if (m_upload_dictionary)
    {
      if (m_dictionary_bytes < std::size(m_dictionary))
      {
        m_dictionary[m_dictionary_bytes++] = uint8_t(data >> 8);
        m_dictionary[m_dictionary_bytes++] = uint8_t(data & 0xff);
      }
    }
    else if (m_tree_words < std::size(m_tree))
    {
      m_tree[m_tree_words++] = data;
    }
  }

  uint8_t C315_5838::DecompressByte()
  {
    if (m_abort || m_tree_words != std::size(m_tree) || m_dictionary_bytes != std::size(m_dictionary))
      return 0xff;

    uint16_t code = 0;
    for (unsigned length = 1; length <= 12; ++length)
    {
      if (!m_bits)
      {
        m_word = Decipher(m_source_cb ? m_source_cb(m_source) : 0xffff);
        m_source = (m_source + 1) & 0x007fffff;
        m_bits = 16;
      }
      code = uint16_t((code << 1) | Bit(m_word, --m_bits));

      for (unsigned entry = 0; entry < std::size(m_tree); entry += 2)
      {
        if (Bits(m_tree[entry], 8, 8) != length)
          continue;

        uint16_t const first = uint16_t(m_tree[entry + 1] >> (12 - length));
        uint16_t limit = uint16_t(1U << length);
        for (unsigned next = entry + 2; next < std::size(m_tree); next += 2)
        {
          if (m_tree[next + 1] != m_tree[entry + 1])
          {
            limit = uint16_t(m_tree[next + 1] >> (12 - length));
            break;
          }
        }

        if (code >= first && code < limit)
        {
          unsigned const index = Bits(m_tree[entry], 0, 8) + code - first;
          if (index < std::size(m_dictionary))
            return m_dictionary[index];
        }
      }
    }

    m_abort = true;   // an invalid compressed code
    return 0xff;
  }

  uint16_t C315_5838::DataRead()
  {
    m_output = uint16_t(DecompressByte()) << 8;
    m_output |= DecompressByte();
    return m_output;
  }

  void C315_5838::SaveState(CBlockFile *state, const char *name)
  {
    state->NewBlock(name, __FILE__);
    state->Write(m_tree, sizeof(m_tree));
    state->Write(m_dictionary, sizeof(m_dictionary));
    state->Write(&m_tree_words, sizeof(m_tree_words));
    state->Write(&m_dictionary_bytes, sizeof(m_dictionary_bytes));
    state->Write(&m_upload_dictionary, sizeof(m_upload_dictionary));
    state->Write(&m_source, sizeof(m_source));
    state->Write(&m_word, sizeof(m_word));
    state->Write(&m_bits, sizeof(m_bits));
    state->Write(&m_output, sizeof(m_output));
    state->Write(&m_abort, sizeof(m_abort));
  }

  void C315_5838::LoadState(CBlockFile *state, const char *name)
  {
    if (Result::OKAY != state->FindBlock(name))
      return;
    state->Read(m_tree, sizeof(m_tree));
    state->Read(m_dictionary, sizeof(m_dictionary));
    state->Read(&m_tree_words, sizeof(m_tree_words));
    state->Read(&m_dictionary_bytes, sizeof(m_dictionary_bytes));
    state->Read(&m_upload_dictionary, sizeof(m_upload_dictionary));
    state->Read(&m_source, sizeof(m_source));
    state->Read(&m_word, sizeof(m_word));
    state->Read(&m_bits, sizeof(m_bits));
    state->Read(&m_output, sizeof(m_output));
    state->Read(&m_abort, sizeof(m_abort));
  }
}
