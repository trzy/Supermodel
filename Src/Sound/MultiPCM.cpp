// license:BSD-3-Clause
// copyright-holders:Miguel Angel Horna
/*
 * Ported to Supermodel from MAME (src/devices/sound/multipcm.cpp, src/devices/sound/gew.cpp).
 *
 * Yamaha YMW-258-F 'GEW8' (aka Sega 315-5560) emulation.
 *
 * by Miguel Angel Horna (ElSemi) for Model 2 Emulator and MAME.
 * Information by R. Belmont and the YMF278B (OPL4) manual.
 *
 * voice registers:
 * 0: Pan (high nibble), effect send level (low nibble, 0-8)
 * 1: Index of sample
 * 2: LSB of pitch (low 2 bits seem unused so)
 * 3: MSB of pitch (ooooppppppppppxx) (o=octave (4 bit signed), p=pitch (10 bits), x=unused?
 * 4: voice control: top bit = 1 for key on, 0 for key off
 * 5: bit 0: 0: interpolate volume changes, 1: direct set volume,
 *    bits 1-7 = volume attenuate (0=max, 7f=min)
 * 6: LFO frequency + Phase LFO depth
 * 7: Attack rate (high nibble), decay 1 rate (low nibble)
 * 8: Decay level (high nibble), decay 2 rate (low nibble)
 * 9: Rate correction (high nibble), release rate (low nibble)
 * 10: Amplitude LFO size
 * Registers 6-10 are loaded from the sample header when the sample is selected.
 *
 * The first sample ROM contains a variable length metadata table with 12
 * bytes per instrument sample. This is very similar to the YMF278B 'OPL4'.
 * This sample format might be derived from the one used by the older YM7138 'GEW6' chip.
 *
 * The first 3 bytes are the offset into the file (big endian). (0, 1, 2).
 * Bit 23 is unknown.
 * Bit 22 is the sample format flag: 0 for 8-bit linear, 1 for 12-bit linear.
 * Bit 21 is used by the MU5 on some samples for as-yet unknown purposes. (YMW-258-F has 22 address pins.)
 * The next 2 bytes are the loop start point, in samples (big endian) (3, 4)
 * The next 2 are the 2's complement negation of of the total number of samples (big endian) (5, 6)
 * The next byte is LFO freq + depth (copied to reg 6) (7)
 * The next 3 are envelope params (Attack, Decay1 and 2, sustain level, release, Key Rate Scaling) (copied to reg 7-9) (8, 9, 10)
 * The next byte is Amplitude LFO size (copied to reg 10) (11)
 *
 * TODO
 * - http://dtech.lv/techarticles_yamaha_chips.html indicates FM support, which we don't have yet.
 *
 * Supermodel notes: the effect DSP send (enable_dsp_send/dsp_cd_callback, register
 * offset 0xd) is MU-series only and is not ported; the send level is still stored
 * in the slot but no send output is produced. MULTIPCM_LOG_SAMPLES (WAV dumping)
 * is not ported.
 */

#include "Sound/MultiPCM.h"
#include "OSD/Logger.h"
#include <algorithm>
#include <cmath>

/*******************************
        ENVELOPE SECTION
*******************************/

// Times are based on a 44100Hz timebase. It's adjusted to the actual sampling rate on startup

const double CMultiPCM::BASE_TIMES[64] = {
	0,          0,          0,          0,
	6222.95,    4978.37,    4148.66,    3556.01,
	3111.47,    2489.21,    2074.33,    1778.00,
	1555.74,    1244.63,    1037.19,    889.02,
	777.87,     622.31,     518.59,     444.54,
	388.93,     311.16,     259.32,     222.27,
	194.47,     155.60,     129.66,     111.16,
	97.23,      77.82,      64.85,      55.60,
	48.62,      38.91,      32.43,      27.80,
	24.31,      19.46,      16.24,      13.92,
	12.15,      9.75,       8.12,       6.98,
	6.08,       4.90,       4.08,       3.49,
	3.04,       2.49,       2.13,       1.90,
	1.72,       1.41,       1.18,       1.04,
	0.91,       0.73,       0.59,       0.50,
	0.45,       0.45,       0.45,       0.45
};

constexpr uint32_t CMultiPCM::TL_SHIFT;
constexpr uint32_t CMultiPCM::EG_SHIFT;

