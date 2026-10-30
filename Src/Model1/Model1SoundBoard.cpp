// license:BSD-3-Clause
// copyright-holders:R. Belmont
/*
 * Ported to Supermodel from MAME (src/mame/shared/segam1audio.cpp).
 *
 * Sega Model 1 sound board (68000 + 2x 315-5560 "MultiPCM")
 *
 * used for Model 1 and early Model 2 games
 *
 * MAME configuration (device_add_mconfig):
 *   68000        20 MHz / 2      (verified on real h/w)
 *   YM3438       16 MHz / 2      routes 0/1 -> speaker L/R at 0.30
 *   MultiPCM x2  20 MHz / 2      routes 0/1 -> speaker L/R at 0.5 each
 *   i8251        16 MHz / 2 (T82C51), RxRDY -> 68K IRQ 2, TxD -> main board
 *   uart_clock   16 MHz / 2 / 16 = 500 kHz -> TxC and RxC
 *     (16 times 31.25kHz, standard Sega/MIDI sound data rate)
 *   DAC output clocks measured: BYTECLK = 10/8 (1.25MHz), WORDCLK = 10/8/28 (44.642857kHz)
 *
 * See Model1SoundBoard.h for the Supermodel timing/audio model.
 */

#include "Model1/Model1SoundBoard.h"
#include "Model3/DSB.h"
#include "OSD/Audio.h"
#include "OSD/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

// Time base
static constexpr uint64_t TICK_HZ             = 40000000;	// 40 MHz master tick
static constexpr uint64_t TICKS_PER_68K_CYCLE = 4;			// 68000 @ 10 MHz
static constexpr uint64_t TICKS_PER_YM_CLOCK  = 5;			// YM3438 @ 8 MHz
static constexpr uint64_t TICKS_PER_UART_CLK  = 80;			// 500 kHz UART clock
static constexpr uint64_t SLICE_TICKS         = 16 * TICKS_PER_UART_CLK;	// one bit time at 31.25 kbaud (320 68K cycles)
static constexpr uint64_t OUTPUT_RATE         = 44100;

static constexpr uint32_t CPU_CLOCK   = 10000000;	// 20_MHz_XTAL / 2
static constexpr uint32_t YM_CLOCK    = 8000000;	// 16_MHz_XTAL / 2
static constexpr uint32_t MPCM_CLOCK  = 10000000;	// 20_MHz_XTAL / 2

static constexpr float YM_GAIN   = 0.30f;
static constexpr float MPCM_GAIN = 0.5f;

static constexpr double MODEL1_REFRESH_HZ = 16000000.0 / (656.0 * 424.0);	// 57.5246 Hz

static constexpr size_t ROM_SIZE = 0xC0000;
static constexpr size_t RAM_SIZE = 0x10000;

// Interrupt acknowledge: autovectored (MAME's 68000 default with set_inputline)
static int Model1SoundIRQAck(int irqLevel)
{
	return M68K_IRQ_AUTOVECTOR;
}

/******************************************************************************
 Construction
******************************************************************************/

CModel1SoundBoard::CModel1SoundBoard(const Util::Config::Node &config)
	: m_config(config),
	  m_in68K(false),
	  m_sliceStart(0),
	  m_sampleROM{ nullptr, nullptr },
	  m_sampleSize{ 0, 0 },
	  m_bank{ 0, 0 },
	  m_ym(YM_CLOCK, TICKS_PER_YM_CLOCK),
	  m_pcm1(MPCM_CLOCK),
	  m_pcm2(MPCM_CLOCK),
	  m_dsb(nullptr),
	  m_now(0),
	  m_uartClocks(0),
	  m_outCount(0),
	  m_sampleRemainder(0.0)
{
	m_streams[0].period = 144 * TICKS_PER_YM_CLOCK;			// ymfm OPN2: clock / (6 * 24)
	m_streams[1].period = 224 * (TICK_HZ / MPCM_CLOCK);		// gew: clock / 224
	m_streams[2].period = 224 * (TICK_HZ / MPCM_CLOCK);

	m_outL.resize(MAX_SAMPLES_PER_FRAME);
	m_outR.resize(MAX_SAMPLES_PER_FRAME);
	m_outRL.assign(MAX_SAMPLES_PER_FRAME, 0.0f);
	m_outRR.assign(MAX_SAMPLES_PER_FRAME, 0.0f);

	// m_uart->rxrdy_handler().set_inputline(m_audiocpu, M68K_IRQ_2)
	// (called with the sound 68K's context active: during RunFrame/Reset/LoadState)
	m_uart.OnRxRdy = [](bool state) { M68KSetIRQ(state ? 2 : 0); };

	// m_uart->txd_handler().set(output_txd) -> m_rxd_handler (main board RXD, and the DSB)
	m_uart.OnTransmit = [this](uint8_t data)
	{
		if (m_dsb != nullptr)
			m_dsb->SendCommand(data);
		if (m_midiOutCallback)
			m_midiOutCallback(data);
		else
			m_midiOut.push_back(data);
	};
}

