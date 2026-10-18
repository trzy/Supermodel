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
 * SCSIHostAdapter.cpp
 *
 * Implementation of the CSCSIHostAdapter class. Refer to SCSIHostAdapter.h
 * for the design and the state machine.
 */

#include "SCSIHostAdapter.h"

#include "Supermodel.h"
#include <algorithm>
#include <cstdio>
#include <utility>

// Messages
static constexpr uint8_t MSG_COMMAND_COMPLETE = 0x00;
static constexpr uint8_t MSG_EXTENDED = 0x01;
static constexpr uint8_t MSG_SAVE_DATA_POINTER = 0x02;
static constexpr uint8_t MSG_RESTORE_POINTERS = 0x03;
static constexpr uint8_t MSG_DISCONNECT = 0x04;
static constexpr uint8_t MSG_MESSAGE_REJECT = 0x07;
static constexpr uint8_t MSG_NO_OPERATION = 0x08;
static constexpr uint8_t MSG_IDENTIFY = 0x80;
static constexpr uint8_t IDENTIFY_DISCONNECT_PRIVILEGE = 0x40;  // the target can disconnect

static const char *PhaseName(SCSIPhase phase)
{
  static const char *names[] = { "data out", "data in", "command", "status", "reserved (4)", "reserved (5)", "message out", "message in" };
  return names[static_cast<uint8_t>(phase) & 7];
}


/******************************************************************************
 Requests
******************************************************************************/

void CSCSIHostAdapter::Submit(const SCSIRequest &request, CompletionFunction onComplete)
{
  std::lock_guard<std::mutex> lock(m_queueMutex);
  m_queue.push_back(PendingRequest{ request, std::move(onComplete) });
}

void CSCSIHostAdapter::StartRequest(PendingRequest &&pending)
{
  m_current = std::move(pending);
  m_result = SCSIResult();
  m_messagesOut.clear();
  m_messagesIn.clear();
  m_cdbPos = 0;
  m_pointers = DataPointers();
  m_savedPointers = DataPointers();
  m_commandComplete = false;
  m_disconnectMessage = false;

  // Identify the logical unit and let the target disconnect, as the usual
  // host adapters do. ATN stays asserted until the target receives the
  // message.
  m_messagesOut.push_back(MSG_IDENTIFY | IDENTIFY_DISCONNECT_PRIVILEGE | (m_current.request.lun & 7));
  m_state = State::Connected;
}

// Completes the current request. If error is not null, the request fails.
void CSCSIHostAdapter::Finish(const char *error)
{
  if (error)
    RecordError(error);
  m_result.completed = m_commandComplete && !error;
  m_state = State::Idle;
  DebugLog("SCSI host adapter: request complete (completed=%d, status=%02X, %u bytes in, error=%s)\n", m_result.completed, m_result.status, unsigned(m_result.dataIn.size()), m_result.error.c_str());

  // Release the request before the call, because the completion function can
  // submit a new request
  CompletionFunction onComplete = std::move(m_current.onComplete);
  SCSIResult result = std::move(m_result);
  m_current = PendingRequest();
  m_result = SCSIResult();
  if (onComplete)
    onComplete(result);
}

void CSCSIHostAdapter::RecordError(const std::string &error)
{
  DebugLog("SCSI host adapter: %s\n", error.c_str());
  if (m_result.error.empty())
    m_result.error = error;
}

void CSCSIHostAdapter::Reset(void)
{
  if (m_state != State::Idle)
    Finish("host adapter reset");

  std::deque<PendingRequest> queue;
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    queue.swap(m_queue);
  }
  for (auto &pending: queue)
  {
    SCSIResult result;
    result.error = "host adapter reset";
    if (pending.onComplete)
      pending.onComplete(result);
  }
}


/******************************************************************************
 Bus Interface (ISCSIInitiator)
******************************************************************************/

bool CSCSIHostAdapter::Selecting(uint16_t respondIDs, unsigned *targetID, unsigned *initiatorID)
{
  if (m_state != State::Idle)
    return false;

  PendingRequest pending;
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_queue.empty())
      return false;

    // If the target does not respond to the ID, the request stays in the
    // queue. The firmware can enable selection response later.
    if (m_queue.front().request.targetID >= 16 || !(respondIDs & (1 << m_queue.front().request.targetID)))
      return false;

    pending = std::move(m_queue.front());
    m_queue.pop_front();
  }

  *targetID = pending.request.targetID;
  *initiatorID = m_hostID;
  DebugLog("SCSI host adapter: selected target %u (CDB opcode %02X)\n", *targetID, pending.request.cdb.empty() ? 0 : pending.request.cdb[0]);
  StartRequest(std::move(pending));
  return true;
}

bool CSCSIHostAdapter::ATN(void) const
{
  return m_state == State::Connected && !m_messagesOut.empty();
}

