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
 * UART.h
 *
 * Header file defining the CUART class: a 16550-style serial port.
 *
 * Register names (RBR, THR, IER, IIR, FCR, LCR, MCR, LSR, MSR, SCR, DLL, DLM)
 * follow the 16550 datasheet.
 */

#ifndef INCLUDED_UART_H
#define INCLUDED_UART_H

#include <cstdint>
#include <deque>
#include <functional>
#include <string>

class IConsoleInput;

/*
 * CUART:
 *
 * Minimal 16550-compatible UART. Transmitted characters are collected into
 * lines and written to the console and to the log at info level. Received
 * characters come from console input: each line typed is received followed
 * by a carriage return, through a 16-byte receive FIFO. Only the received
 * data available interrupt is generated. MCR OUT1 masks the interrupt output
 * (the Pro-1000 firmware sets it while servicing an interrupt).
 */
class CUART
{
public:
  /*
   * Read(reg):
   *
   * Reads a register.
   *
   * Parameters:
   *    reg   Register number (0-7).
   *
   * Returns:
   *    Register value.
   */
  uint8_t Read(unsigned reg);

  /*
   * Write(reg, data):
   *
   * Writes a register.
   *
   * Parameters:
   *    reg   Register number (0-7).
   *    data  Data to write.
   */
  void Write(unsigned reg, uint8_t data);

  /*
   * Reset(void):
   *
   * Resets the device. Any partially transmitted line is flushed to the log.
   */
  void Reset(void);

  /*
   * AttachInput(input):
   *
   * Attaches console input as the source of received characters. The
   * console is claimed for the lifetime of the UART.
   *
   * Parameters:
   *    input   Console input endpoint (may be null).
   */
  void AttachInput(IConsoleInput *input);

  /*
   * AttachInterruptOutput(callback):
   *
   * Sets the function called when the level of the INT pin changes.
   */
  void AttachInterruptOutput(std::function<void(bool)> callback);

  /*
   * Tick(void):
   *
   * Accepts any newly available console input and updates the interrupt
   * output. Should be called periodically.
   */
  void Tick(void);

  ~CUART(void);

private:
  void TransmitByte(uint8_t data);
  void FlushLine(void);
  void FillReceiveFIFO(void);
  bool ReceiveInterruptPending(void) const;
  void UpdateInterrupt(void);

  uint8_t m_ier = 0;  // interrupt enable
  uint8_t m_fcr = 0;  // FIFO control
  uint8_t m_lcr = 0;  // line control
  uint8_t m_mcr = 0;  // modem control
  uint8_t m_scr = 0;  // scratch
  uint8_t m_dll = 0;  // divisor latch (low)
  uint8_t m_dlm = 0;  // divisor latch (high)

  std::string m_line;

  IConsoleInput       *m_input = nullptr;
  std::deque<uint8_t> m_rxFIFO;     // receive FIFO
  std::string         m_rxPending;  // remainder of received line not yet in FIFO

  bool m_intPin = false;
  std::function<void(bool)> m_intCallback;
};

#endif  // INCLUDED_UART_H