CModel1SoundBoard::~CModel1SoundBoard()
{
}

Result CModel1SoundBoard::Init(const UINT8 *programROM, size_t programSize,
                               const UINT8 *sampleROM1, size_t sample1Size,
                               const UINT8 *sampleROM2, size_t sample2Size)
{
	m_rom.reset(new(std::nothrow) UINT8[ROM_SIZE]);
	m_ram.reset(new(std::nothrow) UINT8[RAM_SIZE]);
	if (!m_rom || !m_ram)
		return ErrorLog("Insufficient memory for Model 1 sound board.");
	memset(m_rom.get(), 0, ROM_SIZE);
	memset(m_ram.get(), 0, RAM_SIZE);
	if (programROM != nullptr)
		memcpy(m_rom.get(), programROM, (std::min)(programSize, ROM_SIZE));

	m_sampleROM[0] = sampleROM1;
	m_sampleSize[0] = sampleROM1 ? sample1Size : 0;
	m_sampleROM[1] = sampleROM2;
	m_sampleSize[1] = sampleROM2 ? sample2Size : 0;

	// device_start: banks configured, entry 0 selected by default
	m_bank[0] = m_bank[1] = 0;
	for (int chip = 0; chip < 2; chip++)
	{
		CMultiPCM &pcm = chip ? m_pcm2 : m_pcm1;
		pcm.SetROMPage(0, m_sampleSize[chip] >= 0x100000 ? m_sampleROM[chip] : nullptr);
		pcm.SetROMPage(1, m_sampleSize[chip] >= 0x100000 ? m_sampleROM[chip] : nullptr);
		pcm.SetROMPage(2, nullptr);	// 200000-3FFFFF unmapped
		pcm.SetROMPage(3, nullptr);
	}

	// 68K core
	M68KSetContext(&m_68K);
	M68KInit();
	M68KAttachBus(this);
	M68KSetIRQCallback(Model1SoundIRQAck);
	M68KGetContext(&m_68K);

	return Result::OKAY;
}

void CModel1SoundBoard::AttachDSB(CDSB *dsb)
{
	m_dsb = dsb;
	DebugLog("Model 1 sound board connected to DSB\n");
}

void CModel1SoundBoard::Reset()
{
	M68KSetContext(&m_68K);

	// Child devices
	m_uart.Reset();
	m_midiInTimed.clear();
	m_ym.SetTime(m_now);
	m_ym.Reset();
	UpdateAllStreams(m_now);
	m_pcm1.Reset();
	m_pcm2.Reset();

	// segam1audio_device::device_reset
	m_uart.WriteCTS(0);

	M68KReset();
	M68KGetContext(&m_68K);

	if (m_dsb != nullptr)
		m_dsb->Reset();

	DebugLog("Model 1 Sound Board Reset\n");
}

/******************************************************************************
 Time keeping and streams
******************************************************************************/

uint64_t CModel1SoundBoard::CurrentTick() const
{
	if (m_in68K)
		return m_sliceStart + (uint64_t)(std::max)(0, m68k_cycles_run()) * TICKS_PER_68K_CYCLE;
	return m_now;
}

