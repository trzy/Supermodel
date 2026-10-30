// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/*
 * Ported to Supermodel from MAME (src/devices/sound/ymfm_mame.h, src/devices/sound/ymopn.cpp/.h).
 *
 * YM3438.cpp
 *
 * See YM3438.h.
 */

#include "Model1/YM3438.h"
#include "OSD/Logger.h"
#include <algorithm>

CYM3438::CYM3438(uint32_t clock, uint64_t ticksPerClock)
	: m_clock(clock),
	  m_ticksPerClock(ticksPerClock),
	  m_now(0),
	  m_busy_end(0),
	  m_timer_end{ NEVER, NEVER },
	  m_chip(*this)
{
	// device_start: compute the size of the save buffer by doing an initial save
	ymfm::ymfm_saved_state state(m_save_blob, true);
	m_chip.save_restore(state);
}

void CYM3438::Reset()
{
	m_chip.reset();
}

uint8_t CYM3438::Read(uint32_t offset)
{
	return m_chip.read(offset);
}

void CYM3438::Write(uint32_t offset, uint8_t data)
{
	m_chip.write(offset, data);
}

void CYM3438::Generate(float *left, float *right, int numSamples)
{
	// ymfm_device_base::update_internal
	constexpr int MAX_SAMPLES = 256;
	ymfm::ym3438::output_data output[MAX_SAMPLES];

	for (int sampindex = 0; sampindex < numSamples; sampindex += MAX_SAMPLES)
	{
		int cursamples = (std::min)(numSamples - sampindex, MAX_SAMPLES);
		m_chip.generate(output, cursamples);
		for (int index = 0; index < cursamples; index++)
		{
			// stream.put_int(outnum, sampindex + index, output[index].data[outnum], 32768)
			left[sampindex + index] = (float)output[index].data[0] * (1.0f / 32768.0f);
			right[sampindex + index] = (float)output[index].data[1] * (1.0f / 32768.0f);
		}
	}
}

/******************************************************************************
 ymfm_interface (ym_generic_device)
******************************************************************************/

void CYM3438::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks)
{
	if (duration_in_clocks >= 0)
		m_timer_end[tnum] = m_now + (uint64_t)duration_in_clocks * m_ticksPerClock;
	else
		m_timer_end[tnum] = NEVER;
}

void CYM3438::ymfm_update_irq(bool asserted)
{
	if (OnIRQ)
		OnIRQ(asserted);
}

void CYM3438::ymfm_set_busy_end(uint32_t clocks)
{
	m_busy_end = m_now + (uint64_t)clocks * m_ticksPerClock;
}

bool CYM3438::ymfm_is_busy()
{
	return m_now < m_busy_end;
}

uint64_t CYM3438::NextTimerTick() const
{
	return (std::min)(m_timer_end[0], m_timer_end[1]);
}

void CYM3438::RunTimers(uint64_t nowTicks)
{
	// Fire timers in time order; each handler runs with the time set to its own
	// expiry, so a re-armed timer is scheduled relative to the exact expiry like
	// MAME's emu_timer.
	for (;;)
	{
		int tnum = (m_timer_end[0] <= m_timer_end[1]) ? 0 : 1;
		uint64_t when = m_timer_end[tnum];
		if (when == NEVER || when > nowTicks)
			break;
		m_timer_end[tnum] = NEVER;
		uint64_t saved = m_now;
		m_now = when;
		m_engine->engine_timer_expired(tnum);	// fm_timer_handler
		m_now = saved;
	}
	m_now = nowTicks;
}

/******************************************************************************
 Save states
******************************************************************************/

void CYM3438::SaveState(CBlockFile *StateFile, const char *name)
{
	// device_pre_save
	auto orig_size = m_save_blob.size();
	ymfm::ymfm_saved_state state(m_save_blob, true);
	m_chip.save_restore(state);
	if (m_save_blob.size() != orig_size)
		ErrorLog("YM3438: state size changed (%u -> %u).", (unsigned)orig_size, (unsigned)m_save_blob.size());

	StateFile->NewBlock(name, __FILE__);
	uint32_t size = (uint32_t)m_save_blob.size();
	StateFile->Write(&size, sizeof(size));
	StateFile->Write(m_save_blob.data(), size);
	StateFile->Write(&m_busy_end, sizeof(m_busy_end));
	StateFile->Write(m_timer_end, sizeof(m_timer_end));
}

void CYM3438::LoadState(CBlockFile *StateFile, const char *name)
{
	if (Result::OKAY != StateFile->FindBlock(name))
	{
		ErrorLog("Unable to load YM3438 state. Save state file is corrupt.");
		return;
	}
	uint32_t size = 0;
	StateFile->Read(&size, sizeof(size));
	if (size != m_save_blob.size())
	{
		ErrorLog("Unable to load YM3438 state: size mismatch.");
		return;
	}
	StateFile->Read(m_save_blob.data(), size);
	StateFile->Read(&m_busy_end, sizeof(m_busy_end));
	StateFile->Read(m_timer_end, sizeof(m_timer_end));

	// device_post_load
	ymfm::ymfm_saved_state state(m_save_blob, false);
	m_chip.save_restore(state);
}
