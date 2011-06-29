#ifdef SUPERMODEL_DEBUGGER

#include "Supermodel.h"

#include "ConsoleDebugger.h"
#include "CPUDebug.h"
#include "Label.h"

#include <string.h>
#include <stdio.h>

namespace Debugger
{
	CSupermodelDebugger::CSupermodelDebugger(::CModel3 *model3, ::CInputs *inputs, ::CLogger *logger) : 
		CConsoleDebugger(), m_model3(model3), m_inputs(inputs), m_logger(logger), 
		m_loadEmuState(false), m_saveEmuState(false), m_resetEmu(false)
	{
		AddCPU(new CPPCDebug());
#ifdef SUPERMODEL_SOUND
		AddCPU(new C68KDebug());
#endif
	}

	void CSupermodelDebugger::WaitCommand(CCPUDebug *cpu)
	{
		m_inputs->GetInputSystem()->UngrabMouse();

		CConsoleDebugger::WaitCommand(cpu);

		m_inputs->GetInputSystem()->GrabMouse();
	}

	bool CSupermodelDebugger::ProcessToken(const char *token, const char *cmd)
	{
		//
		// Emulator
		//
		if (CheckToken(token, "les", "loademustate"))				// loademustate <filename>
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				Print("Missing filename.\n");
				return false;
			}

