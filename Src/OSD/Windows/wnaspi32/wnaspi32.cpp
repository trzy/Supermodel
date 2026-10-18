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
 * wnaspi32.cpp
 *
 * Replacement ASPI for Win32 DLL (wnaspi32.dll). It connects programs for the
 * Real3D Pro-1000 to a Pro-1000 that Supermodel emulates.
 *
 * Purpose
 * -------
 * Pro-1000 programs (for example, the SDK demos) send SCSI commands through
 * ASPI. Put this DLL in the folder of the program. The DLL sends each SCSI
 * command to Supermodel through the SCSI TCP bridge (see
 * Src/Pro1000/SCSITCPBridgeProtocol.h). The emulated Pro-1000 firmware does
 * the command. The DLL gives the result to the program.
 *
 * The DLL shows one host adapter (0). Target 0, LUNs 0 and 1, is the
 * Pro-1000. The DLL answers host adapter and device type requests itself.
 *
 * Operation
 * ---------
 * - The DLL connects to Supermodel when the program calls the first ASPI
 *   function that needs the connection (GetASPI32SupportInfo() or the first
 *   SCSI command). It shows the "Supermodel Pro-1000 Host" window, in which
 *   the user gives the host and the port of Supermodel (see
 *   ConnectDialog.cpp). The default is 127.0.0.1, port 31000. The window
 *   stays until the connection is made, or until the user presses Exit,
 *   which ends the program.
 * - The DLL connects only one time. If the connection is lost, the DLL
 *   prints an error, and all SCSI commands after that fail (selection
 *   timeout). The Pro-1000 programs cannot continue after lost commands, so
 *   the DLL does not connect again.
 * - SendASPI32Command() does each SCSI command completely before it
 *   returns. Then it sets the status, and sets the event or calls the post
 *   routine if the program asked for one.
 * - Requests do not always give a direction for the data. The Pro-1000
 *   programs give none. Then the SCSI command (CDB byte 0) gives the
 *   direction.
 * - If the target sends CHECK CONDITION, the DLL gets the sense data with
 *   REQUEST SENSE.
 * - The DLL must be a 32-bit DLL because the programs are 32-bit programs.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include "aspi.h"
#include "ConnectDialog.h"
#include "Pro1000/SCSITCPBridgeProtocol.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace SCSITCPBridgeProtocol;

namespace
{
  constexpr BYTE HOST_ADAPTER_ID = 0;
  constexpr BYTE HOST_SCSI_ID = 7;
  constexpr BYTE TARGET_ID = 0;
  constexpr BYTE MAX_LUN = 1;
  constexpr BYTE DEVICE_TYPE_PROCESSOR = 3;
  constexpr DWORD MAX_TRANSFER_LENGTH = 0x10000;

  constexpr BYTE STATUS_CHECK_CONDITION = 0x02;
  constexpr BYTE SRB_DIR_NONE = SRB_DIR_IN | SRB_DIR_OUT;

  struct CommandOutcome
  {
    bool completed = false;
    uint8_t status = 0xFF;
    std::vector<uint8_t> dataIn;
    std::string error;
  };

  // Connection to Supermodel. The lock prevents simultaneous commands from
  // more than one thread.
  CRITICAL_SECTION g_lock;
  SOCKET g_socket = INVALID_SOCKET;
  bool g_winsockStarted = false;
  uint32_t g_sequence = 0;

  // Host and port of Supermodel: the default, then the values that were used
  // for the connection. After the connection is lost, the DLL does not
  // connect again.
  std::string g_host = "127.0.0.1";
  uint16_t g_port = DEFAULT_PORT;
  bool g_connectedBefore = false;

  bool SendAll(const void *data, size_t length)
  {
    const char *p = static_cast<const char *>(data);
    while (length > 0)
    {
      int sent = send(g_socket, p, int(std::min<size_t>(length, 0x100000)), 0);
      if (sent <= 0)
        return false;
      p += sent;
      length -= sent;
    }
    return true;
  }