void CModel1SoundBoard::GenerateTo(int chip, uint64_t sampleCount)
{
	Stream &s = m_streams[chip];
	uint64_t have = s.Generated();
	if (sampleCount <= have)
		return;
	size_t n = (size_t)(sampleCount - have);
	size_t old = s.l.size();
	s.l.resize(old + n);
	s.r.resize(old + n);
	switch (chip)
	{
	case 0: m_ym.Generate(&s.l[old], &s.r[old], (int)n); break;
	case 1: m_pcm1.Generate(&s.l[old], &s.r[old], (int)n); break;
	default: m_pcm2.Generate(&s.l[old], &s.r[old], (int)n); break;
	}
}

// MAME's stream->update(): generate all samples that start before 'now'
void CModel1SoundBoard::UpdateStream(int chip, uint64_t now)
{
	const Stream &s = m_streams[chip];
	GenerateTo(chip, (now + s.period - 1) / s.period);
}

void CModel1SoundBoard::UpdateAllStreams(uint64_t now)
{
	for (int chip = 0; chip < 3; chip++)
		UpdateStream(chip, now);
}

void CModel1SoundBoard::CatchUpUART(uint64_t now)
{
	// Bytes from the main board are put on the line at the time they were sent
	while (!m_midiInTimed.empty() && m_midiInTimed.front().first <= now)
	{
		uint64_t at = m_midiInTimed.front().first / TICKS_PER_UART_CLK;
		if (at > m_uartClocks)
		{
			m_uart.Clock((unsigned)(at - m_uartClocks));
			m_uartClocks = at;
		}
		m_uart.ReceiveByte(m_midiInTimed.front().second);
		m_midiInTimed.pop_front();
	}

	uint64_t target = now / TICKS_PER_UART_CLK;
	if (target > m_uartClocks)
	{
		m_uart.Clock((unsigned)(target - m_uartClocks));
		m_uartClocks = target;
	}
}

void CModel1SoundBoard::SetBank(int chip, UINT16 data)
{
	// m1_snd_mpcm_bnk1_w / m1_snd_mpcm_bnk2_w: set_entry(data & 3)
	UpdateStream(1 + chip, CurrentTick());	// rom_bank_pre_change -> m_stream->update()
	m_bank[chip] = (UINT8)(data & 3);
	size_t offset = (size_t)m_bank[chip] * 0x100000;
	const UINT8 *page = (m_sampleROM[chip] && offset + 0x100000 <= m_sampleSize[chip]) ? m_sampleROM[chip] + offset : nullptr;
	(chip ? m_pcm2 : m_pcm1).SetROMPage(1, page);
}

/******************************************************************************
 68K address space (segam1audio_map)

 000000-03FFFF  ROM
 080000-09FFFF  ROM (region offset 0x20000; mirror of upper ROM socket)
 C20000-C20003  i8251 (umask16 0x00ff)
 C40000-C40007  MultiPCM #1 (umask16 0x00ff)
 C40012-C40013  nopw
 C50000-C50001  MultiPCM #1 bank (w)
 C60000-C60007  MultiPCM #2 (umask16 0x00ff)
 C70000-C70001  MultiPCM #2 bank (w)
 D00000-D00007  YM3438 (umask16 0x00ff)
 F00000-F0FFFF  RAM (real PCB actually has 2x 8kBx8-bit SRAMs (16kB total))

 Memory is stored as in CSoundBoard: 16-bit words in host (little endian)
 order, so byte accesses use addr ^ 1. Unmapped reads return 0.
******************************************************************************/

UINT8 CModel1SoundBoard::DeviceRead(UINT32 a)
{
	uint64_t now = CurrentTick();
	switch (a & 0xFFFFF0)
	{
	case 0xC20000:
		if (a <= 0xC20003)
		{
			CatchUpUART(now);
			UINT8 v = m_uart.Read((a >> 1) & 1);
			if (!((a >> 1) & 1) && getenv("SUPERMODEL_MODEL1_LOGIO"))
				DebugLog("Sound 68K read UART data %02X (PC=%06X)\n", v, m68k_get_reg(nullptr, M68K_REG_PPC));
			return v;
		}
		break;
	case 0xC40000:
		if (a <= 0xC40007)
			return m_pcm1.Read();
		break;
	case 0xC60000:
		if (a <= 0xC60007)
			return m_pcm2.Read();
		break;
	case 0xD00000:
		if (a <= 0xD00007)
		{
			UpdateStream(0, now);	// update_streams()
			m_ym.SetTime(now);
			return m_ym.Read((a >> 1) & 3);
		}
		break;
	default:
		break;
	}
	return 0;
}

