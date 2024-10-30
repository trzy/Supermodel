/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2016 Bart Trzynadlowski, Nik Henson 
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
 * Crypto.h
 *
 * Header file for security board encryption device. This code was taken from
 * MAME (http://mamedev.org).
 */

// license:BSD-3-Clause
// copyright-holders:David Haywood

#pragma once

#ifndef __SEGA315_5881_CRYPT__
#define __SEGA315_5881_CRYPT__

#include <cstdint>
#include <memory>
#include <functional>

class CBlockFile;

class CCrypto
{
public:
	// construction/destruction
	CCrypto();
	
  void SaveState(CBlockFile *SaveState);
  void LoadState(CBlockFile *SaveState);
  void Init(uint32_t encryptionKey, std::function<uint16_t(uint32_t)> ReadRAMCallback);
  void Reset();


	uint16_t Decrypt(uint8_t **base);
	void SetAddressLow(uint16_t data);
	void SetAddressHigh(uint16_t data);
	void SetSubKey(uint16_t data);

	std::function<uint16_t(uint32_t)> m_read;

	/*
	static void set_read_cb(device_t &device,sega_m2_read_delegate readcb)
	{
		sega_315_5881_crypt_device &dev = downcast<sega_315_5881_crypt_device &>(device);
		dev.m_read = readcb;
	}
	*/

private:

	uint32_t key;

	std::unique_ptr<uint8_t[]> buffer;
	std::unique_ptr<uint8_t[]> line_buffer;
	std::unique_ptr<uint8_t[]> line_buffer_prev;
	uint32_t prot_cur_address;
	uint16_t subkey, dec_hist;
	uint32_t dec_header;

	bool enc_ready;

	int buffer_pos, line_buffer_pos, line_buffer_size, buffer_bit, buffer_bit2;
	uint8_t buffer2[2];
	uint16_t buffer2a;

	int block_size;
	int block_pos;
	int block_numlines;
	int done_compression;

	struct sbox {
		uint8_t table[64];
		int inputs[6];      // positions of the inputs bits, -1 means no input except from key
		int outputs[2];     // positions of the output bits
	};

	static const sbox fn1_sboxes[4][4];
	static const sbox fn2_sboxes[4][4];

	static constexpr int FN1GK = 38;
	static constexpr int FN2GK = 32;
	static const int fn1_game_key_scheduling[FN1GK][2];
	static const int fn2_game_key_scheduling[FN2GK][2];
	static const int fn1_sequence_key_scheduling[20][2];
	static const int fn2_sequence_key_scheduling[16];
	static const int fn2_middle_result_scheduling[16];

	static const uint8_t trees[9][2][32];

	int feistel_function(int input, const struct sbox *sboxes, uint32_t subkeys);
	uint16_t block_decrypt(uint32_t game_key, uint16_t sequence_key, uint16_t counter, uint16_t data);

	uint16_t get_decrypted_16();
	int get_compressed_bit();

	void enc_start();
	void enc_fill();
	void line_fill();

};

#endif
