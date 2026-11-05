// license:BSD-3-Clause
// copyright-holders:smf, Robbbert
/*
 * Ported to Supermodel from MAME (src/devices/machine/i8251.cpp, with the parts of
 * src/emu/diserial.cpp it relies on).
 *
 * Intel 8251 Universal Synchronous/Asynchronous Receiver Transmitter code
 * NEC uPD71051 is a clone
 *
 * See I8251.h for how the bit-level serial interface was replaced with a
 * byte-level, clock-timed link.
 */

#include "Model1/I8251.h"
#include "OSD/Logger.h"

#define BIT(x, n) (((x) >> (n)) & 1)

/******************************************************************************
 Construction / reset
******************************************************************************/

CI8251::CI8251()
	: m_cts(1),
	  m_dsr(1)
{
	// device_start: zerofill
	m_flags = 0;
	m_sync_byte_count = 0;
	m_sync1 = 0;
	m_sync2 = 0;
	m_status = 0;
	m_command = 0;
	m_mode_byte = 0;
	m_delayed_tx_en = false;
	m_txc_count = 0;
	m_br_factor = 0;
	m_rx_data = 0;
	m_tx_data = 0;
	m_sync8 = 0;
	m_sync16 = 0;
	m_tx_sync_shift = 0;
	m_syndet_pin = false;
	m_hunt_on = false;
	m_ext_syn_set = false;
	m_rxd_bits = 0;
	m_data_bits_count = 0;

	// device_serial_interface constructor
	m_df_start_bit_count = 0;
	m_df_word_length = 0;
	m_df_parity = I8251_PARITY_NONE;
	m_df_stop_bit_count = 0;
	m_df_min_rx_stop_bit_count = 0;
	m_rcv_bit_count = 0;
	m_tra_empty = true;
	m_tra_byte = 0;
	m_tra_bit_count = 0;
	m_tra_bit_count_transmitted = 0;

	m_rx_active = false;
	m_rx_byte = 0;
	m_rx_countdown = 0;
	m_line_countdown = 0;
}

void CI8251::Reset()
{
	// what is the default setup when the 8251 has been reset???
	// i8251 datasheet explains the state of tx pin at reset
	// tx is set to 1 (TXD/RTS/DTR not modelled)

	transmit_register_reset();
	receive_register_reset();
	// expecting mode byte
	m_flags = I8251_NEXT_MODE;
	// no character to read by cpu
	// transmitter is ready and is empty
	m_status = I8251_STATUS_TX_EMPTY | I8251_STATUS_TX_READY;
	m_mode_byte = 0;
	m_command = 0;
	m_rx_data = 0;
	m_tx_data = 0;
	m_txc_count = 0;
	m_br_factor = 1;

	// Supermodel: characters in flight on the byte-level line are lost
	m_line.clear();
	m_line_countdown = 0;

	update_tx_empty();
	update_rx_ready();
	update_tx_ready();
}

/******************************************************************************
 device_serial_interface subset
******************************************************************************/

void CI8251::set_data_frame(int start_bit_count, int data_bit_count, parity_t parity, stop_bits_t stop_bits)
{
	bool df_changed = (start_bit_count != m_df_start_bit_count) || (data_bit_count != m_df_word_length) || (parity != m_df_parity);
	uint8_t orig_stop_bit_count = m_df_stop_bit_count;
	uint8_t orig_min_rx_stop_bit_count = m_df_min_rx_stop_bit_count;

	m_df_word_length = (uint8_t)data_bit_count;

	switch (stop_bits)
	{
	case I8251_STOP_BITS_0:
	default:
		m_df_stop_bit_count = 0;
		m_df_min_rx_stop_bit_count = 0;
		break;
	case I8251_STOP_BITS_1:
		m_df_stop_bit_count = 1;
		m_df_min_rx_stop_bit_count = 1;
		break;
	case I8251_STOP_BITS_1_5:
		m_df_stop_bit_count = 2; // TODO: support 1.5 stop bits
		m_df_min_rx_stop_bit_count = 1;
		break;
	case I8251_STOP_BITS_2:
		m_df_stop_bit_count = 2;
		m_df_min_rx_stop_bit_count = 1;
		break;
	}

	df_changed = df_changed || (m_df_stop_bit_count != orig_stop_bit_count) || (m_df_min_rx_stop_bit_count != orig_min_rx_stop_bit_count);
	if (!df_changed)
		return;

	m_df_parity = (uint8_t)parity;
	m_df_start_bit_count = (uint8_t)start_bit_count;

	/* Require at least one stop bit in async RX mode, none in sync RX mode. */
	m_rcv_bit_count = m_df_word_length + m_df_min_rx_stop_bit_count;
	if (m_df_parity != I8251_PARITY_NONE)
		m_rcv_bit_count++;

	receive_register_reset();
}

