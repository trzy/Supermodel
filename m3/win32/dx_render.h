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

/*
 * win32/dx_render.h
 *
 * DirectX9 Renderer interface
 */

#include "d3d9.h"
#include "d3dx9.h"

#define RGB_REG				0xFFFF0000
#define RGB_GREEN			0xFF00FF00
#define RGB_BLUE			0xFF0000FF
#define RGB_YELLOW			0xFFFFFF00
#define RGB_CYAN			0xFF00FFFF

BOOL d3d_init(HWND);
void d3d_shutdown(void);
D3DXMATRIX d3d_matrix_stack_get_top(void);
void d3d_matrix_stack_init(void);