void CMultiPCM::retrigger_sample(slot_t &slot)
{
	slot.m_offset = 0;
	slot.m_prev_sample = 0;
	slot.m_total_level = slot.m_dest_total_level << TL_SHIFT;

	envelope_generator_calc(slot);
	slot.m_envelope_gen.m_state = state_t::ATTACK;
	slot.m_envelope_gen.m_volume = (0x3ff - 0x2a0) << EG_SHIFT;
}

void CMultiPCM::update_step(slot_t &slot)
{
	const uint8_t oct = (slot.m_octave - 1) & 0xf;
	uint32_t pitch = m_freq_step_table[slot.m_pitch];
	if (oct & 0x8)
	{
		pitch >>= (16 - oct);
	}
	else
	{
		pitch <<= oct;
	}
	slot.m_step = (uint32_t)(pitch / m_rate);
}

void CMultiPCM::envelope_generator_init(const double (&rates)[64], double attack_decay_ratio)
{
	for (int32_t i = 4; i < 0x40; ++i)
	{
		// Times are based on 44100Hz clock, adjust to real chip clock
		m_attack_step[i] = (uint32_t)((float)(0x400 << EG_SHIFT) / (float)(rates[i] * 44100.0 / 1000.0));
		m_decay_release_step[i] = (uint32_t)((float)(0x400 << EG_SHIFT) / (float)(rates[i] * attack_decay_ratio * 44100.0 / 1000.0));
	}
	m_attack_step[0] = m_attack_step[1] = m_attack_step[2] = m_attack_step[3] = 0;
	m_attack_step[0x3f] = 0x400 << EG_SHIFT;
	m_decay_release_step[0] = m_decay_release_step[1] = m_decay_release_step[2] = m_decay_release_step[3] = 0;
}

int32_t CMultiPCM::envelope_generator_update(slot_t &slot)
{
	switch (slot.m_envelope_gen.m_state)
	{
	case state_t::ATTACK:
		slot.m_envelope_gen.m_volume += (int32_t)((int64_t((0x817 << (EG_SHIFT - 1)) - slot.m_envelope_gen.m_volume) * slot.m_envelope_gen.m_attack_rate) >> 24);
		if (slot.m_envelope_gen.m_volume >= (0x3ff << EG_SHIFT))
		{
			slot.m_envelope_gen.m_state = state_t::DECAY1;
			if (slot.m_envelope_gen.m_decay1_rate >= (0x400 << EG_SHIFT)) //Skip DECAY1, go directly to DECAY2
			{
				slot.m_envelope_gen.m_state = state_t::DECAY2;
			}
			slot.m_envelope_gen.m_volume = 0x3ff << EG_SHIFT;
		}
		break;
	case state_t::DECAY1:
		slot.m_envelope_gen.m_volume -= slot.m_envelope_gen.m_decay1_rate;
		if (slot.m_envelope_gen.m_volume <= 0)
		{
			slot.m_envelope_gen.m_volume = 0;
		}
		if (slot.m_envelope_gen.m_volume >> (EG_SHIFT + 6) <= slot.m_envelope_gen.m_decay_level)
		{
			slot.m_envelope_gen.m_state = state_t::DECAY2;
		}
		break;
	case state_t::DECAY2:
		slot.m_envelope_gen.m_volume -= slot.m_envelope_gen.m_decay2_rate;
		if (slot.m_envelope_gen.m_volume <= 0)
		{
			slot.m_envelope_gen.m_volume = 0;
		}
		break;
	case state_t::RELEASE:
		slot.m_envelope_gen.m_volume -= slot.m_envelope_gen.m_release_rate;
		if (slot.m_envelope_gen.m_volume <= 0)
		{
			slot.m_envelope_gen.m_volume = 0;
			slot.m_playing = false;
		}
		break;
	default:
		return 1 << TL_SHIFT;
	}

	// TODO: this is currently only implemented for GEW7, it's probably not accurate
	if (slot.m_envelope_gen.m_reverb && slot.m_envelope_gen.m_state != state_t::ATTACK
		&& (slot.m_envelope_gen.m_volume >> EG_SHIFT) <= 0x300)
	{
		slot.m_envelope_gen.m_decay1_rate  = m_decay_release_step[17];
		slot.m_envelope_gen.m_decay2_rate  = m_decay_release_step[17];
		slot.m_envelope_gen.m_release_rate = m_decay_release_step[17];
	}

	return m_linear_to_exp_volume[slot.m_envelope_gen.m_volume >> EG_SHIFT];
}

