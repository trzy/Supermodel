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
 * SCSIBus.h
 *
 * Interface between a SCSI controller in target mode and the initiator on
 * the same SCSI bus.
 *
 * On a SCSI bus, the initiator starts each transaction. It selects a target
 * and gives a command to the target. The target then controls the bus. The
 * target sets the bus phase and requests or sends the bytes of each phase.
 *
 * This interface does not model the bus signals. A target calls it once for
 * each block of bytes that it transfers. Thus the target's SCRIPTS processor
 * calls it once for each block move instruction.
 *
 * Bus phases have the names that the SCSI standard uses. These names give
 * the direction from the point of view of the initiator. For example, in the
 * data out phase, the initiator sends data to the target.
 */

#ifndef INCLUDED_SCSIBUS_H
#define INCLUDED_SCSIBUS_H

#include <cstdint>

/*
 * SCSIPhase:
 *
 * Information transfer phases. The values are the MSG, C/D, and I/O signals
 * (bits 2, 1, and 0). When the I/O bit is set, bytes go from the target to
 * the initiator.
 */
enum class SCSIPhase : uint8_t
{
  DataOut = 0,
  DataIn = 1,
  Command = 2,
  Status = 3,
  Reserved4 = 4,
  Reserved5 = 5,
  MessageOut = 6,
  MessageIn = 7
};

/*
 * IsInitiatorToTarget(phase):
 *
 * Returns true if bytes go from the initiator to the target in this phase.
 */
inline bool IsInitiatorToTarget(SCSIPhase phase)
{
  return (static_cast<uint8_t>(phase) & 1) == 0;
}

/*
 * ISCSIInitiator:
 *
 * The initiator on a SCSI bus, as a target sees it. The target calls all
 * functions. The target calls them on the emulator thread only.
 */
class ISCSIInitiator
{
public:
  /*
   * Selecting(respondIDs, targetID, initiatorID):
   *
   * Asks if the initiator selects a target. The target calls this while it
   * waits for selection.
   *
   * Parameters:
   *    respondIDs    Bit mask of the SCSI IDs to which the target responds.
   *    targetID      Receives the ID that the initiator selects.
   *    initiatorID   Receives the ID of the initiator.
   *
   * Returns:
   *    True if the initiator selects one of the IDs in respondIDs. The
   *    target is then connected to the initiator.
   */
  virtual bool Selecting(uint16_t respondIDs, unsigned *targetID, unsigned *initiatorID) = 0;

  /*
   * ATN(void):
   *
   * Returns:
   *    True if the initiator asserts ATN. The initiator asserts ATN when it
   *    has a message for the target.
   */
  virtual bool ATN(void) const = 0;

  /*
   * SendToTarget(phase, dest, numBytes):
   *
   * Gets bytes from the initiator in a phase that goes from the initiator to
   * the target (data out, command, or message out). The initiator always
   * supplies all of the bytes. If it has no more bytes, it supplies zeros
   * and records an error.
   *
   * Parameters:
   *    phase     Bus phase.
   *    dest      Receives the bytes.
   *    numBytes  Number of bytes.
   */
  virtual void SendToTarget(SCSIPhase phase, uint8_t *dest, unsigned numBytes) = 0;

  /*
   * ReceiveFromTarget(phase, src, numBytes):
   *
   * Gives bytes to the initiator in a phase that goes from the target to the
   * initiator (data in, status, or message in).
   *
   * Parameters:
   *    phase     Bus phase.
   *    src       The bytes.
   *    numBytes  Number of bytes.
   */
  virtual void ReceiveFromTarget(SCSIPhase phase, const uint8_t *src, unsigned numBytes) = 0;

  /*
   * Disconnected(void):
   *
   * Tells the initiator that the target released the bus.
   */
  virtual void Disconnected(void) = 0;

  /*
   * Reselect(targetID, initiatorID):
   *
   * The target tries to reselect an initiator. A target does this to
   * continue a transaction after it disconnected.
   *
   * Parameters:
   *    targetID      ID of the target.
   *    initiatorID   ID of the initiator to reselect.
   *
   * Returns:
   *    True if the initiator responds. The target is then connected to the
   *    initiator.
   */
  virtual bool Reselect(unsigned targetID, unsigned initiatorID) = 0;

  /*
   * BusReset(void):
   *
   * Tells the initiator that a device asserted RST on the bus. All
   * connections and transactions stop.
   */
  virtual void BusReset(void) = 0;

  virtual ~ISCSIInitiator(void)
  {
  }
};

#endif  // INCLUDED_SCSIBUS_H
