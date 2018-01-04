// todo:
// 68k context mixing (important if we don't want to use -no-threads)
// memory mapping correct ? seems to be good now
// find what blocks games to enter attract mode (seems to be related to timing/order irq)
// check if trigger for code downloaded correct (model3.cpp)
// Irq implementation check, timer for irq5 or at every frame? Where to put irq2 (mame says at every register writes), irq4, irq6.
// check all, lol :)
// obsolete : ring network code is blocking, this cause the program to not responding when quit because it enter in receive fonction
//		this also leads to a forcing synchro between cabs, not acting like a real hardware 
// Changing to Ian's udp code. It is non blocking. Now the sync between instance could not be get each time (bug), because 1st data may be not the sync (thread vs sequential code) as the old blocking recv
// ---> setting pause, moving window make lost connection now and bug the game
// ---> crash when quit. Assumption : may be supermodel close while udp thread running ?
// ioreg and ctrlrw are direct recopy from mame (thx to the mame devs). Need more investigation
// commram memory bank ? seems to be automatic
// savestates
// not sure the init network follow real hardware normal operation, seems I miss something
// if host reset, network card would reset too (bug)
// in case of 3 or more network cab, something wrong (I think it isn't a net transfert issue (working ring network), but more a data process issue
//		(->overlapping cars, 3rd cab not really responding to network)
//		not tested with Ian's udp code
// not sure code works on linux (socket) posix compliant....
// very slow : -no-threads , debug code, debug prints
// and a lot of work ;) -> early netcode, many questions without answer
//

// why lemans24 and von2 send 0 0 0 0 instead of a data trame ?
// skichamp & harley go to print id and id node (source modded to these games work, need total rethink)
// srally2, spikofe, daytona2 send only 1 data trame in loop, do not go on after even if slave sends good response (master bug ?)
// spikout network error : slave go in game, master network error
// dayto2pe master and slave reset
// scud canceled network, reset
// dirtdlvs starts to run, this is at least 1 good news (add skichamp & harley)

// add NET_BOARD for building
// do not forget -no-threads when launching
//
// in supermodel.ini
//
// change port/ip you want, I only tested in local on same machine (watch your firewall)
//
// add for master
// EmulateNet=1
// port_in = 1970
// port_out = 1971
// addr_out = "127.0.0.1"
//
// add for slave
// EmulateNet=1
// port_in = 1971
// port_out = 1970
// addr_out = "127.0.0.1"
//
// or in case of 3 cabs
//
// add for master
// EmulateNet=1
// port_in = 1970
// port_out = 1971
// addr_out = "127.0.0.1"
//
// add for slave1
// EmulateNet=1
// port_in = 1971
// port_out = 1972
// addr_out = "127.0.0.1"
//
// add for slave2
// EmulateNet=1
// port_in = 1972
// port_out = 1970
// addr_out = "127.0.0.1"

//#define NET_DEBUG

#include "Supermodel.h"
#include "NetBoard.h"
#include "Util/Format.h"
#include "Util/ByteSwap.h"
#include <thread>
#include "UDPSend.h"
#include "UDPReceive.h"

using namespace SMUDP;

static int(*Runnet68kCB)(int cycles);
static void(*Intnet68kCB)(int irq);

void Net_SetCB(int(*Run68k)(int cycles), void(*Int68k)(int irq))
{
	Intnet68kCB = Int68k;
	Runnet68kCB = Run68k;
}

// Status of IRQ pins (IPL2-0) on 68K
// TODO: can we get rid of this global variable altogether?
static int	irqLine = 0;

// Interrupt acknowledge callback (TODO: don't need this, default behavior in M68K.cpp should be fine)
int NetIRQAck(int irqLevel)
{
	M68KSetIRQ(0);
	irqLine = 0;
	return M68K_IRQ_AUTOVECTOR;
}

// SCSP callback for generating IRQs
void NET68KIRQCallback(int irqLevel)
{
	/*
	* IRQ arbitration logic: only allow higher priority IRQs to be asserted or
	* 0 to clear pending IRQ.
	*/
	if ((irqLevel>irqLine) || (0 == irqLevel))
	{
		irqLine = irqLevel;

	}
	M68KSetIRQ(irqLine);
}

// SCSP callback for running the 68K
int NET68KRunCallback(int numCycles)
{
	return M68KRun(numCycles) - numCycles;
}

