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
 * aspi.h
 *
 * The parts of ASPI for Win32 (wnaspi32.dll) that the replacement DLL uses.
 * The structure layouts are the same as in the ASPI SDK header (wnaspi32.h).
 * Programs give a SCSI request block (SRB) to SendASPI32Command().
 */

#ifndef INCLUDED_ASPI_H
#define INCLUDED_ASPI_H

#include <windows.h>

// SRB commands
#define SC_HA_INQUIRY       0x00
#define SC_GET_DEV_TYPE     0x01
#define SC_EXEC_SCSI_CMD    0x02
#define SC_ABORT_SRB        0x03
#define SC_RESET_DEV        0x04
#define SC_GET_DISK_INFO    0x06
#define SC_RESCAN_SCSI_BUS  0x07

// SRB status
#define SS_PENDING          0x00
#define SS_COMP             0x01
#define SS_ABORTED          0x02
#define SS_ERR              0x04
#define SS_INVALID_CMD      0x80
#define SS_INVALID_HA       0x81
#define SS_NO_DEVICE        0x82

// SRB flags
#define SRB_POSTING         0x01  // call the post routine at completion
#define SRB_DIR_IN          0x08  // data goes from the target to the host
#define SRB_DIR_OUT         0x10  // data goes from the host to the target
#define SRB_EVENT_NOTIFY    0x40  // set the event at completion

// Usual size of the sense data area
#define SENSE_LEN           14

// Host adapter status
#define HASTAT_OK           0x00
#define HASTAT_SEL_TO       0x11  // selection timeout
#define HASTAT_PHASE_ERR    0x14  // unexpected bus phase

#pragma pack(push, 1)

typedef struct
{
  BYTE  Cmd;
  BYTE  Status;
  BYTE  HaId;
  BYTE  Flags;
  DWORD Hdr_Rsvd;
} SRB_Header;

typedef struct
{
  BYTE  Cmd;
  BYTE  Status;
  BYTE  HaId;
  BYTE  Flags;
  DWORD Hdr_Rsvd;
  BYTE  HA_Count;
  BYTE  HA_SCSI_ID;
  BYTE  HA_ManagerId[16];
  BYTE  HA_Identifier[16];
  BYTE  HA_Unique[16];
  WORD  HA_Rsvd1;
} SRB_HAInquiry;

typedef struct
{
  BYTE  Cmd;
  BYTE  Status;
  BYTE  HaId;
  BYTE  Flags;
  DWORD Hdr_Rsvd;
  BYTE  Target;
  BYTE  Lun;
  BYTE  DeviceType;
  BYTE  Rsvd1;
} SRB_GDEVBlock;

typedef struct
{
  BYTE  Cmd;
  BYTE  Status;
  BYTE  HaId;
  BYTE  Flags;
  DWORD Hdr_Rsvd;
  BYTE  Target;
  BYTE  Lun;
  WORD  Rsvd1;
  DWORD BufLen;
  BYTE  *BufPointer;
  BYTE  SenseLen;
  BYTE  CDBLen;
  BYTE  HaStat;
  BYTE  TargStat;
  void  *PostProc;            // event handle (SRB_EVENT_NOTIFY) or post routine (SRB_POSTING)
  void  *Rsvd2;
  BYTE  Rsvd3[16];
  BYTE  CDBByte[16];
  BYTE  SenseArea[SENSE_LEN + 2]; // the program gives the size in SenseLen (it can be larger)
} SRB_ExecSCSICmd;

#pragma pack(pop)

#endif  // INCLUDED_ASPI_H
