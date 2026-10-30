// license:BSD-3-Clause
// copyright-holders:smf, Robbbert
/*
 * Ported to Supermodel from MAME (src/devices/machine/i8251.cpp, src/devices/machine/i8251.h,
 * with the parts of src/emu/diserial.cpp it relies on).
 *
 * I8251.h
 *
 * Intel 8251 USART (NEC uPD71051 is a clone). Used on both ends of the Sega
 * Model 1 main board <-> sound board link: the main board's uPD71051 at
 * 0xC40000 and the sound board's T82C51 at 0xC20000.
 *
 * Register interface, status bits, mode/command handling and the TxRDY /
 * RxRDY / TxEMPTY logic are a direct port of MAME's i8251_device.
 *
 * Framework replacement (the one structural deviation from MAME):
 * MAME shifts individual bits through device_serial_interface on TxC/RxC
 * edges and connects TXD->RXD as a wire. Supermodel runs the main board and
 * the sound board one after the other each frame, so a bit-level wire can't
 * be shared. Instead:
 *  - Clock(n) advances n periods of the TxC/RxC clock (the two are tied
 *    together on Model 1, 500 kHz = 16 x 31.25 kbaud). The transmitter runs
 *    MAME's transmit_clock() logic exactly, counting bits through a
 *    shift-register model instead of driving a TXD pin.
 *  - When the start bit of a character is presented on TXD (the first bit
 *    shifted out), OnTransmit(byte) is called. Connect it to the other end's
 *    ReceiveByte().
 *  - ReceiveByte() puts the byte on an internal "line" FIFO. The receiver
 *    takes characters off the line at most one per character time (start +
 *    data + parity + stop bits, x baud rate factor, in this UART's own clock)
 *    and completes each one 3/2 + (data + parity + 1 stop - 1) bit times after
 *    its start bit, the instant MAME's receive_clock() would have sampled the
 *    stop bit. receive_character() (RxRDY, overrun) is MAME's. So bursts of
 *    bytes handed over between frames arrive at the real 31.25 kbaud pace.
 *  - A character is dropped if the receiver is disabled (command bit 2) when
 *    it starts or completes (MAME would not see its start bit). Parity and
 *    framing errors cannot occur on this byte-level link.
 *  - Synchronous mode: mode/sync-byte register sequencing is ported, but
 *    sync-mode bit transfer (sync1_rxc/sync2_rxc, idle sync transmission,
 *    SYNDET hunt) is not; Model 1 uses async mode only.
 *  - TXD level, RTS/DTR outputs and break are not modelled (unused on Model 1).
 *  - devcb line callbacks become std::function members (not saved in states).
 */

#ifndef INCLUDED_I8251_H
#define INCLUDED_I8251_H

#include "Types.h"
#include "BlockFile.h"
#include <cstdint>
#include <deque>
#include <functional>

class CI8251
{
public:
	CI8251();

	// device_reset()
	void Reset();

	// MAME i8251_device::read/write: offset 0 = data, 1 = status/control
	uint8_t Read(uint32_t offset);
	void Write(uint32_t offset, uint8_t data);

	uint8_t ReadData();				// data_r
	uint8_t ReadStatus();			// status_r
	void WriteData(uint8_t data);	// data_w
	void WriteControl(uint8_t data);	// control_w

	// Pin inputs
	void WriteCTS(int state);		// write_cts (active low: 0 enables the transmitter)
	void WriteDSR(int state);		// write_dsr

	// Pin outputs (MAME txrdy_r / rxrdy_r) and TxEMPTY
	bool TxRdy() const;
	bool RxRdy() const;
	bool TxEmpty() const { return (m_status & I8251_STATUS_TX_EMPTY) != 0; }

	/*
	 * Clock(numClocks):
	 * Advances the TxC/RxC clock by numClocks full periods. For each period
	 * the receiver is clocked (rising edge) then the transmitter (falling
	 * edge), as with MAME's clock_device driving write_rxc + write_txc.
	 */
	void Clock(unsigned numClocks = 1);

	/*
	 * ReceiveByte(data):
	 * A character arriving on RXD from the remote end. Queued on the line and
	 * received at the serial rate as Clock() advances (see top of file).
	 */
	void ReceiveByte(uint8_t data);