void CI8251::receive_register_reset()
{
	// Supermodel: abandon any character being shifted in (MAME would resynchronise on the next start bit)
	m_rx_active = false;
	m_rx_countdown = 0;
}

void CI8251::transmit_register_setup(uint8_t data_byte)
{
	m_tra_byte = data_byte;
	m_tra_bit_count_transmitted = 0;
	m_tra_bit_count = m_df_start_bit_count + m_df_word_length + (m_df_parity != I8251_PARITY_NONE ? 1 : 0) + m_df_stop_bit_count;
	m_tra_empty = false;
}

void CI8251::transmit_register_get_data_bit()
{
	// Supermodel: instead of presenting the bit on TXD, signal the whole character when its first
	// (start) bit goes out. The receiving CI8251 times the rest of the character itself.
	if (m_tra_bit_count_transmitted == 0 && OnTransmit)
		OnTransmit(m_tra_byte);

	m_tra_bit_count_transmitted++;

	/* have all bits of this stream formatted byte been sent? */
	if (m_tra_bit_count_transmitted >= m_tra_bit_count)
		m_tra_empty = true;
}

unsigned CI8251::CharacterClocks() const
{
	unsigned bits = m_df_start_bit_count + m_df_word_length + (m_df_parity != I8251_PARITY_NONE ? 1 : 0) + m_df_stop_bit_count;
	return bits * (unsigned)(m_br_factor > 0 ? m_br_factor : 1);
}

/******************************************************************************
 i8251_device
******************************************************************************/

void CI8251::update_rx_ready()
{
	if (OnRxRdy)
		OnRxRdy(RxRdy());
}

void CI8251::receive_clock()
{
	// Supermodel: byte-level replacement for MAME's bit sampling (see I8251.h)
	if (m_line_countdown)
		--m_line_countdown;

	if (m_rx_active)
	{
		if (--m_rx_countdown == 0)
		{
			m_rx_active = false;
			if (BIT(m_command, 2))
				receive_character((uint8_t)(m_rx_byte & ~(0xff << m_df_word_length)));
		}
	}

	if (!m_rx_active && m_line_countdown == 0 && !m_line.empty())
	{
		uint8_t ch = m_line.front();
		m_line.pop_front();
		m_line_countdown = CharacterClocks();

		/* receive enable? (async only) */
		if (BIT(m_command, 2) && m_sync_byte_count == 0 && m_rcv_bit_count > 0)
		{
			// MAME: start bit seen, then m_rxc_count = 3 * br / 2 clocks to the middle of the first
			// data bit, then br clocks per bit until m_rcv_bit_count bits (data, parity, 1 stop) are in.
			m_rx_active = true;
			m_rx_byte = ch;
			m_rx_countdown = (uint32_t)(3 * m_br_factor / 2 + (m_rcv_bit_count - 1) * m_br_factor);
			if (m_rx_countdown == 0)
				m_rx_countdown = 1;
		}
		else
			DebugLog("I8251: character %02X dropped (receiver disabled)\n", ch);
	}
}

bool CI8251::is_tx_enabled() const
{
	return BIT(m_command, 0) && !m_cts;
}

void CI8251::check_for_tx_start()
{
	if (is_tx_enabled() && is_transmit_register_empty() && (m_status & (I8251_STATUS_TX_READY)) == 0)
		start_tx();
}

void CI8251::start_tx()
{
	transmit_register_setup(m_tx_data);
	m_status &= ~I8251_STATUS_TX_EMPTY;
	m_status |= I8251_STATUS_TX_READY;
}

void CI8251::transmit_clock()
{
	m_txc_count++;
	if (m_txc_count != m_br_factor)
		return;

	m_txc_count = 0;

	if (is_transmit_register_empty())
	{
		if ((m_status & I8251_STATUS_TX_READY) == 0 && (is_tx_enabled() || m_delayed_tx_en))
			start_tx();
		else
		{
			m_status |= I8251_STATUS_TX_EMPTY;
			// MAME: in synchronous mode, sync characters are transmitted while idle. Not ported
			// (sync mode isn't used on Model 1); otherwise TxD returns to marking state.
		}

		update_tx_ready();
		update_tx_empty();
	}

	/* if diserial has bits to send, make them so */
	if (!is_transmit_register_empty())
		transmit_register_get_data_bit();
}