void CSCSIHostAdapter::SendToTarget(SCSIPhase phase, uint8_t *dest, unsigned numBytes)
{
  if (m_state != State::Connected)
  {
    RecordError(std::string("target requested ") + PhaseName(phase) + " bytes while not connected");
    std::fill(dest, dest + numBytes, 0);
    return;
  }

  switch (phase)
  {
  case SCSIPhase::MessageOut:
    for (unsigned i = 0; i < numBytes; i++)
    {
      if (m_messagesOut.empty())
        dest[i] = MSG_NO_OPERATION;  // the host adapter has no message
      else
      {
        dest[i] = m_messagesOut.front();
        m_messagesOut.pop_front();
      }
    }
    break;

  case SCSIPhase::Command:
  {
    // The target can request the CDB in more than one block
    const std::vector<uint8_t> &cdb = m_current.request.cdb;
    bool tooFew = false;
    for (unsigned i = 0; i < numBytes; i++)
    {
      if (m_cdbPos < cdb.size())
        dest[i] = cdb[m_cdbPos++];
      else
      {
        dest[i] = 0;
        tooFew = true;
      }
    }
    if (tooFew)
      RecordError("target requested more command bytes than the CDB has");
    break;
  }

  case SCSIPhase::DataOut:
  {
    const std::vector<uint8_t> &data = m_current.request.dataOut;
    size_t available = m_pointers.dataOut < data.size() ? data.size() - m_pointers.dataOut : 0;
    size_t copy = std::min<size_t>(available, numBytes);
    std::copy(data.begin() + m_pointers.dataOut, data.begin() + m_pointers.dataOut + copy, dest);
    std::fill(dest + copy, dest + numBytes, 0);
    m_pointers.dataOut += copy;
    if (copy < numBytes)
      RecordError("target requested more data out bytes than the request has");
    break;
  }

  default:
    RecordError(std::string("target requested bytes in the ") + PhaseName(phase) + " phase");
    std::fill(dest, dest + numBytes, 0);
    break;
  }
}

void CSCSIHostAdapter::ReceiveFromTarget(SCSIPhase phase, const uint8_t *src, unsigned numBytes)
{
  if (m_state != State::Connected)
  {
    RecordError(std::string("target sent ") + PhaseName(phase) + " bytes while not connected");
    return;
  }

  switch (phase)
  {
  case SCSIPhase::DataIn:
  {
    // Data goes to the current position, which RESTORE POINTERS can move back
    size_t limit = m_current.request.dataInLength;
    for (unsigned i = 0; i < numBytes; i++)
    {
      size_t pos = m_pointers.dataIn++;
      if (pos >= limit)
      {
        RecordError("target sent more data in bytes than the request accepts");
        m_pointers.dataIn = limit;
        break;
      }
      if (pos >= m_result.dataIn.size())
        m_result.dataIn.resize(pos + 1);
      m_result.dataIn[pos] = src[i];
    }
    break;
  }

  case SCSIPhase::Status:
    if (numBytes > 0)
      m_result.status = src[numBytes - 1];
    if (numBytes != 1)
      RecordError("target sent a status phase of " + std::to_string(numBytes) + " bytes");
    break;

  case SCSIPhase::MessageIn:
    m_messagesIn.insert(m_messagesIn.end(), src, src + numBytes);
    ProcessMessages();
    break;

  default:
    RecordError(std::string("target sent bytes in the ") + PhaseName(phase) + " phase");
    break;
  }
}

// Processes all complete messages received from the target
void CSCSIHostAdapter::ProcessMessages(void)
{
  size_t pos = 0;
  while (pos < m_messagesIn.size())
  {
    uint8_t message = m_messagesIn[pos];
    size_t length = 1;

    if (message == MSG_EXTENDED)
    {
      // Extended message: 0x01, length, code, arguments
      if (pos + 1 >= m_messagesIn.size())
        break;  // incomplete
      length = 2 + m_messagesIn[pos + 1];
      if (pos + length > m_messagesIn.size())
        break;  // incomplete
      DebugLog("SCSI host adapter: ignored extended message (code %02X)\n", length > 2 ? m_messagesIn[pos + 2] : 0);
    }
    else if (message & MSG_IDENTIFY)
      ; // the target identifies itself after reselection
    else
    {
      switch (message)
      {
      case MSG_COMMAND_COMPLETE:
        m_commandComplete = true;
        break;
      case MSG_SAVE_DATA_POINTER:
        m_savedPointers = m_pointers;
        break;
      case MSG_RESTORE_POINTERS:
        m_pointers = m_savedPointers;
        break;
      case MSG_DISCONNECT:
        m_disconnectMessage = true;
        break;
      case MSG_MESSAGE_REJECT:
        RecordError("target rejected a message");
        break;
      case MSG_NO_OPERATION:
        break;
      default:
        RecordError("target sent unknown message " + std::to_string(message));
        break;
      }
    }
    pos += length;
  }
  m_messagesIn.erase(m_messagesIn.begin(), m_messagesIn.begin() + pos);
}

void CSCSIHostAdapter::Disconnected(void)
{
  if (m_state != State::Connected)
  {
    RecordError("target disconnected while not connected");
    return;
  }

  if (m_commandComplete)
    Finish(nullptr);
  else if (m_disconnectMessage)
  {
    m_state = State::Disconnected;
    DebugLog("SCSI host adapter: target disconnected, request stays open\n");
  }
  else
    Finish("target disconnected without COMMAND COMPLETE or DISCONNECT");
}

bool CSCSIHostAdapter::Reselect(unsigned targetID, unsigned initiatorID)
{
  if (m_state != State::Disconnected || initiatorID != m_hostID || targetID != m_current.request.targetID)
    return false;

  // Continue the request from the saved data positions
  m_state = State::Connected;
  m_pointers = m_savedPointers;
  m_disconnectMessage = false;
  m_messagesIn.clear();
  DebugLog("SCSI host adapter: reselected by target %u\n", targetID);
  return true;
}

void CSCSIHostAdapter::BusReset(void)
{
  if (m_state != State::Idle)
    Finish("SCSI bus reset");
}


/******************************************************************************
 Construction
******************************************************************************/

CSCSIHostAdapter::CSCSIHostAdapter(unsigned hostID)
  : m_hostID(hostID)
{
}
