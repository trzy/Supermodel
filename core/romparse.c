#include "model3.h"
#include "expat.h"

enum
{
	ELEMENT_NONE = 1,
	ELEMENT_GAMELIST,
	ELEMENT_GAME,
	ELEMENT_DESCRIPTION,
	ELEMENT_YEAR,
	ELEMENT_MANUFACTURER,
	ELEMENT_STEP,
	ELEMENT_CROMSIZE,
	ELEMENT_ROM,
	ELEMENT_PATCH,
	ELEMENT_BRIDGE,
	ELEMENT_INPUT,
};

enum
{
	ATTR_ROM_TYPE = 1,
	ATTR_ROM_NAME,
	ATTR_ROM_SIZE,
	ATTR_ROM_CRC,
	ATTR_ROM_SHA1,
	ATTR_GAME_NAME,
	ATTR_GAME_PARENT,
	ATTR_PATCH_SIZE,
	ATTR_PATCH_ADDRESS,
	ATTR_PATCH_DATA,
	ATTR_INPUT_TYPE,
	ATTR_INPUT_CENTER,
	ATTR_INPUT_MAPPING,
};

static STRING_ID element_id[] =
{
	{ "gamelist",		ELEMENT_GAMELIST		},
	{ "game",			ELEMENT_GAME			},
	{ "description",	ELEMENT_DESCRIPTION		},
	{ "year",			ELEMENT_YEAR			},
	{ "manufacturer",	ELEMENT_MANUFACTURER	},
	{ "step",			ELEMENT_STEP			},
	{ "cromsize",		ELEMENT_CROMSIZE		},
	{ "rom",			ELEMENT_ROM				},
	{ "patch",			ELEMENT_PATCH			},
	{ "bridge",			ELEMENT_BRIDGE			},
	{ "input",			ELEMENT_INPUT			},
	{ "",				0						},
};

static STRING_ID rom_attr_id[] =
{
	{ "type",			ATTR_ROM_TYPE			},
	{ "name",			ATTR_ROM_NAME			},
	{ "size",			ATTR_ROM_SIZE			},
	{ "crc",			ATTR_ROM_CRC			},
	{ "sha1",			ATTR_ROM_SHA1			},
	{ "",				0						},
};

static STRING_ID game_attr_id[] =
{
	{ "name",			ATTR_GAME_NAME			},
	{ "parent",			ATTR_GAME_PARENT		},
	{ "",				0						},
};

static STRING_ID patch_attr_id[] =
{
	{ "size",			ATTR_PATCH_SIZE			},
	{ "address",		ATTR_PATCH_ADDRESS		},
	{ "data",			ATTR_PATCH_DATA			},
	{ "",				0						},
};

static STRING_ID input_attr_id[] =
{
	{ "type",			ATTR_INPUT_TYPE			},
	{ "center",			ATTR_INPUT_CENTER		},
	{ "mapping",		ATTR_INPUT_MAPPING		},
	{ "",				0						},
};

static STRING_ID step_id[] =
{
	{ "1.0",			0x10					},
	{ "1.5",			0x15					},
	{ "2.0",			0x20					},
	{ "2.1",			0x21					},
	{ "",				0						},
};

