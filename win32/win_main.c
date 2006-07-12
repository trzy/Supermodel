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
#include "dx_render.h"
#else   // RENDERER_GL
#include "win_gl.h"
#endif

#define XRES    (496)
#define YRES    (384)

static CHAR app_title[]     = "Supermodel";
static CHAR app_version[]	= "1.0";
static CHAR class_name[]	= "MODEL3";

static CHAR CONFIG_FILE[]	= "config.xml";

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

	if (FAILED(RegisterClassEx(&wcex))) // MinGW: "comparison is always false due to limited range of data"
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

	if (!main_window)
		return FALSE;

	return TRUE;
}

/*
 * NOTE: This should actually return an error if something goes wrong, but
 * it doesn't matter now. This stuff probably needs to be rewritten anyway ;)
 */
static void win_destroy(void)
{
#ifdef RENDERER_D3D
	d3d_shutdown();
#else   // RENDERER_GL
    win_gl_shutdown();
#endif
    DestroyWindow(main_window);
    UnregisterClass(class_name, GetModuleHandle(NULL));
}

void *malloc_exec(int length)
{
	void *ptr;
	ptr = VirtualAlloc(NULL, length, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if (ptr == NULL)
	{
		error("malloc_exec %d failed\n", length);
	}

	return ptr;
}

void free_exec(void *ptr)
{
	if (VirtualFree(ptr, 0, MEM_RELEASE) == FALSE)
	{
		error("free_exec failed\n");
	}
}

BOOL check_cpu_features(void)
{
	BOOL features_ok = TRUE;
	char cpuname[256];
	UINT32 cpu_version;
	UINT32 cpu_features;

	memset(cpuname, 0, sizeof(cpuname));

	__asm
	{
		// get cpu name string
		mov eax, 0x80000002
		cpuid
		mov dword ptr [cpuname+0  ], eax
		mov dword ptr [cpuname+4  ], ebx
		mov dword ptr [cpuname+8  ], ecx
		mov dword ptr [cpuname+12 ], edx
		mov eax, 0x80000003
		cpuid
		mov dword ptr [cpuname+16 ], eax
		mov dword ptr [cpuname+20 ], ebx
		mov dword ptr [cpuname+24 ], ecx
		mov dword ptr [cpuname+28 ], edx
		mov eax, 0x80000004
		cpuid
		mov dword ptr [cpuname+32 ], eax
		mov dword ptr [cpuname+36 ], ebx
		mov dword ptr [cpuname+40 ], ecx
		mov dword ptr [cpuname+44 ], edx

		// get cpu version and features
		mov eax, 1
		cpuid
		mov [cpu_version], eax
		mov [cpu_features], edx
	}
	message(0, "CPU: %s", cpuname);
	if ((cpu_features & (1 << 15)) == 0)
	{
		message(0, "CPU doesn't support Conditional Move/Compare instructions");
		features_ok = FALSE;
	}
	if ((cpu_features & (1 << 23)) == 0)
	{
		message(0, "CPU doesn't support MMX instructions");
		features_ok = FALSE;
	}
	if ((cpu_features & (1 << 25)) == 0)
	{
		message(0, "CPU doesn't support SSE instructions");
		features_ok = FALSE;
	}
	if ((cpu_features & (1 << 26)) == 0)
	{
		message(0, "CPU doesn't support SSE2 instructions");
		features_ok = FALSE;
	}

	if (features_ok == FALSE)
	{
		message(0, "The CPU doesn't meet the requirements, the program will not run further.");
	}
	return features_ok;
}

int main(int argc, char *argv[])
{
    MSG     msg;
    BOOL    quit = FALSE, do_fps;
	INT64	freq, time_start, time_end;
	char title[256];
	double fps = 0.0;
	int frame = 0;

	if(argc < 2) {
		// Show usage
		printf("ERROR: not enough arguments.\n\n");
		printf("Usage: m3.exe [romset]\n");
		return 0;
	}

	message(0, "%s v%s\n", app_title, app_version);

	if (check_cpu_features() == FALSE)
	{
		exit(1);
	}

#if RENDERER_D3D
	if (d3d_pre_init() == FALSE)
	{
		message(0, "The video card doesn't meet the requirements, the program will not run further.");
		exit(1);
	}
#endif

	message(0, "");

	// Load config

	if (parse_config(CONFIG_FILE) == FALSE)
	{
		exit(1);
	}
    m3_config.layer_enable = 0xF;

	// Parse command-line

    strncpy(m3_config.game_id, argv[1], 8);
    m3_config.game_id[8] = '\0';    // in case game name was 8 or more chars

	if (stricmp(m3_config.game_id, "lostwsga") == 0)
	{
		m3_config.has_lightgun = TRUE;
	}

	// Some initialization

	if (!win_register_class())
	{
		message(0, "win_register_class failed.");
		exit(1);
	}
    if (!win_create_window(XRES, YRES))
	{
		message(0, "win_create_window failed.");
		exit(1);
	}

	if (model3_load() == FALSE)
	{
		message(0, "ROM loading failed");
		exit(1);
	}
    model3_init();
	model3_reset();

#if RENDERER_D3D
	if (!d3d_init(main_window))
	{
		message(0, "d3d_init failed.");
		exit(1);
	}
#else
	win_gl_init(XRES, YRES);
#endif

	if (osd_input_init() == FALSE)
	{
		exit(1);
	}

	// Set window title to show the game name
	sprintf(title, "%s: %s", app_title, m3_config.game_name);
	SetWindowText(main_window, title);

	// Now that everything works, we can show the window

	ShowWindow(main_window, SW_SHOWNORMAL);
    SetForegroundWindow(main_window);
    SetFocus(main_window);
	UpdateWindow(main_window);

	if (m3_config.fullscreen && !m3_config.has_lightgun)
	{
		ShowCursor(FALSE);
	}

	if(QueryPerformanceFrequency((LARGE_INTEGER *)&freq))
		do_fps = TRUE;
	else
		do_fps = FALSE;

	QueryPerformanceCounter((LARGE_INTEGER *)&time_start);
	QueryPerformanceCounter((LARGE_INTEGER *)&time_end);

	memset(&msg, 0, sizeof(MSG));
    while (quit == FALSE)
    {
		//QueryPerformanceCounter((LARGE_INTEGER *)&time_start);

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

        model3_run_frame();
		frame++;

		// gather profiler stats

		//QueryPerformanceCounter((LARGE_INTEGER *)&time_end);
		if (frame >= 5)
		{
			frame = 0;
			QueryPerformanceCounter((LARGE_INTEGER *)&time_end);
			fps = 5.0 / ((double)(time_end - time_start) / freq);

			time_start = time_end;
		}

#if RENDERER_D3D
		if (m3_config.show_fps)
		{
			//fps = 1.0 / ((double)(time_end - time_start) / freq);
			sprintf(title, "FPS: %.3f", fps);
			osd_renderer_draw_text(2, 2, title, 0xffff0000, TRUE);
		}
#else
		sprintf(title, "%s: %s, FPS: %.3f", app_title, m3_config.game_name, fps);
		SetWindowText(main_window, title);
#endif

		//osd_renderer_draw_text(2, 2, title, 0x00ff0000, TRUE);

		osd_renderer_blit();

#ifdef _PROFILE_
        profile_print(prof);

		printf(prof);
#endif
	}

    //for (i = 0; i < 32; i += 4)
    //    printf("R%d=%08X\tR%d=%08X\tR%d=%08X\tR%d=%08X\n",
    //    i + 0, ppc_get_reg(PPC_REG_R0 + i + 0),
    //    i + 1, ppc_get_reg(PPC_REG_R0 + i + 1),
    //    i + 2, ppc_get_reg(PPC_REG_R0 + i + 2),
    //    i + 3, ppc_get_reg(PPC_REG_R0 + i + 3));
	exit(0);
}

void osd_warning()
{

}

void osd_error(CHAR * string)
{
	printf("ERROR: %s\n",string);
	exit(0);
}

static LRESULT CALLBACK win_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CHAR    fname[13];
    static UINT xres = 496, yres = 384;

	switch(message)
	{
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}

		case WM_KEYDOWN:
		{
			switch (wParam)
			{
				default:
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
				/*case VK_F7:
					strncpy(fname, m3_config.game_id, 8);
					fname[8] = '\0';
					strcat(fname, ".sta");
					model3_save_state(fname);
					break;
				case VK_F8:
					strncpy(fname, m3_config.game_id, 8);
					fname[8] = '\0';
					strcat(fname, ".sta");
					model3_load_state(fname);
					break;*/
            }
			break;
		}

		case WM_KEYUP:
		{
			switch (wParam)
            {
				default:
					break;
				
				case VK_F11:
				{
					if (m3_config.fps_limit)
					{
						m3_config.fps_limit = FALSE;
					}
					else
					{
						m3_config.fps_limit = TRUE;
					}
					break;
				}
				case VK_F12:
				{
					if (m3_config.show_fps)
					{
						m3_config.show_fps = FALSE;
					}
					else
					{
						m3_config.show_fps = TRUE;
					}
					break;
				}
				break;
			}
		}

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
