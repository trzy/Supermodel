#include "model3.h"
#include "expat.h"

enum
{
	ELEMENT_NONE = 1,
	ELEMENT_CONFIG,
	ELEMENT_ROMPATH,
	ELEMENT_GAMELIST,
	ELEMENT_BACKUPPATH,
	ELEMENT_WIDTH,
	ELEMENT_HEIGHT,
	ELEMENT_FULLSCREEN,
	ELEMENT_FPSLIMIT,
	ELEMENT_STRETCH,
};

static STRING_ID element_id[] =
{
	{ "config",				ELEMENT_CONFIG				},
	{ "rom_path",			ELEMENT_ROMPATH				},
	{ "gamelist",			ELEMENT_GAMELIST			},
	{ "backup_path",		ELEMENT_BACKUPPATH			},
	{ "width",				ELEMENT_WIDTH				},
	{ "height",				ELEMENT_HEIGHT				},
	{ "fullscreen",			ELEMENT_FULLSCREEN			},
	{ "fps_limit",			ELEMENT_FPSLIMIT			},
	{ "stretch",			ELEMENT_STRETCH				},
	{ "",					0							},
};

static STRING_ID boolean_id[] =
{
	{ "yes",				TRUE						},
	{ "no",					FALSE						},
	{ "",					0							},
};

static UINT8 *config_file;
static int current_element;


static void XMLCALL element_start(void *data, const char *el, const char **attr)
{
	int element = get_string_id(el, element_id);

	if (element < 0)
	{
		message(0, "Unknown element %s while parsing XML file\n", el);
	}
	else
	{
		current_element = element;
	}
}

static void XMLCALL element_end(void *data, const char *el)
{
	int element = get_string_id(el, element_id);

	if (element < 0)
	{
		message(0, "Unknown element %s while parsing XML\n", el);
	}
	else
	{
		switch (element)
		{
			case ELEMENT_CONFIG:
			case ELEMENT_ROMPATH:
			case ELEMENT_GAMELIST:
			case ELEMENT_BACKUPPATH:
			case ELEMENT_WIDTH:
			case ELEMENT_HEIGHT:
			case ELEMENT_FULLSCREEN:
			case ELEMENT_FPSLIMIT:
			case ELEMENT_STRETCH:
			{
				current_element = ELEMENT_NONE;
				break;
			}
		}
	}
}

static void XMLCALL character_data(void *data, const char *s, int len)
{
	char temp[2000];
	memcpy(temp, s, len);
	temp[len] = 0;

	switch (current_element)
	{
		case ELEMENT_ROMPATH:
		{
			strcpy(m3_config.rom_path, temp);
			break;
		}
		case ELEMENT_GAMELIST:
		{
			strcpy(m3_config.rom_list, temp);
			break;
		}
		case ELEMENT_BACKUPPATH:
		{
			strcpy(m3_config.backup_path, temp);
			break;
		}
		case ELEMENT_WIDTH:
		{
			int width;
			sscanf(temp, "%d", &width);
			m3_config.width = width;
			break;
		}
		case ELEMENT_HEIGHT:
		{
			int height;
			sscanf(temp, "%d", &height);
			m3_config.height = height;
			break;
		}
		case ELEMENT_FULLSCREEN:
		{
			BOOL fullscreen = get_string_id(temp, boolean_id);
			m3_config.fullscreen = fullscreen;
			break;
		}
		case ELEMENT_FPSLIMIT:
		{
			BOOL fpslimit = get_string_id(temp, boolean_id);
			m3_config.fps_limit = fpslimit;
			break;
		}
		case ELEMENT_STRETCH:
		{
			BOOL stretch = get_string_id(temp, boolean_id);
			m3_config.stretch = stretch;
			break;
		}
	}
}

BOOL parse_config(const char *config_name)
{
	XML_Parser parser;

	int length;
	FILE *file;

	file = open_file(FILE_READ|FILE_BINARY, "%s", config_name);
	if (file == NULL)
	{
		message(0, "Couldn't open %s", config_name);
		return FALSE;
	}

	length = (int)get_open_file_size(file);

	config_file = (UINT8*)malloc(length);

	if (!read_from_file(file, config_file, length))
	{
		message(0, "I/O error while reading %s", config_name);
		return FALSE;
	}

	close_file(file);

	// parse the XML file
	parser = XML_ParserCreate(NULL);

	XML_SetElementHandler(parser, element_start, element_end);
	XML_SetCharacterDataHandler(parser, character_data);

	if (XML_Parse(parser, config_file, length, 1) != XML_STATUS_OK)
	{
		message(0, "Error while parsing the XML file %s", config_name);
		return FALSE;
	}

	XML_ParserFree(parser);
	return TRUE;
}