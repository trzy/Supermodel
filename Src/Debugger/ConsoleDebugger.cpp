#ifdef SUPERMODEL_DEBUGGER

#include "ConsoleDebugger.h"
#include "CPUDebug.h"
#include "CodeAnalyser.h"
#include "Label.h"

#include <string.h>

namespace Debugger
{
	CConsoleDebugger::CConsoleDebugger() : CDebugger(), m_listDism(0), m_listMem(0), m_analyseCode(true), 
		m_addrFmt(HexDollar), m_portFmt(Decimal), m_dataFmt(HexDollar),
		m_showLabels(true), m_labelsOverAddr(true), m_showOpCodes(false), m_memBytesPerRow(12)
	{
		//
	}

	void CConsoleDebugger::WaitCommand(CCPUDebug *cpu)
	{
		m_cpu = cpu;

		UINT32 pc = m_cpu->pc;
		m_listDism = (m_cpu->instrCount > 0 && pc > 10 * m_cpu->minInstrLen ? pc - 10 * m_cpu->minInstrLen : 0);

		char bpChr;
		char addrStr[20];
		char labelStr[13];
		char opCodes[50];
		int codesLen;
		char mnemonic[255];
		char operands[255];
		char cmd[255];
		
		for (;;)
		{
			// Get code analyser and if available analyse code now if required
			if (m_analyseCode)
			{
				CCodeAnalyser *analyser = m_cpu->GetCodeAnalyser();
				if (analyser->NeedsAnalysis())
				{
					printf("Analysing %s...\n", m_cpu->name);
					analyser->AnalyseCode();
				}
			}
		
			// Get details for current PC address
			bool hasLabel;
			if (m_cpu->instrCount > 0)
			{
				m_cpu->FormatAddress(addrStr, pc);
				hasLabel = GetLabelText(labelStr, 12, pc);
				codesLen = m_cpu->Disassemble(pc, mnemonic, operands);
				FormatOpCodes(opCodes, pc, abs(codesLen));
			}
			else
			{
				addrStr[0] = '-';
				addrStr[1] = '\0';
				hasLabel = false;
				labelStr[0] = '\0';
				opCodes[0] = '-';
				opCodes[1] = '\0';
				codesLen = 0;
			}
			CBreakpoint *bp = m_cpu->GetBreakpoint(pc);
			bpChr = (bp != NULL ? bp->symbol : ' ');
		
			// Output command prompt
			printf("%s%c", m_cpu->name, bpChr);
			if (m_showLabels)
			{
				if (m_labelsOverAddr)
					printf("%-12s ", (hasLabel ? labelStr : addrStr));
				else
					printf("%s %-12s ", addrStr, labelStr);
			}
			else
				printf("%s ", addrStr);
			if (m_showOpCodes) 
				printf("[%s] ", opCodes);
			if (codesLen > 0)
				printf("%-*s %s > ", (int)m_cpu->maxMnemLen, mnemonic, operands);
			else
				printf("??? > ");
			fflush(stdout);

			// Wait for command
			gets(cmd);

			if (cmd[0] == '\0')
			{
				m_cpu->SetStepMode(StepInto);
				break;
			}

			char *token = strtok(cmd, " ");
			if (ProcessToken(token, cmd))
				break;

			pc = m_cpu->pc;
		}
	}

