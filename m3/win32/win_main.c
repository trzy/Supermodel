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
/* Windows Main													  */
/******************************************************************/

#include "model3.h"
#ifdef RENDERER_D3D
    // ...
#else   // RENDERER_GL
#include "win_gl.h"
#endif

#define XRES    (496)
#define YRES    (384)

static CHAR app_title[]     = "Model 3 Emulator";
static CHAR class_name[]	= "MODEL3";

HWND main_window;

// Window Procedure prototype
static LRESULT CALLBACK win_window_proc(HWND, UINT, WPARAM, LPARAM);

static BOOL win_register_class(void)
{
	WNDCLASSEX wcex;

	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)win_window_proc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= GetModuleHandle(NULL);
	wcex.hIcon			= NULL;
	wcex.hIconSm		= NULL;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= class_name;

	if( FAILED(RegisterClassEx(&wcex)) ) // MinGW: "comparison is always false due to limited range of data"
		return FALSE;

	return TRUE;
}

static BOOL win_create_window(UINT xres, UINT yres)
{
	DWORD frame_width, frame_height, caption_height;
	int width, height, window_width, window_height;

    window_width = xres;
    window_height = yres;

	frame_width			= GetSystemMetrics(SM_CXSIZEFRAME);
	frame_height		= GetSystemMetrics(SM_CYSIZEFRAME);
	caption_height		= GetSystemMetrics(SM_CYCAPTION);

	width	= (window_width - 1) + (frame_width * 2);
	height	= (window_height - 1) + (frame_height * 2) + caption_height;

	main_window = CreateWindow(class_name,
							   app_title,
                               WS_CLIPSIBLINGS | WS_CLIPCHILDREN |  // required for OpenGL
							   WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
							   CW_USEDEFAULT, CW_USEDEFAULT,	// Window X & Y coords
							   width - 1, height - 1,			// Width & Height
							   NULL, NULL,						// Parent Window & Menu
							   GetModuleHandle(NULL), NULL );

	if(!main_window)
		return FALSE;

#ifdef RENDERER_D3D
    // ...
#else   // RENDERER_GL
    win_gl_init(XRES, YRES);
#endif

	return TRUE;
}

/*
 * NOTE: This should actually return an error if something goes wrong, but
 * it doesn't matter now. This stuff probably needs to be rewritten anyway ;)
 */
static void win_destroy(void)
{
#ifdef RENDERER_D3D
    // ...
#else   // RENDERER_GL
    win_gl_shutdown();
#endif
    DestroyWindow(main_window);
    UnregisterClass(class_name, GetModuleHandle(NULL));
}


int main(int argc, char *argv[])
{
    CHAR    mnem[16], oprs[48], prof[1024];
    MSG     msg;
    BOOL    quit = FALSE;
    FILE    *fp;
    UINT    i, frame = 0, op;

	if(argc < 2) {
		// Show usage
		printf("ERROR: not enough arguments.\n\n");
		printf("Usage: m3.exe [romset]\n");
		return 0;
	}

	// Load config

    m3_config.layer_enable = 0xF;

	// Parse command-line

    strncpy(m3_config.game_id, argv[1], 8);
    m3_config.game_id[8] = '\0';    // in case game name was 8 or more chars

	// Some initialization

	if(!win_register_class()) {
		printf("win_register_class failed.");
		return 0;
	}
    if(!win_create_window(XRES, YRES)) {
		printf("win_create_window failed.");
		return 0;
	}

    m3_init();
	m3_reset();

	// Now that everything works, we can show the window

	ShowWindow(main_window, SW_SHOWNORMAL);
    SetForegroundWindow(main_window);
    SetFocus(main_window);
	UpdateWindow(main_window);

	memset(&msg, 0, sizeof(MSG));
    while (quit == FALSE)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
                quit = TRUE;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

        printf("frame %d, PC = %08X, ", frame++, ppc_get_reg(PPC_REG_PC));
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

		// gather profiler stats

#ifdef _PROFILE_
        profile_print(prof);

		printf(prof);
#endif
	}

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

static LRESULT CALLBACK win_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CHAR    fname[13];
    static UINT xres = 496, yres = 384;

	switch(message)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_KEYDOWN:
            switch (wParam)
            {
            default:
                printf("KEY = %02X\n", wParam);
                break;

            case '7':
                m3_config.layer_enable ^= 1;
                break;
            case '8':
                m3_config.layer_enable ^= 2;
                break;
            case '9':
                m3_config.layer_enable ^= 4;
                break;
            case '0':
                m3_config.layer_enable ^= 8;
                break;
            case 0x36:
                m3_config.layer_enable = 0xF;
                break;
            case VK_ESCAPE:
                DestroyWindow(hWnd);
                break;
            case VK_F5:
                strncpy(fname, m3_config.game_id, 8);
                fname[8] = '\0';
                strcat(fname, ".sta");
                m3_save_state(fname);
                break;
            case VK_F7:
                strncpy(fname, m3_config.game_id, 8);
                fname[8] = '\0';
                strcat(fname, ".sta");
                m3_load_state(fname);
                break;
            }
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