static STRING_ID rom_id[] =
{
	{ "prog0",			ROMTYPE_PROG0			},
	{ "prog1",			ROMTYPE_PROG1			},
	{ "prog2",			ROMTYPE_PROG2			},
	{ "prog3",			ROMTYPE_PROG3			},
	{ "crom00",			ROMTYPE_CROM00			},
	{ "crom01",			ROMTYPE_CROM01			},
	{ "crom02",			ROMTYPE_CROM02			},
	{ "crom03",			ROMTYPE_CROM03			},
	{ "crom10",			ROMTYPE_CROM10			},
	{ "crom11",			ROMTYPE_CROM11			},
	{ "crom12",			ROMTYPE_CROM12			},
	{ "crom13",			ROMTYPE_CROM13			},
	{ "crom20",			ROMTYPE_CROM20			},
	{ "crom21",			ROMTYPE_CROM21			},
	{ "crom22",			ROMTYPE_CROM22			},
	{ "crom23",			ROMTYPE_CROM23			},
	{ "crom30",			ROMTYPE_CROM30			},
	{ "crom31",			ROMTYPE_CROM31			},
	{ "crom32",			ROMTYPE_CROM32			},
	{ "crom33",			ROMTYPE_CROM33			},
	{ "vrom00",			ROMTYPE_VROM00			},
	{ "vrom01",			ROMTYPE_VROM01			},
	{ "vrom02",			ROMTYPE_VROM02			},
	{ "vrom03",			ROMTYPE_VROM03			},
	{ "vrom04",			ROMTYPE_VROM04			},
	{ "vrom05",			ROMTYPE_VROM05			},
	{ "vrom06",			ROMTYPE_VROM06			},
	{ "vrom07",			ROMTYPE_VROM07			},
	{ "vrom10",			ROMTYPE_VROM10			},
	{ "vrom11",			ROMTYPE_VROM11			},
	{ "vrom12",			ROMTYPE_VROM12			},
	{ "vrom13",			ROMTYPE_VROM13			},
	{ "vrom14",			ROMTYPE_VROM14			},
	{ "vrom15",			ROMTYPE_VROM15			},
	{ "vrom16",			ROMTYPE_VROM16			},
	{ "vrom17",			ROMTYPE_VROM17			},
	{ "sprog",			ROMTYPE_SPROG			},
	{ "srom0",			ROMTYPE_SROM0			},
	{ "srom1",			ROMTYPE_SROM1			},
	{ "srom2",			ROMTYPE_SROM2			},
	{ "srom3",			ROMTYPE_SROM3			},
	{ "dsbprog",		ROMTYPE_DSBPROG			},
	{ "dsbrom0",		ROMTYPE_DSBROM0			},
	{ "dsbrom1",		ROMTYPE_DSBROM1			},
	{ "dsbrom2",		ROMTYPE_DSBROM2			},
	{ "dsbrom3",		ROMTYPE_DSBROM3			},
	{ "",				0						},
};

#define INPUT_TYPE_BUTTON		0x10000
#define INPUT_TYPE_ANALOG		0x20000

static STRING_ID input_id[] =
{
	{ "button01",		INPUT_TYPE_BUTTON | 0	},
	{ "button02",		INPUT_TYPE_BUTTON | 1	},
	{ "button03",		INPUT_TYPE_BUTTON | 2	},
	{ "button04",		INPUT_TYPE_BUTTON | 3	},
	{ "button05",		INPUT_TYPE_BUTTON | 4	},
	{ "button06",		INPUT_TYPE_BUTTON | 5	},
	{ "button07",		INPUT_TYPE_BUTTON | 6	},
	{ "button08",		INPUT_TYPE_BUTTON | 7	},
	{ "button11",		INPUT_TYPE_BUTTON | 8	},
	{ "button12",		INPUT_TYPE_BUTTON | 9	},
	{ "button13",		INPUT_TYPE_BUTTON | 10	},
	{ "button14",		INPUT_TYPE_BUTTON | 11	},
	{ "button15",		INPUT_TYPE_BUTTON | 12	},
	{ "button16",		INPUT_TYPE_BUTTON | 13	},
	{ "button17",		INPUT_TYPE_BUTTON | 14	},
	{ "button18",		INPUT_TYPE_BUTTON | 15	},
	{ "analog1",		INPUT_TYPE_ANALOG | 0	},
	{ "analog2",		INPUT_TYPE_ANALOG | 1	},
	{ "analog3",		INPUT_TYPE_ANALOG | 2	},
	{ "analog4",		INPUT_TYPE_ANALOG | 3	},
	{ "analog5",		INPUT_TYPE_ANALOG | 4	},
	{ "analog6",		INPUT_TYPE_ANALOG | 5	},
	{ "analog7",		INPUT_TYPE_ANALOG | 6	},
	{ "analog8",		INPUT_TYPE_ANALOG | 7	},
	{ "",				0						},
};

