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
 * SCANPSC100F.cpp
 *
 * Implementation of the CSCANPSC100F class: National Semiconductor
 * SCANPSC100F Embedded Boundary Scan Controller. Based on the September 1998
 * datasheet (DS100325).
 *
 * Registers (A2:0)
 * ----------------
 *            Write                     Read
 *    0       TDO shifter/buffer        Counter register 1 (CNT32 bits 15-8)
 *    1       TDI (SSC seed only)       TDI shifter/buffer
 *    2       TMS0 shifter/buffer       Counter register 2 (CNT32 bits 23-16)
 *    3       TMS1 shifter/buffer       Counter register 3 (CNT32 bits 31-24)
 *    4       CNT32 (4 writes, LSB 1st) Counter register 0 (CNT32 bits 7-0)
 *    5       MODE0                     MODE0
 *    6       MODE1                     MODE1
 *    7       MODE2                     MODE2 (status)
 *
 * All shifter/buffers shift least significant bit first. Incoming TDI bits
 * enter at the most significant bit and shift right, so a partial byte read
 * back from TDI has its valid bits in the upper bit positions.
 */

#include "SCANPSC100F.h"

#include "Supermodel.h"

// MODE0
static constexpr uint8_t MODE0_TDO_ENABLE     = 0x80;
static constexpr uint8_t MODE0_TDI_ENABLE     = 0x40;
static constexpr uint8_t MODE0_CNT32_ENABLE   = 0x20;
static constexpr uint8_t MODE0_TMS0_ENABLE    = 0x10;
static constexpr uint8_t MODE0_TMS1_ENABLE    = 0x08;
static constexpr uint8_t MODE0_AUTO_TMS_HIGH  = 0x02;

// MODE1
static constexpr uint8_t MODE1_TDO_INT        = 0x80;
static constexpr uint8_t MODE1_TDI_INT        = 0x40;
static constexpr uint8_t MODE1_CNT32_INT      = 0x20;

// MODE2 (write)
static constexpr uint8_t MODE2_CONTINUOUS     = 0x08;
static constexpr uint8_t MODE2_UPDATE_STATUS  = 0x04;
static constexpr uint8_t MODE2_RESET          = 0x02;
static constexpr uint8_t MODE2_SINGLE_STEP    = 0x01;

// MODE2 (read) status bits
static constexpr uint8_t STATUS_TDO_NOT_FULL  = 0x80;
static constexpr uint8_t STATUS_TDI_NOT_EMPTY = 0x40;
static constexpr uint8_t STATUS_CNT32_DONE    = 0x20; // not loaded or at terminal count
static constexpr uint8_t STATUS_TMS0_NOT_FULL = 0x10;
static constexpr uint8_t STATUS_TMS1_NOT_FULL = 0x08;

// Guards against runaway shifting if firmware leaves only free-running
// shifters enabled
static constexpr unsigned MAX_TCKS_PER_ACCESS = 0x100000;


void CSCANPSC100F::ShifterBuffer::Push(uint8_t byte)
{
  if (count < 2)
    data[count++] = byte;
}

uint8_t CSCANPSC100F::ShifterBuffer::Pop()
{
  uint8_t byte = data[0];
  data[0] = data[1];
  if (count > 0)
    count--;
  return byte;
}

uint8_t CSCANPSC100F::Read(unsigned reg)
{
  uint8_t data = 0xFF;

  switch (reg & 7)
  {
  case 0:
    data = uint8_t(m_counter >> 8);
    break;
  case 1:
    if (m_tdi.Empty())
    {
      // Real hardware would hold off the bus with RDY until data arrives
      DebugLog("SCANPSC100F: read from empty TDI shifter/buffer\n");
      data = m_lastTDIRead;
    }
    else
    {
      data = m_tdi.Pop();
      m_lastTDIRead = data;
      RunTCK();         // space is now available for more incoming bits
    }
    break;
  case 2:
    data = uint8_t(m_counter >> 16);
    break;
  case 3:
    data = uint8_t(m_counter >> 24);
    break;
  case 4:
    data = uint8_t(m_counter >> 0);
    break;
  case 5:
    data = m_mode0;
    break;
  case 6:
    data = m_mode1;
    break;
  case 7:
    if (m_continuousUpdate)
      m_statusLatch = LiveStatus();
    data = (m_statusLatch & 0xF8) | (m_continuousUpdate ? 0x04 : 0x00);  // reset and single step complete instantly
    break;
  }

  UpdateInterrupt();
  return data;
}

