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
		CConsoleDebugger(), m_model3(model3), m_inputs(inputs), m_logger(logger)
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
		if (CheckToken(token, "les", "loademustate"))			// loademustate FILENAME
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing filename.");
				return false;
			}

			if (LoadModel3State(token))
				printf("Emulator state successfully loaded from <%s>\n", token);
			else
				printf("Unable to load emulator state from <%s>\n", token);
			return false;
		}
		else if (CheckToken(token, "ses", "savestate"))				// saveemustate FILENAME
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing filename.");
				return false;
			}

			if (SaveModel3State(token))
				printf("Emulator state successfully saved to <%s>\n", token);
			else
				printf("Unable to save emulator state to <%s>\n", token);
			return false;
		}
		else if (CheckToken(token, "lip", "listinputs"))				// listinputs
		{
			ListInputs();
			return false;
		}
		else if (CheckToken(token, "pip", "printinput"))				// printinput NAME
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Mising input name.");
				return false;
			}
			::CInput *input = (*m_inputs)[token];
			if (input == NULL)
			{
				printf("No input with id or label '%s'.\n", token);
				return false;
			}
			if (!InputIsValid(input))
			{
				printf("Input '%s' is not valid for current game.\n", token);
				return false;
			}

			if (!input->IsVirtual())
			{
				char mapTrunc[41];
				Truncate(mapTrunc, 40, input->GetMapping());
				printf("Input %s (%s) [%s] = %04X (%d)\n", input->id, input->label, mapTrunc, input->value, input->value);
			}
			else
				printf("Input %s (%s) = %04X (%d)\n", input->id, input->label, input->value, input->value);
			return false;
		}
		else if (CheckToken(token, "sip", "setinput"))				// setinput NAME MAPPING
		{
			// Parse arguments
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Mising input name.");
				return false;
			}
			::CInput *input = (*m_inputs)[token];
			if (input == NULL)
			{
				printf("No input with id or label '%s'.\n", token);
				return false;
			}
			if (!InputIsValid(input))
			{
				printf("Input '%s' is not valid for current game.\n", token);
				return false;
			}
			token = strtok(NULL, " ");
			if (token == NULL)
			{
				puts("Missing mapping to set.");
				return false;
			}

			input->SetMapping(token);

			printf("Set input %s (%s) to [%s]\n", input->id, input->label, input->GetMapping());
			return false;
		}
		else if (CheckToken(token, "cip", "configinput"))			// configinput NAME
		{
			//// Parse arguments
			//token = strtok(NULL, " ");
			//if (token == NULL)
			//{
			//	puts("Missing mode (a)ll, (s)et single, (a)ppend single or (r)eset single");
			//	return false;
			//}
			//if (CheckToken("a", "all"))
			//{
			//	//m_inputs->ConfigureInputs();
			//}
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
		puts("Inputs:");
		if (m_inputs->Count() == 0)
			puts(" None");

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
				printf(" %s:\n", groupLabel);
			}

			sprintf(idAndLabel, "%s (%s)", input->id, input->label);
			printf("  %-*s", (int)idAndLabelWidth, idAndLabel);
			if (!input->IsVirtual())
				Truncate(mapping, 20, input->GetMapping());
			else
				mapping[0] = '\0';
			printf(" %-*s %04X (%d)\n", (int)mappingWidth, mapping, input->value, input->value);
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
}

#endif  // SUPERMODEL_DEBUGGER