static unsigned s_debugWrites[4];	// debugging: UART, MultiPCM 1, MultiPCM 2, YM3438 writes

void CModel1SoundBoard::DeviceWrite(UINT32 a, UINT8 d)
{
	uint64_t now = CurrentTick();
	switch (a & 0xFFFFF0) { case 0xC20000: s_debugWrites[0]++; break; case 0xC40000: s_debugWrites[1]++; break; case 0xC60000: s_debugWrites[2]++; break; case 0xD00000: s_debugWrites[3]++; break; default: break; }
	switch (a & 0xFFFFF0)
	{
	case 0xC20000:
		if (a <= 0xC20003)
		{
			CatchUpUART(now);
			m_uart.Write((a >> 1) & 1, d);
		}
		break;
	case 0xC40000:
		if (a <= 0xC40007)
		{
			UpdateStream(1, now);
			m_pcm1.Write((a >> 1) & 3, d);
		}
		break;
	case 0xC60000:
		if (a <= 0xC60007)
		{
			UpdateStream(2, now);
			m_pcm2.Write((a >> 1) & 3, d);
		}
		break;
	case 0xD00000:
		if (a <= 0xD00007)
		{
			UpdateStream(0, now);
			m_ym.SetTime(now);
			m_ym.Write((a >> 1) & 3, d);
		}
		break;
	default:
		break;
	}
}

UINT8 CModel1SoundBoard::Read8(UINT32 a)
{
	a &= 0xFFFFFF;
	if (a < 0x40000)
		return m_rom[a ^ 1];
	if (a >= 0x80000 && a < 0xA0000)
		return m_rom[(0x20000 + (a - 0x80000)) ^ 1];
	if (a >= 0xF00000 && a < 0xF10000)
		return m_ram[(a & 0xFFFF) ^ 1];
	if (a & 1)	// devices are on D0-D7 (umask16 0x00ff); even byte accesses don't reach them
		return DeviceRead(a);
	return 0;
}

UINT16 CModel1SoundBoard::Read16(UINT32 a)
{
	a &= 0xFFFFFE;
	if (a < 0x40000)
		return *(UINT16 *)&m_rom[a];
	if (a >= 0x80000 && a < 0xA0000)
		return *(UINT16 *)&m_rom[0x20000 + (a - 0x80000)];
	if (a >= 0xF00000 && a < 0xF10000)
		return *(UINT16 *)&m_ram[a & 0xFFFF];
	return DeviceRead(a | 1);
}

UINT32 CModel1SoundBoard::Read32(UINT32 a)
{
	UINT32 hi = Read16(a);
	UINT32 lo = Read16(a + 2);
	return (hi << 16) | lo;
}

void CModel1SoundBoard::Write8(UINT32 a, UINT8 d)
{
	a &= 0xFFFFFF;
	if (a >= 0xF00000 && a < 0xF10000)
	{
		m_ram[(a & 0xFFFF) ^ 1] = d;
		return;
	}
	if ((a & ~1u) == 0xC50000)
	{
		// 16-bit handler: a byte write to the even address puts the data on D8-D15
		SetBank(0, (a & 1) ? d : (UINT16)(d << 8));
		return;
	}
	if ((a & ~1u) == 0xC70000)
	{
		SetBank(1, (a & 1) ? d : (UINT16)(d << 8));
		return;
	}
	if (a & 1)
		DeviceWrite(a, d);
}

void CModel1SoundBoard::Write16(UINT32 a, UINT16 d)
{
	a &= 0xFFFFFE;
	if (a >= 0xF00000 && a < 0xF10000)
	{
		*(UINT16 *)&m_ram[a & 0xFFFF] = d;
		return;
	}
	if (a == 0xC50000)
	{
		SetBank(0, d);
		return;
	}
	if (a == 0xC70000)
	{
		SetBank(1, d);
		return;
	}
	DeviceWrite(a | 1, (UINT8)d);
}

void CModel1SoundBoard::Write32(UINT32 a, UINT32 d)
{
	Write16(a, (UINT16)(d >> 16));
	Write16(a + 2, (UINT16)d);
}