void CSCANPSC100F::Write(unsigned reg, uint8_t data)
{
  reg &= 7;

  // CNT32 must be loaded with four consecutive writes; writing any other
  // register re-initializes the load logic
  if (reg != 4)
    m_counterWriteIdx = 0;

  switch (reg)
  {
  case 0:
    m_tdo.Push(data);   // ignored if full
    break;
  case 1:
    break;              // only meaningful in SSC mode (not emulated)
  case 2:
    m_tms0.Push(data);
    break;
  case 3:
    m_tms1.Push(data);
    break;
  case 4:
    WriteCounter(data);
    break;
  case 5:
    m_mode0 = data;
    break;
  case 6:
    m_mode1 = data;
    break;
  case 7:
    m_continuousUpdate = (data & MODE2_CONTINUOUS) != 0;
    if (data & MODE2_RESET)
      SynchronousReset();
    if (data & MODE2_SINGLE_STEP)
    {
      // Count for one SCK cycle without generating TCK (no wrap from 0)
      if (m_counter != 0)
        m_counter--;
    }
    if (data & MODE2_UPDATE_STATUS)
      m_statusLatch = LiveStatus();
    break;
  }

  RunTCK();
  if (m_continuousUpdate)
    m_statusLatch = LiveStatus();
  UpdateInterrupt();
}

void CSCANPSC100F::WriteCounter(uint8_t data)
{
  if (m_counterWriteIdx == 0)
    m_counterLoadValue = 0;
  m_counterLoadValue |= uint32_t(data) << (8 * m_counterWriteIdx);
  if (++m_counterWriteIdx == 4)
  {
    m_counter = m_counterLoadValue;
    m_counterLoaded = m_counter != 0;
    m_counterWriteIdx = 0;
  }
}

bool CSCANPSC100F::TCKEnabled(void) const
{
  bool anyEnabled = (m_mode0 & (MODE0_TDO_ENABLE | MODE0_TDI_ENABLE | MODE0_CNT32_ENABLE | MODE0_TMS0_ENABLE | MODE0_TMS1_ENABLE)) != 0;
  if (!anyEnabled)
    return false; // TCK would free-run on real hardware; we do not clock the chain needlessly

  if ((m_mode0 & MODE0_TDO_ENABLE) && m_tdo.Empty())
    return false;
  if ((m_mode0 & MODE0_TDI_ENABLE) && m_tdi.Full())
    return false;
  if ((m_mode0 & MODE0_TMS0_ENABLE) && m_tms0.Empty())
    return false;
  if ((m_mode0 & MODE0_TMS1_ENABLE) && m_tms1.Empty())
    return false;
  if ((m_mode0 & MODE0_CNT32_ENABLE) && !m_counterLoaded)
    return false;
  return true;
}