  bool ReceiveAll(void *dest, size_t length)
  {
    char *p = static_cast<char *>(dest);
    while (length > 0)
    {
      int received = recv(g_socket, p, int(std::min<size_t>(length, 0x100000)), 0);
      if (received <= 0)
        return false;
      p += received;
      length -= received;
    }
    return true;
  }

  void Disconnect(void)
  {
    if (g_socket != INVALID_SOCKET)
    {
      closesocket(g_socket);
      g_socket = INVALID_SOCKET;
    }
  }

  // Connects to Supermodel the first time it is called. Shows the
  // "Supermodel Pro-1000 Host" window, which stays until there is a
  // connection or the user ends the program. Returns false after the
  // connection is lost (the DLL does not connect again).
  bool Connect(void)
  {
    if (g_socket != INVALID_SOCKET)
      return true;

    if (!g_winsockStarted)
    {
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
      {
        fprintf(stderr, "wnaspi32: unable to start Winsock\n");
        return false;
      }
      g_winsockStarted = true;
    }

    if (g_connectedBefore)
      return false;   // the connection was lost
    g_connectedBefore = true;

    g_socket = ShowConnectDialog(&g_host, &g_port, std::string());
    if (g_socket == INVALID_SOCKET)
    {
      fprintf(stderr, "wnaspi32: unable to show the connection window\n");
      return false;
    }

    // Send each command immediately. Commands are small and the program waits
    // for each result.
    BOOL noDelay = TRUE;
    setsockopt(g_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&noDelay), sizeof(noDelay));
    fprintf(stderr, "wnaspi32: connected to Supermodel at %s (port %u)\n", g_host.c_str(), unsigned(g_port));
    return true;
  }

  // Sends a SCSI command to Supermodel and waits for the result. Returns
  // false if the connection fails.
  bool ExecuteRemote(BYTE target, BYTE lun, const BYTE *cdb, BYTE cdbLength, const BYTE *dataOut, DWORD dataOutLength, DWORD dataInLength, CommandOutcome *outcome)
  {
    if (!Connect())
      return false;

    // Frame: FrameHeader, ExecuteCommand, CDB, data out
    ExecuteCommand command = {};
    command.header.version = VERSION;
    command.header.type = uint16_t(MessageType::ExecuteCommand);
    command.header.sequence = ++g_sequence;
    command.targetID = target;
    command.lun = lun;
    command.cdbLength = cdbLength;
    command.dataOutLength = dataOutLength;
    command.dataInLength = dataInLength;

    FrameHeader frame;
    frame.length = uint32_t(sizeof(command) + cdbLength + dataOutLength);

    std::vector<uint8_t> buffer(sizeof(frame) + frame.length);
    uint8_t *p = buffer.data();
    memcpy(p, &frame, sizeof(frame));
    p += sizeof(frame);
    memcpy(p, &command, sizeof(command));
    p += sizeof(command);
    memcpy(p, cdb, cdbLength);
    p += cdbLength;
    if (dataOutLength > 0)
      memcpy(p, dataOut, dataOutLength);

    // The result: FrameHeader, CommandResult, data in, error text
    FrameHeader resultFrame;
    CommandResult result;
    std::vector<uint8_t> message;
    if (!SendAll(buffer.data(), buffer.size()) || !ReceiveAll(&resultFrame, sizeof(resultFrame)) || resultFrame.length < sizeof(result) || resultFrame.length > MAX_MESSAGE_SIZE)
    {
      fprintf(stderr, "wnaspi32: connection to Supermodel lost. SCSI commands will fail.\n");
      Disconnect();
      return false;
    }
    message.resize(resultFrame.length);
    if (!ReceiveAll(message.data(), message.size()))
    {
      fprintf(stderr, "wnaspi32: connection to Supermodel lost. SCSI commands will fail.\n");
      Disconnect();
      return false;
    }
    memcpy(&result, message.data(), sizeof(result));
    if (result.header.version != VERSION || result.header.type != uint16_t(MessageType::CommandResult) || result.header.sequence != command.header.sequence ||
        sizeof(result) + size_t(result.dataInLength) + size_t(result.errorLength) != message.size())
    {
      fprintf(stderr, "wnaspi32: incorrect result from Supermodel. Connection closed, SCSI commands will fail.\n");
      Disconnect();
      return false;
    }

    const uint8_t *data = message.data() + sizeof(result);
    outcome->completed = result.completed != 0;
    outcome->status = result.status;
    outcome->dataIn.assign(data, data + result.dataInLength);
    outcome->error.assign(reinterpret_cast<const char *>(data + result.dataInLength), result.errorLength);
    return true;
  }

  // Returns true if the SCSI command sends data to the target. Use this only
  // if the request does not give the direction.
  bool CommandSendsData(BYTE opcode)
  {
    switch (opcode)
    {
    case 0x0A:  // WRITE(6)
    case 0x15:  // MODE SELECT(6)
    case 0x2A:  // WRITE(10)
    case 0x3B:  // WRITE BUFFER
    case 0x55:  // MODE SELECT(10)
      return true;
    default:
      return false;
    }
  }

  BYTE ExecSCSICmd(SRB_ExecSCSICmd *srb)
  {
    srb->HaStat = HASTAT_OK;
    srb->TargStat = 0;
    if (srb->HaId != HOST_ADAPTER_ID)
      return SS_INVALID_HA;
    if (srb->Target != TARGET_ID || srb->Lun > MAX_LUN)
      return SS_NO_DEVICE;
    if (srb->CDBLen == 0 || srb->CDBLen > MAX_CDB_LENGTH)
      return SS_ERR;

    // Direction of the data
    bool dataOut;
    bool dataIn;
    switch (srb->Flags & SRB_DIR_NONE)
    {
    case SRB_DIR_IN:
      dataIn = true;
      dataOut = false;
      break;
    case SRB_DIR_OUT:
      dataIn = false;
      dataOut = true;
      break;
    case SRB_DIR_NONE:
      dataIn = false;
      dataOut = false;
      break;
    default:  // the SCSI command gives the direction
      dataOut = CommandSendsData(srb->CDBByte[0]);
      dataIn = !dataOut;
      break;
    }
    DWORD length = srb->BufPointer ? srb->BufLen : 0;

    CommandOutcome outcome;
    EnterCriticalSection(&g_lock);
    bool connected = ExecuteRemote(srb->Target, srb->Lun, srb->CDBByte, srb->CDBLen, srb->BufPointer, dataOut ? length : 0, dataIn ? length : 0, &outcome);
    LeaveCriticalSection(&g_lock);
    if (!connected)
    {
      srb->HaStat = HASTAT_SEL_TO;
      return SS_ERR;
    }

    if (dataIn && length > 0)
      memcpy(srb->BufPointer, outcome.dataIn.data(), std::min<size_t>(length, outcome.dataIn.size()));
    if (!outcome.completed)
    {
      fprintf(stderr, "wnaspi32: SCSI command %02X failed: %s\n", srb->CDBByte[0], outcome.error.c_str());
      srb->HaStat = HASTAT_PHASE_ERR;
      return SS_ERR;
    }

    srb->TargStat = outcome.status;
    if (outcome.status == 0)
      return SS_COMP;

    // Get the sense data for CHECK CONDITION
    if (outcome.status == STATUS_CHECK_CONDITION && srb->SenseLen > 0)
    {
      BYTE requestSense[6] = { 0x03, BYTE(srb->Lun << 5), 0, 0, srb->SenseLen, 0 };
      CommandOutcome sense;
      EnterCriticalSection(&g_lock);
      bool ok = ExecuteRemote(srb->Target, srb->Lun, requestSense, sizeof(requestSense), nullptr, 0, srb->SenseLen, &sense);
      LeaveCriticalSection(&g_lock);
      if (ok && sense.completed)
        memcpy(srb->SenseArea, sense.dataIn.data(), std::min<size_t>(srb->SenseLen, sense.dataIn.size()));
    }
    return SS_ERR;
  }
}