uint32_t CMultiPCM::get_rate(uint32_t *steps, int32_t rate, uint32_t val)
{
	if (val == 0)
	{
		return steps[0];
	}
	if (val == 0xf)
	{
		return steps[0x3f];
	}

	const int r = std::clamp(4 * (int)val + rate, 0, 0x3f);
	return steps[r];
}

void CMultiPCM::envelope_generator_calc(slot_t &slot)
{
	int32_t octave = slot.m_octave;
	if (octave & 8) {
		octave = octave - 16;
	}

	int32_t rate;
	if (slot.m_sample.m_key_rate_scale != 0xf)
	{
		rate = (octave + slot.m_sample.m_key_rate_scale) * 2 + ((slot.m_pitch >> 9) & 1);
	}
	else
	{
		rate = 0;
	}

	slot.m_envelope_gen.m_attack_rate = get_rate(m_attack_step.get(), rate, slot.m_sample.m_attack_reg);
	slot.m_envelope_gen.m_decay1_rate = get_rate(m_decay_release_step.get(), rate, slot.m_sample.m_decay1_reg);
	slot.m_envelope_gen.m_decay2_rate = get_rate(m_decay_release_step.get(), rate, slot.m_sample.m_decay2_reg);
	slot.m_envelope_gen.m_release_rate = get_rate(m_decay_release_step.get(), rate, slot.m_sample.m_release_reg);
	slot.m_envelope_gen.m_decay_level = 0xf - slot.m_sample.m_decay_level;
	slot.m_envelope_gen.m_reverb = false;
}

/*****************************
        LFO  SECTION
*****************************/

constexpr uint32_t CMultiPCM::LFO_SHIFT;

const float CMultiPCM::LFO_FREQ[8] = // In Hertz
{
	0.168f,
	2.019f,
	3.196f,
	4.206f,
	5.215f,
	5.888f,
	6.224f,
	7.066f
};

const float CMultiPCM::PHASE_SCALE_LIMIT[8] = // In Cents
{
	0.0f,
	3.378f,
	5.065f,
	6.750f,
	10.114f,
	20.170f,
	40.180f,
	79.307f
};

const float CMultiPCM::AMPLITUDE_SCALE_LIMIT[8] = // In Decibels
{
	0.0f,
	0.4f,
	0.8f,
	1.5f,
	3.0f,
	6.0f,
	12.0f,
	24.0f
};

void CMultiPCM::lfo_init()
{
	m_pitch_table = std::make_unique<int32_t[]>(256);
	m_amplitude_table = std::make_unique<int32_t[]>(256);
	for (int32_t i = 0; i < 256; ++i)
	{
		if (i < 64)
		{
			m_pitch_table[i] = i * 2 + 128;
		}
		else if (i < 128)
		{
			m_pitch_table[i] = 383 - i * 2;
		}
		else if (i < 192)
		{
			m_pitch_table[i] = 384 - i * 2;
		}
		else
		{
			m_pitch_table[i] = i * 2 - 383;
		}

		if (i < 128)
		{
			m_amplitude_table[i] = 255 - (i * 2);
		}
		else
		{
			m_amplitude_table[i] = (i * 2) - 256;
		}
	}

	for (int32_t table = 0; table < 8; ++table)
	{
		float limit = PHASE_SCALE_LIMIT[table];
		m_pitch_scale_tables[table] = std::make_unique<int32_t[]>(256);
		for (int32_t i = -128; i < 128; ++i)
		{
			const float value = (limit * (float)i) / 128.0f;
			const float converted = powf(2.0f, value / 1200.0f);
			m_pitch_scale_tables[table][i + 128] = value_to_fixed(LFO_SHIFT, converted);
		}

		limit = -AMPLITUDE_SCALE_LIMIT[table];
		m_amplitude_scale_tables[table] = std::make_unique<int32_t[]>(256);
		for (int32_t i = 0; i < 256; ++i)
		{
			const float value = (limit * (float)i) / 256.0f;
			const float converted = powf(10.0f, value / 20.0f);
			m_amplitude_scale_tables[table][i] = value_to_fixed(LFO_SHIFT, converted);
		}
	}
}