static STRING_ID input_mapping_id[] =
{
	{ "p1_button_1",		P1_BUTTON_1				},
	{ "p1_button_2",		P1_BUTTON_2				},
	{ "p1_button_3",		P1_BUTTON_3				},
	{ "p1_button_4",		P1_BUTTON_4				},
	{ "p1_button_5",		P1_BUTTON_5				},
	{ "p1_button_6",		P1_BUTTON_6				},
	{ "p1_button_7",		P1_BUTTON_7				},
	{ "p1_button_8",		P1_BUTTON_8				},
	{ "p2_button_1",		P2_BUTTON_1				},
	{ "p2_button_2",		P2_BUTTON_2				},
	{ "p2_button_3",		P2_BUTTON_3				},
	{ "p2_button_4",		P2_BUTTON_4				},
	{ "p2_button_5",		P2_BUTTON_5				},
	{ "p2_button_6",		P2_BUTTON_6				},
	{ "p2_button_7",		P2_BUTTON_7				},
	{ "p2_button_8",		P2_BUTTON_8				},
	{ "analog_axis_1",		ANALOG_AXIS_1			},
	{ "analog_axis_2",		ANALOG_AXIS_2			},
	{ "analog_axis_3",		ANALOG_AXIS_3			},
	{ "analog_axis_4",		ANALOG_AXIS_4			},
	{ "analog_axis_5",		ANALOG_AXIS_5			},
	{ "analog_axis_6",		ANALOG_AXIS_6			},
	{ "analog_axis_7",		ANALOG_AXIS_7			},
	{ "analog_axis_8",		ANALOG_AXIS_8			},
	{ "p1_joystick_up",		P1_JOYSTICK_UP			},
	{ "p1_joystick_down",	P1_JOYSTICK_DOWN		},
	{ "p1_joystick_left",	P1_JOYSTICK_LEFT		},
	{ "p1_joystick_right",	P1_JOYSTICK_RIGHT		},
	{ "p2_joystick_up",		P2_JOYSTICK_UP			},
	{ "p2_joystick_down",	P2_JOYSTICK_DOWN		},
	{ "p2_joystick_left",	P2_JOYSTICK_LEFT		},
	{ "p2_joystick_right",	P2_JOYSTICK_RIGHT		},
	{ "",					0						},
};



static UINT8 *romlist;

static ROMSET *romset;

static int current_attr;
static int current_element;