UINT8 CNetBoard::Read8(UINT32 a)
{
	switch ((a >> 16) & 0xF)
	{
	case 0x0:
		//printf("Netboard R8\tRAM[%x]=%x\n", a,RAM[a]);
		if (a > 0x0ffff) 
		{
			printf("OUT OF RANGE RAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		return RAM[a];

	case 0x4:
		//printf("Netboard R8\tctrlrw[%x]=%x\n", a&0xff, ctrlrw[a&0xff]);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ctrlrw[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		switch (a & 0xff)
		{
		case 0x0:
			#ifdef NET_DEBUG
			printf("Netboard R8\tctrlrw[%x]=%x\tcommbank = %x\n", a & 0xff, ctrlrw[a & 0xff], commbank);
			#endif		
			return ctrlrw[a&0xff];//commbank;
			break;

		default:
			printf("unknown 400(%x)\n", a & 0xff);
			MessageBox(NULL, "Unknown R8 CTRLRW", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
			return ctrlrw[a&0xff];
			break;

		}
		//return ctrlrw[a];

	case 0x8: // dirt devils
		//if(((a&0xffff) > 0 && (a&0xffff) < 0xff) || ((a&0xffff) > 0xefff && (a&0xffff) < 0xffff)) printf("Netboard R8\tCommRAM[%x]=%x\n", a & 0xffff, CommRAM[a & 0xffff]);
		if ((a & 0x3ffff) > 0xffff)
		{
			printf("OUT OF RANGE CommRAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		return CommRAM[a & 0xffff];

	case 0xc: // dirt devils
		//printf("Netboard R8\tioreg[%x]=%x\t\t", a&0xff, ioreg[a&0xff]);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ioreg[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		
		switch (a & 0xff)
		{
		case 0x11: // ioreg[c0011]
			#ifdef NET_DEBUG
			printf("Netboard R8\tioreg[%x]=%x\t\treceive result status\n", a & 0xff, ioreg[a & 0xff]);
			#endif
			return ioreg[a&0xff]; 
			break;

		case 0x19: // ioreg[c0019]
			#ifdef NET_DEBUG
			printf("Netboard R8\tioreg[%x]=%x\t\ttransmit result status\n", a & 0xff, ioreg[a & 0xff]);
			#endif
			return ioreg[a&0xff];
			break;

		case 0x81: // ioreg[c0081]
			#ifdef NET_DEBUG
			printf("Netboard R8\tioreg[%x]=%x\t\n", a & 0xff, ioreg[a & 0xff]);
			#endif
			return ioreg[a&0xff];
			break;

		case 0x83: // ioreg[c0083]
			#ifdef NET_DEBUG
			printf("Netboard R8\tioreg[%x]=%x\t\tirq status\n", a & 0xff, ioreg[a & 0xff]);
			#endif
			return ioreg[a&0xff];
			break;

		case 0x89: // ioreg[c0089]
			#ifdef NET_DEBUG
			printf("Netboard R8\tioreg[%x]=%x\t\n", a & 0xff, ioreg[a & 0xff]);
			#endif
			return ioreg[a&0xff];
			break;

		case 0x8a: // ioreg[c008a]
			#ifdef NET_DEBUG
			printf("Netboard R8\tioreg[%x]=%x\t\n", a & 0xff, ioreg[a & 0xff]);
			#endif
			return ioreg[a & 0xff];
			break;

		default:
			printf("unknown c00(%x)\n", a & 0xff);
			//MessageBeep(MB_ICONWARNING);
			MessageBox(NULL, "Unknown R8 IOREG", NULL, MB_OK);
			return ioreg[a&0xff];
			break;

		}


	default:
		printf("NetBoard 68K: Unknown R8 (%02X) addr=%x\n", (a >> 16) & 0xF,a&0x0fffff);
		//MessageBeep(MB_ICONWARNING);
		MessageBox(NULL, "Unknown R8", NULL, MB_OK);
		break;
	}

	return 0;
}

UINT16 CNetBoard::Read16(UINT32 a)
{
	UINT16 result;
	switch ((a >> 16) & 0xF)
	{
	case 0x0:
		//printf("Netboard Read16 (0x0) \tRAM[%x]=%x\n",a, *(UINT16 *)&RAM[a]);
		if (a > 0x0ffff)
		{
			printf("OUT OF RANGE RAM[%x]\n", a);
			//MessageBeep(MB_ICONWARNING);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
		}
		result = *(UINT16 *)&RAM[a];
		return result;

	case 0x4: // dirt devils
		//printf("Netboard R16\tctrlrw[%x] = %x\t", a&0xff, *(UINT16 *)&ctrlrw[a&0xff]);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ctrlrw[%x]\n", a);
			//MessageBeep(MB_ICONWARNING);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
		}
				
		switch (a & 0xff)
		{
		case 0x0:
			result = *(UINT16 *)&ctrlrw[a & 0xff];
			//printf("Netboard R16\tctrlrw[%x] = %x\t\tcommbank = %x\n", a & 0xff, *(UINT16 *)&ctrlrw[a & 0xff],commbank & 1);
			return result; // commbank;
			break;

		default:
			result = *(UINT16 *)&ctrlrw[a & 0xff];
			printf("unknown 400(%x)\n", a & 0xff);
			MessageBox(NULL, "Unknown R16 CTRLRW", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
			return result;
			break;

		}

	case 0x8: // dirt devils
		//if (((a & 0xffff) > 0 && (a & 0xffff) < 0xff) || ((a & 0xffff) > 0xefff && (a & 0xffff) < 0xffff)) printf("Netboard R16\tCommRAM[%x] = %x\n", a & 0xffff, *(UINT16 *)&CommRAM[a & 0xffff]);
		if ((a & 0x3ffff) > 0xffff)
		{
			printf("OUT OF RANGE CommRAM[%x]\n", a);
			//MessageBeep(MB_ICONWARNING);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
		}
		result = *(UINT16 *)&CommRAM[a & 0xffff];
		return result;

	case 0xc:
		//printf("Netboard Read16 (0xc) \tioreg[%x] = %x\t", a&0xff, *(UINT16 *)&ioreg[a&0xff]);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ioreg[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
				
		switch (a & 0xff)
		{
		case 0x88: // ioreg[c0088]
			result = *(UINT16 *)&ioreg[a & 0xff];
			#ifdef NET_DEBUG
			printf("Netboard R16\tioreg[%x] = %x\n", a & 0xff, *(UINT16 *)&ioreg[a & 0xff]);
			#endif
			return result;

		case 0x8a: // ioreg[c008a]
			result = *(UINT16 *)&ioreg[a & 0xff];
			#ifdef NET_DEBUG
			printf("Netboard R16\tioreg[%x] = %x\n", a & 0xff, *(UINT16 *)&ioreg[a & 0xff]);
			#endif
			return result;

		default:
			result = *(UINT16 *)&ioreg[a & 0xff];
			printf("unknown c00(%x)\n", a & 0xff);
			MessageBox(NULL, "Unknown R16 IOREG", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
			return result;
		}

	default:
		printf("NetBoard 68K: Unknown R16 %02X addr=%x\n", (a >> 16) & 0xF,a&0x0fffff);
		//MessageBeep(MB_ICONWARNING);
		MessageBox(NULL, "Unknown R16", NULL, MB_OK);
		break;
	}

	return 0;
}

UINT32 CNetBoard::Read32(UINT32 a)
{
	UINT32	hi, lo;
	UINT32 result;
	switch ((a >> 16) & 0xF)
	{
	case 0x0:
		hi = *(UINT16 *)&RAM[a];
		lo = *(UINT16 *)&RAM[a + 2];
		if (a > 0x0ffff)
		{
			printf("OUT OF RANGE RAM[%x]\n", a);
			//MessageBeep(MB_ICONWARNING);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
		}
		//printf("Netboard R32\tRAM[%x]=%x\n", a,(hi << 16) | lo);
		result = (hi << 16) | lo;
		return result;

	/*case 0x4: // no access
		hi = *(UINT16 *)&ctrlrw[a];
		lo = *(UINT16 *)&ctrlrw[a + 2];
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ctrlrw[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		printf("Netboard R32\tctrlrw[%x]=%x\n", a, (hi << 16) | lo);
		result = (hi << 16) | lo;
		return result;*/

	case 0x8: // dirt devils
		hi = *(UINT16 *)&CommRAM[a & 0xffff];
		lo = *(UINT16 *)&CommRAM[(a & 0xffff) + 2];
		if ((a & 0x3ffff) > 0xffff)
		{
			printf("OUT OF RANGE CommRAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		//if (((a & 0xffff) > 0 && (a & 0xffff) < 0xff) || ((a & 0xffff) > 0xefff && (a & 0xffff) < 0xffff)) printf("Netboard R32\tCommRAM[%x] = %x\n", a & 0xffff, (hi << 16) | lo);
		result = (hi << 16) | lo;
		return result;

	/*case 0xc: // no access
		hi = *(UINT16 *)&ioreg[a];
		lo = *(UINT16 *)&ioreg[a + 2];
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ioreg[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		printf("Netboard R32\tioreg[%x]=%x\n", a, (hi << 16) | lo);
		result = (hi << 16) | lo;
		return result;*/

	default:
		printf("NetBoard 68K: Unknown R32 (%02X) a=%x\n", (a >> 16) & 0xF, a & 0xffff);
		//MessageBeep(MB_ICONWARNING);
		MessageBox(NULL, "Unknown R32", NULL, MB_OK);
		break;
	}

	return 0;
}

void CNetBoard::Write8(UINT32 a, UINT8 d)
{
	switch ((a >> 16) & 0xF)
	{
	case 0x0:
		//printf("Netboard Write8 (0x0) \tRAM[%x] <- %x\n", a, d); 
		if (a > 0x0ffff)
		{
			printf("OUT OF RANGE RAM[%x]\n", a);
			//MessageBeep(MB_ICONWARNING);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
		}
		RAM[a] = d;
		break;

	case 0x4:
		//printf("Netboard W8\tctrlrw[%x] <- %x\t", a&0xff, d);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ctrlrw[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}

		switch (a & 0xff)
		{
		case 0x40:
			ctrlrw[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tctrlrw[%x] <- %x\tIRQ 5 ack\n", a & 0xff, d);
			#endif
			NetIRQAck(5);
			//NetIRQAck(0);
			M68KSetIRQ(4); // dirtdvls needs ????????????????????????????????????????????????????????????????????????????????????????????????????
			//M68KRun(10000);
			break;

		case 0xa0:
			ctrlrw[a & 0xff] = d;
			ioreg[0] = ioreg[0] | 0x01; // Do I force this myself or is it automatic, need investigation, bad init way actually ?
			#ifdef NET_DEBUG
			printf("Netboard W8\tctrlrw[%x] <- %x\tIRQ 2 ack\n", a & 0xff, d);
			#endif
			NetIRQAck(2);
			//NetIRQAck(0);
			//M68KRun(10000);
			break;

		case 0x80:
			ctrlrw[a & 0xff] = d;
			#ifdef NET_DEBUG
			//printf("Netboard W8\tctrlrw[%x] <- %x\tleds <- %x\n", a & 0xff, d,d);
			#endif
			break;

		default:
			printf("unknown 400(%x)\n", a & 0xff);
			ctrlrw[a & 0xff] = d;
			MessageBox(NULL, "Unknown W8 CTRLRW", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
			break;

		}
		
		break;

	case 0x8: // dirt devils
		//if (((a & 0xffff) > 0 && (a & 0xffff) < 0xff) || ((a & 0xffff) > 0xefff && (a & 0xffff) < 0xffff)) printf("Netboard W8\tCommRAM[%x] <- %x\n", a & 0xffff, d);
		if ((a & 0x3ffff) > 0xffff)
		{
			printf("OUT OF RANGE CommRAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		CommRAM[a & 0xffff] = d;
		break;

	case 0xc: // dirt devils
		//printf("Netboard Write8 (0xc) \tioreg[%x] <- %x\t", a&0xff, d);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ioreg[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}

		switch (a & 0xff)
		{
		case 0x01: // ioreg[c0001]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x03: // ioreg[c0003]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x05: // ioreg[c0005]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;
		
		/*case 0x15: // 0x00 0x01 0x80 // ioreg[c0015]
			ioreg[a & 0xff] = d;
			printf("Netboard W8\tioreg[%x] <- %x\t\t", a & 0xff, d);
			
			if ((d & 0xFF) != 0x80)
			{
				if ((d & 0xFF) == 0x01)
				{
					printf("Irq 6 ack\n");
					NetIRQAck(6);
					//NetIRQAck(0);
				}
				else
				{
					printf("\n");
				}
			}
			else
			{
				printf("data receive enable ???\n");
				//M68KSetIRQ(6); // irq6 ici reset dirt a la fin de la synchro
				//M68KRun(10000);
			}
			break;

		case 0x17: // 0x00 0x8c // ioreg[c0017]
			ioreg[a & 0xff] = d;
			//M68KSetIRQ(6); // si irq6 ici, reset master a la fin de la synchro
			printf("Netboard W8\tioreg[%x] <- %x\t\t", a & 0xff, d);
			if ((d & 0xFF) == 0x8C)
			{
				printf("receive enable off=%x size=%x\n", recv_offset, recv_size);
				printf("receiving : ");
				
				recv_size = recv_size & 0x7fff;

				receive2(CommRAM + recv_offset, recv_size, recv_offset);

				//for (int i = 0; i < recv_size; i++)
				if (recv_size > 50)
				{
					for (int i = 0; i < 100; i++)
					{
						printf("%x ", CommRAM[recv_offset + i]);
					}
				}
				else
				{
					for (int i = 0; i < recv_size; i++)
					{
						printf("%x ", CommRAM[recv_offset + i]);
					}
				}
				printf("\n");

				M68KSetIRQ(6); // no carrier error if removed
				//M68KRun(10000); // obligatoire, warning 500 cycles not enough // pas obligatoire si 3*call dans le netrunframe ???
			}
			else
			{
				printf("??? receive disable\n");
				//M68KSetIRQ(6);

			}
			break;
			*/

			// 0x15 0x17 inverted (correct or not ?)
			// 15 and 17 = receive part
			// master starts with 1b 15 17 (set id node) 1d
			// slave follow with 15 17 then (set id node) 1b 1d
			
		case 0x15: // 0x00 0x01 0x80 // ioreg[c0015]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\t\t", a & 0xff, d);
			#endif
			switch (d & 0xff)
			{
			case 0x80:
				#ifdef NET_DEBUG
				printf("\nreceiving : \n");
				#endif		
				//if (recv_offset > 0x1000)
				if (recv_size < 0x0019) // must find a better condition
				{
					auto recv_data = udpReceive.ReadData(5000);
					printf("-> nb recu : %x\n", recv_data.size());
					memcpy(CommRAM + recv_offset, recv_data.data(), recv_data.size());
					#ifdef NET_DEBUG
					printf("receive enable off=%x size=%x\n", recv_offset, recv_size);
					#endif
				}
				else
				{
					#ifdef NET_DEBUG
					printf("receive enable original value off=%x size=%x\n", recv_offset, recv_size);
					#endif
					slot = (recv_size >> 12) & 0x0f;
					recv_size = recv_size & 0x0fff;
					recv_size = (recv_size << 4) | ((recv_size >> 8) & 0x000f);
					recv_size = recv_size & 0x0fff;
					if (slot != 0)
					{
						recv_size = recv_size * slot;
					}
					recv_offset = (recv_offset << 8) | (recv_offset >> 8);
					#ifdef NET_DEBUG
					printf("receive enable off=%x size=%x slot=%x\n", recv_offset, recv_size, slot);
					#endif

					auto recv_data = udpReceive.ReadData(5000);
					printf("-> nb recu : %x\n", recv_data.size());
					memcpy(CommRAM + recv_offset, recv_data.data(), recv_data.size());
				}

				#ifdef NET_DEBUG
				if (recv_size > 50) // too long to print so...
				{
					for (int i = 0; i < 100; i++)
					{
						printf("%x ", CommRAM[recv_offset + i]);
					}
				}
				else
				{
					for (int i = 0; i < recv_size; i++)
					{
						printf("%x ", CommRAM[recv_offset + i]);
					}
				}
				printf("\n");
				#endif
				break;

			case 0x00:
				#ifdef NET_DEBUG
				printf("??? receive disable\n");
				#endif
				break;

			case 0x01:
				#ifdef NET_DEBUG
				printf("Irq 6 ack\n");
				#endif
				NetIRQAck(6);
				break;

			default:
				printf("15 : other value %x\n", d & 0xff);
				break;
			}
			break;

		case 0x17: // 0x00 0x8c // ioreg[c0017]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\t\t", a & 0xff, d);
			#endif
			switch (d & 0xff)
			{
			case 0x8c:
				#ifdef NET_DEBUG
				printf("data receive enable\n");
				#endif
				M68KSetIRQ(6);
				M68KRun(6000); // 6000 enough
				break;
			case 0x00:
				#ifdef NET_DEBUG
				printf("??? data receive disable\n");
				#endif
				break;
			default:
				printf("17 : other value %x\n",d & 0xff);
				break;

			}
			break;


		case 0x19: // ioreg[c0019]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\t\ttransmit result status\n");
			#endif
			break;

			// 1b 1d = send part
			// 1b is where real send must be because master always initiate dial with 1b (sure)

		case 0x1b: // 0x80 // ioreg[c001b]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\t\t\n", a & 0xff, d);
			#endif
			switch (d & 0xff)
			{
			case 0x80:
				//if (send_offset > 0x1000)
				if (send_size < 0x0011)	// must find a better condition
				{
					udpSend.SendAsync(addr_out.data(), port_out, send_size, (const char*)CommRAM + send_offset, 1000);
					#ifdef NET_DEBUG
					printf("send enable off=%x size=%x\n", send_offset, send_size);
					#endif
				}
				else
				{
					#ifdef NET_DEBUG
					printf("send enable original value off=%x size=%x\n", send_offset, send_size);
					#endif
					slot = (send_size >> 12) & 0x0f;
					send_size = send_size & 0x0fff;
					send_size = (send_size << 4) | ((send_size >> 8) & 0x000f);
					send_size = send_size & 0x0fff;

					if (slot == 0) slot = (recv_size >> 12) & 0x0f; // cheat hack for harley,skichamp, and dirt in the same time

					if (slot != 0)
					{
						send_size = send_size * slot;
					}
					send_offset = (send_offset << 8) | (send_offset >> 8);

					udpSend.SendAsync(addr_out.data(), port_out, send_size, (const char*)CommRAM + send_offset, 1000);
					#ifdef NET_DEBUG
					printf("send enable off=%x size=%x slot=%x\n", send_offset, send_size, slot);
					#endif

				}
				#ifdef NET_DEBUG
				printf("transmitting : ");
				if (send_size > 50) //too big for print, so...
				{
					for (int i = 0; i < 100; i++)
					{
						printf("%x ", CommRAM[send_offset + i]);
					}
				}
				else
				{
					for (int i = 0; i < send_size; i++)
					{
						printf("%x ", CommRAM[send_offset + i]);
					}
				}
				printf("\n");
				#endif
				break;
				
			case 0x00:
				#ifdef NET_DEBUG
				printf("??? transmit disable\n");
				#endif
				break;
			
			default:
				printf("1b : other value %x\n", d & 0xff);
				break;
			}
			break;

		case 0x1d: // 0x8c // ioreg[c001d]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			switch (d & 0xff)
			{
			case 0x8c:
				M68KSetIRQ(6); //obligatoire (pas de irq4, pas de ack, pas de cycle) irq4 : harley master other board not ready or... 
				M68KRun(10000);
				break;

			case 0x00:
				break;

			default:
				printf("1d : other value %x\n", d & 0xff);
				break;
			}
			break;


		case 0x29: // ioreg[c0029]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;
		
		case 0x2f: // 0x00 or 0x00->0x40->0x00 or 0x82// ioreg[c002f]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			/*if ((d & 0xff) == 0x00)
			{
				M68KSetIRQ(2);
				M68KRun(10000);
			}*/

			if ((d & 0xff) == 0x40)
			{
				#ifdef NET_DEBUG
				printf("********************************* trigger something ????\n");
				#endif
			}
			if ((d & 0xff) == 0x82)
			{
				#ifdef NET_DEBUG
				printf("********************************* trigger something number 2 ????\n");
				#endif
			}

			break;

		case 0x41: // ioreg[c0041]
			ioreg[a & 0xff] = d;
			recv_offset = (recv_offset >> 8) | (d << 8 );
			#ifdef NET_DEBUG
			//printf("recv off = %x\n",recv_offset);
			printf("recv off = %x\n", d);
			//printf("\n");
			#endif
			break;

		case 0x43: // ioreg[c0043]
			ioreg[a & 0xff] = d;
			recv_size = (recv_size >> 8) | (d << 8);
			#ifdef NET_DEBUG
			//printf("recv size = %x\n", recv_size);
			printf("recv size = %x\n", d);
			//printf("\n");
			#endif
			break;

		case 0x45: // ioreg[c0045]
			ioreg[a & 0xff] = d;
			send_offset = (send_offset >> 8) | (d << 8);
			#ifdef NET_DEBUG
			//printf("send off = %x\n", send_offset);
			printf("send off = %x\n", d);
			//printf("\n");
			#endif
			break;

		case 0x47: // ioreg[c0047]
			ioreg[a & 0xff] = d;
			send_size = (send_size >> 8) | (d << 8);
			#ifdef NET_DEBUG
			//printf("send size = %x\n", send_size);
			printf("send size = %x\n", d);
			//printf("\n");
			#endif
			break;

		case 0x51: //0x04 0x18 // ioreg[c0051]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x55: // ioreg[c0055]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x57: // 0x04->0x09 // ioreg[c0057]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x59: // ioreg[c0059]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x5b: // ioreg[c005b]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x5d: // ioreg[c005d]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x5f: // ioreg[c005f]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x81: // ioreg[c0081]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x83: // 0x35 once and after 0x00 always // ioreg[c0083] // just apres le ioreg[83]=0 on a ppc R32 ioreg[114] et R32 ioreg[110] et apres ack irq5
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x85: // ioreg[c0085]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x87: // ioreg[c0087]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x88: // ioreg[c0088]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x89: // ioreg[c0089] // dayto2pe loops with values 00 01 02 during type 2 frame 
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		case 0x8a: // ioreg[c008a]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif		
			break;

		case 0x8b: // ioreg[c008b]
			ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W8\tioreg[%x] <- %x\n", a & 0xff, d);
			#endif
			break;

		default:
			printf("unknown c00(%x)\n", a & 0xff);
			ioreg[a & 0xff] = d;
			MessageBox(NULL, "Unknown W8 IOREG", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
			break;

		}
		//M68KSetIRQ(2); //nope, infinite loop
		//M68KRun(10000);
		break;

	default:
		printf("NetBoard 68K: Unknown W8 (%x) %06X<-%02X\n", (a >> 16) & 0xF, a, d);
		MessageBox(NULL, "Unknown W8", NULL, MB_OK); 
		//MessageBeep(MB_ICONWARNING);
		break;
	}
}

void CNetBoard::Write16(UINT32 a, UINT16 d)
{
	switch ((a >> 16) & 0xF)
	{
	case 0x0:
		//printf("Netboard Write16 (0x0) \tRAM[%x] <- %x\n", a, d);
		if (a > 0x0ffff)
		{
			printf("OUT OF RANGE RAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		*(UINT16 *)&RAM[a] = d;
		break;
	
	case 0x4:
		//printf("Netboard W16\tctrlrw[%x] <- %x\t", a&0xff, d);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ctrlrw[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}

		switch (a & 0xff)
		{
		case 0x00:
			*(UINT16 *)&ctrlrw[a & 0xff] = d;
			commbank = d;
			#ifdef NET_DEBUG
			printf("Netboard W16\tctrlrw[%x] <- %x\t\tCommBank <- %x\n", a & 0xff, d,commbank & 1);
			#endif
			// sans swap ca avance, avec ca bloque pour scud
			//CommRAM = Buffer + ((commbank & 1) ? 0x10000 : 0); //swap
			//CommRAM = ((commbank & 1) ? bank : Buffer); // with bank swap harley doesn't pass to print id, no incidence in dirtdevils
			//CommRAM = Buffer + ((commbank & 1) ? 0x900 : 0); //swap - interessant
			//CommRAM = Buffer + ((commbank & 1) ? 0x1000 : 0); //swap - interessant
			//CommRAM = Buffer + ((commbank & 1) ? 0x7000 : 0); //swap - interessant
			//CommRAM = Buffer + ((commbank & 1) ? 0x8000 : 0); //swap - interessant
			//memcpy(CommRAM + 0x900, CommRAM+0x100, 0x800);
			//M68KSetIRQ(4); // harley transmit-receive // ?????????????????????????????????????????????????????????????????????????????????????????????????????
			//M68KRun(10000);
			//CommRAM = ((commbank & 1) ? netBuffer : Buffer);
			//CommRAM = ((commbank & 1) ? bank2 : Buffer);
			//memcpy(CommRAM+0x1000, netBuffer+0x100, 0xe00);
			//NetIRQAck(0);
			break;
		case 0x40:
			*(UINT16 *)&ctrlrw[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W16\tctrlrw[%x] <- %x\t\tIRQ 5 ack\n", a & 0xff, d);
			#endif
			NetIRQAck(5);
			M68KSetIRQ(4);  // ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????
			//M68KRun(10000);
			break;
		case 0xa0:
			*(UINT16 *)&ctrlrw[a & 0xff] = d;
			*(UINT8 *)&ioreg[0] = *(UINT8 *)&ioreg[0] | 0x01; // Do I force this myself or is it automatic, need investigation, bad init way actually ?
			#ifdef NET_DEBUG
			printf("Netboard W16\tctrlrw[%x] <-%x\t\tIRQ 2 ack\n", a & 0xff, d);
			#endif
			NetIRQAck(2);
			//NetIRQAck(0);
			break;
		case 0x80:
			*(UINT16 *)&ctrlrw[a & 0xff] = d;
			#ifdef NET_DEBUG
			//printf("Netboard W16\tctrlrw[%x] <-%x\tleds <- %x\n",a & 0xff, d,d);
			#endif
			break;
		case 0xc0:
			*(UINT16 *)&ctrlrw[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W16\tctrlrw[%x] <-%x\tNode ID <- %x\n", a & 0xff, d, d);
			#endif
			break;
		case 0xe0:
			*(UINT16 *)&ctrlrw[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W16\tctrlrw[%x] <-%x\t\treceive complete <- %x\n", a & 0xff, d, d);
			#endif
			break;
		default:
			*(UINT16 *)&ctrlrw[a & 0xff] = d;
			printf("unknown 400(%x)\n", a & 0xff);
			MessageBox(NULL, "Unknown W16 CTRLRW", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
			break;

		}
		break;

	case 0x8: // dirt devils
		//if (((a & 0xffff) > 0 && (a & 0xffff) < 0xff) || ((a & 0xffff) > 0xefff && (a & 0xffff) < 0xffff)) printf("Netboard W16\tCommRAM[%x] <- %x\n", a & 0xffff, d);
		if ((a & 0x3ffff) > 0xffff)
		{
			printf("OUT OF RANGE CommRAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK); 
			//MessageBeep(MB_ICONWARNING);
		}
		*(UINT16 *)&CommRAM[a & 0xffff] = d;
		break;

	case 0xc:
		//printf("Netboard W16\tioreg[%x] <- %x\t", a&0xff, d);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ioreg[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		
		switch (a & 0xff)
		{
		case 0x88: // ioreg[c0088]
			// register value change, do I made something special here ????
			if (d == 0) 
			{
				*(UINT16 *)&ioreg[a & 0xff] = d;
				#ifdef NET_DEBUG
				printf("Netboard W16\tioreg[%x] <- %x\t\t", a & 0xff, d);
				#endif
			}
			
			if (d == 1) 
			{
				*(UINT16 *)&ioreg[a & 0xff] = d;
				#ifdef NET_DEBUG
				printf("Netboard W16\tioreg[%x] <- %x\t\t", a & 0xff, d);
				#endif
			}

			if (d == 2)
			{
				*(UINT16 *)&ioreg[a & 0xff] = d;
				#ifdef NET_DEBUG
				printf("Netboard W16\tioreg[%x] <- %x\t\t", a & 0xff, d);
				#endif
			}

			if (d > 2) 
			{
				#ifdef NET_DEBUG
				printf("d=%x\n", d);
				#endif
				*(UINT16 *)&ioreg[a & 0xff] = d;
				//MessageBox(NULL, "d > 1", NULL, MB_OK);
				//MessageBeep(MB_ICONWARNING);
			}
			
			#ifdef NET_DEBUG
			printf("d = %x \n",d);
			#endif
			M68KSetIRQ(4); // network error if removed
			M68KRun(1000); // 1000 is enought
			M68KSetIRQ(2); // oui sinon pas de trame, pas de cycle sinon crash	
			break;

		case 0x8a: // ioreg[c008a]
			*(UINT16 *)&ioreg[a & 0xff] = d;
			#ifdef NET_DEBUG
			printf("Netboard W16\tioreg[%x] <- %x\t", a & 0xff, d);
			printf("d = %x\n",d);
			#endif
			break;

		default:
			printf("unknown c00(%x)\n", a & 0xff);
			MessageBox(NULL, "Unknown W16 IOREG", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
			break;

		}
		//M68KSetIRQ(2); // oui sinon pas de trame, pas de cycle sinon crash	
		//M68KRun(40000);
		break;

	default:
		printf("NetBoard 68K: Unknown W16 (%x) %06X<-%04X\n", (a >> 16) & 0xF,a, d);
		MessageBox(NULL, "Unknown W16", NULL, MB_OK); 
		//MessageBeep(MB_ICONWARNING);
		break;
	}
}

void CNetBoard::Write32(UINT32 a, UINT32 d)
{
	switch ((a >> 16) & 0xF)
	{
	case 0x0:
		//printf("Netboard Write32 (0x0) \tRAM[%x] <- %x\n", a, d);
		if (a > 0x0ffff)
		{
			printf("OUT OF RANGE RAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		*(UINT16 *)&RAM[a] = (d >> 16);
		*(UINT16 *)&RAM[a + 2] = (d & 0xFFFF);
		break;

	/*case 0x4: // not used
		printf("Netboard W32\tctrlrw[%x] <- %x\n", a, d);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ctrlrw[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		*(UINT16 *)&ctrlrw[a] = (d >> 16);
		*(UINT16 *)&ctrlrw[a + 2] = (d & 0xFFFF);
		break;*/

	case 0x8: // dirt devils
		//if (((a & 0xffff) > 0 && (a & 0xffff) < 0xff) || ((a & 0xffff) > 0xefff && (a & 0xffff) < 0xffff)) printf("Netboard W32\tCommRAM[%x] <- %x\n", a & 0xffff, d);
		if ((a & 0x3ffff) > 0xffff)
		{
			printf("OUT OF RANGE CommRAM[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}

		*(UINT16 *)&CommRAM[a & 0xffff] = (d >> 16);
		*(UINT16 *)&CommRAM[(a & 0xffff) + 2] = (d & 0xFFFF);
		break;

	/*case 0xc: // not used
		printf("Netboard W32\tioreg[%x] <- %x\n", a, d);
		if ((a & 0xfff) > 0xff)
		{
			printf("OUT OF RANGE ioreg[%x]\n", a);
			MessageBox(NULL, "Out of Range", NULL, MB_OK);
			//MessageBeep(MB_ICONWARNING);
		}
		*(UINT16 *)&ioreg[a] = (d >> 16);
		*(UINT16 *)&ioreg[a + 2] = (d & 0xFFFF);

		//M68KSetIRQ(2);
		//M68KRun(40000); // just some cycles for now
		break;*/

	default:
		printf("NetBoard 68K: Unknown W32 (%x) %08X<-%08X\n", (a >> 16) & 0xF,a, d);
		MessageBox(NULL, "Unknown W32", NULL, MB_OK); 
		//MessageBeep(MB_ICONWARNING);
		break;
	}

}


#define MEMORY_POOL_SIZE	0x40000 // contiguous, not sure
#define OFFSET_COMMRAM		0x0 // size 256kb 0x80000-0xbffff

bool CNetBoard::Init(UINT8 * netRAMPtr, UINT8 *netBufferPtr)
{
	netRAM = netRAMPtr;
	netBuffer = netBufferPtr;
	m_attached = true;
	test_irq = 0;

	// Allocate all memory for RAM
	memoryPool = new(std::nothrow) UINT8[MEMORY_POOL_SIZE];

	if (NULL == memoryPool)
	{
		printf("error mem\n");
		return ErrorLog("Insufficient memory for net board");
	}
	memset(memoryPool, 0, MEMORY_POOL_SIZE);


	//Buffer = netBuffer;
	RAM = netRAM;

	//////////////////////////////////////////////////////////////
	//CommRAM = &memoryPool[OFFSET_COMMRAM]; // not linked (not good)
	CommRAM = netBuffer; // linked (good)
	//////////////////////////////////////////////////////////////

	// only for swap test
	/*bank = new UINT8[0x10000];
	if(NULL == bank)
	{
		printf("error mem bank\n");
		return ErrorLog("Insufficient memory for net board");
	}
	memset(bank, 0, 0x10000);*/

	// control register alloc
	ct = new UINT8[0x100];
	if (NULL == ct)
	{
		printf("error mem ct\n");
		return ErrorLog("Insufficient memory for net board ct");
	}
	memset(ct, 0, 0x100);

	//CommRAM = bank;
	Buffer = CommRAM; // for swap test

	ioreg = netBuffer + 0x10000;
	
	ctrlrw = ct;

	printf("Init netboard netbuffer=%x netram=%x CommRAM=%x ioreg=%x ctrlrw=%x bank=%x\n", netBuffer, netRAM, CommRAM, ioreg, ctrlrw,bank);


	// Initialize 68K core
	M68KSetContext(&M68K);
	M68KInit();
	M68KAttachBus(this);
	M68KSetIRQCallback(NetIRQAck);
	//M68KSetIRQCallback(NULL);
	M68KGetContext(&M68K);
	//Net_SetCB(NET68KRunCallback, NET68KIRQCallback);


	//netsocks
	port_in = m_config["port_in"].ValueAs<unsigned>();
	port_out = m_config["port_out"].ValueAs<unsigned>();
	addr_out = m_config["addr_out"].ValueAs<std::string>();
	udpReceive.Bind(port_in);

	return OKAY;
}

CNetBoard::CNetBoard(const Util::Config::Node &config) : m_config(config)
{
	memoryPool	= NULL;
	netRAM		= NULL;
	netBuffer	= NULL;
	Buffer		= NULL;
	RAM			= NULL;
	CommRAM		= NULL;
	ioreg		= NULL;
	ctrlrw		= NULL;
	bank		= NULL;

	CodeReady	= false;
	test_irq	= 0;

	int5		= false;
}

CNetBoard::~CNetBoard(void)
{
	if (memoryPool != NULL)
	{
		delete[] memoryPool;
		memoryPool = NULL;
	}

	netRAM = NULL;
	netBuffer = NULL;
	Buffer = NULL;
	RAM = NULL;
	CommRAM = NULL;
	ioreg = NULL;
	ctrlrw = NULL;

	if (bank != NULL)
	{
		delete bank;
		bank = NULL;
	}

	if (ct != NULL)
	{
		delete ct;
		ct = NULL;
	}
	
	/*if (int5 == true)
	{
		int5 = false;
		interrupt5.join();
	}*/
}

void CNetBoard::SaveState(CBlockFile * SaveState)
{
}

void CNetBoard::LoadState(CBlockFile * SaveState)
{
}

bool CNetBoard::RunFrame(void)
{
	if (!CodeReady)
	{
		return true;
	}
	
	
	
	
	M68KSetContext(&M68K);
	
	/*if (int5 == false)
	{
		int5 = true;
		interrupt5 = std::thread([this] { inter5(); });
	}*/

	
	M68KSetIRQ(5); // apparently, must be called every xx milli secondes or every frames or 3-4 in a frame
	/*if (test_irq == 0 || test_irq<2)
	{
		test_irq ++;
	}
	else
	{
		test_irq = 0;
		M68KSetIRQ(5);
		//M68KRun(10000);
	}*/

	M68KRun((4000000 / 60)); // original
	//M68KRun((4000000 / 60)*3); // 12Mhz
	
	//printf("NetBoard PC=%06X\n", M68KGetPC());

	//M68KSetIRQ(6);
	//M68KRun(10000);
	
	// 3 times more avoid network error canceled on certain games (certainly due to irq5 that would be calling 3-4 times in a frame)
	M68KSetIRQ(5);
	M68KRun((4000000 / 60));
	M68KSetIRQ(5);
	M68KRun((4000000 / 60));
	M68KSetIRQ(5);
	M68KRun((4000000 / 60));


	M68KGetContext(&M68K);

	return true;
}

void CNetBoard::Reset(void)
{
	/*********************************************************************************************/
	
	commbank = 0;
	recv_offset=0;
	recv_size=0;
	send_offset=0;
	send_size=0;
	
	
	// uncomment to dump network memory for analyse with IDA or 68k disasm
	/*FILE *test;
	Util::FlipEndian16(netRAM, 0x8000); //flip endian for IDA dump only
	test = fopen("netram.bin", "wb");
	fwrite(netRAM,0x4000,1,test);
	fclose(test);
	Util::FlipEndian16(netRAM, 0x8000);*/

	
	M68KSetContext(&M68K);
	printf("RESET NetBoard PC=%06X\n", M68KGetPC());
	M68KReset();

	M68KGetContext(&M68K);
	
}

M68KCtx * CNetBoard::GetM68K(void)
{
	return &M68K;
}

bool CNetBoard::IsAttached(void)
{
	return m_attached;
}