void CI8251::update_tx_ready()
{
	if (OnTxRdy)
		OnTxRdy(TxRdy());
}

void CI8251::update_tx_empty()
{
	if (OnTxEmpty)
		OnTxEmpty((m_status & I8251_STATUS_TX_EMPTY) != 0);
}

void CI8251::update_syndet(bool voltage)
{
	// Adjust status register
	if (voltage)
		m_status |= 0x40;
	else
		m_status &= ~0x40;
	// SYNDET output pin not modelled
}

void CI8251::command_w(uint8_t data)
{
	/* command */
	m_command = data;

	// bit 0: transmit enable, 1: DTR, 2: receive enable, 3: send break,
	// 4: error reset, 5: RTS, 6: internal reset, 7: hunt mode

	if (BIT(data, 4))
		m_status &= ~(I8251_STATUS_PARITY_ERROR | I8251_STATUS_OVERRUN_ERROR | I8251_STATUS_FRAMING_ERROR);

	if (BIT(data, 6))
	{
		// datasheet says "returns to mode format", not
		// completely resets the chip.  behavior of DEC Rainbow
		// backs this up.
		m_flags = I8251_NEXT_MODE;
	}

	// Hunt mode
	m_hunt_on = false;
	update_syndet(false);
	if (m_sync_byte_count)
		m_hunt_on = BIT(data, 7);
	if (m_hunt_on)
		m_ext_syn_set = false;

	check_for_tx_start();
	update_rx_ready();
	update_tx_ready();
	update_tx_empty();
}

void CI8251::mode_w(uint8_t data)
{
	m_mode_byte = data;

	m_data_bits_count = ((data >> 2) & 0x03) + 5;

	parity_t parity = I8251_PARITY_NONE;
	switch (data & 0x30)
	{
		case 0x10:
			parity = I8251_PARITY_ODD;
			break;
		case 0x30:
			parity = I8251_PARITY_EVEN;
			break;
		default:
			break;
	}

	stop_bits_t stop_bits = I8251_STOP_BITS_0;
	m_br_factor = 1;
	m_flags = I8251_NEXT_COMMAND;
	m_sync_byte_count = 0;
	m_syndet_pin = false;

	/* Synchronous or Asynchronous? */
	if (data & 0x03)
	{
		switch (data & 0xc0)
		{
			case 0x40:
				stop_bits = I8251_STOP_BITS_1;
				break;
			case 0x80:
				stop_bits = I8251_STOP_BITS_1_5;
				break;
			case 0xc0:
				stop_bits = I8251_STOP_BITS_2;
				break;
			default:
				break;
		}

		set_data_frame(1, m_data_bits_count, parity, stop_bits);

		switch (data & 0x03)
		{
			case 2:
				m_br_factor = 16;
				break;
			case 3:
				m_br_factor = 64;
				break;
		}
	}
	else
	{
		/* setup for sync byte(s) */
		m_flags = BIT(data, 7) ? I8251_NEXT_SYNC2 : I8251_NEXT_SYNC1;
		m_sync_byte_count = BIT(data, 7) ? 1 : 2;
		set_data_frame(0, m_data_bits_count, parity, stop_bits);
		m_syndet_pin = BIT(data, 6);
		m_sync8 = 0;
		m_sync16 = 0;
		m_tx_sync_shift = 8;
		m_rxd_bits = 0;
		m_sync1 = 0;
	}

	receive_register_reset();
	m_txc_count = 0;
}

void CI8251::sync1_w(uint8_t data)
{
	m_sync16 = (uint16_t)(data << 8);
	m_flags = I8251_NEXT_SYNC2;
}

void CI8251::sync2_w(uint8_t data)
{
	if (m_sync_byte_count == 2)
		m_sync16 |= data;
	else
		m_sync8 = data;
	m_flags = I8251_NEXT_COMMAND;
}

void CI8251::WriteControl(uint8_t data)
{
	if (m_flags == I8251_NEXT_SYNC1)
		sync1_w(data);
	else if (m_flags == I8251_NEXT_SYNC2)
		sync2_w(data);
	else if (m_flags == I8251_NEXT_MODE)
		mode_w(data);
	else
		command_w(data);
}

uint8_t CI8251::ReadStatus()
{
	uint8_t status = (uint8_t)((m_dsr << 7) | m_status);
	// Syndet always goes off after status read
	update_syndet(false);
	return status;
}

