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
 * SCSIHostAdapter.h
 *
 * Header file for the CSCSIHostAdapter class: a virtual SCSI host adapter.
 *
 * Purpose
 * -------
 * The Pro-1000 is a SCSI target. On real hardware, a host computer sends
 * commands to it through a SCSI host adapter (a card in the computer). This
 * class does the work of that host adapter. It is the initiator on the
 * emulated SCSI bus. The emulated SCSI controller (C53C810) is the target.
 *
 * The requests come from programs on the host computer. On real hardware,
 * these programs use ASPI (wnaspi32.dll) to send SCSI commands. In
 * Supermodel, a replacement wnaspi32.dll sends each request to a bridge in
 * Supermodel. The bridge gives the request to the host adapter with
 * Submit(). Tests can also use Submit().
 *
 * A request has the same contents as an ASPI "execute SCSI command"
 * request: a target ID, a command descriptor block (CDB), data to send to
 * the target, and the maximum quantity of data to receive. When the target
 * completes the request, the host adapter calls a completion function with
 * the result. The bridge then sends the result back to the program.
 *
 * The host adapter does one request at a time, in the sequence received.
 * It does not model the bus signals or timing. The target calls it through
 * the ISCSIInitiator interface each time that the target transfers a block
 * of bytes.
 *
 * Threads
 * -------
 * Submit() is thread safe. Other threads can use it to queue requests. All
 * other functions operate on the emulator thread. The completion function
 * also operates on the emulator thread. It must not block.
 *
 * State Machine
 * -------------
 *
 *   Idle --(target accepts selection)--> Connected
 *   Connected --(target disconnects after COMMAND COMPLETE)--> Idle
 *   Connected --(target disconnects after DISCONNECT message)--> Disconnected
 *   Connected --(target disconnects with no message)--> Idle (request fails)
 *   Disconnected --(target reselects host adapter)--> Connected
 *   Connected or Disconnected --(bus reset)--> Idle (request fails)
 *
 * Idle:
 *   There is no current request. Requests can be in the queue. When the
 *   target waits for selection, the host adapter selects it for the first
 *   request in the queue. The host adapter selects only if the target
 *   responds to the target ID of the request.
 *
 * Connected:
 *   The target controls the bus. The host adapter supplies and keeps bytes
 *   for each phase that the target requests:
 *     message out  The IDENTIFY message, then NO OPERATION messages.
 *     command      The CDB.
 *     data out     The data of the request.
 *     data in      Kept as the result data.
 *     status       Kept as the status byte.
 *     message in   Read as messages (see below).
 *   The host adapter asserts ATN until the target receives the IDENTIFY
 *   message.
 *
 * Disconnected:
 *   The target released the bus but did not complete the request. The
 *   request stays open. The host adapter does not select a target in this
 *   state. The target must reselect the host adapter to continue.
 *
 * Messages from the target:
 *   COMMAND COMPLETE (0x00)   The request is complete when the target
 *                             disconnects.
 *   SAVE DATA POINTER (0x02)  Saves the data positions.
 *   RESTORE POINTERS (0x03)   Restores the data positions.
 *   DISCONNECT (0x04)         The target disconnects but continues later.
 *   MESSAGE REJECT (0x07)     The target did not accept a message.
 *   NO OPERATION (0x08)       No action.
 *   Extended (0x01)           Read and ignored.
 *   IDENTIFY (0x80-0xFF)      Sent by the target after reselection.
 * After reselection, the host adapter restores the saved data positions.
 *
 * Errors
 * ------
 * The host adapter does not stop a transfer when an error occurs. It
 * supplies zeros for bytes that it does not have and it discards bytes that
 * it cannot keep. It records the first error in the result.
 */

#ifndef INCLUDED_SCSIHOSTADAPTER_H
#define INCLUDED_SCSIHOSTADAPTER_H

#include "Model3/SCSIBus.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

/*
 * SCSIRequest:
 *
 * A command for a SCSI target.
 */
struct SCSIRequest
{
  unsigned targetID = 0;
  std::vector<uint8_t> cdb;       // command descriptor block
  std::vector<uint8_t> dataOut;   // data to send to the target (data out phase)
  size_t dataInLength = 0;        // maximum number of bytes to receive (data in phase)
};

/*
 * SCSIResult:
 *
 * The result of a request.
 */
struct SCSIResult
{
  bool completed = false;         // true if the target sent COMMAND COMPLETE
  uint8_t status = 0xFF;          // status byte (0x00 = GOOD, 0x02 = CHECK CONDITION)
  std::vector<uint8_t> dataIn;    // data received from the target
  std::string error;              // first protocol error, or empty
};

/*
 * CSCSIHostAdapter:
 *
 * Virtual SCSI host adapter. It is the initiator on the emulated SCSI bus.
 */
class CSCSIHostAdapter : public ISCSIInitiator
{
public:
  using CompletionFunction = std::function<void(const SCSIResult &result)>;

  /*
   * Submit(request, onComplete):
   *
   * Puts a request in the queue. This function is thread safe.
   *
   * Parameters:
   *    request     The request.
   *    onComplete  Function to call when the request completes or fails.
   *                The emulator thread calls it.
   */
  void Submit(const SCSIRequest &request, CompletionFunction onComplete);

  /*
   * Reset(void):
   *
   * Stops the current request and all requests in the queue. Each of these
   * requests fails.
   */
  void Reset(void);

  // ISCSIInitiator
  bool Selecting(uint16_t respondIDs, unsigned *targetID, unsigned *initiatorID) override;
  bool ATN(void) const override;
  void SendToTarget(SCSIPhase phase, uint8_t *dest, unsigned numBytes) override;
  void ReceiveFromTarget(SCSIPhase phase, const uint8_t *src, unsigned numBytes) override;
  void Disconnected(void) override;
  bool Reselect(unsigned targetID, unsigned initiatorID) override;
  void BusReset(void) override;

  /*
   * CSCSIHostAdapter(hostID):
   *
   * Parameters:
   *    hostID  SCSI ID of the host adapter (7 is usual for host adapters).
   */
  CSCSIHostAdapter(unsigned hostID = 7);

private:
  enum class State
  {
    Idle,
    Connected,
    Disconnected
  };

  struct PendingRequest
  {
    SCSIRequest request;
    CompletionFunction onComplete;
  };

  // Data positions that SAVE DATA POINTER and RESTORE POINTERS use
  struct DataPointers
  {
    size_t dataOut = 0;
    size_t dataIn = 0;
  };

  void StartRequest(PendingRequest &&pending);
  void Finish(const char *error);
  void RecordError(const std::string &error);
  void ProcessMessages(void);

  const unsigned m_hostID;

  // Queue of requests. The mutex is necessary because other threads add
  // requests.
  std::mutex m_queueMutex;
  std::deque<PendingRequest> m_queue;

  // Current request. Only the emulator thread uses these.
  State m_state = State::Idle;
  PendingRequest m_current;
  SCSIResult m_result;
  std::deque<uint8_t> m_messagesOut;  // messages to send to the target
  std::vector<uint8_t> m_messagesIn;  // message bytes received but not yet processed
  size_t m_cdbPos = 0;
  DataPointers m_pointers;            // current data positions
  DataPointers m_savedPointers;       // data positions saved by SAVE DATA POINTER
  bool m_commandComplete = false;     // COMMAND COMPLETE received
  bool m_disconnectMessage = false;   // DISCONNECT received
};

#endif  // INCLUDED_SCSIHOSTADAPTER_H