/******************************************************************************
 Serial link
******************************************************************************/

void CModel1SoundBoard::WriteMIDI(UINT8 data)
{
	m_uart.ReceiveByte(data);
}

// The main board runs a frame before the sound board does. A byte it sent at
// a fraction of its frame reaches the line at the same fraction of the sound
// board's next frame, as if the two ran together; delivered at the start of
// the frame, the first command after power-on would arrive before the sound
// program is ready for it, and half of it would be lost.
void CModel1SoundBoard::WriteMIDI(UINT8 data, double frameFraction)
{
	double refreshHz = std::abs(m_config["RefreshRate"].ValueAsDefault<float>((float)MODEL1_REFRESH_HZ));
	refreshHz = (std::max)(45.0, (std::min)(120.0, refreshHz));
	frameFraction = (std::max)(0.0, (std::min)(1.0, frameFraction));
	uint64_t at = m_now + (uint64_t)(frameFraction * (double)TICK_HZ / refreshHz);
	if (!m_midiInTimed.empty() && at < m_midiInTimed.back().first)
		at = m_midiInTimed.back().first;
	m_midiInTimed.emplace_back(at, data);
}

unsigned CModel1SoundBoard::PendingMIDIIn() const
{
	return m_uart.PendingLineBytes() + (unsigned)m_midiInTimed.size();
}

void CModel1SoundBoard::SetMIDIOutCallback(std::function<void(UINT8)> callback)
{
	m_midiOutCallback = std::move(callback);
	// Hand over anything queued before the callback was connected
	while (m_midiOutCallback && !m_midiOut.empty())
	{
		UINT8 b = m_midiOut.front();
		m_midiOut.pop_front();
		m_midiOutCallback(b);
	}
}

bool CModel1SoundBoard::ReadMIDI(UINT8 *data)
{
	if (m_midiOut.empty())
		return false;
	*data = m_midiOut.front();
	m_midiOut.pop_front();
	return true;
}

/******************************************************************************
 Frame execution and audio
******************************************************************************/

int CModel1SoundBoard::NextFrameSamples()
{
	// As CSoundBoard: the fraction left over from each frame is carried to the next one
	double refreshHz = std::abs(m_config["RefreshRate"].ValueAsDefault<float>((float)MODEL1_REFRESH_HZ));
	refreshHz = (std::max)(45.0, (std::min)(120.0, refreshHz));
	double samples = (double)OUTPUT_RATE / refreshHz + m_sampleRemainder;
	int numSamples = (std::min)((int)samples, MAX_SAMPLES_PER_FRAME);
	m_sampleRemainder = samples - numSamples;
	return numSamples;
}

void CModel1SoundBoard::Mix(int numSamples)
{
	// Resample each chip's native stream to 44.1 kHz (linear interpolation; MAME's mixer
	// uses its own resampler) and mix with the device_add_mconfig routing gains.
	static const float gains[3] = { YM_GAIN, MPCM_GAIN, MPCM_GAIN };

	for (int k = 0; k < numSamples; k++)
	{
		m_outL[k] = 0.0f;
		m_outR[k] = 0.0f;
	}

	for (int chip = 0; chip < 3; chip++)
	{
		Stream &s = m_streams[chip];
		const uint64_t den = OUTPUT_RATE * s.period;
		uint64_t lastIndex = s.base;
		for (int k = 0; k < numSamples; k++)
		{
			uint64_t num = (m_outCount + k) * TICK_HZ;	// output sample time in ticks x 44100
			uint64_t i = num / den;
			float frac = (float)(num % den) / (float)den;
			if (i < s.base)	// can't happen unless state was restored oddly
				i = s.base;
			GenerateTo(chip, i + 2);
			size_t j = (size_t)(i - s.base);
			float l = s.l[j] + (s.l[j + 1] - s.l[j]) * frac;
			float r = s.r[j] + (s.r[j + 1] - s.r[j]) * frac;
			m_outL[k] += l * gains[chip];
			m_outR[k] += r * gains[chip];
			lastIndex = i;
		}

		// Drop samples no longer needed
		size_t drop = (size_t)(lastIndex - s.base);
		if (drop > 0)
		{
			s.l.erase(s.l.begin(), s.l.begin() + drop);
			s.r.erase(s.r.begin(), s.r.begin() + drop);
			s.base = lastIndex;
		}
	}

	m_outCount += numSamples;
}