void CI8251::WriteData(uint8_t data)
{
	m_tx_data = data;

	/* writing clears */
	m_status &=~I8251_STATUS_TX_READY;
	update_tx_ready();

	// Store state of tx enable when writing to DB buffer
	m_delayed_tx_en = is_tx_enabled();

	check_for_tx_start();

	/* if transmitter is active, then tx empty will be signalled */
	update_tx_ready();
	update_tx_empty();
}

void CI8251::receive_character(uint8_t ch)
{
	m_rx_data = ch;

	/* char has not been read and another has arrived! */
	if (m_status & I8251_STATUS_RX_READY)
		m_status |= I8251_STATUS_OVERRUN_ERROR;

	m_status |= I8251_STATUS_RX_READY;

	update_rx_ready();
}

uint8_t CI8251::ReadData()
{
	/* reading clears */
	m_status &= ~I8251_STATUS_RX_READY;
	update_rx_ready();
	return m_rx_data;
}

uint8_t CI8251::Read(uint32_t offset)
{
	if (offset)
		return ReadStatus();
	else
		return ReadData();
}

void CI8251::Write(uint32_t offset, uint8_t data)
{
	if (offset)
		WriteControl(data);
	else
		WriteData(data);
}

void CI8251::WriteCTS(int state)
{
	m_cts = state ? 1 : 0;

	check_for_tx_start();
	update_tx_ready();
	update_tx_empty();
}

void CI8251::WriteDSR(int state)
{
	m_dsr = !state;
}

bool CI8251::TxRdy() const
{
	return is_tx_enabled() && (m_status & I8251_STATUS_TX_READY) != 0;
}

bool CI8251::RxRdy() const
{
	// masked?
	if (!BIT(m_command, 2))
		return false;

	return (m_status & I8251_STATUS_RX_READY) != 0;
}

void CI8251::Clock(unsigned numClocks)
{
	// clock_device output: rising edge (write_rxc -> receive_clock), then falling edge
	// (write_txc -> transmit_clock), once per period. Sync-mode receive is not ported.
	for (unsigned i = 0; i < numClocks; i++)
	{
		receive_clock();
		transmit_clock();
	}
}

void CI8251::ReceiveByte(uint8_t data)
{
	m_line.push_back(data);
}

/******************************************************************************
 Save states
******************************************************************************/

void CI8251::SaveState(CBlockFile *StateFile, const char *name)
{
	StateFile->NewBlock(name, __FILE__);
	StateFile->Write(&m_flags, sizeof(m_flags));
	StateFile->Write(&m_sync_byte_count, sizeof(m_sync_byte_count));
	StateFile->Write(&m_sync1, sizeof(m_sync1));
	StateFile->Write(&m_sync2, sizeof(m_sync2));
	StateFile->Write(&m_status, sizeof(m_status));
	StateFile->Write(&m_command, sizeof(m_command));
	StateFile->Write(&m_mode_byte, sizeof(m_mode_byte));
	StateFile->Write(&m_delayed_tx_en, sizeof(m_delayed_tx_en));
	StateFile->Write(&m_cts, sizeof(m_cts));
	StateFile->Write(&m_dsr, sizeof(m_dsr));
	StateFile->Write(&m_txc_count, sizeof(m_txc_count));
	StateFile->Write(&m_br_factor, sizeof(m_br_factor));
	StateFile->Write(&m_rx_data, sizeof(m_rx_data));
	StateFile->Write(&m_tx_data, sizeof(m_tx_data));
	StateFile->Write(&m_sync8, sizeof(m_sync8));
	StateFile->Write(&m_sync16, sizeof(m_sync16));
	StateFile->Write(&m_tx_sync_shift, sizeof(m_tx_sync_shift));
	StateFile->Write(&m_syndet_pin, sizeof(m_syndet_pin));
	StateFile->Write(&m_hunt_on, sizeof(m_hunt_on));
	StateFile->Write(&m_ext_syn_set, sizeof(m_ext_syn_set));
	StateFile->Write(&m_rxd_bits, sizeof(m_rxd_bits));
	StateFile->Write(&m_data_bits_count, sizeof(m_data_bits_count));
	StateFile->Write(&m_df_start_bit_count, sizeof(m_df_start_bit_count));
	StateFile->Write(&m_df_word_length, sizeof(m_df_word_length));
	StateFile->Write(&m_df_parity, sizeof(m_df_parity));
	StateFile->Write(&m_df_stop_bit_count, sizeof(m_df_stop_bit_count));
	StateFile->Write(&m_df_min_rx_stop_bit_count, sizeof(m_df_min_rx_stop_bit_count));
	StateFile->Write(&m_rcv_bit_count, sizeof(m_rcv_bit_count));
	StateFile->Write(&m_tra_empty, sizeof(m_tra_empty));
	StateFile->Write(&m_tra_byte, sizeof(m_tra_byte));
	StateFile->Write(&m_tra_bit_count, sizeof(m_tra_bit_count));
	StateFile->Write(&m_tra_bit_count_transmitted, sizeof(m_tra_bit_count_transmitted));
	StateFile->Write(&m_rx_active, sizeof(m_rx_active));
	StateFile->Write(&m_rx_byte, sizeof(m_rx_byte));
	StateFile->Write(&m_rx_countdown, sizeof(m_rx_countdown));
	StateFile->Write(&m_line_countdown, sizeof(m_line_countdown));
	uint32_t lineSize = (uint32_t)m_line.size();
	StateFile->Write(&lineSize, sizeof(lineSize));
	for (uint8_t b : m_line)
		StateFile->Write(&b, sizeof(b));
}