uint32_t CMultiPCM::value_to_fixed(const uint32_t bits, const float value)
{
	const float float_shift = float(1 << bits);
	return uint32_t(float_shift * value);
}

int32_t CMultiPCM::pitch_lfo_step(lfo_t &lfo)
{
	lfo.m_phase += lfo.m_phase_step;
	int32_t p = lfo.m_table[(lfo.m_phase >> LFO_SHIFT) & 0xff];
	p = lfo.m_scale[p];
	return p << (TL_SHIFT - LFO_SHIFT);
}

int32_t CMultiPCM::amplitude_lfo_step(lfo_t &lfo)
{
	lfo.m_phase += lfo.m_phase_step;
	int32_t p = lfo.m_table[(lfo.m_phase >> LFO_SHIFT) & 0xff];
	p = lfo.m_scale[p];
	return p << (TL_SHIFT - LFO_SHIFT);
}

void CMultiPCM::lfo_compute_step(lfo_t &lfo, uint32_t lfo_frequency, uint32_t lfo_scale, int32_t amplitude_lfo)
{
	float step = (float)LFO_FREQ[lfo_frequency] * 256.0f / (float)m_rate;
	lfo.m_phase_step = uint32_t(float(1 << LFO_SHIFT) * step);
	if (amplitude_lfo)
	{
		lfo.m_table = m_amplitude_table.get();
		lfo.m_scale = m_amplitude_scale_tables[lfo_scale].get();
	}
	else
	{
		lfo.m_table = m_pitch_table.get();
		lfo.m_scale = m_pitch_scale_tables[lfo_scale].get();
	}
}

/*****************************
     multipcm_device SECTION
*****************************/

const int32_t CMultiPCM::VALUE_TO_CHANNEL[32] =
{
	0, 1, 2, 3, 4, 5, 6 , -1,
	7, 8, 9, 10,11,12,13, -1,
	14,15,16,17,18,19,20, -1,
	21,22,23,24,25,26,27, -1,
};

void CMultiPCM::init_sample(sample_t &sample, uint32_t index)
{
	uint32_t address = index * 12;

	sample.m_start = (read_byte(address) << 16) | (read_byte(address + 1) << 8) | read_byte(address + 2);
	sample.m_format = (sample.m_start>>20) & 0xfe;
	sample.m_start &= 0x3fffff;
	sample.m_loop = (read_byte(address + 3) << 8) | read_byte(address + 4);
	sample.m_end = 0x10000 - ((read_byte(address + 5) << 8) | read_byte(address + 6));
	sample.m_attack_reg = (read_byte(address + 8) >> 4) & 0xf;
	sample.m_decay1_reg = read_byte(address + 8) & 0xf;
	sample.m_decay2_reg = read_byte(address + 9) & 0xf;
	sample.m_decay_level = (read_byte(address + 9) >> 4) & 0xf;
	sample.m_release_reg = read_byte(address + 10) & 0xf;
	sample.m_key_rate_scale = (read_byte(address + 10) >> 4) & 0xf;
	sample.m_lfo_vibrato_reg = read_byte(address + 7);
	sample.m_lfo_amplitude_reg = read_byte(address + 11) & 0xf;
}

