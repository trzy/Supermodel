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
 * 8259.cpp
 *
 * Implementation of the C8259 class: Intel 8259A programmable interrupt
 * controller.
 *
 * Port 0 (A0=0)
 * -------------
 *    Write: ICW1 (bit 4 set), OCW3 (bits 4,3 = 01), or OCW2 (bits 4,3 = 00)
 *    Read:  IRR or ISR (selected by OCW3), or poll word after a poll command
 *
 * Port 1 (A0=1)
 * -------------
 *    Write: ICW2-ICW4 during initialization, otherwise OCW1 (IMR)
 *    Read:  IMR
 *
 * Not emulated: cascading, special mask mode, special fully nested mode,
 * and automatic rotation in AEOI mode.
 */

#include "8259.h"

#include "Supermodel.h"
#include "CPU/PowerPC/ppc.h"

static constexpr uint8_t ICW1_IC4   = 0x01; // ICW4 needed
static constexpr uint8_t ICW1_SNGL  = 0x02; // single (no ICW3)
static constexpr uint8_t ICW1_LTIM  = 0x08; // level triggered
static constexpr uint8_t ICW4_AEOI  = 0x02; // automatic end of interrupt


int C8259::HighestPriority(uint8_t bits) const
{
  // Highest priority line is the one following the lowest priority line
  for (unsigned i = 1; i <= 8; i++)
  {
    unsigned line = (m_lowestPriority + i) & 7;
    if (bits & (1 << line))
      return int(line);
  }
  return -1;
}

bool C8259::Priority(int a, int b) const
{
  if (b < 0)
    return a >= 0;
  if (a < 0)
    return false;
  unsigned rankA = (unsigned(a) - m_lowestPriority - 1) & 7;  // 0 = highest
  unsigned rankB = (unsigned(b) - m_lowestPriority - 1) & 7;
  return rankA < rankB;
}

bool C8259::InterruptPending(void) const
{
  int request = HighestPriority(m_irr & ~m_imr);
  int inService = HighestPriority(m_isr);
  return Priority(request, inService);
}

void C8259::UpdateOutput(bool allowRaise)
{
  bool pending = InterruptPending();
  if (pending && !m_cpuLine && allowRaise)
  {
    m_cpuLine = true;
    ppc_set_irq_line(1);
  }
  else if (!pending && m_cpuLine)
  {
    m_cpuLine = false;
    ppc_set_irq_line(0);
  }
}

void C8259::Tick(void)
{
  UpdateOutput(true);
}

void C8259::Assert(uint16_t lines)
{
  for (unsigned line = 0; line < 8; line++)
  {
    if (lines & (1 << line))
      SetInput(line, true);
  }
}

void C8259::Deassert(uint16_t lines)
{
  for (unsigned line = 0; line < 8; line++)
  {
    if (lines & (1 << line))
      SetInput(line, false);
  }
}

void C8259::SetInput(unsigned line, bool level)
{
  uint8_t bit = uint8_t(1 << (line & 7));
  bool wasHigh = (m_inputs & bit) != 0;
  if (level)
    m_inputs |= bit;
  else
    m_inputs &= ~bit;

  if (m_icw1 & ICW1_LTIM)
  {
    if (level)
      m_irr |= bit;
    else
      m_irr &= ~bit;
  }
  else if (level && !wasHigh)
    m_irr |= bit;   // rising edge

  UpdateOutput(false);
}

void C8259::Acknowledge(void)
{
  if (!m_secondINTA)
  {
    int line = HighestPriority(m_irr & ~m_imr);
    if (line >= 0 && Priority(line, HighestPriority(m_isr)))
    {
      uint8_t bit = uint8_t(1 << line);
      m_isr |= bit;
      m_irr &= ~bit;
      if ((m_icw1 & ICW1_LTIM) && (m_inputs & bit))
        m_irr |= bit;   // level remains asserted
    }
    // else: spurious interrupt (would be reported as IR7 without setting ISR)
    m_secondINTA = true;
  }
  else
  {
    if (m_icw4 & ICW4_AEOI)
    {
      int line = HighestPriority(m_isr);
      if (line >= 0)
        m_isr &= ~(1 << line);
    }
    m_secondINTA = false;
  }
  UpdateOutput(false);
}