void CI8251::LoadState(CBlockFile *StateFile, const char *name)
{
	if (Result::OKAY != StateFile->FindBlock(name))
	{
		ErrorLog("Unable to load 8251 UART state. Save state file is corrupt.");
		return;
	}
	StateFile->Read(&m_flags, sizeof(m_flags));
	StateFile->Read(&m_sync_byte_count, sizeof(m_sync_byte_count));
	StateFile->Read(&m_sync1, sizeof(m_sync1));
	StateFile->Read(&m_sync2, sizeof(m_sync2));
	StateFile->Read(&m_status, sizeof(m_status));
	StateFile->Read(&m_command, sizeof(m_command));
	StateFile->Read(&m_mode_byte, sizeof(m_mode_byte));
	StateFile->Read(&m_delayed_tx_en, sizeof(m_delayed_tx_en));
	StateFile->Read(&m_cts, sizeof(m_cts));
	StateFile->Read(&m_dsr, sizeof(m_dsr));
	StateFile->Read(&m_txc_count, sizeof(m_txc_count));
	StateFile->Read(&m_br_factor, sizeof(m_br_factor));
	StateFile->Read(&m_rx_data, sizeof(m_rx_data));
	StateFile->Read(&m_tx_data, sizeof(m_tx_data));
	StateFile->Read(&m_sync8, sizeof(m_sync8));
	StateFile->Read(&m_sync16, sizeof(m_sync16));
	StateFile->Read(&m_tx_sync_shift, sizeof(m_tx_sync_shift));
	StateFile->Read(&m_syndet_pin, sizeof(m_syndet_pin));
	StateFile->Read(&m_hunt_on, sizeof(m_hunt_on));
	StateFile->Read(&m_ext_syn_set, sizeof(m_ext_syn_set));
	StateFile->Read(&m_rxd_bits, sizeof(m_rxd_bits));
	StateFile->Read(&m_data_bits_count, sizeof(m_data_bits_count));
	StateFile->Read(&m_df_start_bit_count, sizeof(m_df_start_bit_count));
	StateFile->Read(&m_df_word_length, sizeof(m_df_word_length));
	StateFile->Read(&m_df_parity, sizeof(m_df_parity));
	StateFile->Read(&m_df_stop_bit_count, sizeof(m_df_stop_bit_count));
	StateFile->Read(&m_df_min_rx_stop_bit_count, sizeof(m_df_min_rx_stop_bit_count));
	StateFile->Read(&m_rcv_bit_count, sizeof(m_rcv_bit_count));
	StateFile->Read(&m_tra_empty, sizeof(m_tra_empty));
	StateFile->Read(&m_tra_byte, sizeof(m_tra_byte));
	StateFile->Read(&m_tra_bit_count, sizeof(m_tra_bit_count));
	StateFile->Read(&m_tra_bit_count_transmitted, sizeof(m_tra_bit_count_transmitted));
	StateFile->Read(&m_rx_active, sizeof(m_rx_active));
	StateFile->Read(&m_rx_byte, sizeof(m_rx_byte));
	StateFile->Read(&m_rx_countdown, sizeof(m_rx_countdown));
	StateFile->Read(&m_line_countdown, sizeof(m_line_countdown));
	uint32_t lineSize = 0;
	StateFile->Read(&lineSize, sizeof(lineSize));
	m_line.clear();
	for (uint32_t i = 0; i < lineSize && i < 0x10000; i++)
	{
		uint8_t b = 0;
		StateFile->Read(&b, sizeof(b));
		m_line.push_back(b);
	}

	// Re-drive the output pins
	update_rx_ready();
	update_tx_ready();
	update_tx_empty();
}