static int current_rom;
static int current_romset;
static int current_patch;
static int current_input;



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

		switch (element)
		{
			case ELEMENT_ROM:
			{
				int i=0;
	
				// handle ROM types
				while (attr[i] != NULL)
				{
					int id = get_string_id(attr[i], rom_attr_id);

					if (id >= 0)
					{
						current_attr = id;
					}
					else
					{
						// not a type
						switch (current_attr)
						{
							case ATTR_ROM_TYPE:
							{
								int romtype = get_string_id(attr[i], rom_id);
								romset[current_romset].rom[current_rom].type = romtype;
								break;
							}
							case ATTR_ROM_NAME:
							{
								strcpy(romset[current_romset].rom[current_rom].name, attr[i]);
								break;
							}
							case ATTR_ROM_SIZE:
							{
								int romsize;
								sscanf(attr[i], "%d", &romsize);
								romset[current_romset].rom[current_rom].size = romsize;
								break;
							}
							case ATTR_ROM_CRC:
							{
								UINT32 romcrc;
								sscanf(attr[i], "%X", &romcrc);
								romset[current_romset].rom[current_rom].crc32 = romcrc;
								break;
							}
							case ATTR_ROM_SHA1:
							{
								// not handled
								break;
							}
						}
					}
					i++;
				};
				break;
			}

			case ELEMENT_GAME:
			{
				int i=0;

				while (attr[i] != NULL)
				{
					int id = get_string_id(attr[i], game_attr_id);

					if (id >= 0)
					{
						current_attr = id;
					}
					else
					{
						switch (current_attr)
						{
							case ATTR_GAME_NAME:
							{
								strcpy(romset[current_romset].game, attr[i]);
								break;
							}
							case ATTR_GAME_PARENT:
							{
								strcpy(romset[current_romset].parent, attr[i]);
								break;
							}
						}
					}
					i++;
				};
				break;
			}

			case ELEMENT_PATCH:
			{
				int i=0;

				while (attr[i] != NULL)
				{
					int id = get_string_id(attr[i], patch_attr_id);

					if (id >= 0)
					{
						current_attr = id;
					}
					else
					{
						switch (current_attr)
						{
							case ATTR_PATCH_SIZE:
							{
								int patch_size;
								sscanf(attr[i], "%d", &patch_size);
								romset[current_romset].patch[current_patch].size = patch_size;
								break;
							}
							case ATTR_PATCH_ADDRESS:
							{
								UINT32 address;
								sscanf(attr[i], "%X", &address);
								romset[current_romset].patch[current_patch].address = address;
								break;
							}
							case ATTR_PATCH_DATA:
							{
								UINT32 data;
								sscanf(attr[i], "%X", &data);
								romset[current_romset].patch[current_patch].data = data;
								break;
							}
						}
					}
					i++;
				};
				break;
			}

			case ELEMENT_INPUT:
			{
				int i=0;

				while (attr[i] != NULL)
				{
					int id = get_string_id(attr[i], input_attr_id);

					if (id >= 0)
					{
						current_attr = id;
					}
					else
					{
						switch (current_attr)
						{
							case ATTR_INPUT_TYPE:
							{
								current_input = get_string_id(attr[i], input_id);

								if (current_input & INPUT_TYPE_BUTTON)
								{
									int button = current_input & 0xffff;
									int set = (button / 8) & 1;
									int bit = 1 << (button % 8);
									romset[current_romset].controls.button[button].enabled = 1;
									romset[current_romset].controls.button[button].control_set = set;
									romset[current_romset].controls.button[button].control_bit = bit;
								}
								else if (current_input & INPUT_TYPE_ANALOG)
								{
									int axis = current_input & 0xffff;
									romset[current_romset].controls.analog_axis[axis].enabled = 1;
								}
								break;
							}
							case ATTR_INPUT_CENTER:
							{
								if (current_input & INPUT_TYPE_ANALOG)
								{
									int axis = current_input & 0xffff;
									int center;
									sscanf(attr[i], "%d", &center);
									romset[current_romset].controls.analog_axis[axis].center = center;
								}
								break;
							}
							case ATTR_INPUT_MAPPING:
							{
								int mapping = get_string_id(attr[i], input_mapping_id);
								if (current_input & INPUT_TYPE_BUTTON)
								{
									int button = current_input & 0xffff;
									romset[current_romset].controls.button[button].mapping = mapping;
								}
								else if (current_input & INPUT_TYPE_ANALOG)
								{
									int axis = current_input & 0xffff;
									romset[current_romset].controls.analog_axis[axis].mapping = mapping;
								}
								break;
							}		
						}
					}
					i++;
				};
				break;
			}
		}
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
			case ELEMENT_ROM:
			{
				current_rom++;
				current_element = ELEMENT_GAMELIST;
				break;
			}
			case ELEMENT_GAME:
			{
				romset[current_romset].num_patches = current_patch;
				current_romset++;
				current_rom = 0;
				current_patch = 0;
				current_element = ELEMENT_GAMELIST;
				break;
			}
			case ELEMENT_DESCRIPTION:
			case ELEMENT_YEAR:
			case ELEMENT_MANUFACTURER:
			case ELEMENT_STEP:
			case ELEMENT_BRIDGE:
			case ELEMENT_CROMSIZE:
			{
				current_element = ELEMENT_GAMELIST;
				break;
			}
			case ELEMENT_PATCH:
			{
				current_patch++;
				current_element = ELEMENT_GAMELIST;
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
		case ELEMENT_DESCRIPTION:
		{
			strcpy(romset[current_romset].title, temp);
			break;
		}
		case ELEMENT_YEAR:
		{
			int year;
			sscanf(temp, "%d", &year);
			romset[current_romset].year = year;
			break;
		}
		case ELEMENT_MANUFACTURER:
		{
			strcpy(romset[current_romset].manufacturer, temp);
			break;
		}
		case ELEMENT_STEP:
		{
			int step = get_string_id(temp, step_id);

			if (step >= 0)
			{
				romset[current_romset].step = step;
			}
			else
			{
				message(0, "Invalid step %s while parsing XML file\n", temp);
			}
			break;
		}
		case ELEMENT_BRIDGE:
		{
			if (_stricmp(temp, "mpc106") == 0)
			{
				romset[current_romset].bridge = 2;
			}
			else
			{
				message(0, "Unknown bridge %s while parsing XML file\n", temp);
			}
			break;
		}
		case ELEMENT_CROMSIZE:
		{
			if (_stricmp(temp, "64M") == 0)
			{
				romset[current_romset].cromsize = 1;
			}
			else
			{
				message(0, "Unknown cromsize %s while parsing XML file\n", temp);
			}
			break;
		}
	}
}

int parse_romlist(char *romlist_name, ROMSET *_romset)
{
	XML_Parser parser;

	int length;
	FILE *file;

	file = open_file(FILE_READ|FILE_BINARY, "%s", romlist_name);
	if (file == NULL)
	{
		message(0, "Couldn't open %s", romlist_name);
		return 0;
	}

	length = (int)get_open_file_size(file);

	romlist = (UINT8*)malloc(length);

	if (!read_from_file(file, romlist, length))
	{
		message(0, "I/O error while reading %s", romlist_name);
		return 0;
	}

	close_file(file);

	romset = _romset;

	// parse the XML file
	parser = XML_ParserCreate(NULL);

	XML_SetElementHandler(parser, element_start, element_end);
	XML_SetCharacterDataHandler(parser, character_data);

	if (XML_Parse(parser, romlist, length, 1) != XML_STATUS_OK)
	{
		message(0, "Error while parsing the XML file %s", romlist_name);
		return 0;
	}

	XML_ParserFree(parser);
	return current_romset;
}