void CMultiPCM::write_slot(slot_t &slot, int32_t reg, uint8_t data)
{
	// MAME: m_stream->update(); -- the owner brings the stream up to date before calling Write()
	slot.m_regs[reg] = data;

	switch(reg)
	{
		case 0: // PANPOT
			slot.m_pan = (data >> 4) & 0xf;
			slot.m_dsp_send = data & 0xf;
			break;

		case 1: // Sample
		{
			// according to YMF278 sample write causes some base params written to the regs (envelope+lfos)
			init_sample(slot.m_sample, slot.m_regs[1] | ((slot.m_regs[2] & 1) << 8));
			slot.m_regs[7] = (slot.m_sample.m_attack_reg << 4) | slot.m_sample.m_decay1_reg;
			slot.m_regs[8] = (slot.m_sample.m_decay_level << 4) | slot.m_sample.m_decay2_reg;
			slot.m_regs[9] = (slot.m_sample.m_key_rate_scale << 4) | slot.m_sample.m_release_reg;
			write_slot(slot, 6, slot.m_sample.m_lfo_vibrato_reg);
			write_slot(slot, 10, slot.m_sample.m_lfo_amplitude_reg);

			// retrigger if key is on
			if (slot.m_playing)
				retrigger_sample(slot);

			break;
		}
		case 2: // Pitch
		case 3:
			{
				slot.m_octave = slot.m_regs[3] >> 4;
				slot.m_pitch = ((slot.m_regs[3] & 0xf) << 6) | (slot.m_regs[2] >> 2);
				update_step(slot);
			}
			break;
		case 4: // KeyOn/Off
			if (data & 0x80) // KeyOn
			{
				slot.m_playing = true;
				retrigger_sample(slot);
			}
			else
			{
				if (slot.m_playing)
				{
					if (slot.m_sample.m_release_reg != 0xf)
					{
						slot.m_envelope_gen.m_state = state_t::RELEASE;
					}
					else
					{
						slot.m_playing = false;
					}
				}
			}
			break;
		case 5: // TL + Interpolation
			slot.m_dest_total_level = (data >> 1) & 0x7f;
			if (!(data & 1)) // Interpolate TL
			{
				if ((slot.m_total_level >> TL_SHIFT) > slot.m_dest_total_level)
				{
					slot.m_total_level_step = m_total_level_steps[0]; // decrease
				}
				else
				{
					slot.m_total_level_step = m_total_level_steps[1]; // increase
				}
			}
			else
			{
				slot.m_total_level = slot.m_dest_total_level << TL_SHIFT;
			}
			break;
		case 6: // LFO frequency + Pitch LFO
		case 10: // Amplitude LFO
			slot.m_lfo_frequency = (slot.m_regs[6] >> 3) & 7;
			slot.m_vibrato = slot.m_regs[6] & 7;
			slot.m_tremolo = slot.m_regs[10] & 7;
			if (data)
			{
				lfo_compute_step(slot.m_pitch_lfo, slot.m_lfo_frequency, slot.m_vibrato, 0);
				lfo_compute_step(slot.m_amplitude_lfo, slot.m_lfo_frequency, slot.m_tremolo, 1);
			}
			break;
		case 7: // Attack rate + Decay 1 rate
		case 8: // Decay level + Decay 2 rate
		case 9: // Rate correction + Release rate
			slot.m_sample.m_attack_reg = slot.m_regs[7] >> 4;
			slot.m_sample.m_decay1_reg = slot.m_regs[7] & 0xf;
			slot.m_sample.m_decay_level = slot.m_regs[8] >> 4;
			slot.m_sample.m_decay2_reg = slot.m_regs[8] & 0xf;
			slot.m_sample.m_key_rate_scale = slot.m_regs[9] >> 4;
			slot.m_sample.m_release_reg = slot.m_regs[9] & 0xf;
			envelope_generator_calc(slot);
			break;
	}
}

uint8_t CMultiPCM::Read()
{
	return 0;
}

void CMultiPCM::Write(uint32_t offset, uint8_t data)
{
	switch(offset)
	{
		case 0: // Data write
			// Supermodel: VALUE_TO_CHANNEL yields -1 for every 8th value; MAME would then index
			// m_slots out of bounds (undefined behaviour). Such writes are dropped here.
			if (m_cur_slot >= m_voices)
				DebugLog("MultiPCM: write to invalid slot %d ignored\n", (int)m_cur_slot);
			else if (m_address < 11)
				write_slot(m_slots[m_cur_slot], m_address, data);
			else
				DebugLog("MultiPCM: unknown voice register %02x = %02x (slot %d)\n", m_address, data, m_cur_slot);
			break;
		case 1:
			m_cur_slot = VALUE_TO_CHANNEL[data & 0x1f];
			break;

		case 2:
			m_address = data;
			break;

		case 0xd: // control data for the effect DSP (not ported)
			break;
	}
}

/*****************************
     Construction / reset
*****************************/

