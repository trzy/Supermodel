#ifndef _UDP_H_
#define _UDP_H_

namespace SMUDP
{
	struct Packet
	{

		static const UINT32 BUFFER_SIZE_UDP = 1460;

		UINT32 crc;
		UINT16 currentID;
		UINT16 totalIDs;
		UINT16 flags;
		UINT16 length;
		UCHAR  data[BUFFER_SIZE_UDP];

		Packet() {
			Init();
		}

		enum class PacketFlags {
			newConnection = (1 << 0),
			resend = (1 << 1),
			ping = (1 << 2)
		};

		void Init() {
			crc			= 0;
			currentID	= 0;
			totalIDs	= 0;
			flags		= 0;
			length		= 0;
		}

		UINT32 CalcCRC() {
			crc = CalcCRCVal();
		}

		UINT32 CalcCRCVal() {

			UINT32 val = 0;

			for (int i = 0; i < _countof(data); i++) {
				val += data[i];		// crude but will catch the odd off by one error
			}

			return val;
		}

		bool ValidateCRC() {
			return CalcCRCVal() == crc;
		}

		void CreatePacket(PacketFlags p) {
			flags |= (UINT16)p;
		}

		void CalcTotalIDs(int bytes) {
			totalIDs = bytes / sizeof(data);

			if (bytes % sizeof(data)) {
				totalIDs++;
			}
		}

		int HeaderSize() {
			return 12;
		}

		int Size() {
			return HeaderSize() + length;
		}

		operator char*()		{ return (char*)this; }
		operator const char*()	{ return (char*)this; }
	};

	typedef char PacketReply;
}

#endif