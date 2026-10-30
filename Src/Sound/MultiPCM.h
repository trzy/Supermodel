// license:BSD-3-Clause
// copyright-holders:Miguel Angel Horna
/*
 * Ported to Supermodel from MAME (src/devices/sound/multipcm.h, src/devices/sound/gew.h).
 *
 * MultiPCM.h
 *
 * Yamaha YMW-258-F 'GEW8' (aka Sega 315-5560 "MultiPCM"). MAME splits this
 * into gew_pcm_device (shared GEW engine) and multipcm_device; both are
 * folded into CMultiPCM here. The YMW-258-F effect DSP send output (only used
 * by MAME's Yamaha MU-series drivers) is not ported.
 *
 * Framework replacements:
 *  - device_rom_interface<22> -> four 1 MB page pointers (SetROMPage). An
 *    unmapped page reads as 0, like an unmapped address in MAME's ROM space.
 *  - sound_stream -> Generate(left, right, numSamples), called by the owner
 *    at the chip's native rate (clock / 224). The owner is responsible for
 *    "updating the stream" (generating samples up to the current time) before
 *    Write() and before changing a ROM page, as MAME does with m_stream->update().
 *  - save_item -> SaveState/LoadState.
 */

#ifndef INCLUDED_MULTIPCM_H
#define INCLUDED_MULTIPCM_H

#include "Types.h"
#include "BlockFile.h"
#include <cstdint>
#include <memory>

class CMultiPCM
{
public:
	/*
	 * CMultiPCM(clock):
	 * clock is the chip's input clock in Hz (10 MHz on the Model 1 sound board).
	 * The sample rate is clock / 224.
	 */
	explicit CMultiPCM(uint32_t clock);

	// device_reset
	void Reset();

	// Register interface (offset 0: data, 1: slot select, 2: register select).
	void Write(uint32_t offset, uint8_t data);
	uint8_t Read();

	/*
	 * SetROMPage(page, ptr):
	 * Maps 1 MB of sample ROM at page * 0x100000 in the chip's 22-bit
	 * (4 MB) sample address space. nullptr unmaps the page (reads 0).
	 */
	void SetROMPage(unsigned page, const uint8_t *ptr);

	/*
	 * Generate(left, right, numSamples):
	 * Produces numSamples stereo samples at SampleRate(). Values are normalised
	 * to [-1, 1) exactly as MAME's stream.put_int_clamp(.., 32768).
	 */
	void Generate(float *left, float *right, int numSamples);

	double SampleRate() const { return m_rate; }

	void SaveState(CBlockFile *StateFile, const char *name);
	void LoadState(CBlockFile *StateFile, const char *name);

private:
	struct sample_t
	{
		uint32_t m_start = 0;
		uint32_t m_loop = 0;
		uint32_t m_end = 0;
		uint8_t m_attack_reg = 0;
		uint8_t m_decay1_reg = 0;
		uint8_t m_decay2_reg = 0;
		uint8_t m_decay_level = 0;
		uint8_t m_release_reg = 0;
		uint8_t m_key_rate_scale = 0;
		uint8_t m_lfo_vibrato_reg = 0;
		uint8_t m_lfo_amplitude_reg = 0;
		uint8_t m_format = 0;
	};

	enum class state_t : uint8_t
	{
		ATTACK,
		DECAY1,
		DECAY2,
		RELEASE
	};

	struct envelope_gen_t
	{
		int32_t m_volume = 0;
		state_t m_state = state_t::ATTACK;
		uint8_t m_reverb = 0;
		int32_t step = 0;
		//step vals
		int32_t m_attack_rate = 0;  // Attack
		int32_t m_decay1_rate = 0;  // Decay1
		int32_t m_decay2_rate = 0;  // Decay2
		int32_t m_release_rate = 0; // Release
		int32_t m_decay_level = 0;  // Decay level
	};

