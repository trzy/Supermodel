// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/*
 * Ported to Supermodel from MAME (src/devices/sound/ymfm_mame.h, src/devices/sound/ymopn.cpp/.h).
 *
 * YM3438.h
 *
 * Yamaha YM3438 (OPN2C) wrapper around the ymfm library (Src/Pkgs/ymfm),
 * equivalent to MAME's ym3438_device = ymfm_device_base<ymfm::ym3438>.
 *
 * Framework replacements:
 *  - MAME time (attotime) -> an abstract 64-bit tick counter supplied by the
 *    owner through SetTime(). The owner states at construction how many ticks
 *    make one chip clock (e.g. 40 MHz ticks / 8 MHz chip clock = 5).
 *  - emu_timer for the two FM timers -> expiry tick stored here; the owner
 *    calls RunTimers(now) to fire expired timers and may use NextTimerTick()
 *    to schedule its CPU slices.
 *  - scheduler().synchronize() in ymfm_sync_mode_write/ymfm_sync_check_interrupts
 *    -> direct calls (ymfm's defaults); there is only one thread of execution.
 *  - sound_stream -> Generate(), at SampleRate() = clock / 144. The owner
 *    updates the stream before Read()/Write(), as MAME's update_streams() does.
 *  - IRQ output: OnIRQ callback (not connected on the Model 1 sound board).
 */

#ifndef INCLUDED_YM3438_H
#define INCLUDED_YM3438_H

#include "Types.h"
#include "BlockFile.h"
#include "ymfm/ymfm_opn.h"
#include <cstdint>
#include <functional>
#include <vector>

class CYM3438 : public ymfm::ymfm_interface
{
public:
	/*
	 * CYM3438(clock, ticksPerClock):
	 * clock          Chip clock in Hz (8 MHz on the Model 1 sound board).
	 * ticksPerClock  Owner's time base ticks per chip clock (integer).
	 */
	CYM3438(uint32_t clock, uint64_t ticksPerClock);

	void Reset();									// device_reset

	// Owner's current time, used for busy flag and timers
	void SetTime(uint64_t nowTicks) { m_now = nowTicks; }

	uint8_t Read(uint32_t offset);					// ymfm_device_base::read
	void Write(uint32_t offset, uint8_t data);		// ymfm_device_base::write

	// Stereo samples normalised as MAME's stream.put_int(.., 32768) (no clamping)
	void Generate(float *left, float *right, int numSamples);
	uint32_t SampleRate() const { return m_chip.sample_rate(m_clock); }

	// Timers
	uint64_t NextTimerTick() const;				// ~0 if no timer running
	void RunTimers(uint64_t nowTicks);			// fire all timers expiring at or before nowTicks

	std::function<void(bool)> OnIRQ;

	void SaveState(CBlockFile *StateFile, const char *name);
	void LoadState(CBlockFile *StateFile, const char *name);

protected:
	// ymfm_interface
	void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
	void ymfm_update_irq(bool asserted) override;
	void ymfm_set_busy_end(uint32_t clocks) override;
	bool ymfm_is_busy() override;

private:
	static constexpr uint64_t NEVER = ~uint64_t(0);

	uint32_t m_clock;
	uint64_t m_ticksPerClock;
	uint64_t m_now;
	uint64_t m_busy_end;
	uint64_t m_timer_end[2];
	ymfm::ym3438 m_chip;
	std::vector<uint8_t> m_save_blob;
};

#endif	// INCLUDED_YM3438_H
