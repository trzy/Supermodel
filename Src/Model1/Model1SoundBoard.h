// license:BSD-3-Clause
// copyright-holders:R. Belmont
/*
 * Ported to Supermodel from MAME (src/mame/shared/segam1audio.cpp, src/mame/shared/segam1audio.h).
 *
 * Model1SoundBoard.h
 *
 * Sega Model 1 sound board (68000 + YM3438 + 2x 315-5560 "MultiPCM" + T82C51
 * UART), used for Model 1 and early Model 2 games.
 *
 * Timing: everything on the board runs on one 40 MHz tick counter
 * (68000 @ 10 MHz = 4 ticks/cycle, YM3438 @ 8 MHz = 5 ticks/clock,
 * UART clock 500 kHz = 80 ticks, MultiPCM sample = 224 * 4 ticks,
 * YM3438 sample = 144 * 5 ticks). The 68K runs in slices of one serial bit
 * time (16 UART clocks = 320 cycles), cut short at YM3438 timer expiries.
 * Chip streams are brought up to date before every register access, as
 * MAME's stream->update() does.
 *
 * Audio: each frame produces 44100 / RefreshRate samples (fraction carried, as
 * CSoundBoard), and the emulated time per frame is exactly that many 44.1 kHz
 * sample periods. With RefreshRate = 57.524 Hz (Model 1: 16 MHz / (656 x 424))
 * that is 766 or 767 samples = ~173,840 68K cycles per frame. The native
 * streams (MultiPCM 44642.9 Hz, YM3438 55555.6 Hz) are resampled to 44.1 kHz
 * by linear interpolation and mixed with MAME's routing: YM3438 L/R x 0.30,
 * each MultiPCM L/R x 0.50, then scaled to Supermodel's 16-bit float range
 * (x 32768) and SoundVolume. Output goes to the front channels via OutputAudio().
 */

#ifndef INCLUDED_MODEL1SOUNDBOARD_H
#define INCLUDED_MODEL1SOUNDBOARD_H

#include "Types.h"
#include "CPU/Bus.h"
#include "CPU/68K/68K.h"
#include "BlockFile.h"
#include "Util/NewConfig.h"
#include "Model1/I8251.h"
#include "Model1/YM3438.h"
#include "Sound/MultiPCM.h"
#include <deque>
#include <functional>
#include <memory>
#include <vector>

class CDSB;

class CModel1SoundBoard : public IBus
{
public:
	/*
	 * CModel1SoundBoard(config):
	 * config is held by reference (read at run time: RefreshRate, SoundVolume,
	 * EmulateSound, FlipStereo).
	 */
	CModel1SoundBoard(const Util::Config::Node &config);
	~CModel1SoundBoard();

	/*
	 * Init(programROM, programSize, sampleROM1, sample1Size, sampleROM2, sample2Size):
	 * One-time initialisation. ROM regions as in MAME:
	 *   programROM  M1AUDIO_CPU_REGION, up to 0xC0000 bytes, 68K program in
	 *               Supermodel's 68K byte order (16-bit words stored little
	 *               endian, like CSoundBoard's sound ROM). This is the raw
	 *               byte order of ROMs MAME loads with ROM_LOAD16_WORD_SWAP.
	 *               Mapped at 000000-03FFFF, with 080000-09FFFF mirroring
	 *               region offset 0x20000.
	 *   sampleROM1  M1AUDIO_MPCM1_REGION (0x400000 bytes), MultiPCM #1.
	 *   sampleROM2  M1AUDIO_MPCM2_REGION (0x400000 bytes), MultiPCM #2.
	 * Each MultiPCM sees region 000000-0FFFFF fixed and one of the four 1 MB
	 * banks of its region at 100000-1FFFFF (bank registers C50000 / C70000).
	 * The program ROM is copied; the sample ROMs must stay valid.
	 */
	Result Init(const UINT8 *programROM, size_t programSize,
	            const UINT8 *sampleROM1, size_t sample1Size,
	            const UINT8 *sampleROM2, size_t sample2Size);

	// Resets the board (68K, UART, YM3438, MultiPCMs; attached DSB).
	void Reset();

	/*
	 * RunFrame():
	 * Runs one video frame (1 / RefreshRate s, default 57.524 Hz) of the
	 * board, then outputs the frame's audio. Returns OutputAudio()'s
	 * "buffer full" result, like CSoundBoard::RunFrame().
	 */
	bool RunFrame();