uint8_t C8259::Read(unsigned port)
{
  uint8_t data;
  if ((port & 1) == 0)
  {
    if (m_poll)
    {
      // Poll command: acknowledge highest priority request and report it
      m_poll = false;
      int line = HighestPriority(m_irr & ~m_imr);
      if (line >= 0 && Priority(line, HighestPriority(m_isr)))
      {
        m_isr |= uint8_t(1 << line);
        m_irr &= ~uint8_t(1 << line);
        data = uint8_t(0x80 | line);
      }
      else
        data = 0x00;
    }
    else
      data = m_readISR ? m_isr : m_irr;
  }
  else
    data = m_imr;
  UpdateOutput(false);
  return data;
}

void C8259::Write(unsigned port, uint8_t data)
{
  if ((port & 1) == 0)
  {
    if (data & 0x10)
    {
      // ICW1: begin initialization
      m_icw1 = data;
      m_icw4 = 0;
      m_imr = 0;
      m_isr = 0;
      m_irr = 0;
      m_lowestPriority = 7;
      m_readISR = false;
      m_poll = false;
      m_secondINTA = false;
      m_initState = InitState::ExpectICW2;
    }
    else if (data & 0x08)
    {
      // OCW3
      if (data & 0x04)
        m_poll = true;
      if ((data & 0x02))
        m_readISR = (data & 0x01) != 0;
    }
    else
    {
      // OCW2
      unsigned level = data & 7;
      switch ((data >> 5) & 7)
      {
      case 1:   // non-specific EOI
      case 5:   // rotate on non-specific EOI
      {
        int line = HighestPriority(m_isr);
        if (line >= 0)
        {
          m_isr &= ~(1 << line);
          if (((data >> 5) & 7) == 5)
            m_lowestPriority = unsigned(line);
        }
        break;
      }
      case 3:   // specific EOI
        m_isr &= ~(1 << level);
        break;
      case 7:   // rotate on specific EOI
        m_isr &= ~(1 << level);
        m_lowestPriority = level;
        break;
      case 6:   // set priority
        m_lowestPriority = level;
        break;
      default:  // no operation, rotate in AEOI mode set/clear (not emulated)
        break;
      }
    }
  }
  else
  {
    switch (m_initState)
    {
    case InitState::ExpectICW2:
      m_vectorBase = data & 0xF8;
      if (!(m_icw1 & ICW1_SNGL))
        m_initState = InitState::ExpectICW3;
      else if (m_icw1 & ICW1_IC4)
        m_initState = InitState::ExpectICW4;
      else
        m_initState = InitState::Ready;
      break;
    case InitState::ExpectICW3:
      m_initState = (m_icw1 & ICW1_IC4) ? InitState::ExpectICW4 : InitState::Ready;
      break;
    case InitState::ExpectICW4:
      m_icw4 = data;
      m_initState = InitState::Ready;
      break;
    case InitState::Ready:
      m_imr = data;   // OCW1
      break;
    }
  }
  UpdateOutput(false);
}

void C8259::Reset(void)
{
  m_irr = 0;
  m_isr = 0;
  m_imr = 0xFF;
  m_inputs = 0;
  m_icw1 = 0;
  m_icw4 = 0;
  m_vectorBase = 0;
  m_lowestPriority = 7;
  m_readISR = false;
  m_poll = false;
  m_secondINTA = false;
  m_initState = InitState::Ready;
  if (m_cpuLine)
    ppc_set_irq_line(0);
  m_cpuLine = false;
}

C8259::C8259(void)
  : m_cpuLine(false)
{
  Reset();
}