	// TODO - tidy up this function - it is a mess!
	bool CConsoleDebugger::ProcessToken(const char *token, const char *cmd)
	{
		UINT32 pc = m_cpu->pc;

		UINT32 addr;
		UINT16 portNum;
		char addrStr[50];
		char portNumStr[50];
		if (CheckToken(token, "x", "exit"))							// exit
		{
			puts("Exiting...");
			SetExit();
			return true;
		}
		else if (CheckToken(token, "n", "next"))					// next [COUNT]
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				int count;
				if (!ParseInt(token, &count) || count <= 0)
				{
					puts("Enter a valid instruction count.");
					return false;
				}
				
				if (count > 1)
					printf("Running %d instructions.\n", count);
				m_cpu->SetCount(count);
			}
			else
				m_cpu->SetStepMode(StepInto);
			return true;
		}
		else if (CheckToken(token, "s", "stepover"))				// stepover
		{
			m_cpu->SetStepMode(StepOver);
			return true;
		}
		else if (CheckToken(token, "si", "stepinto"))				// stepinto
		{
			m_cpu->SetStepMode(StepInto);
			return true;
		}
		else if (CheckToken(token, "so", "stepout"))				// stepout
		{
			m_cpu->SetStepMode(StepOut);
			return true;
		}
		else if (CheckToken(token, "c", "continue"))				// continue [ADDR]
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (!ParseAddress(m_cpu, token, &addr))
				{
					puts("Enter a valid address.");
					return false;
				}

				m_cpu->FormatAddress(addrStr, addr);
				printf("Continuing until %s.\n", addrStr);
				m_cpu->SetUntil(addr);
			}
			else
				m_cpu->SetContinue();
			return true;
		}
		else if (CheckToken(token, "lc", "listcpus"))				// listcpus
		{
			ListCPUs();
		}
		else if (CheckToken(token, "sc", "switchcpu"))				// switchcpu (NAME|NUM)
		{	
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing CPU name or number.");
				return false;
			}

			CCPUDebug *cpu = NULL;
			int cpuNum;
			if (ParseInt(token, &cpuNum))
			{
				if (cpuNum >= 0 && cpuNum < (int)cpus.size())
					cpu = cpus[cpuNum];
			}
			if (cpu == NULL)
			{
				cpu = GetCPU(token);
				if (cpu == NULL)
				{
					puts("No CPU with that name or number.");
					return false;
				}
			}
			
			m_cpu = cpu;
			pc = cpu->pc;
			m_listDism = (cpu->instrCount > 0 && pc > 10 * cpu->minInstrLen ? pc - 10 * cpu->minInstrLen : 0);
			return false;
		}
		else if (CheckToken(token, "lr", "listregisters"))			// listregisters
		{
			ListRegisters();
		}
		else if (CheckToken(token, "le", "listexceptions"))			// listexceptions
		{
			ListExceptions();
		}
		else if (CheckToken(token, "li", "listinterrupts"))			// listinterrupts
		{
			ListInterrupts();
		}
		else if (CheckToken(token, "lo", "listios"))				// listios
		{
			ListIOs();
		}
		else if (CheckToken(token, "ln", "listregions"))			// listregions
		{
			ListRegions();
		}
		else if (CheckToken(token, "al", "addlabel"))				// addlabel ADDR NAME
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing label address.");
				return false;
			}
			else if (!ParseAddress(m_cpu, token, &addr))
			{
				puts("Enter a valid label address.");
				return false;
			}

			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing label name.");
				return false;
			}
			const char *name = token;
			
			// Add label
			CLabel *label = m_cpu->AddLabel(addr, name);
			m_cpu->FormatAddress(addrStr, label->addr);
			printf("Label %s added at %s.\n", label->name, addrStr);
		}
		else if (CheckToken(token, "ll", "listlabels"))				// listlabels
		{
			// Parse arguments
			token = strtok(NULL, " ");

			bool customLabels;
			bool autoLabels;
			if (token != NULL)
			{
				if (CheckToken(token, "a", "all"))
				{
					customLabels = true;
					autoLabels = true;
				}
				else if (CheckToken(token, "c", "custom"))
				{
					customLabels = true;
					autoLabels = false;
				}
				else if (CheckToken(token, "u", "auto"))
				{
					customLabels = false;
					autoLabels = true;
				}
				else
				{
					puts("Enter a valid mode (a)ll, (c)ustom or a(u)to.");
					return false;
				}
			}
			else
			{
				customLabels = true;
				autoLabels = true;
			}

			ELabelFlags autoLabelFlags = (autoLabels ? (ELabelFlags)(LFEntryPoint | LFExcepHandler | LFInterHandler | LFSubroutine) : LFNone);
			ListLabels(customLabels, autoLabelFlags);
		}
		else if (CheckToken(token, "rl", "removelabel"))
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing mode (a)ll, (n)ame or a(d)dress.");
				return false;
			}

			if (CheckToken(token, "a", "all"))
			{
				m_cpu->RemoveAllLabels();
				puts("All labels removed.");
			}
			else if (CheckToken(token, "n", "name"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing label name.");
					return false;
				}

				// Remove label
				if (m_cpu->RemoveLabel(token))
					printf("Label [%s] removed.\n", token);
				else
					puts("Enter a valid label name.");
			}
			else if (CheckToken(token, "d", "address"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing label address.");
					return false;
				}
				if (!ParseAddress(m_cpu, token, &addr))
				{
					puts("Enter a valid label address.");
					return false;
				}

				// Remove label
				m_cpu->FormatAddress(addrStr, addr);
				if (m_cpu->RemoveLabel(addr))
					printf("Label at address %s removed.\n", addrStr);
				else
					printf("No label at address %s.\n", addrStr);
			}
			else
			{
				puts("Enter a valid mode (a)ll, (n)ame or a(d)dress.");
				return false;
			}
		}
		else if (CheckToken(token, "ac", "addcomment"))				// addcomment ADDR TEXT...
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing comment address.");
				return false;
			}
			else if (!ParseAddress(m_cpu, token, &addr))
			{
				puts("Enter a valid comment address.");
				return false;
			}

			char text[255];
			text[0] = '\0';
			token = strtok(NULL, " ");
			while (token != NULL)
			{
				size_t len = strlen(text);
				if (len + strlen(token) > 253)
					break;
				if (len > 0)
					strcat(text, " ");
				strcat(text, token);
				token = strtok(NULL, " ");
			}
			if (text[0] == '\0')
			{
				puts("Missing comment text.");
				return false;
			}

			// Add comment
			CComment *comment = m_cpu->AddComment(addr, text);
			m_cpu->FormatAddress(addrStr, comment->addr);
			printf("Comment added at %s.\n", addrStr);
		}
		else if (CheckToken(token, "rc", "removecomment"))			// removecomment ADDR
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing comment address.");
				return false;
			}
			else if (!ParseAddress(m_cpu, token, &addr))
			{
				puts("Enter a valid comment address.");
				return false;
			}

			m_cpu->FormatAddress(addrStr, addr);
			if (m_cpu->RemoveComment(addr))
				printf("Comment at address %s removed.\n", addrStr);
			else
				printf("No comment at address %s.\n", addrStr);
		}
		else if (CheckToken(token, "t", "trap") ||					// addtrap (e|i) ID
			CheckToken(token, "at", "addtrap"))
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing type (e)xception or (i)interrupt");
				return false;
			}
			
			if (CheckToken(token, "e", "exception"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing exception id.");
					return false;
				}
				CException *ex = m_cpu->GetException(token);
				if (ex == NULL)
				{
					puts("Enter a valid exception id.");
					return false;
				}

				if (!ex->trap)
				{
					ex->trap = true;
					printf("Trap added for exceptions of type %s (%s).\n", ex->id, ex->name);
				}
			}
			else if (CheckToken(token, "i", "interrupt"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing interrupt id.");
					return false;
				}
				CInterrupt *in = m_cpu->GetInterrupt(token);
				if (in == NULL)
				{
					puts("Enter a valid interrupt id.");
					return false;
				}

				if (!in->trap)
				{
					in->trap = true;
					printf("Trap added for interrupts of type %s (%s).\n", in->id, in->name);
				}
			}
			else
			{
				puts("Enter valid type (e)xception or (i)interrupt.");
				return false;
			}
		}
		else if (CheckToken(token, "rt", "removetrap"))				// removetrap (e|i) (a|i) ID
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing type (e)xception or (i)interrupt");
				return false;
			}
			
			if (CheckToken(token, "e", "exception"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing mode (a)ll or (i)d.");
					return false;
				}

				if (CheckToken(token, "a", "all"))
				{
					bool removed = false;
					for (vector<CException*>::iterator it = m_cpu->exceps.begin(); it != m_cpu->exceps.end(); it++)
					{
						if ((*it)->trap)
							(*it)->trap = false;
						removed = true;
					}
					if (removed)
						puts("All exception traps removed.");
				}
				else if (CheckToken(token, "i", "id"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing exception id.");
						return false;
					}

					CException *ex = m_cpu->GetException(token);
					if (ex != NULL)
					{
						if (ex->trap)
						{
							ex->trap = false;
							printf("Trap for exception %s removed.\n", ex->name);
						}
					}
					else
						puts("Enter a valid exception id.");
				}
				else
				{
					puts("Enter valid mode (a)ll or (i)d.");
					return false;
				}
			}
			else if (CheckToken(token, "i", "interrupt"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing mode (a)ll or (i)d.");
					return false;
				}

				if (CheckToken(token, "a", "all"))
				{
					bool removed = false;
					for (vector<CInterrupt*>::iterator it = m_cpu->inters.begin(); it != m_cpu->inters.end(); it++)
					{
						if ((*it)->trap)
							(*it)->trap = false;
						removed = true;
					}
					if (removed)
						puts("All interrupt traps removed.");
				}
				else if (CheckToken(token, "i", "id"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing exception id.");
						return false;
					}

					CInterrupt *in = m_cpu->GetInterrupt(token);
					if (in != NULL)
					{
						if (in->trap)
						{
							in->trap = false;
							printf("Trap for interrupt %s removed.\n", in->name);
						}
					}
					else
						puts("Enter a valid interrupt id.");
				}
				else
				{
					puts("Enter valid mode (a)ll or (i)d.");
					return false;
				}
			}
			else
			{
				puts("Enter valid type (e)xception or (i)interrupt.");
				return false;
			}
		}
		else if (CheckToken(token, "w", "memwatch") ||				// addmemwatch ADDR [(n|r|w|rw) [SIZE [(s|c|m|h|f) [ARGS]]]]
			CheckToken(token, "aw", "addmemwatch"))
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing address.");
				return false;
			}
			if (!ParseAddress(m_cpu, token, &addr))
			{
				puts("Enter a valid address.");
				return false;
			}
			bool read;
			bool write;
			token = strtok(NULL, " ");
			if (token == NULL || CheckToken(token, "n", "none"))
			{
				read = false;
				write = false;
			}
			else if (CheckToken(token, "r", "read"))
			{
				read = true;
				write = false;
			}
			else if (CheckToken(token, "w", "write"))
			{
				read = false;
				write = true;
			}
			else if (CheckToken(token, "rw", "read/write") || CheckToken(token, "wr", "write/read"))
			{
				read = true;
				write = true;
			}
			else
			{
				puts("Enter valid read/write flags (n)one, (r)ead, (w)rite or (rw)ead/write.");
				return false;
			}
			unsigned size;
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (!ParseDataSize(token, size))
					return false;
			}
			else
				size = 1;

			// Add mem watch
			CWatch *watch;
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (CheckToken(token, "s", "simple"))
					watch = m_cpu->AddSimpleMemWatch(addr, size, read, write);
				else if (CheckToken(token, "c", "count"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing watch count.");
						return false;
					}
					int count;
					if (!ParseInt(token, &count) || count <= 0)
					{
						puts("Enter a valid watch count.");
						return false;
					}
					watch = m_cpu->AddCountMemWatch(addr, size, read, write, count);
				}
				else if (CheckToken(token, "m", "match"))
				{
					vector<UINT64> dataSeq;
					while ((token = strtok(NULL, " ")) != NULL)
					{
						UINT64 data;
						if (m_cpu->ParseData(token, size, &data))
							dataSeq.push_back(data);
					}
					if (dataSeq.size() == 0)
					{
						const char *sizeStr = GetSizeString(size);
						printf("Enter a sequence of %s data to match.", sizeStr);
						return false;
					}
					watch = m_cpu->AddMatchMemWatch(addr, size, read, write, dataSeq);
				}
				else if (CheckToken(token, "r", "capture"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing maximum capture length.");
						return false;
					}
					int maxLen;
					if (!ParseInt(token, &maxLen) || maxLen <= 0)
					{
						puts("Enter a valid maximum capture length.");
						return false;
					}
					watch = m_cpu->AddCaptureMemWatch(addr, size, read, write, maxLen);
				}
				else if (CheckToken(token, "p", "print"))
					watch = m_cpu->AddPrintMemWatch(addr, size, read, write);
				else
				{
					puts("Enter a valid watch type (s)imple, (c)ount, (m)atch, captu(r)e or (p)rint.");
					return false;
				}
			}
			else
				watch = m_cpu->AddSimpleMemWatch(addr, size, read, write);	
			m_cpu->FormatAddress(addrStr, watch->addr);
			printf("Memory watch [%s] added at %s.\n", watch->type, addrStr);
		}
		else if (CheckToken(token, "lw", "listmemwatches"))
		{
			ListMemWatches();
		}
		else if (CheckToken(token, "rw", "removememwatch"))			// removememwatch (a|n|d) [NUM|ADDR]
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing mode (a)ll, (n)umber or a(d)dress.");
				return false;
			}
			
			if (CheckToken(token, "a", "all"))
			{
				// Remove all memory watches
				vector<CWatch*> watches;
				GetAllMemWatches(watches);
				for (vector<CWatch*>::iterator it = watches.begin(); it != watches.end(); it++)
					m_cpu->RemoveWatch(*it);
				puts("All memory watches removed.");
			}
			else if (CheckToken(token, "n", "number"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing watch number.");
					return false;
				}

				// Remove memory watch by number
				vector<CWatch*> watches;
				GetAllMemWatches(watches);
				int number;
				if (ParseInt(token, &number) && number >= 0 && number < watches.size())
				{
					m_cpu->RemoveWatch(watches[number]);
					printf("Memory watch %d removed.\n", number);
				}
				else
					puts("Enter a valid watch number.");
			}
			else if (CheckToken(token, "d", "address"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing address.");
					return false;
				}
				if (!ParseAddress(m_cpu, token, &addr))
				{
					puts("Enter a valid address.");
					return false;
				}

				// Remove watch
				m_cpu->FormatAddress(addrStr, addr);
				if (m_cpu->RemoveMemWatch(addr, 1))
					printf("Memory watch at address %s removed.\n", addrStr);
				else
					printf("No memory watch at address %s.\n", addrStr);
			}
			else
			{
				puts("Enter a valid mode (a)ll, (n)umber or a(d)dress.");
				return false;
			}
		}
		else if (CheckToken(token, "pw", "portwatch") ||			// addportwatch PORT [(i|o|io) [(s|c|m|h|f) [ARGS]]]
			CheckToken(token, "apw", "addportwatch"))
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing port number.");
				return false;
			}
			if (!m_cpu->ParsePortNum(token, &portNum))
			{
				puts("Enter a valid port number.");
				return false;
			}
			bool input = false;
			bool output = false;
			token = strtok(NULL, " ");
			if (token == NULL || CheckToken(token, "n", "none"))
			{
				input = false;
				output = false;
			}
			else if (CheckToken(token, "i", "input"))
			{
				input = true;
				output = false;
			}
			else if (CheckToken(token, "o", "output"))
			{
				input = false;
				output = true;
			}
			else if (CheckToken(token, "io", "input/output") || CheckToken(token, "oi", "output/input"))
			{
				input = true;
				output = true;
			}
			else
			{
				puts("Enter valid input/output flags (n)one, (i)nput, (o)utput or (io)nput/output.");
				return false;
			}

			// Add watch
			CPortIO *port = m_cpu->GetPortIO(portNum);
			CWatch *watch;
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (CheckToken(token, "s", "simple"))
					watch = port->AddSimpleWatch(input, output);
				else if (CheckToken(token, "c", "count"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing watch count.");
						return false;
					}
					int count;
					if (!ParseInt(token, &count) || count <= 0)
					{
						puts("Enter a valid watch count.");
						return false;
					}
					watch = port->AddCountWatch(input, output, count);
				}
				else if (CheckToken(token, "m", "match"))
				{
					vector<UINT64> dataSeq;
					while ((token = strtok(NULL, " ")) != NULL)
					{
						UINT64 data;
						if (m_cpu->ParseData(token, port->dataSize, &data))
							dataSeq.push_back(data);
					}
					if (dataSeq.size() == 0)
					{
						printf("Enter a sequence of %s to match.", GetSizeString(port->dataSize));
						return false;
					}
					watch = port->AddMatchWatch(input, output, dataSeq);
				}
				else if (CheckToken(token, "r", "capture"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing maximum capture length.");
						return false;
					}
					int maxLen;
					if (!ParseInt(token, &maxLen) || maxLen <= 0)
					{
						puts("Enter a valid maximum capture length.");
						return false;
					}
					watch = port->AddCaptureWatch(input, output, maxLen);
				}
				else if (CheckToken(token, "p", "print"))
					watch = port->AddPrintWatch(input, output);
				else
				{
					puts("Enter a valid I/O watch type (s)imple, (c)ount, (m)atch, captu(r)e or (p)rint.");
					return false;
				}
			}
			else
				watch = port->AddSimpleWatch(input, output);	

			m_cpu->FormatPortNum(portNumStr, portNum);
			printf("Port watch (%s) added for port %u.\n", watch->type, portNumStr);
		}
		else if (CheckToken(token, "lpw", "listportwatches"))		// listportwatches
		{
			ListPortWatches();
		}
		else if (CheckToken(token, "rpw", "removeportwatch"))		// removeportwatch (a|n|p) [NUM|PORT]
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing mode (a)ll, (n)umber or (p)ort.");
				return false;
			}
			
			if (CheckToken(token, "a", "all"))
			{
				// Remove all port watches
				vector<CWatch*> watches;
				GetAllPortWatches(watches);
				for (vector<CWatch*>::iterator it = watches.begin(); it != watches.end(); it++)
					m_cpu->RemoveWatch(*it);
				puts("All port watches removed.");
			}
			else if (CheckToken(token, "n", "number"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing watch number.");
					return false;
				}

				// Remove port watch by number
				vector<CWatch*> watches;
				GetAllPortWatches(watches);
				int number;
				if (ParseInt(token, &number) && number >= 0 && number < watches.size())
				{
					m_cpu->RemoveWatch(watches[number]);
					printf("Port watch %d removed.\n", number);
				}
				else
					puts("Enter a valid port watch number.");
			}
			else if (CheckToken(token, "p", "port"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing port number.");
					return false;
				}
				if (!m_cpu->ParsePortNum(token, &portNum))
				{
					puts("Enter a valid port number.");
					return false;
				}

				// Remove port watch by port number
				CPortIO *port = m_cpu->GetPortIO(portNum);
				m_cpu->FormatPortNum(portNumStr, portNum);
				if (port->RemoveWatch())
					printf("Port watch for port %s removed.\n", portNumStr);
				else
					printf("No port watch for port %s.\n", portNumStr);
			}
			else
			{
				puts("Enter a valid mode (a)ll, (n)umber or (p)ort.");
				return false;
			}
		}
		else if (CheckToken(token, "b", "breakpoint") ||			// addbreakpoint [ADDR [(s|c) [ARGS]]] 
			CheckToken(token, "ab", "addbreakpoint"))
		{
			// Parse arguments
			token = strtok(NULL, " ");
			CBreakpoint *bp;
			if (token != NULL)
			{
				if (!ParseAddress(m_cpu, token, &addr))
				{
					puts("Enter a valid address.");
					return false;
				}
			
				token = strtok(NULL, " ");
				if (token != NULL)
				{
					if (CheckToken(token, "s", "simple"))
						bp = m_cpu->AddSimpleBreakpoint(addr);
					else if (CheckToken(token, "c", "count"))
					{
						token = strtok(NULL, " ");
						if (token == NULL)
						{
							puts("Missing breakpoint count.");
							return false;
						}
						int count;
						if (!ParseInt(token, &count) || count <= 0)
						{
							puts("Enter a valid breakpoint count.");
							return false;
						}

						bp = m_cpu->AddCountBreakpoint(addr, count);
					}
					else
					{
						puts("Enter a valid breakpoint type (s)imple or (c)ount.");
						return false;
					}
				}
				else
					bp = m_cpu->AddSimpleBreakpoint(addr);
			}
			else
			{
				if (m_cpu->instrCount == 0)
				{
					puts("Cannot set breakpoint at current position as processor not ready.");
					return false;
				}
				bp = m_cpu->AddSimpleBreakpoint(pc);
			}

			m_cpu->FormatAddress(addrStr, bp->addr);
			printf("Breakpoint [%s] added at %s.\n", bp->type, addrStr);
		}
		else if (CheckToken(token, "lb", "listbreakpoints"))		// listbreakpoints
		{
			ListBreakpoints();
		}
		else if (CheckToken(token, "rb", "removebreakpoint"))		// removebreakpoint [(a|n|d) [NUM|ADDR]]
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (CheckToken(token, "a", "all"))
				{
					m_cpu->RemoveAllBreakpoints();
					puts("All breakpoints removed.");
				}
				else if (CheckToken(token, "n", "number"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing breakpoint number.");
						return false;
					}

					// Remove breakpoint
					int number;
					if (ParseInt(token, &number) && number >= 0 && number < m_cpu->bps.size())
					{
						m_cpu->RemoveBreakpoint(m_cpu->bps[number]);
						printf("Breakpoint %d removed.\n", number);
					}
					else
						puts("Enter a valid breakpoint number.");
				}
				else if (CheckToken(token, "d", "address"))
				{
					token = strtok(NULL, " ");
					if (token == NULL)
					{
						puts("Missing breakpoint address.");
						return false;
					}
					if (!ParseAddress(m_cpu, token, &addr))
					{
						puts("Enter a valid breakpoint address.");
						return false;
					}

					// Remove breakpoint
					m_cpu->FormatAddress(addrStr, addr);
					if (m_cpu->RemoveBreakpoint(addr))
						printf("Breakpoint at address %s removed.\n", addrStr);
					else
						printf("No breakpoint at address %s.\n", addrStr);
				}
				else
					puts("Enter a valid mode (a)ll, (n)umber or a(d)dress.");
			}
			else
			{
				if (m_cpu->RemoveBreakpoint(pc))
					puts("Current breakpoint removed.");
			}
		}
		else if (CheckToken(token, "m", "monitor") ||				// addmonitor REG
			CheckToken(token, "am", "addmonitor"))
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing register name.");
				return false;
			}
			CRegMonitor *regMon = m_cpu->AddRegMonitor(token);
			if (regMon == NULL)
			{
				puts("Enter a valid register name.");
				return false;
			}
			
			printf("Monitor added to register %s.\n", regMon->reg->name);
		}
		else if (CheckToken(token, "lm", "listmonitors"))			// listmonitors
		{
			ListMonitors();
		}
		else if (CheckToken(token, "rm", "removemonitor"))			// removemonitor (a|n) [REG]
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing mode (a)ll or (n)ame.");
				return false;
			}

			if (CheckToken(token, "a", "all"))
			{
				m_cpu->RemoveAllRegMonitors();
				puts("All register monitors removed.");
			}
			else if (CheckToken(token, "n", "name"))
			{
				token = strtok(NULL, " ");
				if (token == NULL)
				{
					puts("Missing register name.");
					return false;
				}

				CRegister *reg = m_cpu->GetRegister(token);
				if (reg != NULL)
				{
					m_cpu->RemoveRegMonitor(reg->name);
					printf("Monitor for register %s removed.\n", reg->name);
				}
				else
					puts("Enter a valid register name.");
			}
			else
				puts("Enter a valid mode (a)ll or (n)ame.");
		}
		else if (CheckToken(token, "l", "list") ||					// listdisassem [START [END]]
			CheckToken(token, "ld", "listdisassem"))
		{
			// Get start address
			UINT32 start;
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (!ParseAddress(m_cpu, token, &start))
				{
					puts("Enter a valid start address.");
					return false;
				}
			}
			else
			{
				// Default is end of last listing
				start = m_listDism;
			}

			// Get end address
			UINT32 end;
			unsigned numInstrs;
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (!ParseAddress(m_cpu, token, &end))
				{
					puts("Enter a valid end address.");
					return false;
				}
				numInstrs = 0xFFFFFFFF;
			}
			else
			{
				// Default is 20 instructions after start
				end = 0xFFFFFFFF;
				numInstrs = 20;
			}

			// List the disassembled code
			m_listDism = ListDisassembly(start, end, numInstrs);
		}
		else if (CheckToken(token, "ly", "listmemory"))				// listmemory [START [END]]
		{
			// Get start address
			UINT32 start;
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (!ParseAddress(m_cpu, token, &start))
				{
					puts("Enter a valid start address.");
					return false;
				}
			}
			else
			{
				// Default is end of last listing
				start = m_listMem;
			}

			// Get end address
			UINT32 end;
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				if (!ParseAddress(m_cpu, token, &end))
				{
					puts("Enter a valid end address.");
					return false;
				}
			}
			else
				// Default is 8 rows after start
				end = start + 8 * m_memBytesPerRow;

			// List the code
			m_listMem = ListMemory(start, end, m_memBytesPerRow);
		}
		else if (CheckToken(token, "p", "print"))					// print EXPR
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{	
				puts("Missing argument.");
				return false;
			}

			UINT64 data;
			char str[255];
			const char *result;
			if (ParseAddress(m_cpu, token, &addr))
			{
				m_cpu->FormatAddress(str, addr, true, LFAll);
				if (stricmp(token, str) == 0)
					m_cpu->FormatAddress(str, addr, false, LFNone);
				result = str;
			}
			else if (m_cpu->ParseData(token, 8, &data))
			{
				m_cpu->FormatData(str, 8, data);
				result = str;
			}
			else 
				result = token;	
			printf("%s = %s\n", token, result);
		}
		else if (CheckToken(token, "pr", "printregister"))			// printregister REG
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing register name.");
				return false;
			}
			CRegister *reg = m_cpu->GetRegister(token);
			if (reg == NULL)
			{
				printf("No register called '%s'.\n", token);
				return false;
			}

			char valStr[50];
			reg->GetValue(valStr);
			printf("Register %s = %s\n", reg->name, valStr);
		}
		else if (CheckToken(token, "sr", "setregister"))			// setregister REG VALUE
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing register name.");
				return false;
			}
			CRegister *reg = m_cpu->GetRegister(token);
			if (reg == NULL)
			{
				printf("No register called '%s'.\n", token);
				return false;
			}
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing value to set.");
				return false;
			}

			if (!reg->SetValue(token))
			{
				printf("Unable to set value of register %s.\n", reg->name);
				return false;
			}

			char valStr[50];
			reg->GetValue(valStr);
			printf("Set register %s to %s.\n", reg->name, valStr);
		}
		else if (CheckToken(token, "py", "printmemory"))			// printmemory ADDR SIZE
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing address.");
				return false;
			}
			if (!ParseAddress(m_cpu, token, &addr))
			{
				puts("Enter a valid address.");
				return false;
			}

			token = strtok(NULL, " ");
			unsigned dataSize = -1;
			if (token != NULL)
			{
				if (!ParseDataSize(token, dataSize))
					return false;
			}
			else
				dataSize = 1;
			
			const char *sizeStr = GetSizeString(dataSize);
			UINT64 data = m_cpu->ReadMem(addr, dataSize);
			char dataStr[50];
			m_cpu->FormatData(dataStr, dataSize, data);
			m_cpu->FormatAddress(addrStr, addr);
			printf("%s data at %s = %s.\n", sizeStr, addrStr, dataStr);
		}
		else if (CheckToken(token, "sy", "setmemory"))				// setmemory ADDR SIZE VALUE
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing address.");
				return false;
			}
			if (!ParseAddress(m_cpu, token, &addr))
			{
				puts("Enter a valid address.");
				return false;
			}
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing a size (b)yte, (w)ord, (l)ong or (v)erylong or number (1, 2, 4 or 8).");
				return false;
			}
			
			unsigned dataSize;
			if (!ParseDataSize(token, dataSize))
				return false;
			
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing value to set.");
				return false;
			}
			const char *sizeStr = GetSizeString(dataSize);
			UINT64 data;
			if (!m_cpu->ParseData(token, dataSize, &data))
			{
				printf("Enter a valid %s value.\n", sizeStr);
				return false;
			}	
			
			m_cpu->WriteMem(addr, dataSize, data);
			char dataStr[50];
			m_cpu->FormatData(dataStr, dataSize, data);
			m_cpu->FormatAddress(addrStr, addr);
			printf("Set %s data at %s to %s.\n", sizeStr, addrStr, dataStr);
		}
		else if (CheckToken(token, "pp", "printio"))				// printio PORT
		{
			// TODO - read I/O
		}
		else if (CheckToken(token, "sp", "sendio"))					// sendio PORT VALUE
		{
			// TODO - send I/O
		}
		else if (CheckToken(token, "cfg", "configure"))				// configure ...
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				// If no arguments, then print out current configuration
				printf("Configuration:\n");
				printf(" %-20s %-12s %s\n", "Code Analysis",     (m_analyseCode ? "On" : "Off"), "(a)nalysis"); 
				printf(" %-20s %-12s %s\n", "Address Format",    GetFmtConfig(m_addrFmt),        "a(d)dressfmt"); 
				printf(" %-20s %-12s %s\n", "Port Format",       GetFmtConfig(m_portFmt),        "(p)ortfmt"); 
				printf(" %-20s %-12s %s\n", "Data Format",       GetFmtConfig(m_dataFmt),        "da(t)afmt");
				printf(" %-20s %-12s %s\n", "Show Labels",       (m_showLabels  ? "On" : "Off"), "show(l)abels");
				printf(" %-20s %-12s %s\n", "Show Opcodes",      (m_showOpCodes ? "On" : "Off"), "show(o)pcodes");
				printf(" %-20s %-12u %s\n", "Mem Bytes Per Row", m_memBytesPerRow,               "mem(b)ytesrow");
				return false;
			}

			if (CheckToken(token, "a", "analysis"))
			{
				token = strtok(NULL, " ");
				SetBoolConfig(token, m_analyseCode);
			}
			else if (CheckToken(token, "d", "addressfmt"))
			{
				token = strtok(NULL, " ");
				SetFmtConfig(token, m_addrFmt);
			}
			else if (CheckToken(token, "p", "portfmt"))
			{
				token = strtok(NULL, " ");
				SetFmtConfig(token, m_portFmt);
			}
			else if (CheckToken(token, "t", "datafmt"))
			{            
				token = strtok(NULL, " ");
				SetFmtConfig(token, m_dataFmt);
			}
			else if (CheckToken(token, "l", "showlabels"))
			{
				token = strtok(NULL, " ");
				SetBoolConfig(token, m_showLabels);
			}
			else if (CheckToken(token, "o", "showopcodes"))
			{
				token = strtok(NULL, " ");
				SetBoolConfig(token, m_showOpCodes);
			}
			else if (CheckToken(token, "b", "membytesrow"))
			{
				token = strtok(NULL, " ");
				SetNumConfig(token, m_memBytesPerRow);
			}
			else
			{
				puts("Enter a valid option (a)nalysis, a(d)dressfmt, (p)ortfmt, da(t)afmt.");
				return false;
			}
		}
		else if (CheckToken(token, "ls", "loadstate"))				// loadstate FILENAME
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing filename.");
				return false;
			}

			if (LoadState(token))
				printf("Debugger state successfully loaded from <%s>\n", token);
			else
				printf("Unable to load debugger state from <%s>\n", token);
		}
		else if (CheckToken(token, "ss", "savestate"))				// savestate FILENAME
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing filename.");
				return false;
			}

			if (SaveState(token))
				printf("Debugger state successfully saved to <%s>\n", token);
			else
				printf("Unable to save debugger state to <%s>\n", token);
		}
		else if (CheckToken(token, "h", "help"))					// help
		{
			const char *fmt = " %-5s %-15s %s\n";
			printf("Debugger Commands:\n");
			printf(fmt, "x",    "exit",           "");
			printf(fmt, "n",    "next",           "[COUNT]");
			printf(fmt, "s",    "stepover",       "");
			printf(fmt, "si",   "stepinto",       "");
			printf(fmt, "so",   "stepout",        "");
			printf(fmt, "si",   "stepinto",       "");
			printf(fmt, "si",   "continue",       "[ADDR]");
			printf(fmt, "lc",   "listcpus",       "");
			printf(fmt, "sc",   "switchcpu",      "(NAME|NUM)");
			printf(fmt, "lr",   "listregisters",  "");
			printf(fmt, "le",   "listexceptions", "");
			printf(fmt, "li",   "listinterrupts", "");
			printf(fmt, "lo",   "listios",        "");
			printf(fmt, "ln",   "listregions",    "");
			printf(fmt, "al",   "addlabel",       "ADDR NAME");
			printf(fmt, "ll",   "listlabels",     "[(a)ll|(c)ustom|a(u)to]");
			printf(fmt, "rl",   "removelabel",    "((a)ll|(n)ame|a(d)dress) [NAME|ADDR]");
			printf(fmt, "ac",   "addcomment",     "ADDR TEXT...");
			printf(fmt, "rc",   "removecomment",  "ADDR");
			printf(fmt, "t/at", "addtrap",        "(e|i) ID");
			// TODO - finish
		}
		else
			printf("Unknown command '%s'.\n", token);
		return false;
	}

	bool CConsoleDebugger::CheckToken(const char *str, const char *simple, const char *full)
	{
		return (stricmp(str, simple) == 0 || stricmp(str, full) == 0);
	}

	void CConsoleDebugger::Truncate(char *dst, size_t maxLen, const char *src)
	{
		strncpy(dst, src, maxLen);
		if (strlen(src) > maxLen)
			strncpy(&dst[maxLen - 3], "...", 3);
		dst[maxLen] = '\0';
	}

	void CConsoleDebugger::FormatOpCodes(char *str, int addr, int codesLen)
	{
		char *p = str;
		int i = 0;
		while (i < codesLen)
		{
			UINT8 opCode = (UINT8)m_cpu->ReadMem(addr++, 1);
			sprintf(p, "%02X", opCode);
			p += 2;
			i++;
		}
		while (i++ < m_cpu->maxInstrLen)
		{
			*p++ = ' ';
			*p++ = ' ';
		}
		*p = '\0';
	}

	bool CConsoleDebugger::GetLabelText(char *str, int maxLen, UINT32 addr)
	{
		char labelStr[255];
				
		CLabel *label = m_cpu->GetLabel(addr);
		if (label != NULL)
		{
			Truncate(str, maxLen, label->name);
			return true;
		}

		if (m_analyseCode)
		{
			CCodeAnalyser *analyser = m_cpu->GetCodeAnalyser();
			CAutoLabel *autoLabel = analyser->analysis->GetAutoLabel(addr);
			if (autoLabel != NULL && autoLabel->GetLabel(labelStr))
			{
				Truncate(str, maxLen, labelStr);		
				return true;
			}
		}
		
		str[0] = '\0';
		return false;
	}

	bool CConsoleDebugger::ParseAddress(CCPUDebug *cpu, const char *str, UINT32 *addr)
	{
		if (cpu->instrCount > 0 && CheckToken(str, "-", "current"))
		{
			*addr = cpu->pc;
			return true;
		}
		return cpu->ParseAddress(str, addr);
	}

	bool CConsoleDebugger::ParseDataSize(const char *str, unsigned &dataSize)
	{
		int num = -1;
		ParseInt(str, &num);
		if (CheckToken(str, "b", "byte") || num == 1)
		{
			dataSize = 1;
			return true;
		}
		else if (CheckToken(str, "w", "word") || num == 2)
		{
			dataSize = 2;
			return true;
		}
		else if (CheckToken(str, "l", "long") || num == 4)
		{
			dataSize = 4;
			return true;
		}
		else if (CheckToken(str, "v", "verylong") || num == 8)
		{
			dataSize = 8;
			return true;
		}
		else
		{
			puts("Enter a valid size (b)yte, (w)ord, (l)ong or (v)erylong or a number 1, 2, 4 or 8.");
			return false;
		}
	}

	bool CConsoleDebugger::SetBoolConfig(const char *str, bool &cfg)
	{
		if (str == NULL)
		{
			printf("Current setting: %-12s\n", (cfg ? "On" : "Off"));
			puts("Change setting with (o)n, o(f)f.");
			return false;
		}

		if (CheckToken(str, "o", "on"))
		{
			cfg = true;
			printf("Changed setting: On\n");
			ApplyConfig();
			return true;
		}
		else if (CheckToken(str, "f", "off"))
		{
			cfg = false;
			printf("Changed setting: Off\n");
			ApplyConfig();
			return true;
		}
		else
		{
			puts("Enter a valid setting (o)n, o(f)f.");
			return false;
		}
	}

	bool CConsoleDebugger::SetNumConfig(const char *str, unsigned &cfg)
	{
		if (str == NULL)
		{
			printf("Current setting: %-12u\n", cfg);
			return false;
		}

		int number;
		if (!ParseInt(str, &number))
		{
			puts("Enter a valid number.");
			return false;
		}

		cfg = (unsigned)number;
		printf("Changed setting: %-12u\n", cfg);
		ApplyConfig();
		return true;
	}

	const char *CConsoleDebugger::GetFmtConfig(EFormat fmt)
	{
		switch (fmt)
		{
			case Hex0x:     return "Hex (0x00)";
			case HexDollar: return "Hex ($00)";
			case HexPostH:  return "Hex (00h)";
			case Decimal:   return "Decimal";
			case Binary:    return "Binary";
			default:        return "-";
		}
	}

	bool CConsoleDebugger::SetFmtConfig(const char *str, EFormat &cfg)
	{
		if (str == NULL)
		{
			printf("Current setting: %-12s\n", GetFmtConfig(cfg));
			puts("Change setting with (h)ex, hex(z)ero, hexdo(l)ar, hex(p)osth, (d)ecimal, (b)inary.");
			return false;
		}

		if      (CheckToken(str, "h", "hex"))       cfg = Hex;
		else if (CheckToken(str, "z", "hexzero"))   cfg = Hex0x;
		else if (CheckToken(str, "l", "hexdollar")) cfg = HexDollar;	
		else if (CheckToken(str, "p", "hexposth"))  cfg = HexPostH;
		else if (CheckToken(str, "d", "decimal"))   cfg = Decimal;
		else if (CheckToken(str, "b", "binary"))    cfg = Binary;
		else
		{
			puts("Enter a valid setting (h)ex, hex(z)ero, hexdo(l)ar, hex(p)osth, (d)ecimal, (b)inary.");
			return false;
		}

		printf("Changed setting: %-12s\n", GetFmtConfig(cfg));
		ApplyConfig();
		return true;
	}

	void CConsoleDebugger::ListCPUs()
	{
		puts("CPUs:");
		if (cpus.size() == 0)
		{
			puts(" None");
			return;
		}

		int num = 0;
		for (vector<CCPUDebug*>::iterator it = cpus.begin(); it != cpus.end(); it++)
			printf(" %d - %s\n", num++, (*it)->name); 
	}

	void CConsoleDebugger::ListRegisters()
	{
		printf("%s Registers:\n", m_cpu->name);
		if (m_cpu->regs.size() == 0)
		{
			puts(" None");
			return;
		}
		
		// Get groups
		vector<const char*> groups;
		vector<vector<CRegister*> > regsByGroup;
		size_t totalRows = 0;
		for (vector<CRegister*>::iterator it = m_cpu->regs.begin(); it != m_cpu->regs.end(); it++)
		{
			const char *group = (*it)->group;
			// TODO - find with stricmp rather than default find
			if (find(groups.begin(), groups.end(), group) == groups.end())
			{
				groups.push_back(group);
				regsByGroup.resize(regsByGroup.size() + 1);
			}
			int index = find(groups.begin(), groups.end(), group) - groups.begin();
			vector<CRegister*> *pRegsInGroup = &regsByGroup[index];
			pRegsInGroup->push_back(*it);
			totalRows = max<size_t>(totalRows, pRegsInGroup->size());
		}

		// Get max label and value widths in each group and print group headers
		size_t numGroups = groups.size();
		vector<size_t> labelWidths(numGroups);
		vector<size_t> valueWidths(numGroups);
		vector<size_t> groupWidths(numGroups);
		char valStr[50];
		for (size_t index = 0; index < numGroups; index++)
		{
			labelWidths[index] = 0;
			valueWidths[index] = 0;

			vector<CRegister*> *pRegsInGroup = &regsByGroup[index];
			for (vector<CRegister*>::iterator it = pRegsInGroup->begin(); it != pRegsInGroup->end(); it++)
			{
				labelWidths[index] = max<size_t>(labelWidths[index], strlen((*it)->name));
				(*it)->GetValue(valStr);
				valueWidths[index] = max<size_t>(valueWidths[index], strlen(valStr));
			}

			const char *group = groups[index];
			groupWidths[index] = max<size_t>(labelWidths[index] + valueWidths[index] + 3, strlen(group) + 1);
			printf("%-*s", (int)groupWidths[index], group);
		}
		puts("");

		// Print rows of register values
		char rowStr[50];
		for (size_t row = 0; row < totalRows; row++)
		{
			for (size_t index = 0; index < numGroups; index++)
			{
				vector<CRegister*> *pRegsInGroup = &regsByGroup[index];
				if (row < pRegsInGroup->size())
				{
					CRegister *reg = (*pRegsInGroup)[row];
					reg->GetValue(valStr);
					bool hasMon = m_cpu->GetRegMonitor(reg->name) != NULL;

					sprintf(rowStr, "%c%-*s %-*s", (hasMon ? '*' : ' '), (int)labelWidths[index], reg->name, (int)valueWidths[index], valStr);
				}
				else
					rowStr[0] = '\0';

				printf("%-*s", (int)groupWidths[index], rowStr);
			}
			puts("");
		}
	}	

	void CConsoleDebugger::ListExceptions()
	{
		printf("%s Exceptions:\n", m_cpu->name);
		if (m_cpu->exceps.size() == 0)
		{
			puts(" None");
			return;
		}

		char addrStr[20];
		UINT32 handlerAddr;
		for (vector<CException*>::iterator it = m_cpu->exceps.begin(); it != m_cpu->exceps.end(); it++)
		{
			if (m_cpu->GetHandlerAddr(*it, handlerAddr))
				m_cpu->FormatAddress(addrStr, handlerAddr, true, (m_analyseCode ? LFExcepHandler : LFNone));
			else
				addrStr[0] = '\0';
			printf("%c%-12s %-30s %-12s %u\n", ((*it)->trap ? '*' : ' '), (*it)->id, (*it)->name, addrStr, (*it)->count);
		}
	}

	void CConsoleDebugger::ListInterrupts()
	{
		printf("%s Interrupts:\n", m_cpu->name);
		if (m_cpu->inters.size() == 0)
		{
			puts(" None");
			return;
		}

		char addrStr[20];
		UINT32 handlerAddr;
		for (vector<CInterrupt*>::iterator it = m_cpu->inters.begin(); it != m_cpu->inters.end(); it++)
		{
			if (m_cpu->GetHandlerAddr(*it, handlerAddr))
				m_cpu->FormatAddress(addrStr, handlerAddr, true, (m_analyseCode ? LFInterHandler : LFNone));
			else
				addrStr[0] = '\0';
			printf("%c%-12s %-30s %-12s %u\n", ((*it)->trap ? '*' : ' '), (*it)->id, (*it)->name, addrStr, (*it)->count);
		}
	}

	void CConsoleDebugger::ListIOs()
	{
		printf("%s I/O Ports:\n", m_cpu->name);
		if (m_cpu->ios.size() == 0)
		{
			puts(" None");
			return;
		}

		const char *group = NULL;
		char locStr[255];
		char dirChar;
		char inStr[50];
		char outStr[50];
		for (vector<CIO*>::iterator it = m_cpu->ios.begin(); it != m_cpu->ios.end(); it++)
		{
			// Print group heading if starting a new group
			if (group == NULL || stricmp((*it)->group, group) != 0)
			{
				group = (*it)->group;
				printf(" %s:\n", group);
			}

			// Get location string (memory address or port number)
			(*it)->GetLocation(locStr);
			
			// See whether port was last read or written
			if ((*it)->inCount + (*it)->outCount > 0)
				dirChar = ((*it)->last == &(*it)->lastIn ? '<' : '>');
			else
				dirChar = ' ';
			// Formatt last input
			if ((*it)->inCount > 0)
				m_cpu->FormatData(inStr, (*it)->dataSize, (*it)->lastIn);
			else
			{
				inStr[0] = '*';
				inStr[1] = '\0';
			}
			size_t inLen = strlen(inStr);
			int inLPad = 5 - (int)inLen / 2;
			int inRPad = 5 - (int)inLen + (int)inLen / 2;
			// Format last output
			if ((*it)->inCount > 0)
				m_cpu->FormatData(outStr, (*it)->dataSize, (*it)->lastOut);
			else
			{
				outStr[0] = '*';
				outStr[1] = '\0';
			}
			size_t outLen = strlen(outStr);
			int outLPad = 5 - (int)outLen / 2;
			int outRPad = 5 - (int)outLen + (int)outLen / 2;
			
			// Print details
			printf(" %c%-12s %-30s %-8s %*s%s%*s%c%*s%s%*s\n", ((*it)->watch != NULL ? '*' : ' '), locStr, (*it)->name, GetSizeString((*it)->dataSize),
				inLPad, "", inStr, inRPad, "", dirChar, outLPad, "", outStr, outRPad, "");
		}
	}

	void CConsoleDebugger::ListRegions()
	{
		printf("%s Regions:\n", m_cpu->name);
		if (m_cpu->regions.size() == 0)
		{
			puts(" None");
			return;
		}

		char startStr[255];
		char endStr[255];
		for (vector<CRegion*>::iterator it = m_cpu->regions.begin(); it != m_cpu->regions.end(); it++)
		{
			// Format start and end address
			m_cpu->FormatAddress(startStr, (*it)->addr);
			m_cpu->FormatAddress(endStr, (*it)->addrEnd);

			// Print details
			printf("%c%s-%s %s\n", ((*it)->isCode ? '*' : ' '), startStr, endStr, (*it)->name);
		}
	}

	void CConsoleDebugger::ListLabels(bool customLabels, ELabelFlags autoLabelFlags)
	{
		printf("%s Labels:\n", m_cpu->name);
		
		unsigned count = 0;

		char addrStr[20];
		if (customLabels && m_cpu->labels.size() > 0)
		{
			puts(" Custom Labels:");
			for (vector<CLabel*>::iterator it = m_cpu->labels.begin(); it != m_cpu->labels.end(); it++)
			{
				m_cpu->FormatAddress(addrStr, (*it)->addr);
				printf("  %s %s\n", addrStr, (*it)->name);
				count++;
			}
		}

		if (m_analyseCode)
		{
			char labelStr[255];
			CCodeAnalyser *analyser = m_cpu->GetCodeAnalyser();
			for (unsigned index = 0; index < CAutoLabel::numLabelFlags; index++)
			{
				ELabelFlags flag = CAutoLabel::GetLabelFlag(index);
				if (!(autoLabelFlags & flag))
					continue;
				vector<CAutoLabel*> withFlag = analyser->analysis->GetAutoLabels(flag);
				if (withFlag.size() == 0)
					continue;
				const char *flagStr = CAutoLabel::GetFlagString(flag);
				printf(" %ss:\n", flagStr);
				for (vector<CAutoLabel*>::iterator it = withFlag.begin(); it != withFlag.end(); it++)
				{
					if (!(*it)->GetLabel(labelStr, flag))
						continue;
					CBreakpoint *bp = m_cpu->GetBreakpoint((*it)->addr);
					char bpChr = (bp != NULL ? bp->symbol : ' ');
					m_cpu->FormatAddress(addrStr, (*it)->addr);
					printf(" %c%s %s\n", bpChr, addrStr, labelStr);
					count++;
				}
			}
		}

		if (count == 0)
		{
			puts(" None");
			return;
		}
	}

	void CConsoleDebugger::GetAllMemWatches(vector<CWatch*> &watches)
	{
		for (vector<CWatch*>::iterator it = m_cpu->memWatches.begin(); it != m_cpu->memWatches.end(); it++)
			watches.push_back(*it);
		for (vector<CWatch*>::iterator it = m_cpu->ioWatches.begin(); it != m_cpu->ioWatches.end(); it++)
		{
			if (dynamic_cast<CMappedIO*>((*it)->io) != NULL)
				watches.push_back(*it);
		}
	}

	void CConsoleDebugger::ListMemWatches()
	{
		printf("%s Memory Watches:\n", m_cpu->name);

		vector<CWatch*> watches;
		GetAllMemWatches(watches);

		if (watches.size() == 0)
		{
			puts(" None");
			return;
		}

		printf(" %-3s %-8s %-12s %-4s %-20s %s\n", "Num", "Type", "Address", "Trig", "Value", "Info");

		char addrStr[20];
		const char *trigStr;
		char valStr[20];
		char infoStr[255];
		unsigned wNum = 0;
		for (vector<CWatch*>::iterator it = watches.begin(); it != watches.end(); it++)
		{
			m_cpu->FormatAddress(addrStr, (*it)->addr, true);
			if      ((*it)->trigRead && (*it)->trigWrite) trigStr = "R/W";
			else if ((*it)->trigRead)                     trigStr = "R";
			else if ((*it)->trigWrite)                    trigStr = "W";
			else                                          trigStr = "-";
			m_cpu->FormatData(valStr, (*it)->size, (*it)->GetValue());
			if (!(*it)->GetInfo(infoStr))
				infoStr[0] = '\0';
			printf(" %-3u %-8s %-12s %-4s %-20s %s\n", wNum++, (*it)->type, addrStr, trigStr, valStr, infoStr);
		}
	}

	void CConsoleDebugger::GetAllPortWatches(vector<CWatch*> &watches)
	{
		for (vector<CWatch*>::iterator it = m_cpu->ioWatches.begin(); it != m_cpu->ioWatches.end(); it++)
		{
			if (dynamic_cast<CPortIO*>((*it)->io) != NULL)
				watches.push_back(*it);
		}
	}

	void CConsoleDebugger::ListPortWatches()
	{
		printf("%s I/O Port Watches:\n", m_cpu->name);

		vector<CWatch*> watches;
		GetAllPortWatches(watches);

		if (watches.size() == 0)
		{
			puts(" None");
			return;
		}

		printf(" %-3s %-8s %-12s %-4s %-20s %s\n", "Num", "Type", "Location", "Trig", "Last In/Out", "Info");

		char locStr[255];
		const char *trigStr;
		char valStr[20];
		char infoStr[255];
		unsigned wNum;
		for (vector<CWatch*>::iterator it = watches.begin(); it != watches.end(); it++)
		{
			(*it)->io->GetLocation(locStr);
			if      ((*it)->trigRead && (*it)->trigWrite) trigStr = "I/O";
			else if ((*it)->trigRead)                     trigStr = "I";
			else if ((*it)->trigWrite)                    trigStr = "O";
			else                                          trigStr = "-";
			if ((*it)->readCount + (*it)->writeCount == 0)
			{
				valStr[0] = '-';
				valStr[1] = '\0';
			}
			else
				m_cpu->FormatData(valStr, (*it)->size, (*it)->GetValue());
			if (!(*it)->GetInfo(infoStr))
				infoStr[0] = '\0';
			printf(" %-3u %-8s %-12s %-4s %-20s %s\n", wNum++, (*it)->type, locStr, trigStr, valStr, infoStr);
		}
	}

	void CConsoleDebugger::ListBreakpoints()
	{
		printf("%s Breakpoints:\n", m_cpu->name);
		if (m_cpu->bps.size() == 0)
		{
			puts(" None");
			return;
		}

		char addrStr[20];
		char infoStr[255];
		unsigned bpNum = 0;
		for (vector<CBreakpoint*>::iterator it = m_cpu->bps.begin(); it != m_cpu->bps.end(); it++)
		{
			m_cpu->FormatAddress(addrStr, (*it)->addr, true, (m_analyseCode ? LFAll : LFNone));
			if ((*it)->GetInfo(infoStr))
				printf(" %u - %s breakpoint [%s] at %s\n", bpNum++, (*it)->type, infoStr, addrStr);
			else
				printf(" %u - %s breakpoint at %s\n", bpNum++, (*it)->type, addrStr);
		}
	}

	void CConsoleDebugger::ListMonitors()
	{
		printf("%s Register Monitors:\n", m_cpu->name);
		if (m_cpu->regMons.size() == 0)
		{
			puts(" None");
			return;
		}

		char valStr[255];
		int num = 0;
		for (vector<CRegMonitor*>::iterator it = m_cpu->regMons.begin(); it != m_cpu->regMons.end(); it++)
		{
			(*it)->GetBeforeValue(valStr);
			printf(" %d - %s change from: %s\n", num++, (*it)->reg->name, valStr);
		}
	}

	UINT32 CConsoleDebugger::ListDisassembly(UINT32 start, UINT32 end, unsigned numInstrs)
	{
		UINT32 addr;
		char startStr[20];
		char endStr[20];
		char opCodes[50];
		char mnemonic[100];
		char operands[155];
		char addrStr[20];
		char labelStr[13];
		unsigned instr;

		UINT32 pc = m_cpu->pc;		
		CCodeAnalyser *analyser = (m_analyseCode ? m_cpu->GetCodeAnalyser() : NULL);

		// Align address to instruction boundary
		start -= start%m_cpu->minInstrLen;

		if (analyser != NULL)
		{
			// Use code analyser to find valid start address
			if (!analyser->analysis->GetNextValidAddr(start))
				return start;
		}
		else
		{
			// In the absence of code analyser, try to align code with current PC address
			if (m_cpu->instrCount > 0 && pc >= start && pc <= end)
			{
				while (start < end)
				{	
					bool okay = false;
					addr = start;
					instr = 0;
					while (addr < end && instr < numInstrs)
					{
						if (addr == pc)
						{
							okay = true;
							break;
						}
						int codesLen = m_cpu->GetOpLength(addr);
						addr += abs(codesLen);
						instr++;
					}
					if (okay)
						break;
					start += m_cpu->minInstrLen;
				}
			}
		}

		// Get true end address
		addr = start;
		instr = 0;
		while (addr < end && instr < numInstrs)
		{
			// Get instruction length
			int codesLen = m_cpu->Disassemble(addr, mnemonic, operands);
			// Move onto next valid instruction address
			addr += abs(codesLen);
			if (analyser != NULL && !analyser->analysis->GetNextValidAddr(addr))
				break;
			instr++;
		}
		end = addr;
				
		// Format start and end addresses and output title
		m_cpu->FormatAddress(startStr, start);
		m_cpu->FormatAddress(endStr, end);
		printf("%s Code %s - %s:\n", m_cpu->name, startStr, endStr);

		// Output the disassembly
		addr = start;
		instr = 0;
		while (addr < end)
		{
			// Add markers for current PC address and any breakpoints
			char ind[4];
			if (m_cpu->instrCount > 0 && addr == pc)
			{
				ind[0] = '>';
				ind[1] = '>';
			}
			else
			{
				ind[0] = ' ';
				ind[1] = ' ';
			}
			CBreakpoint *bp = m_cpu->GetBreakpoint(addr);
			ind[2] = (bp != NULL ? bp->symbol : ' ');
			ind[3] = '\0';
			
			// Format current address
			m_cpu->FormatAddress(addrStr, addr);

			// Get labels at address (if any)
			bool hasLabel = GetLabelText(labelStr, 12, addr);

			// Get mnemonic, operands and instruction length and print them
			int codesLen = m_cpu->Disassemble(addr, mnemonic, operands);		
			FormatOpCodes(opCodes, addr, abs(codesLen));
			
			// Get comment at address (if any)
			CComment *comment = m_cpu->GetComment(addr);
			
			// Output line
			printf("%s", ind);
			if (m_showLabels)
			{
				if (m_labelsOverAddr)
					printf("%-12s ", (hasLabel ? labelStr : addrStr));
				else
					printf("%s %-12s ", addrStr, labelStr);
			}
			else
				printf("%s ", addrStr);
			if (m_showOpCodes) 
				printf("[%s] ", opCodes);
			if (codesLen > 0)
				printf("%-*s %-20s", (int)m_cpu->maxMnemLen, mnemonic, operands);
			else
				printf("???");
			if (comment != NULL)
				printf(" ; %s", comment->text);
			printf("\n");

			// Move onto next valid instruction address
			addr += abs(codesLen);
			if (analyser != NULL && !analyser->analysis->GetNextValidAddr(addr))
				break;
		}
		return end;
	}

	UINT32 CConsoleDebugger::ListMemory(UINT32 start, UINT32 end, unsigned bytesPerRow)
	{
		char startStr[255];
		char endStr[255];
		char addrStr[20];
		char labelStr[13];
		char wChar, dChar;
		UINT8 data;
		
		// Adjust start/end points to be on row boundary
		start -= (start % bytesPerRow);
		end += bytesPerRow - (end % bytesPerRow);

		// Format start and end addresses and output title
		m_cpu->FormatAddress(startStr, start);
		m_cpu->FormatAddress(endStr, end);
		printf("%s Memory %s - %s:\n", m_cpu->name, startStr, endStr);

		UINT32 addr = start;
		while (addr < end)
		{
			// TODO - check address going out of region or out of range

			// Format current address
			m_cpu->FormatAddress(addrStr, addr);
			
			// Get labels at address (if any)
			bool hasLabel = GetLabelText(labelStr, 12, addr);

			// Output line
			if (m_showLabels)
			{
				if (m_labelsOverAddr)
					printf("   %-12s", (hasLabel ? labelStr : addrStr));
				else
					printf("   %s %-12s", addrStr, labelStr);
			}
			else
				printf("   %s%c", addrStr);
			UINT32 lAddr = addr;
			for (unsigned i = 0; i < bytesPerRow; i++)
			{
				CWatch *watch = m_cpu->GetMemWatch(lAddr, 1);
				// TODO - handling of mapped I/O
				//CMappedIO *io = m_cpu->GetMappedIO(lAddr);
				wChar = (watch != NULL ? watch->symbol : ' ');
				//if (io != NULL)
				//	data = (UINT8)io->last;
				//else
					data = (UINT8)m_cpu->ReadMem(lAddr, 1);
				printf("%c%02X", wChar, data);
				lAddr++;
			}
			printf(" ");
			lAddr = addr;
			for (unsigned i = 0; i < bytesPerRow; i++)
			{
				// TODO - handling of mapped I/O
				//CMappedIO *io = m_cpu->GetMappedIO(lAddr);
				//if (io != NULL)
				//	data = io->last;
				//else
					data = (UINT8)m_cpu->ReadMem(lAddr, 1);
				dChar = (data >= 32 && data <= 126 ? (char)data : '.');
				printf("%c", dChar);
			}
			printf("\n");
			addr += bytesPerRow;
		}
		return addr;
	}

	void CConsoleDebugger::Attach()
	{
		CDebugger::Attach();

		ApplyConfig();

		Attached();
	}

	void CConsoleDebugger::Detach()
	{
		Detaching();

		CDebugger::Detach();
	}

	void CConsoleDebugger::ApplyConfig()
	{
		for (vector<CCPUDebug*>::iterator it = cpus.begin(); it != cpus.end(); it++)
		{
			(*it)->addrFmt = m_addrFmt;
			(*it)->portFmt = m_portFmt;
			(*it)->dataFmt = m_dataFmt;
		}
	}

	void CConsoleDebugger::Attached()
	{
		//
	}

	void CConsoleDebugger::Detaching()
	{
		//
	}

	void CConsoleDebugger::AnalysisUpdated(CCodeAnalyser *analyser)
	{
		//
	}

	void CConsoleDebugger::ExceptionTrapped(CException *ex)
	{
		char addrStr[255];
		ex->cpu->FormatAddress(addrStr, ex->cpu->pc, true, (m_analyseCode ? LFExcepHandler : LFNone));
		printf("Exception %s (%s) on %s trapped at %s.\n", ex->id, ex->name, ex->cpu->name, addrStr);
	}

	void CConsoleDebugger::InterruptTrapped(CInterrupt *in)
	{
		char addrStr[255];
		in->cpu->FormatAddress(addrStr, in->cpu->pc, true, (m_analyseCode ? LFInterHandler : LFNone));
		printf("Interrupt %s (%s) on %s trapped at %s.\n", in->id, in->name, in->cpu->name, addrStr);
	}

	void CConsoleDebugger::MemWatchTriggered(CWatch *watch, UINT32 addr, unsigned dataSize, UINT64 data, bool isRead)
	{
		const char *sizeStr = GetSizeString(dataSize);
		char dataStr[20];
		m_cpu->FormatData(dataStr, dataSize, data);
		const char *rwStr = (isRead ? "read" : "write");
		char addrStr[255];
		char infoStr[255];
		watch->cpu->FormatAddress(addrStr, addr, true);
		if (watch->GetInfo(infoStr))
			printf("%s memory %s (%s) by %s at address %s triggered %s watch.\n", sizeStr, rwStr, dataStr, watch->cpu->name, addrStr, watch->type, infoStr);
		else
			printf("%s memory %s (%s) by %s at address %s triggered %s watch.\n", sizeStr, rwStr, dataStr, watch->cpu->name, addrStr, watch->type);
	}

	void CConsoleDebugger::IOWatchTriggered(CWatch *watch, CIO *io, UINT64 data, bool isInput)
	{
		const char *sizeStr = GetSizeString(io->dataSize);
		char dataStr[20];
		m_cpu->FormatData(dataStr, io->dataSize, data);
		const char *ioStr = (isInput ? "input" : "output");
		const char *tfStr = (isInput ? "from" : "to");
		char locStr[255];
		char infoStr[255];
		watch->io->GetLocation(locStr);
		if (watch->GetInfo(infoStr))
			printf("%s I/O %s (%s) by %s %s %s triggered %s watch [%s].\n", sizeStr, ioStr, dataStr, watch->cpu->name, tfStr, locStr, watch->type, infoStr);
		else
			printf("%s I/O %s (%s) by %s %s %s triggered %s watch.\n", sizeStr, ioStr, dataStr, watch->cpu->name, tfStr, locStr, watch->type);
	}

	void CConsoleDebugger::BreakpointReached(CBreakpoint *bp)
	{
		char addrStr[255];
		char infoStr[255];
		bp->cpu->FormatAddress(addrStr, bp->cpu->pc, true, (m_analyseCode ? LFAll : LFNone));
		if (bp->GetInfo(infoStr))
			printf("%s reached %s breakpoint [%s] at %s.\n", bp->cpu->name, bp->type, infoStr, addrStr);
		else
			printf("%s reached %s breakpoint at %s.\n", bp->cpu->name, bp->type, addrStr);
	}

	void CConsoleDebugger::MonitorTriggered(CRegMonitor *regMon)
	{
		char addrStr[255];
		char valStr[255];
		CRegister *reg = regMon->reg;
		reg->cpu->FormatAddress(addrStr, reg->cpu->pc, true, (m_analyseCode ? LFAll : LFNone));
		reg->GetValue(valStr);
		printf("Register %s of %s has changed value at %s to %s.\n", reg->name, reg->cpu->name, addrStr, valStr);
	}

	void CConsoleDebugger::ExecutionHalted(CCPUDebug *cpu, EHaltReason reason)
	{
		if (reason&HaltUser)
		{
			char addrStr[255];
			cpu->FormatAddress(addrStr, cpu->pc, true, (m_analyseCode ? LFAll : LFNone));
			printf("Execution halted on %s at %s.\n", cpu->name, addrStr);
		}
	}

	void CConsoleDebugger::WriteOut(CCPUDebug *cpu, const char *typeStr, const char *fmtStr, va_list vl)
	{
		if (cpu != NULL)
			printf("%s: ", cpu->name);
		if (typeStr != NULL)
			printf(" %s - ", typeStr);
		vprintf(fmtStr, vl);
	}
		
	void CConsoleDebugger::FlushOut(CCPUDebug *cpu)
	{
		fflush(stdout);
	}
}

#endif  // SUPERMODEL_DEBUGGER
