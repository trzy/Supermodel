/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/******************************************************************/
/* Linux Main  							  */
/******************************************************************/

#include <SDL.h>
#include "model3.h"
#include "lin_gl.h"

#define XRES    (496)
#define YRES    (384)

static OSD_CONTROLS controls;

static CHAR app_title[]     = "Model 3 Emulator";
static CHAR class_name[]	= "MODEL3";

static void osd_handle_key(int keysym, int updown);

static BOOL win_create_window(UINT xres, UINT yres)
{
	int width, height;
        const SDL_VideoInfo* info = NULL;
	int bpp = 0;
    	int flags = 0;

	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) 
	{
		fprintf( stderr, "Video initialization failed: %s\n",
		        SDL_GetError( ) );
		return FALSE;
	}

	info = SDL_GetVideoInfo( );
	if( !info ) 
	{
		fprintf( stderr, "Video query failed: %s\n",
		        SDL_GetError( ) );
		return FALSE;
	}

	width = xres;
	height = yres;
	bpp = info->vfmt->BitsPerPixel;

	// request 555rgb
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	flags = SDL_OPENGL;
	if(m3_config.fullscreen) 
	{
//		flags |= SDL_FULLSCREEN;
	}

	if( SDL_SetVideoMode( width, height, bpp, flags ) == 0 ) 
	{
		fprintf( stderr, "Video mode set failed: %s\n",
		        SDL_GetError( ) );
		return FALSE;
	}

	SDL_WM_SetCaption(app_title, "m3");

	return TRUE;
}

/* FPS calculator swiped from Modeler */
static unsigned long lastFPSTick = 0;
static long lastFPS = 0;
static long fpsCountUp = 0;

static inline unsigned long timeGetTime(void)
{
  struct timeval tv;
  gettimeofday(&tv, 0);                                
  return tv.tv_sec * 1000 + tv.tv_usec/1000;           
}

static void calcfps(void)
{
	unsigned long curTick;

	curTick = timeGetTime();

	if (curTick - lastFPSTick > 1000)
	{
		lastFPSTick = curTick;
		lastFPS = fpsCountUp;
		fpsCountUp = 0;
	}
	else
	{
		fpsCountUp++;
	}
}

int main(int argc, char *argv[])
{
    CHAR    mnem[16], oprs[48];
    BOOL    quit = FALSE;
    FILE    *fp;
    UINT    i, frame = 0, op;

	if(argc < 2) {
		// Show usage
		printf("ERROR: not enough arguments.\n\n");
		printf("Usage: %s [romset]\n", argv[0]);
		return 0;
	}

	// Load config

    m3_config.layer_enable = 0xF;
    memset(controls.system_controls, 0xFF, 2);
    memset(controls.game_controls, 0xFF, 2);

	// Parse command-line

    strncpy(m3_config.game_id, argv[1], 8);
    m3_config.game_id[8] = '\0';    // in case game name was 8 or more chars

	// Some initialization

	if(!win_create_window(XRES, YRES)) {
		printf("win_create_window failed.");
		return 0;
	}

	lin_gl_init(XRES, YRES);

	m3_init();
	m3_reset();

	// Now that everything works, we can show the window

    while (quit == FALSE)
    {
	SDL_Event event;

	if (SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_KEYDOWN:
				if ((event.key.keysym.sym&0xffff) == 27)
					quit = TRUE;
				else
					osd_handle_key(event.key.keysym.sym, 1);
				break;

			case SDL_KEYUP:
				osd_handle_key(event.key.keysym.sym, 0);
				break;

			case SDL_QUIT:
				quit = TRUE;
				break;
		}
	}

	calcfps();

        printf("frame %d, FPS = %02d, PC = %08X, ", frame++, lastFPS, ppc_get_reg(PPC_REG_PC));
        op = ppc_read_word(ppc_get_reg(PPC_REG_PC));
        if (DisassemblePowerPC(op, ppc_get_reg(PPC_REG_PC), mnem, oprs, 1))
        {
            if (mnem[0] != '\0')    // invalid form
                printf("%08X %s*\t%s\n", op, mnem, oprs);
            else
                printf("%08X ?\n", op);
        }
        else
            printf("%08X %s\t%s\n", op, mnem, oprs);

        m3_run_frame();

	}

	lin_gl_shutdown();

	SDL_Quit( );

    for (i = 0; i < 32; i += 4)
        printf("R%d=%08X\tR%d=%08X\tR%d=%08X\tR%d=%08X\n",
        i + 0, ppc_get_reg(PPC_REG_R0 + i + 0),
        i + 1, ppc_get_reg(PPC_REG_R0 + i + 1),
        i + 2, ppc_get_reg(PPC_REG_R0 + i + 2),
        i + 3, ppc_get_reg(PPC_REG_R0 + i + 3));
	exit(0);
}

