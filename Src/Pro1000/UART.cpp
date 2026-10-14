/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * UART.cpp
 *
 * Implementation of the CUART class: a 16550-style serial port.
 *
 * Registers
 * ---------
 * The register selected also depends on the divisor latch access bit (DLAB,
 * bit 7 of LCR), which swaps the baud rate divisor in at registers 0 and 1.
 *
 * Reg  DLAB  Read                            Write
 * ---  ----  ------------------------------  ------------------------------
 * 0    0     RBR  receive buffer             THR  transmit holding
 * 0    1     DLL  divisor latch, low byte    DLL  divisor latch, low byte
 * 1    0     IER  interrupt enable           IER  interrupt enable
 * 1    1     DLM  divisor latch, high byte   DLM  divisor latch, high byte
 * 2    x     IIR  interrupt identification   FCR  FIFO control
 * 3    x     LCR  line control               LCR  line control
 * 4    x     MCR  modem control              MCR  modem control
 * 5    x     LSR  line status                (read-only)
 * 6    x     MSR  modem status               (read-only)
 * 7    x     SCR  scratch                    SCR  scratch
 *
 * Only the registers' contents matter to us: the divisor, word length,
 * parity, and stop bits are stored but have no effect.
 *
 * Transmission is instantaneous: THR is always empty, so LSR.THRE and
 * LSR.TEMT are always set. Received characters are taken from console input
 * a line at a time (followed by a carriage return) whenever LSR or RBR is
 * read and the receive FIFO has room, and on each Tick(). LSR.DR is set while
 * the FIFO is not empty. Characters arrive immediately rather than at the
 * baud rate.
 *
 * Interrupts: only received data available (IER bit 0) is implemented. It is
 * pending while the receive FIFO is not empty (trigger level of 1). The INT
 * output is the pending condition gated by MCR OUT1 (bit 2): the Pro-1000
 * firmware sets OUT1 on entry to its interrupt handler and clears it on exit,
 * producing a new edge for the edge triggered 8259 if more data remains.
 */

#include "UART.h"

#include "Supermodel.h"
#include "OSD/ConsoleInput.h"
#include <cstdio>

static constexpr uint8_t LCR_DLAB = 0x80;  // divisor latch access: registers 0 and 1 become DLL and DLM
static constexpr uint8_t IER_RDA  = 0x01;  // received data available interrupt enable
static constexpr uint8_t IER_MASK = 0x0F;  // IER bits 7-4 are always 0
static constexpr uint8_t FCR_FIFO_ENABLE = 0x01;  // FIFOs enabled (16550 mode)
static constexpr uint8_t IIR_FIFOS_ENABLED = 0xC0;  // IIR bits 7-6: both set when FIFOs are enabled
static constexpr uint8_t IIR_NONE = 0x01;  // no interrupt pending (bit 0 is active low)
static constexpr uint8_t IIR_RDA  = 0x04;  // received data available interrupt pending
static constexpr uint8_t MCR_OUT1 = 0x04;  // user output 1; used by the firmware to mask our interrupt
static constexpr uint8_t MCR_MASK = 0x1F;  // MCR bits 7-5 are always 0
static constexpr uint8_t LSR_DR   = 0x01;  // data ready: receive buffer/FIFO holds at least one character
static constexpr uint8_t LSR_THRE = 0x20;  // transmit holding register empty: THR can accept a character
static constexpr uint8_t LSR_TEMT = 0x40;  // transmitter empty: THR and shift register both idle
static constexpr uint8_t MSR_DCD  = 0x80;  // data carrier detect
static constexpr uint8_t MSR_DSR  = 0x20;  // data set ready
static constexpr uint8_t MSR_CTS  = 0x10;  // clear to send

static constexpr size_t MAX_LINE_LENGTH = 256;  // transmitted lines longer than this are split
static constexpr size_t RX_FIFO_SIZE = 16;      // 16550 receive FIFO depth