	/*
	 * Serial link to the main board (main uPD71051 TXD -> sound 8251 RXD,
	 * sound 8251 TXD -> main RXD and the DSB).
	 *
	 * WriteMIDI(byte): a byte sent by the main board. Queued on the line and
	 *   received by the sound UART at 31.25 kbaud as the board runs (RxRDY
	 *   raises 68K IRQ 2). Call it from the main board UART's OnTransmit.
	 * PendingMIDIIn(): bytes sent with WriteMIDI not yet received.
	 * SetMIDIOutCallback(cb): bytes sent by the sound 68K are passed to cb
	 *   (e.g. the main board CI8251's ReceiveByte), at the moment their start
	 *   bit goes out. Without a callback they are queued for ReadMIDI().
	 * ReadMIDI(byte): pops a byte sent by the sound board; false if none.
	 * GetUART(): the sound board's CI8251 itself.
	 */
	void WriteMIDI(UINT8 data);
	// The same, for a byte the main board sent at frameFraction (0-1) of the
	// frame it just ran: it is put on the line at that point of this board's
	// next frame
	void WriteMIDI(UINT8 data, double frameFraction);
	unsigned PendingMIDIIn() const;
	void SetMIDIOutCallback(std::function<void(UINT8)> callback);
	bool ReadMIDI(UINT8 *data);
	CI8251 &GetUART() { return m_uart; }

	/*
	 * AttachDSB(dsb):
	 * Connects a Digital Sound Board (Star Wars Arcade). As in MAME
	 * (rxd_handler().append(dsbz80::write_txd)) the DSB receives the bytes the
	 * sound 68K transmits; it is reset with the board and run each frame,
	 * mixed into the front channels like CSoundBoard does.
	 */
	void AttachDSB(CDSB *dsb);
	CDSB *GetDSB() { return m_dsb; }

	M68KCtx *GetM68K() { return &m_68K; }

	void SaveState(CBlockFile *StateFile);
	void LoadState(CBlockFile *StateFile);

	// IBus (68K address space)
	UINT8 Read8(UINT32 addr) override;
	UINT16 Read16(UINT32 addr) override;
	UINT32 Read32(UINT32 addr) override;
	void Write8(UINT32 addr, UINT8 data) override;
	void Write16(UINT32 addr, UINT16 data) override;
	void Write32(UINT32 addr, UINT32 data) override;

private:
	// Native-rate sample buffer for one chip, resampled to 44.1 kHz per frame
	struct Stream
	{
		uint64_t period = 1;		// ticks per native sample
		uint64_t base = 0;			// global index of l[0]
		std::vector<float> l, r;
		uint64_t Generated() const { return base + l.size(); }
	};

	uint64_t CurrentTick() const;
	void GenerateTo(int chip, uint64_t sampleCount);
	void UpdateStream(int chip, uint64_t now);
	void UpdateAllStreams(uint64_t now);
	void CatchUpUART(uint64_t now);
	void SetBank(int chip, UINT16 data);
	int NextFrameSamples();
	void Mix(int numSamples);

	UINT8 DeviceRead(UINT32 addr);				// odd-byte (umask16 0x00ff) device reads
	void DeviceWrite(UINT32 addr, UINT8 data);	// odd-byte device writes

	const Util::Config::Node &m_config;

	M68KCtx m_68K;
	bool m_in68K;
	uint64_t m_sliceStart;

	std::unique_ptr<UINT8[]> m_rom;		// 0xC0000 bytes, Supermodel 68K byte order
	std::unique_ptr<UINT8[]> m_ram;		// 0x10000 bytes at F00000
	const UINT8 *m_sampleROM[2];
	size_t m_sampleSize[2];
	UINT8 m_bank[2];

	CI8251 m_uart;
	CYM3438 m_ym;
	CMultiPCM m_pcm1, m_pcm2;
	CDSB *m_dsb;

	std::function<void(UINT8)> m_midiOutCallback;
	std::deque<UINT8> m_midiOut;
	std::deque<std::pair<uint64_t, UINT8>> m_midiInTimed;	// bytes from the main board and the tick they reach the line

	uint64_t m_now;				// 40 MHz ticks
	uint64_t m_uartClocks;		// UART clocks run so far (m_now / 80 once caught up)
	uint64_t m_outCount;		// 44.1 kHz samples output so far
	double m_sampleRemainder;

	Stream m_streams[3];		// 0 = YM3438, 1 = MultiPCM #1, 2 = MultiPCM #2
	std::vector<float> m_outL, m_outR, m_outRL, m_outRR;
};

#endif	// INCLUDED_MODEL1SOUNDBOARD_H