void CSCANPSC100F::RunTCK(void)
{
  unsigned numTCKs = 0;
  while (TCKEnabled())
  {
    if (++numTCKs > MAX_TCKS_PER_ACCESS)
    {
      ErrorLog("SCANPSC100F: TCK appears to be running indefinitely (MODE0=%02X).", m_mode0);
      break;
    }

    bool cnt32Enabled = (m_mode0 & MODE0_CNT32_ENABLE) != 0;

    // Outputs for this TCK
    if (m_mode0 & MODE0_TMS0_ENABLE)
      m_tms0Pin = (m_tms0.data[0] >> m_cnt3) & 1;
    if (m_mode0 & MODE0_TDO_ENABLE)
      m_tdoPin = (m_tdo.data[0] >> m_cnt3) & 1;
    bool tms = m_tms0Pin;
    if ((m_mode0 & MODE0_AUTO_TMS_HIGH) && cnt32Enabled && m_counter == 1)
      tms = true;

    // Clock the chain
    bool tdi = m_chain ? m_chain->Clock(tms, m_tdoPin) : true;
    if (m_mode0 & MODE0_TDI_ENABLE)
      m_tdiShift = uint8_t((m_tdiShift >> 1) | (tdi ? 0x80 : 0x00));

    // Count
    bool terminalCount = false;
    if (cnt32Enabled)
    {
      m_counter--;
      if (m_counter == 0)
      {
        terminalCount = true;
        m_counterLoaded = false;
      }
    }

    if (++m_cnt3 == 8 || terminalCount)
      EndOfByte(terminalCount);
  }
}

void CSCANPSC100F::EndOfByte(bool terminalCount)
{
  // Enabled shifter/buffers switch to their other register. At terminal count,
  // remaining bits in outgoing bytes are discarded and a partial incoming byte
  // is made available.
  if (m_mode0 & MODE0_TDO_ENABLE)
    m_tdo.Pop();
  if (m_mode0 & MODE0_TMS0_ENABLE)
    m_tms0.Pop();
  if (m_mode0 & MODE0_TMS1_ENABLE)
    m_tms1.Pop();
  if (m_mode0 & MODE0_TDI_ENABLE)
  {
    m_tdi.Push(m_tdiShift);
    m_tdiShift = 0;
  }
  m_cnt3 = 0;
}

uint8_t CSCANPSC100F::LiveStatus(void) const
{
  uint8_t status = 0;
  if (!m_tdo.Full())
    status |= STATUS_TDO_NOT_FULL;
  if (!m_tdi.Empty())
    status |= STATUS_TDI_NOT_EMPTY;
  if (!m_counterLoaded)
    status |= STATUS_CNT32_DONE;
  if (!m_tms0.Full())
    status |= STATUS_TMS0_NOT_FULL;
  if (!m_tms1.Full())
    status |= STATUS_TMS1_NOT_FULL;
  return status;
}

void CSCANPSC100F::UpdateInterrupt(void)
{
  uint8_t status = LiveStatus();
  bool level = ((m_mode1 & MODE1_TDO_INT) && (status & STATUS_TDO_NOT_FULL)) ||
               ((m_mode1 & MODE1_TDI_INT) && (status & STATUS_TDI_NOT_EMPTY)) ||
               ((m_mode1 & MODE1_CNT32_INT) && (status & STATUS_CNT32_DONE));
  if (level != m_intPin)
  {
    m_intPin = level;
    if (m_intCallback)
      m_intCallback(level);
  }
}

void CSCANPSC100F::SynchronousReset(void)
{
  // Resets everything except the parallel interface and CNT32 value
  m_mode0 = 0x20;
  m_mode1 = 0x00;
  m_continuousUpdate = false;
  m_cnt3 = 0;
  m_tdo.Clear();
  m_tms0.Clear();
  m_tms1.Clear();
  m_tdi.Clear();
  m_tdiShift = 0;
  m_tdoPin = true;
  m_tms0Pin = true;
}

void CSCANPSC100F::Reset(void)
{
  SynchronousReset();
  m_counter = 0;
  m_counterLoaded = false;
  m_counterLoadValue = 0;
  m_counterWriteIdx = 0;
  m_statusLatch = 0xB8;
  m_lastTDIRead = 0xFF;
  UpdateInterrupt();
}

void CSCANPSC100F::AttachScanChain(IJTAGScanChain *chain)
{
  m_chain = chain;
}

void CSCANPSC100F::AttachInterruptOutput(std::function<void(bool)> callback)
{
  m_intCallback = callback;
}

CSCANPSC100F::CSCANPSC100F(void)
  : m_intPin(false),
    m_chain(nullptr)
{
  Reset();
}