CMultiPCM::CMultiPCM(uint32_t clock)
	: m_rom_pages{ nullptr, nullptr, nullptr, nullptr },
	  m_rate(0),
	  m_voices(28),
	  m_clock_divider(224),
	  m_cur_slot(0),
	  m_address(0)
{
	// gew_pcm_device::device_start
	m_rate = (float)clock / m_clock_divider;

	// Volume + pan table
	m_left_pan_table = std::make_unique<int32_t[]>(0x800);
	m_right_pan_table = std::make_unique<int32_t[]>(0x800);
	for (int32_t level = 0; level < 0x80; ++level)
	{
		const float vol_db = (float)level * (-24.0f) / 64.0f;
		const float total_level = powf(10.0f, vol_db / 20.0f) / 4.0f;

		for (int32_t pan = 0; pan < 0x10; ++pan)
		{
			float pan_left, pan_right;
			if (pan == 0x8)
			{
				pan_left = 0.0;
				pan_right = 0.0;
			}
			else if (pan == 0x0)
			{
				pan_left = 1.0;
				pan_right = 1.0;
			}
			else if (pan & 0x8)
			{
				pan_left = 1.0;

				const int32_t inverted_pan = 0x10 - pan;
				const float pan_vol_db = (float)inverted_pan * (-12.0f) / 4.0f;

				pan_right = (float)pow(10.0f, pan_vol_db / 20.0f);

				if ((inverted_pan & 0x7) == 7)
				{
					pan_right = 0.0;
				}
			}
			else
			{
				pan_right = 1.0;

				const float pan_vol_db = (float)pan * (-12.0f) / 4.0f;

				pan_left = (float)pow(10.0f, pan_vol_db / 20.0f);

				if ((pan & 0x7) == 7)
				{
					pan_left = 0.0;
				}
			}

			m_left_pan_table[(pan << 7) | level] = value_to_fixed(TL_SHIFT, pan_left * total_level);
			m_right_pan_table[(pan << 7) | level] = value_to_fixed(TL_SHIFT, pan_right * total_level);
		}
	}

	// Pitch steps
	m_freq_step_table = std::make_unique<uint32_t[]>(0x400);
	for (int32_t i = 0; i < 0x400; ++i)
	{
		const float fcent = m_rate * (1024.0f + (float)i) / 1024.0f;
		m_freq_step_table[i] = value_to_fixed(TL_SHIFT, fcent);
	}

	// Envelope steps
	m_attack_step = std::make_unique<uint32_t[]>(0x40);
	m_decay_release_step = std::make_unique<uint32_t[]>(0x40);
	envelope_generator_init(BASE_TIMES, 14.32833);

	// Total level interpolation steps
	m_total_level_steps = std::make_unique<int32_t[]>(2);
	m_total_level_steps[0] = (int32_t)(-(float)(0x80 << TL_SHIFT) / (78.2f * 44100.0f / 1000.0f)); // lower
	m_total_level_steps[1] = (int32_t)((float)(0x80 << TL_SHIFT) / (78.2f * 2 * 44100.0f / 1000.0f)); // raise

	// build the linear->exponential ramps
	m_linear_to_exp_volume = std::make_unique<int32_t[]>(0x400);
	for (int32_t i = 0; i < 0x400; ++i)
	{
		const float db = -(96.0f - (96.0f * (float)i / (float)0x400));
		const float exp_volume = powf(10.0f, db / 20.0f);
		m_linear_to_exp_volume[i] = value_to_fixed(TL_SHIFT, exp_volume);
	}

	// Slots
	m_slots = std::make_unique<slot_t[]>(m_voices);

	lfo_init();
}

void CMultiPCM::Reset()
{
	// gew_pcm_device::device_reset
	for (uint32_t slot = 0; slot < m_voices; ++slot)
	{
		m_slots[slot].m_playing = false;
	}
}

void CMultiPCM::SetROMPage(unsigned page, const uint8_t *ptr)
{
	// MAME: rom_bank_pre_change() updates the stream; the owner does this before calling.
	m_rom_pages[page & 3] = ptr;
}

/*****************************
        Sample generation
*****************************/

