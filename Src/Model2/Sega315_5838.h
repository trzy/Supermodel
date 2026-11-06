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
 * Sega315_5838.h
 *
 * The Sega 315-5838 / 317-0229 compression and encryption chip, ported from
 * MAME's sega/315-5838_317-0229_comp.cpp. On Model 2, Dead or Alive has the
 * 317-0229: it decompresses a string from i960 RAM that the game checks at
 * start-up.
 *
 * Based on original work by David Haywood, Samuel Neves, Peter Wilhelmsen and
 * Morten Shearman Kirkegaard in MAME. Portions Copyright (c) David Haywood,
 * Samuel Neves, Peter Wilhelmsen and Morten Shearman Kirkegaard, used under
 * the BSD 3-Clause License: see Docs/README.txt.
 */

#ifndef INCLUDED_SEGA315_5838_H
#define INCLUDED_SEGA315_5838_H

#include <cstdint>
#include <functional>

class CBlockFile;

namespace Model2
{
  class C315_5838
  {
  public:
    enum class Variant : uint8_t { SEGA_315_5838, SEGA_317_0229, SEGA_317_0230, SEGA_317_0231 };

    // source: a word address -> the encrypted word there
    void Init(Variant variant, std::function<uint16_t(uint32_t)> source);
    void Reset();

    void SourceWrite(uint32_t data, uint32_t mask);
    void TableWrite(unsigned offset, uint16_t data);
    uint16_t DataRead();

    void SaveState(CBlockFile *state, const char *name);
    void LoadState(CBlockFile *state, const char *name);

  private:
    uint16_t Decipher(uint16_t ciphertext) const;
    uint8_t DecompressByte();

    Variant m_variant = Variant::SEGA_315_5838;
    std::function<uint16_t(uint32_t)> m_source_cb;

    uint16_t m_tree[24] = {};
    uint8_t m_dictionary[256] = {};
    uint8_t m_tree_words = 0;
    uint16_t m_dictionary_bytes = 0;
    bool m_upload_dictionary = false;

    uint32_t m_source = 0;
    uint16_t m_word = 0;
    uint8_t m_bits = 0;
    uint16_t m_output = 0xffff;
    bool m_abort = false;
  };
}

#endif  // INCLUDED_SEGA315_5838_H
