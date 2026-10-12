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
 * 0  RBR (read) / THR (write) / DLL (when LCR.DLAB=1)
 * 1  IER / DLM (when LCR.DLAB=1)
 * 2  IIR (read) / FCR (write)
 * 3  LCR
 * 4  MCR
 * 5  LSR
 * 6  MSR
 * 7  SCR
 *
 * Transmission is instantaneous: THR is always empty, so LSR.THRE and
 * LSR.TEMT are always set. Nothing is ever received, so LSR.DR is clear.
 */

#include "UART.h"

#include "Supermodel.h"
#include <cstdio>

static constexpr uint8_t LCR_DLAB = 0x80;
static constexpr uint8_t LSR_THRE = 0x20;
static constexpr uint8_t LSR_TEMT = 0x40;

static constexpr size_t MAX_LINE_LENGTH = 256;

uint8_t CUART::Read(unsigned reg)
{
  switch (reg & 7)
  {
  case 0:
    return (m_lcr & LCR_DLAB) ? m_dll : 0x00; // receive buffer always empty
  case 1:
    return (m_lcr & LCR_DLAB) ? m_dlm : m_ier;
  case 2:
    return (m_fcr & 1) ? 0xC1 : 0x01;         // no interrupt pending; FIFO enabled bits
  case 3:
    return m_lcr;
  case 4:
    return m_mcr;
  case 5:
    return LSR_THRE | LSR_TEMT;
  case 6:
    return 0xB0;                              // DCD, DSR, CTS asserted
  default:  // 7
    return m_scr;
  }
}

void CUART::Write(unsigned reg, uint8_t data)
{
  switch (reg & 7)
  {
  case 0:
    if (m_lcr & LCR_DLAB)
      m_dll = data;
    else
      TransmitByte(data);
    break;
  case 1:
    if (m_lcr & LCR_DLAB)
      m_dlm = data;
    else
      m_ier = data & 0x0F;
    break;
  case 2:
    m_fcr = data;
    break;
  case 3:
    m_lcr = data;
    break;
  case 4:
    m_mcr = data & 0x1F;
    break;
  case 5:
  case 6:
    break;  // read-only
  default:  // 7
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

void CUART::Reset(void)
{
  if (!m_line.empty())
    FlushLine();
  m_ier = 0;
  m_fcr = 0;
  m_lcr = 0;
  m_mcr = 0;
  m_scr = 0;
  m_dll = 0;
  m_dlm = 0;
}

CUART::~CUART(void)
{
  if (!m_line.empty())
    FlushLine();
}