void CMultiPCM::Generate(float *left, float *right, int numSamples)
{
	// gew_pcm_device::sound_stream_update
	for (int32_t i = 0; i < numSamples; ++i)
	{
		int32_t smpl = 0;
		int32_t smpr = 0;
		for (uint32_t sl = 0; sl < m_voices; ++sl)
		{
			slot_t& slot = m_slots[sl];
			if (slot.m_playing)
			{
				uint32_t vol = (slot.m_total_level >> TL_SHIFT) | (slot.m_pan << 7);
				uint32_t spos = slot.m_offset >> TL_SHIFT;
				uint32_t step = slot.m_step;
				int32_t csample = 0;
				int32_t fpart = slot.m_offset & ((1 << TL_SHIFT) - 1);

				if (slot.m_reverse)
				{
					spos = slot.m_sample.m_end - spos - 1;
				}

				if (slot.m_sample.m_format & 4)  // 12-bit linear
				{
					uint32_t adr = slot.m_sample.m_start + (spos >> 1) * 3;
					if (!(spos & 1))
					{ // ab.c ..
						int16_t w0 = (int16_t)(read_byte(adr) << 8 | ((read_byte(adr + 1) & 0xf) << 4));
						csample = w0;
					}
					else
					{ // ..C. AB
						int16_t w0 = (int16_t)((read_byte(adr + 2) << 8) | (read_byte(adr + 1) & 0xf0));
						csample = w0;
					}
				}
				else
				{
					csample = (int16_t)(read_byte(slot.m_sample.m_start + spos) << 8);
				}

				int32_t sample = (csample * fpart + slot.m_prev_sample * ((1 << TL_SHIFT) - fpart)) >> TL_SHIFT;

				if (slot.m_vibrato) // Vibrato enabled
				{
					step = step * pitch_lfo_step(slot.m_pitch_lfo);
					step >>= TL_SHIFT;
				}

				slot.m_offset += step;

				if (spos ^ (slot.m_offset >> TL_SHIFT))
				{
					slot.m_prev_sample = csample;
				}

				if (slot.m_offset >= (slot.m_sample.m_end << TL_SHIFT))
				{
					slot.m_offset -= (slot.m_sample.m_end - slot.m_sample.m_loop) << TL_SHIFT;
					// DD-9 expects the looped silence at the end of some samples to be the same whether reversed or not
					slot.m_reverse = false;
				}

				if ((slot.m_total_level >> TL_SHIFT) != slot.m_dest_total_level)
				{
					slot.m_total_level += slot.m_total_level_step;
				}

				if (slot.m_tremolo) // Tremolo enabled
				{
					sample = sample * amplitude_lfo_step(slot.m_amplitude_lfo);
					sample >>= TL_SHIFT;
				}

				sample = (sample * envelope_generator_update(slot)) >> 10;

				smpl += (m_left_pan_table[vol] * sample) >> TL_SHIFT;
				smpr += (m_right_pan_table[vol] * sample) >> TL_SHIFT;
			}
		}

		// stream.put_int_clamp(ch, i, value, 32768)
		left[i] = (float)std::clamp(smpl, -32768, 32767) * (1.0f / 32768.0f);
		right[i] = (float)std::clamp(smpr, -32768, 32767) * (1.0f / 32768.0f);
	}
}

/*****************************
        Save states
*****************************/

void CMultiPCM::SaveState(CBlockFile *StateFile, const char *name)
{
	StateFile->NewBlock(name, __FILE__);
	StateFile->Write(&m_cur_slot, sizeof(m_cur_slot));
	StateFile->Write(&m_address, sizeof(m_address));
	for (uint32_t i = 0; i < m_voices; i++)
	{
		const slot_t &s = m_slots[i];
		StateFile->Write(s.m_regs, sizeof(s.m_regs));
		StateFile->Write(&s.m_playing, sizeof(s.m_playing));
		StateFile->Write(&s.m_sample, sizeof(s.m_sample));
		StateFile->Write(&s.m_offset, sizeof(s.m_offset));
		StateFile->Write(&s.m_octave, sizeof(s.m_octave));
		StateFile->Write(&s.m_pitch, sizeof(s.m_pitch));
		StateFile->Write(&s.m_step, sizeof(s.m_step));
		StateFile->Write(&s.m_reverse, sizeof(s.m_reverse));
		StateFile->Write(&s.m_pan, sizeof(s.m_pan));
		StateFile->Write(&s.m_dsp_send, sizeof(s.m_dsp_send));
		StateFile->Write(&s.m_total_level, sizeof(s.m_total_level));
		StateFile->Write(&s.m_dest_total_level, sizeof(s.m_dest_total_level));
		StateFile->Write(&s.m_total_level_step, sizeof(s.m_total_level_step));
		StateFile->Write(&s.m_prev_sample, sizeof(s.m_prev_sample));
		StateFile->Write(&s.m_envelope_gen, sizeof(s.m_envelope_gen));
		StateFile->Write(&s.m_lfo_frequency, sizeof(s.m_lfo_frequency));
		StateFile->Write(&s.m_vibrato, sizeof(s.m_vibrato));
		StateFile->Write(&s.m_tremolo, sizeof(s.m_tremolo));
		StateFile->Write(&s.m_pitch_lfo.m_phase, sizeof(s.m_pitch_lfo.m_phase));
		StateFile->Write(&s.m_pitch_lfo.m_phase_step, sizeof(s.m_pitch_lfo.m_phase_step));
		StateFile->Write(&s.m_amplitude_lfo.m_phase, sizeof(s.m_amplitude_lfo.m_phase));
		StateFile->Write(&s.m_amplitude_lfo.m_phase_step, sizeof(s.m_amplitude_lfo.m_phase_step));
	}
}