	// Number of characters on the line not yet completely received
	unsigned PendingLineBytes() const { return (unsigned)m_line.size() + (m_rx_active ? 1u : 0u); }

	// Length in clocks of one character as currently framed (start+data+parity+stop) x rate factor
	unsigned CharacterClocks() const;

	// Callbacks (devcb replacements)
	std::function<void(uint8_t)> OnTransmit;	// start bit of 'byte' presented on TXD
	std::function<void(bool)> OnRxRdy;			// rxrdy_handler
	std::function<void(bool)> OnTxRdy;			// txrdy_handler
	std::function<void(bool)> OnTxEmpty;		// txempty_handler

	void SaveState(CBlockFile *StateFile, const char *name);
	void LoadState(CBlockFile *StateFile, const char *name);

private:
	enum
	{
		I8251_STATUS_FRAMING_ERROR = 0x20,
		I8251_STATUS_OVERRUN_ERROR = 0x10,
		I8251_STATUS_PARITY_ERROR = 0x08,
		I8251_STATUS_TX_EMPTY = 0x04,
		I8251_STATUS_RX_READY = 0x02,
		I8251_STATUS_TX_READY = 0x01
	};

	enum
	{
		I8251_NEXT_COMMAND = 0,
		I8251_NEXT_MODE,
		I8251_NEXT_SYNC1,
		I8251_NEXT_SYNC2,
	};

	enum parity_t
	{
		I8251_PARITY_NONE,
		I8251_PARITY_ODD,
		I8251_PARITY_EVEN,
		I8251_PARITY_MARK,
		I8251_PARITY_SPACE
	};

	enum stop_bits_t
	{
		I8251_STOP_BITS_0,
		I8251_STOP_BITS_1 = 1,
		I8251_STOP_BITS_1_5 = 2,
		I8251_STOP_BITS_2 = 3
	};

	void command_w(uint8_t data);
	void mode_w(uint8_t data);
	void sync1_w(uint8_t data);
	void sync2_w(uint8_t data);
	void receive_character(uint8_t ch);
	void update_rx_ready();
	void update_tx_ready();
	void update_tx_empty();
	void update_syndet(bool voltage);
	void transmit_clock();
	void receive_clock();
	bool is_tx_enabled() const;
	void check_for_tx_start();
	void start_tx();

	// device_serial_interface subset
	void set_data_frame(int start_bit_count, int data_bit_count, parity_t parity, stop_bits_t stop_bits);
	void receive_register_reset();
	void transmit_register_reset() { m_tra_empty = true; }
	void transmit_register_setup(uint8_t data_byte);
	void transmit_register_get_data_bit();
	bool is_transmit_register_empty() const { return m_tra_empty; }

	// i8251 state (names as in MAME)
	uint8_t m_flags;
	uint8_t m_sync_byte_count;
	uint8_t m_sync1;
	uint16_t m_sync2;
	uint8_t m_status;
	uint8_t m_command;
	uint8_t m_mode_byte;
	bool m_delayed_tx_en;
	int32_t m_cts;
	int32_t m_dsr;
	int m_txc_count;
	int m_br_factor;
	uint8_t m_rx_data;
	uint8_t m_tx_data;
	uint8_t m_sync8;
	uint16_t m_sync16;
	uint8_t m_tx_sync_shift;
	bool m_syndet_pin;
	bool m_hunt_on;
	bool m_ext_syn_set;
	uint8_t m_rxd_bits;
	uint8_t m_data_bits_count;

	// diserial data frame
	uint8_t m_df_start_bit_count;
	uint8_t m_df_word_length;
	uint8_t m_df_parity;
	uint8_t m_df_stop_bit_count;
	uint8_t m_df_min_rx_stop_bit_count;
	uint8_t m_rcv_bit_count;

	// transmit shift register model
	bool m_tra_empty;
	uint8_t m_tra_byte;
	uint8_t m_tra_bit_count;
	uint8_t m_tra_bit_count_transmitted;

	// byte-level line / receiver model
	std::deque<uint8_t> m_line;
	bool m_rx_active;
	uint8_t m_rx_byte;
	uint32_t m_rx_countdown;		// clocks until the active character completes
	uint32_t m_line_countdown;		// clocks until the next character's start bit may begin
};

#endif	// INCLUDED_I8251_H