void osd_warning()
{

}

void osd_error(CHAR * string)
{
	printf("ERROR: %s\n",string);

	exit(0);

	/* terminate emulation, renderer, input etc. */
	/* revert to plain GUI. */
}

OSD_CONTROLS * osd_input_update_controls(void)
{
    return &controls;
}

static void osd_handle_key(int keysym, int updown)
{
    CHAR    fname[13];

    controls.game_controls[0] = 0xFF;
    controls.game_controls[1] = 0xFF;
    controls.system_controls[0] = 0xFF;
    controls.system_controls[1] = 0xFF;

    // $$$TODO
    #if 0
        case WM_LBUTTONDOWN:
            controls.game_controls[0] &= ~0x01;
            break;
        case WM_LBUTTONUP:
            controls.game_controls[0] |= 0x01;
            break;
        case WM_MOUSEMOVE:
            controls.gun_x[0] = LOWORD(lParam);
            controls.gun_y[0] = HIWORD(lParam);
	    break;
     #endif

	if (updown)
	{
	        switch (keysym)
	        {
	        case 'A':  // A
		        controls.game_controls[0] &= ~0x01;
		        break;
	        case 'S':  // S
		        controls.game_controls[0] &= ~0x02;
		        break;
	        case 'D':  // D
		        controls.game_controls[0] &= ~0x04;
		        break;
	        case 'F':  // F
		        controls.game_controls[0] &= ~0x08;
		        break;
	        case 'G':  // G
		        controls.game_controls[0] &= ~0x80;
		        break;
	        case 'H':  // H
		        controls.game_controls[0] &= ~0x40;
		        break;

		case 'Q':
			controls.game_controls[0] &= ~0x80;
		        break;
		case 'W':
			controls.game_controls[0] &= ~0x40;
		        break;
		case 'E':
			controls.game_controls[0] &= ~0x20;
		        break;
		case 'R':
			controls.game_controls[0] &= ~0x10;
		        break;
		case 'T':
			controls.game_controls[1] &= ~0x80;
		        break;
		case 'Y':
			controls.game_controls[1] &= ~0x40;
		        break;
		case 'U':
			controls.game_controls[1] &= ~0x20;
		        break;
		case 'I':
			controls.game_controls[1] &= ~0x10;
		        break;

		case 'Z':
			controls.game_controls[0] &= ~0x01;
		        break;
		case 'X':
			controls.game_controls[1] &= ~0x01;
		        break;
		case 'C':
			controls.game_controls[0] &= ~0x02;
		        break;
		case 'V':
			controls.game_controls[1] &= ~0x02;
		        break;

		case SDLK_UP:
		        controls.game_controls[0] &= ~0x20; // VON2, forward
			controls.game_controls[1] &= ~0x20;
			break;
		case SDLK_DOWN:
        		controls.game_controls[0] &= ~0x10; // VON2, backward
			controls.game_controls[1] &= ~0x10;
			break;
		case SDLK_LEFT:
        		controls.game_controls[0] &= ~0x10; // VON2, turn left
			controls.game_controls[1] &= ~0x20;
			break;
		case SDLK_RIGHT:
        		controls.game_controls[0] &= ~0x20; // VON2, turn right
			controls.game_controls[1] &= ~0x10;
			break;

	        case SDLK_F12:
		        m3_config.layer_enable ^= 1;
		        break;
	        case SDLK_F11:
		        m3_config.layer_enable ^= 2;
		        break;
	        case SDLK_F10:
		        m3_config.layer_enable ^= 4;
		        break;
	        case SDLK_F9:
		        m3_config.layer_enable ^= 8;
		        break;
	        case SDLK_F8:
		        m3_config.layer_enable = 0xF;
		        break;
	        case SDLK_F5:
		        strncpy(fname, m3_config.game_id, 8);
		        fname[8] = '\0';
		        strcat(fname, ".sta");
		        m3_save_state(fname);
		        break;
	        case SDLK_F7:
		        strncpy(fname, m3_config.game_id, 8);
		        fname[8] = '\0';
		        strcat(fname, ".sta");
		        m3_load_state(fname);
		        break;

		// mame/hotrod-compatible keys
	        case SDLK_F2: // Test
		        controls.system_controls[0] &= ~0x04;
		        controls.system_controls[1] &= ~0x04;
		        break;
	        case '9':  // Service
		        controls.system_controls[0] &= ~0x08;
		        controls.system_controls[1] &= ~0x80;
		        break;
	        case '1': // Start
		        controls.system_controls[0] &= ~0x10;
		        break;
	        case '5': // Coin #1
		        controls.system_controls[0] &= ~0x01;
		        break;
	        }
	}
	return 0;
}

void osd_input_init(void)
{
}

void osd_input_shutdown(void)
{
}
