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
 * 8259.h
 *
 * Header file defining the C8259 class: Intel 8259A programmable interrupt
 * controller.
 *
 * Naming conventions follow the Intel datasheet: IR0-IR7 are the request
 * inputs, IRR/ISR/IMR are the interrupt request, in-service, and mask
 * registers, ICWn are initialization command words, and OCWn are operation
 * command words. Port 0 and port 1 refer to the A0 address line.
 */

#ifndef INCLUDED_8259_H
#define INCLUDED_8259_H

#include "CPU/IRQController.h"
#include <cstdint>

/*
 * C8259:
 *
 * Single (non-cascaded) 8259A. The interrupt acknowledge (INTA) cycle is
 * performed explicitly by calling Acknowledge() because the PowerPC has no
 * acknowledge cycle. The INT output drives the PowerPC external interrupt
 * line.
 *
 * As an IIRQController, bit n of the line mask is IRn.
 *
 * Raising the CPU interrupt line is deferred until Tick() so that the CPU
 * gets to execute some instructions after a device signals an interrupt,
 * as it would on real hardware where devices take time to complete their
 * operations. Lowering it takes effect immediately.
 */
class C8259: public IIRQController
{
public:
  // IIRQController interface
  void Assert(uint16_t lines) override;
  void Deassert(uint16_t lines) override;

  uint8_t Read(unsigned port);
  void Write(unsigned port, uint8_t data);

  /*
   * Acknowledge(void):
   *
   * Performs one INTA pulse. In 8086 mode, two pulses make up an interrupt
   * acknowledge sequence. The first moves the highest priority pending
   * request from IRR to ISR.
   */
  void Acknowledge(void);

  /*
   * Tick(void):
   *
   * Asserts the CPU interrupt line if an interrupt is pending. Should be
   * called periodically between CPU time slices.
   */
  void Tick(void);

  void Reset(void);

  C8259(void);

private:
  enum class InitState
  {
    Ready,
    ExpectICW2,
    ExpectICW3,
    ExpectICW4
  };

  void  SetInput(unsigned line, bool level);
  int   HighestPriority(uint8_t bits) const;
  bool  Priority(int a, int b) const;   // true if line a has higher priority than b
  bool  InterruptPending(void) const;
  void  UpdateOutput(bool allowRaise);

  uint8_t   m_irr;
  uint8_t   m_isr;
  uint8_t   m_imr;
  uint8_t   m_inputs;         // current level of IR pins
  uint8_t   m_icw1;
  uint8_t   m_icw4;
  uint8_t   m_vectorBase;
  unsigned  m_lowestPriority; // line with lowest priority (7 after init)
  bool      m_readISR;        // OCW3 read register select
  bool      m_poll;           // OCW3 poll command pending
  bool      m_secondINTA;     // next INTA pulse is the second of a pair
  InitState m_initState;
  bool      m_cpuLine;
};

#endif  // INCLUDED_8259_H