	struct lfo_t
	{
		uint16_t m_phase = 0;
		uint32_t m_phase_step = 0;
		int32_t *m_table = nullptr;
		int32_t *m_scale = nullptr;
	};

	struct slot_t
	{
		uint8_t m_regs[11] = { };
		bool m_playing = false;
		sample_t m_sample;
		uint32_t m_offset = 0;
		uint8_t m_octave = 0;
		uint16_t m_pitch = 0;
		uint32_t m_step = 0;
		bool m_reverse = false;
		uint32_t m_pan = 0;
		uint8_t m_dsp_send = 0;
		uint32_t m_total_level = 0;
		uint32_t m_dest_total_level = 0;
		int32_t m_total_level_step = 0;
		int32_t m_prev_sample = 0;
		envelope_gen_t m_envelope_gen;
		uint8_t m_lfo_frequency = 0;	// uninitialised in MAME
		lfo_t m_pitch_lfo; // Pitch lfo
		uint8_t m_vibrato = 0;
		lfo_t m_amplitude_lfo; // AM lfo
		uint8_t m_tremolo = 0;
	};

	// device_rom_interface replacement
	uint8_t read_byte(uint32_t address) const
	{
		address &= 0x3fffff;
		const uint8_t *page = m_rom_pages[address >> 20];
		return page ? page[address & 0xfffff] : 0;
	}

	// gew_pcm_device
	uint32_t value_to_fixed(const uint32_t bits, const float value);

	void retrigger_sample(slot_t &slot);
	void update_step(slot_t &slot);

	static constexpr uint32_t TL_SHIFT = 12;
	static constexpr uint32_t EG_SHIFT = 16;
	static constexpr uint32_t LFO_SHIFT = 8;

	void lfo_init();
	void lfo_compute_step(lfo_t &lfo, uint32_t lfo_frequency, uint32_t LFOS, int32_t amplitude_lfo);
	int32_t pitch_lfo_step(lfo_t &lfo);
	int32_t amplitude_lfo_step(lfo_t &lfo);

	void envelope_generator_init(const double (&rates)[64], double attack_decay_ratio);
	int32_t envelope_generator_update(slot_t &slot);
	void envelope_generator_calc(slot_t &slot);
	uint32_t get_rate(uint32_t *steps, int32_t rate, uint32_t val);

	static const double BASE_TIMES[64];
	static const float LFO_FREQ[8];
	static const float PHASE_SCALE_LIMIT[8];
	static const float AMPLITUDE_SCALE_LIMIT[8];

	// multipcm_device
	static const int32_t VALUE_TO_CHANNEL[32];
	void init_sample(sample_t &sample, uint32_t index);
	void write_slot(slot_t &slot, int32_t reg, uint8_t data);

	// internal state
	const uint8_t *m_rom_pages[4];
	std::unique_ptr<slot_t[]> m_slots;
	float m_rate;
	uint32_t m_voices;
	uint32_t m_clock_divider;

	uint32_t m_cur_slot;
	uint32_t m_address;

	std::unique_ptr<uint32_t[]> m_attack_step;
	std::unique_ptr<uint32_t[]> m_decay_release_step;   // Envelope step tables
	std::unique_ptr<uint32_t[]> m_freq_step_table;      // Frequency step table

	std::unique_ptr<int32_t[]> m_left_pan_table;
	std::unique_ptr<int32_t[]> m_right_pan_table;
	std::unique_ptr<int32_t[]> m_linear_to_exp_volume;
	std::unique_ptr<int32_t[]> m_total_level_steps;

	std::unique_ptr<int32_t[]> m_pitch_table;
	std::unique_ptr<int32_t[]> m_pitch_scale_tables[8];
	std::unique_ptr<int32_t[]> m_amplitude_table;
	std::unique_ptr<int32_t[]> m_amplitude_scale_tables[8];
};

#endif	// INCLUDED_MULTIPCM_H