bool CModel1SoundBoard::RunFrame()
{
	const int numSamples = NextFrameSamples();

	// Emulated time for this frame is exactly numSamples output periods
	const uint64_t endTick = ((m_outCount + numSamples) * TICK_HZ) / OUTPUT_RATE;

	M68KSetContext(&m_68K);
	while (m_now < endTick)
	{
		// Fire YM3438 timers that have expired (stream brought up to date first: CSM key-on)
		if (m_ym.NextTimerTick() <= m_now)
		{
			UpdateStream(0, m_now);
			m_ym.RunTimers(m_now);
		}

		uint64_t sliceEnd = (std::min)(endTick, m_now + SLICE_TICKS);
		uint64_t timer = m_ym.NextTimerTick();
		if (timer > m_now && timer < sliceEnd)
			sliceEnd = timer;

		int cycles = (int)((sliceEnd - m_now + TICKS_PER_68K_CYCLE - 1) / TICKS_PER_68K_CYCLE);
		if (cycles < 1)
			cycles = 1;

		m_sliceStart = m_now;
		m_in68K = true;
		int done = M68KRun(cycles);
		m_in68K = false;
		m_now += (uint64_t)(std::max)(done, 1) * TICKS_PER_68K_CYCLE;

		CatchUpUART(m_now);
	}
	if (m_ym.NextTimerTick() <= m_now)
	{
		UpdateStream(0, m_now);
		m_ym.RunTimers(m_now);
	}
	M68KGetContext(&m_68K);

	// Audio
	Mix(numSamples);

	const bool emulateSound = m_config["EmulateSound"].ValueAsDefault<bool>(true);
	float soundVol = (float)(std::max)(0, (std::min)(200, m_config["SoundVolume"].ValueAsDefault<int>(100))) * (1.0f / 100.0f);
	const float scale = emulateSound ? 32768.0f * soundVol : 0.0f;
	for (int i = 0; i < numSamples; i++)
	{
		m_outL[i] *= scale;
		m_outR[i] *= scale;
		m_outRL[i] = 0.0f;
		m_outRR[i] = 0.0f;
	}

	// DSB mixes itself in and applies its own (music) volume
	if (m_dsb != nullptr)
		m_dsb->RunFrame(m_outL.data(), m_outR.data(), numSamples);

	// Debugging aid: peak output level once a second
	if (getenv("SUPERMODEL_MODEL1_LOGIO"))
	{
		static float peak = 0.0f;
		static int frames = 0;
		for (int i = 0; i < numSamples; i++)
			peak = (std::max)(peak, (std::max)(std::fabs(m_outL[i]), std::fabs(m_outR[i])));
		if (++frames == 57)
		{
			DebugLog("Model 1 sound: peak %.1f, 68K PC=%06X, MIDI pending %u, writes UART %u PCM1 %u PCM2 %u YM %u\n", peak, m_68K.musashiCtx.pc, PendingMIDIIn(),
				s_debugWrites[0], s_debugWrites[1], s_debugWrites[2], s_debugWrites[3]);
			s_debugWrites[0] = s_debugWrites[1] = s_debugWrites[2] = s_debugWrites[3] = 0;
			peak = 0.0f;
			frames = 0;
		}
	}

	return OutputAudio(numSamples, m_outL.data(), m_outR.data(), m_outRL.data(), m_outRR.data(),
		m_config["FlipStereo"].ValueAsDefault<bool>(false));
}

/******************************************************************************
 Save states
******************************************************************************/

static void SaveStream(CBlockFile *f, const std::vector<float> &l, const std::vector<float> &r, uint64_t base)
{
	uint32_t n = (uint32_t)l.size();
	f->Write(&base, sizeof(base));
	f->Write(&n, sizeof(n));
	if (n)
	{
		f->Write((void *)l.data(), n * sizeof(float));
		f->Write((void *)r.data(), n * sizeof(float));
	}
}