/******************************************************************************
 ASPI Functions
******************************************************************************/

extern "C" DWORD __cdecl SendASPI32Command(void *srbPointer)
{
  SRB_Header *header = static_cast<SRB_Header *>(srbPointer);
  BYTE status;

  switch (header->Cmd)
  {
  case SC_HA_INQUIRY:
  {
    SRB_HAInquiry *srb = static_cast<SRB_HAInquiry *>(srbPointer);
    if (srb->HaId != HOST_ADAPTER_ID)
    {
      status = SS_INVALID_HA;
      break;
    }
    srb->HA_Count = 1;
    srb->HA_SCSI_ID = HOST_SCSI_ID;
    memcpy(srb->HA_ManagerId, "ASPI for WIN32  ", 16);
    memcpy(srb->HA_Identifier, "Supermodel      ", 16);
    memset(srb->HA_Unique, 0, sizeof(srb->HA_Unique));
    srb->HA_Unique[3] = 8;                                // maximum number of targets
    memcpy(&srb->HA_Unique[4], &MAX_TRANSFER_LENGTH, 4);  // maximum transfer length
    status = SS_COMP;
    break;
  }

  case SC_GET_DEV_TYPE:
  {
    SRB_GDEVBlock *srb = static_cast<SRB_GDEVBlock *>(srbPointer);
    if (srb->HaId == HOST_ADAPTER_ID && srb->Target == TARGET_ID && srb->Lun <= MAX_LUN)
    {
      srb->DeviceType = DEVICE_TYPE_PROCESSOR;
      status = SS_COMP;
    }
    else
      status = SS_NO_DEVICE;
    break;
  }

  case SC_EXEC_SCSI_CMD:
  {
    // Set the status before the program is told that the command is complete
    SRB_ExecSCSICmd *srb = static_cast<SRB_ExecSCSICmd *>(srbPointer);
    status = ExecSCSICmd(srb);
    srb->Status = status;
    if ((srb->Flags & SRB_EVENT_NOTIFY) && srb->PostProc)
      SetEvent(static_cast<HANDLE>(srb->PostProc));
    else if ((srb->Flags & SRB_POSTING) && srb->PostProc)
      reinterpret_cast<void (__cdecl *)(void *)>(srb->PostProc)(srb);
    return status;
  }

  case SC_ABORT_SRB:
  case SC_RESET_DEV:
  case SC_RESCAN_SCSI_BUS:
    status = SS_COMP;
    break;

  default:
    status = SS_INVALID_CMD;
    break;
  }

  header->Status = status;
  return status;
}

extern "C" DWORD __cdecl GetASPI32SupportInfo(void)
{
  // Programs call this first. Connect now, so the connection window opens
  // when the program starts.
  EnterCriticalSection(&g_lock);
  Connect();
  LeaveCriticalSection(&g_lock);
  return (SS_COMP << 8) | 1;  // status, and one host adapter
}

extern "C" DWORD __cdecl GetASPI32DLLVersion(void)
{
  return 2;
}

extern "C" BOOL __cdecl GetASPI32Buffer(void *)
{
  return FALSE;
}

extern "C" BOOL __cdecl FreeASPI32Buffer(void *)
{
  return FALSE;
}

extern "C" BOOL __cdecl TranslateASPI32Address(DWORD *, DWORD *)
{
  return FALSE;
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID reserved)
{
  switch (reason)
  {
  case DLL_PROCESS_ATTACH:
    InitializeCriticalSection(&g_lock);
    break;
  case DLL_PROCESS_DETACH:
    // When the process terminates (reserved is not null), Windows releases
    // the resources
    if (reserved == nullptr)
    {
      Disconnect();
      if (g_winsockStarted)
        WSACleanup();
      DeleteCriticalSection(&g_lock);
    }
    break;
  }
  return TRUE;
}