void CMultiPCM::LoadState(CBlockFile *StateFile, const char *name)
{
	if (Result::OKAY != StateFile->FindBlock(name))
	{
		ErrorLog("Unable to load MultiPCM state. Save state file is corrupt.");
		return;
	}
	StateFile->Read(&m_cur_slot, sizeof(m_cur_slot));
	StateFile->Read(&m_address, sizeof(m_address));
	for (uint32_t i = 0; i < m_voices; i++)
	{
		slot_t &s = m_slots[i];
		StateFile->Read(s.m_regs, sizeof(s.m_regs));
		StateFile->Read(&s.m_playing, sizeof(s.m_playing));
		StateFile->Read(&s.m_sample, sizeof(s.m_sample));
		StateFile->Read(&s.m_offset, sizeof(s.m_offset));
		StateFile->Read(&s.m_octave, sizeof(s.m_octave));
		StateFile->Read(&s.m_pitch, sizeof(s.m_pitch));
		StateFile->Read(&s.m_step, sizeof(s.m_step));
		StateFile->Read(&s.m_reverse, sizeof(s.m_reverse));
		StateFile->Read(&s.m_pan, sizeof(s.m_pan));
		StateFile->Read(&s.m_dsp_send, sizeof(s.m_dsp_send));
		StateFile->Read(&s.m_total_level, sizeof(s.m_total_level));
		StateFile->Read(&s.m_dest_total_level, sizeof(s.m_dest_total_level));
		StateFile->Read(&s.m_total_level_step, sizeof(s.m_total_level_step));
		StateFile->Read(&s.m_prev_sample, sizeof(s.m_prev_sample));
		StateFile->Read(&s.m_envelope_gen, sizeof(s.m_envelope_gen));
		StateFile->Read(&s.m_lfo_frequency, sizeof(s.m_lfo_frequency));
		StateFile->Read(&s.m_vibrato, sizeof(s.m_vibrato));
		StateFile->Read(&s.m_tremolo, sizeof(s.m_tremolo));
		StateFile->Read(&s.m_pitch_lfo.m_phase, sizeof(s.m_pitch_lfo.m_phase));
		StateFile->Read(&s.m_pitch_lfo.m_phase_step, sizeof(s.m_pitch_lfo.m_phase_step));
		StateFile->Read(&s.m_amplitude_lfo.m_phase, sizeof(s.m_amplitude_lfo.m_phase));
		StateFile->Read(&s.m_amplitude_lfo.m_phase_step, sizeof(s.m_amplitude_lfo.m_phase_step));

		// Supermodel: LFO table pointers are not saved (MAME relies on them staying valid
		// in-process). Re-derive them from the saved registers without touching the phase.
		s.m_lfo_frequency &= 7;
		s.m_vibrato &= 7;
		s.m_tremolo &= 7;
		s.m_pitch_lfo.m_table = m_pitch_table.get();
		s.m_pitch_lfo.m_scale = m_pitch_scale_tables[s.m_vibrato].get();
		s.m_amplitude_lfo.m_table = m_amplitude_table.get();
		s.m_amplitude_lfo.m_scale = m_amplitude_scale_tables[s.m_tremolo].get();
	}
}