void CModel1SoundBoard::SaveState(CBlockFile *StateFile)
{
	StateFile->NewBlock("Model1 Sound Board", __FILE__);
	StateFile->Write(m_ram.get(), RAM_SIZE);
	StateFile->Write(m_bank, sizeof(m_bank));
	StateFile->Write(&m_now, sizeof(m_now));
	StateFile->Write(&m_uartClocks, sizeof(m_uartClocks));
	StateFile->Write(&m_outCount, sizeof(m_outCount));
	StateFile->Write(&m_sampleRemainder, sizeof(m_sampleRemainder));
	uint32_t n = (uint32_t)m_midiOut.size();
	StateFile->Write(&n, sizeof(n));
	for (UINT8 b : m_midiOut)
		StateFile->Write(&b, sizeof(b));
	for (int chip = 0; chip < 3; chip++)
		SaveStream(StateFile, m_streams[chip].l, m_streams[chip].r, m_streams[chip].base);

	M68KSetContext(&m_68K);
	M68KSaveState(StateFile, "Model1 Sound Board 68K");
	m_uart.SaveState(StateFile, "Model1 Sound Board UART");
	m_ym.SaveState(StateFile, "Model1 Sound Board YM3438");
	m_pcm1.SaveState(StateFile, "Model1 Sound Board MultiPCM 1");
	m_pcm2.SaveState(StateFile, "Model1 Sound Board MultiPCM 2");
	if (m_dsb != nullptr)
		m_dsb->SaveState(StateFile);
}

void CModel1SoundBoard::LoadState(CBlockFile *StateFile)
{
	if (Result::OKAY != StateFile->FindBlock("Model1 Sound Board"))
	{
		ErrorLog("Unable to load Model 1 sound board state. Save state file is corrupt.");
		return;
	}
	StateFile->Read(m_ram.get(), RAM_SIZE);
	UINT8 bank[2];
	StateFile->Read(bank, sizeof(bank));
	StateFile->Read(&m_now, sizeof(m_now));
	StateFile->Read(&m_uartClocks, sizeof(m_uartClocks));
	StateFile->Read(&m_outCount, sizeof(m_outCount));
	StateFile->Read(&m_sampleRemainder, sizeof(m_sampleRemainder));
	uint32_t n = 0;
	StateFile->Read(&n, sizeof(n));
	m_midiOut.clear();
	for (uint32_t i = 0; i < n && i < 0x10000; i++)
	{
		UINT8 b = 0;
		StateFile->Read(&b, sizeof(b));
		m_midiOut.push_back(b);
	}
	for (int chip = 0; chip < 3; chip++)
	{
		Stream &s = m_streams[chip];
		uint32_t count = 0;
		StateFile->Read(&s.base, sizeof(s.base));
		StateFile->Read(&count, sizeof(count));
		count = (std::min<uint32_t>)(count, 0x10000);
		s.l.resize(count);
		s.r.resize(count);
		if (count)
		{
			StateFile->Read(s.l.data(), count * sizeof(float));
			StateFile->Read(s.r.data(), count * sizeof(float));
		}
	}

	m_sliceStart = m_now;
	m_in68K = false;
	for (int chip = 0; chip < 2; chip++)
	{
		// SetBank would update the stream; just remap the page
		m_bank[chip] = bank[chip] & 3;
		size_t offset = (size_t)m_bank[chip] * 0x100000;
		const UINT8 *page = (m_sampleROM[chip] && offset + 0x100000 <= m_sampleSize[chip]) ? m_sampleROM[chip] + offset : nullptr;
		(chip ? m_pcm2 : m_pcm1).SetROMPage(1, page);
	}

	M68KSetContext(&m_68K);	// so we don't lose callback pointers when copying context back
	M68KLoadState(StateFile, "Model1 Sound Board 68K");
	m_uart.LoadState(StateFile, "Model1 Sound Board UART");	// re-drives IRQ 2 in the active context
	M68KGetContext(&m_68K);
	m_ym.LoadState(StateFile, "Model1 Sound Board YM3438");
	m_ym.SetTime(m_now);
	m_pcm1.LoadState(StateFile, "Model1 Sound Board MultiPCM 1");
	m_pcm2.LoadState(StateFile, "Model1 Sound Board MultiPCM 2");
	if (m_dsb != nullptr)
		m_dsb->LoadState(StateFile);
}