			strncpy(m_stateFile, token, 254);
			m_stateFile[254] = '\0';
			m_loadEmuState = true;
			return true;
		}
		else if (CheckToken(token, "ses", "saveemustate"))			// saveemustate  <filename>
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				Print("Missing filename.\n");
				return false;
			}

			strncpy(m_stateFile, token, 254);
			m_stateFile[254] = '\0';
			m_saveEmuState = true;
			return true;
		}
		else if (CheckToken(token, "res", "resetemu"))				// resetemu
		{
			m_resetEmu = true;
			return true;
		}
		//
		// Inputs
		//
		else if (CheckToken(token, "lip", "listinputs"))			// listinputs
		{
			ListInputs();
			return false;
		}
		else if (CheckToken(token, "pip", "printinput"))			// printinput (<id>|<label>)
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				Print("Mising input name.\n");
				return false;
			}
			::CInput *input = (*m_inputs)[token];
			if (input == NULL)
			{
				Print("No input with id or label '%s'.\n", token);
				return false;
			}
			if (!InputIsValid(input))
			{
				Print("Input '%s' is not valid for current game.\n", token);
				return false;
			}

			if (!input->IsVirtual())
			{
				char mapTrunc[41];
				Truncate(mapTrunc, 40, input->GetMapping());
				Print("Input %s (%s) [%s] = %04X (%d)\n", input->id, input->label, mapTrunc, input->value, input->value);
			}
			else
				Print("Input %s (%s) = %04X (%d)\n", input->id, input->label, input->value, input->value);
			return false;
		}
		else if (CheckToken(token, "sip", "setinput"))				// setinput (<id>|<label>) <mapping>
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				Print("Mising input id or label.\n");
				return false;
			}
			::CInput *input = (*m_inputs)[token];
			if (input == NULL)
			{
				Print("No input with id or label '%s'.\n", token);
				return false;
			}
			if (!InputIsValid(input))
			{
				Print("Input '%s' is not valid for current game.\n", token);
				return false;
			}
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				Print("Missing mapping to set.\n");
				return false;
			}

			input->SetMapping(token);

			Print("Set input %s (%s) to [%s]\n", input->id, input->label, input->GetMapping());
			return false;
		}
		else if (CheckToken(token, "rip", "resetinput"))            // resetinput (<id>|<label>)
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				Print("Mising input id or label.\n");
				return false;
			}
			::CInput *input = (*m_inputs)[token];
			if (input == NULL)
			{
				Print("No input with id or label '%s'.\n", token);
				return false;
			}
			if (!InputIsValid(input))
			{
				Print("Input '%s' is not valid for current game.\n", token);
				return false;
			}

			input->ResetToDefaultMapping();

			Print("Reset input %s (%s) to [%s]\n", input->id, input->label, input->GetMapping());
			return false;
		}
		else if (CheckToken(token, "cip", "configinput"))			// configinput (<id>|<label>) [(s)et|(a)ppend]
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				Print("Mising input id or label.\n");
				return false;
			}
			CInput *input = (*m_inputs)[token];
			if (input == NULL)
			{
				Print("No input with id or label '%s'.\n", token);
				return false;
			}
			if (!InputIsValid(input))
			{
				Print("Input '%s' is not valid for current game.\n", token);
				return false;
			}
			token = strtok(NULL, " ");
			bool append;
			if (token == NULL || CheckToken(token, "s", "set"))
				append = false;
			else if (CheckToken(token, "a", "append"))
				append = true;
			else 
			{
				Print("Enter a valid mode (s)et or (a)ppend.\n");
				return false;
			}

			Print("Configure input %s [%s]: %s...", input->label, input->GetMapping(), (append ? "Appending" : "Setting"));
			fflush(stdout);	// required on terminals that use buffering
	
			// Configure the input
			if (input->Configure(append, "KEY_ESCAPE"))
				Print(" %s\n", input->GetMapping());
			else
				Print(" [Cancelled]\n");
			return false;
		}
		else if (CheckToken(token, "caip", "configallinputs"))		// configallinputs
		{
			m_inputs->ConfigureInputs(m_model3->GetGameInfo());
			return false;
		}
		//
		// Help
		//
		else if (CheckToken(token, "h", "help"))
		{
			CConsoleDebugger::ProcessToken(token, cmd);

			const char *fmt = "  %-6s %-25s %s\n";
			Print(" Emulator:\n");
			Print(fmt, "les",    "loademustate",           "<filename>");
			Print(fmt, "ses",    "saveemustate",           "<filename>");
			Print(fmt, "res",    "resetemu",               "");
			Print(" Inputs:\n");
			Print(fmt, "lip",    "listinputs",             "");
			Print(fmt, "pip",    "printinput",             "(<id>|<label>)");
			Print(fmt, "sip",    "setinput",               "(<id>|<label>) <mapping>");
			Print(fmt, "rip",    "resetinput",             "(<id>|<label>)");
			Print(fmt, "cip",    "configinput",            "(<id>|<label>) [(s)et|(a)ppend]");
			Print(fmt, "caip",   "configallinputs",        "");   
			return false;
		}
		else
			return CConsoleDebugger::ProcessToken(token, cmd);		
	}

	bool CSupermodelDebugger::InputIsValid(::CInput *input)
	{
		return input->IsUIInput() || (input->gameFlags & m_model3->GetGameInfo()->inputFlags);
	}

	void CSupermodelDebugger::ListInputs()
	{
		Print("Inputs:\n");
		if (m_inputs->Count() == 0)
			Print(" None\n");

		// Get maximum id, label and mapping widths
		size_t idAndLabelWidth = 0;
		size_t mappingWidth    = 0;
		for (unsigned i = 0; i < m_inputs->Count(); i++)
		{
			::CInput *input = (*m_inputs)[i];
			if (!InputIsValid(input))
				continue;

			idAndLabelWidth = max<size_t>(idAndLabelWidth, strlen(input->id) + strlen(input->label) + 3);
			if (!input->IsVirtual())
				mappingWidth = max<size_t>(mappingWidth, strlen(input->GetMapping()));
		}
		mappingWidth = min<size_t>(mappingWidth, 20);
		
		// Print labels, mappings and values for each input
		const char *groupLabel = NULL;
		char idAndLabel[255];
		char mapping[21];
		for (unsigned i = 0; i < m_inputs->Count(); i++)
		{
			::CInput *input = (*m_inputs)[i];
			if (!InputIsValid(input))
				continue;

			if (groupLabel == NULL || stricmp(groupLabel, input->GetInputGroup()) != 0)
			{
				groupLabel = input->GetInputGroup();
				Print(" %s:\n", groupLabel);
			}

			sprintf(idAndLabel, "%s (%s)", input->id, input->label);
			Print("  %-*s", (int)idAndLabelWidth, idAndLabel);
			if (!input->IsVirtual())
				Truncate(mapping, 20, input->GetMapping());
			else
				mapping[0] = '\0';
			Print(" %-*s %04X (%d)\n", (int)mappingWidth, mapping, input->value, input->value);
		}
	}

	void CSupermodelDebugger::Attached()
	{
		CConsoleDebugger::Attached();

		char fileName[25];
		sprintf(fileName, "Debug/%s.ds", m_model3->GetGameInfo()->id);
		LoadState(fileName);
	}

	void CSupermodelDebugger::Detaching()
	{
		char fileName[25];
		sprintf(fileName, "Debug/%s.ds", m_model3->GetGameInfo()->id);
		SaveState(fileName);
		
		CConsoleDebugger::Detaching();
	}

	bool CSupermodelDebugger::LoadModel3State(const char *fileName)
	{
		// Open file and find header 
		CBlockFile state;
		if (state.Load(fileName) != OKAY)
			return false;
		if (state.FindBlock("Debugger Model3 State") != OKAY)
		{
			state.Close();
			return false;
		}

		// Check version in header matches
		unsigned version;
		state.Read(&version, sizeof(version));
		if (version != MODEL3_STATEFILE_VERSION)
		{
			state.Close();
			return false;
		}

		// Load Model3 state
		m_model3->LoadState(&state);
		state.Close();

		// Reset debugger
		Reset();
		return true;
	}

	bool CSupermodelDebugger::SaveModel3State(const char *fileName)
	{
		// Create file with header
		CBlockFile state;
		if (state.Create(fileName, "Debugger Model3 State", __FILE__) != OKAY)
			return false;

		// Write out version in header
		unsigned version = MODEL3_STATEFILE_VERSION;
		state.Write(&version, sizeof(version));

		// Save Model3 state
		m_model3->SaveState(&state);
		state.Close();
		return true;
	}

	void CSupermodelDebugger::ResetModel3()
	{
		// Reset Model3
		m_model3->Reset();

		// Reset debugger
		Reset();
	}

	void CSupermodelDebugger::DebugLog(const char *fmt, va_list vl)
	{
		// Use the supplied logger, if any
		if (m_logger != NULL)
			m_logger->DebugLog(fmt, vl);
		else
			CConsoleDebugger::DebugLog(fmt, vl);
	}
		
	void CSupermodelDebugger::InfoLog(const char *fmt, va_list vl)
	{
		// Use the supplied logger, if any
		if (m_logger != NULL)
			m_logger->InfoLog(fmt, vl);
		else
			CConsoleDebugger::InfoLog(fmt, vl);
	}
	
	void CSupermodelDebugger::ErrorLog(const char *fmt, va_list vl)
	{
		// Use the supplied logger, if any
		if (m_logger != NULL)
			m_logger->ErrorLog(fmt, vl);
		else
			CConsoleDebugger::ErrorLog(fmt, vl);
	}

	void CSupermodelDebugger::Poll()
	{
		CConsoleDebugger::Poll();

		// Load/saving of emulator state and resetting emulator must be done here
		if (m_loadEmuState)
		{
			LoadModel3State(m_stateFile);
			m_loadEmuState = false;
			ForceBreak(false);
		}
		else if (m_saveEmuState)
		{
			SaveModel3State(m_stateFile);
			m_saveEmuState = false;
			ForceBreak(false);
		}
		else if (m_resetEmu)
		{
			ResetModel3();
			m_resetEmu = false;
			ForceBreak(false);
		}
	}
}

#endif  // SUPERMODEL_DEBUGGER