uint8_t CUART::Read(unsigned reg)
{
  switch (reg & 7)
  {
  case 0:
  {
    // DLL, or RBR: pop the next received character (0 if none). Popping may
    // empty the FIFO, which clears the receive interrupt.
    if (m_lcr & LCR_DLAB)
      return m_dll;
    FillReceiveFIFO();
    if (m_rxFIFO.empty())
      return 0x00;
    uint8_t data = m_rxFIFO.front();
    m_rxFIFO.pop_front();
    UpdateInterrupt();
    return data;
  }
  case 1:
    // DLM or IER
    return (m_lcr & LCR_DLAB) ? m_dlm : m_ier;
  case 2:
  {
    // IIR: identifies the highest priority pending interrupt. Received data
    // available is the only one we generate. Bits 7-6 reflect FCR bit 0.
    uint8_t fifoBits = (m_fcr & FCR_FIFO_ENABLE) ? IIR_FIFOS_ENABLED : 0x00;
    if (ReceiveInterruptPending())
      return fifoBits | IIR_RDA;
    return fifoBits | IIR_NONE;
  }
  case 3:
    // LCR
    return m_lcr;
  case 4:
    // MCR
    return m_mcr;
  case 5:
    // LSR: transmitter is always idle and data ready reflects the receive
    // FIFO, which is first topped up from console input so that polling
    // firmware (e.g., the boot stub) sees new characters
    FillReceiveFIFO();
    return LSR_THRE | LSR_TEMT | (m_rxFIFO.empty() ? 0 : LSR_DR);
  case 6:
    // MSR: a terminal is always connected and ready
    return MSR_DCD | MSR_DSR | MSR_CTS;
  default:  // 7
    // SCR: plain read/write storage with no function
    return m_scr;
  }
}

void CUART::Write(unsigned reg, uint8_t data)
{
  switch (reg & 7)
  {
  case 0:
    // DLL, or THR: send a character to the console
    if (m_lcr & LCR_DLAB)
      m_dll = data;
    else
      TransmitByte(data);
    break;
  case 1:
    // DLM or IER. Enabling or disabling the receive interrupt may change the
    // interrupt output.
    if (m_lcr & LCR_DLAB)
      m_dlm = data;
    else
      m_ier = data & IER_MASK;
    UpdateInterrupt();
    break;
  case 2:
    // FCR: only the FIFO enable bit is used (reflected in IIR). FIFO reset
    // bits and the receive trigger level are ignored.
    m_fcr = data;
    break;
  case 3:
    // LCR: only DLAB has an effect
    m_lcr = data;
    break;
  case 4:
    // MCR: OUT1 gates the interrupt output
    m_mcr = data & MCR_MASK;
    UpdateInterrupt();
    break;
  case 5:
  case 6:
    // LSR and MSR are read-only
    break;
  default:  // 7
    // SCR
    m_scr = data;
    break;
  }
}

void CUART::TransmitByte(uint8_t data)
{
  if (data == '\n')
  {
    FlushLine();
    return;
  }

  if (data == '\r')
    return;

  if ((data >= 0x20 && data < 0x7F) || data == '\t')
    m_line += char(data);
  else
  {
    char hex[8];
    snprintf(hex, sizeof(hex), "\\x%02X", data);
    m_line += hex;
  }

  if (m_line.length() >= MAX_LINE_LENGTH)
    FlushLine();
}

void CUART::FlushLine(void)
{
  printf("UART: %s\n", m_line.c_str());
  InfoLog("UART: %s", m_line.c_str());
  m_line.clear();
}

void CUART::FillReceiveFIFO(void)
{
  while (m_rxFIFO.size() < RX_FIFO_SIZE)
  {
    if (m_rxPending.empty())
    {
      std::string line;
      if (!m_input || !m_input->ReadLine(&line, false))
        break;
      m_rxPending = line + '\r';   // a terminal sends carriage return at end of line
    }
    m_rxFIFO.push_back(uint8_t(m_rxPending[0]));
    m_rxPending.erase(0, 1);
  }
}

bool CUART::ReceiveInterruptPending(void) const
{
  return (m_ier & IER_RDA) && !m_rxFIFO.empty();
}

void CUART::UpdateInterrupt(void)
{
  bool level = ReceiveInterruptPending() && !(m_mcr & MCR_OUT1);
  if (level != m_intPin)
  {
    m_intPin = level;
    if (m_intCallback)
      m_intCallback(level);
  }
}

void CUART::Tick(void)
{
  FillReceiveFIFO();
  UpdateInterrupt();
}

void CUART::AttachInterruptOutput(std::function<void(bool)> callback)
{
  m_intCallback = callback;
}

void CUART::AttachInput(IConsoleInput *input)
{
  if (m_input)
    m_input->Release();
  m_input = input;
  if (m_input)
    m_input->Claim();
}

void CUART::Reset(void)
{
  if (!m_line.empty())
    FlushLine();
  m_rxFIFO.clear();
  m_rxPending.clear();
  m_ier = 0;
  m_fcr = 0;
  m_lcr = 0;
  m_mcr = 0;
  m_scr = 0;
  m_dll = 0;
  m_dlm = 0;
  UpdateInterrupt();
}

CUART::~CUART(void)
{
  if (!m_line.empty())
    FlushLine();
  if (m_input)
    m_input->Release();
}
