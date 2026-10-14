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
 * IRQController.h
 *
 * Header file defining the IIRQController interface.
 */

#ifndef INCLUDED_IRQCONTROLLER_H
#define INCLUDED_IRQCONTROLLER_H

#include <cstdint>

/*
 * IIRQController:
 *
 * Interrupt controller as seen by devices that raise interrupts. Input lines
 * are identified by a bit mask whose meaning is specific to the controller
 * (e.g., bit n is IRn on an 8259). Devices are given their line mask by the
 * system that owns both the device and the controller and need not know
 * which controller they are attached to. A mask of 0 means the device is not
 * connected and asserting it has no effect.
 *
 * Assert() and Deassert() drive the level of the line(s). How the controller
 * responds (e.g., latching a request on a rising edge) is up to the
 * controller.
 */
class IIRQController
{
public:
  /*
   * Assert(lines):
   *
   * Drives line(s) active.
   *
   * Parameters:
   *    lines   Bit mask of lines to assert.
   */
  virtual void Assert(uint16_t lines) = 0;

  /*
   * Deassert(lines):
   *
   * Releases line(s).
   *
   * Parameters:
   *    lines   Bit mask of lines to deassert.
   */
  virtual void Deassert(uint16_t lines) = 0;

  virtual ~IIRQController()
  {
  }
};

#endif  // INCLUDED_IRQCONTROLLER_H
