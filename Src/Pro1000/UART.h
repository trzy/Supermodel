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
 */

#ifndef INCLUDED_UART_H
#define INCLUDED_UART_H

#include <cstdint>
#include <string>

/*
 * CUART:
 *
 * Minimal 16550-compatible UART. Transmitted characters are collected into
 * lines and written to the log at info level. There is no receive source, so
 * the receive buffer is always empty. Interrupts are not generated.
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

  ~CUART(void);

private:
  void TransmitByte(uint8_t data);
  void FlushLine(void);

  uint8_t m_ier = 0;  // interrupt enable
  uint8_t m_fcr = 0;  // FIFO control
  uint8_t m_lcr = 0;  // line control
  uint8_t m_mcr = 0;  // modem control
  uint8_t m_scr = 0;  // scratch
  uint8_t m_dll = 0;  // divisor latch (low)
  uint8_t m_dlm = 0;  // divisor latch (high)

  std::string m_line;
};

#endif  // INCLUDED_UART_H
