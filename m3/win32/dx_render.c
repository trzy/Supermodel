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
/* DirectX 9 Renderer                                             */
/******************************************************************/

#include "MODEL3.H"

static LPDIRECT3D9			d3d;
static LPDIRECT3DDEVICE9	device;
static LPDIRECT3DTEXTURE9	layer_data[4];
static LPD3DXSPRITE			sprite;
static D3DVIEWPORT9			viewport;

static D3DFORMAT supported_formats[] = { D3DFMT_A1R5G5B5,
										 D3DFMT_A8R8G8B8,
										 D3DFMT_A8B8G8R8 };

static D3DFORMAT layer_format = 0;

extern HWND main_window;

void osd_renderer_init(void)
{
	D3DPRESENT_PARAMETERS		d3dpp;
	D3DDISPLAYMODE				d3ddm;
	D3DCAPS9					caps;
	HRESULT hr;
	DWORD flags = 0;
	int i, num_buffers, width, height;

    atexit(osd_renderer_shutdown);

	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if(!d3d) 
		osd_error("Direct3DCreate9 failed.");

	hr = IDirect3D9_GetAdapterDisplayMode( d3d, D3DADAPTER_DEFAULT, &d3ddm );
	if(FAILED(hr))
		osd_error("IDirect3D9_GetAdapterDisplayMode failed.");

	hr = IDirect3D9_GetDeviceCaps( d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps );
	if(FAILED(hr))
		osd_error("IDirect3D9_GetDeviceCaps failed.");

	if(m3_config.triple_buffer)
		num_buffers = 3;
	else
		num_buffers = 2;

	if(!m3_config.fullscreen) {
		width	= MODEL3_SCREEN_WIDTH;
		height	= MODEL3_SCREEN_HEIGHT;
	} else {
		width	= m3_config.width;
		height	= m3_config.height;
	}

	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.Windowed			= m3_config.fullscreen ? FALSE : TRUE;
	d3dpp.SwapEffect		= m3_config.stretch ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_FLIP;
	d3dpp.BackBufferWidth	= width;
	d3dpp.BackBufferHeight	= height;
	d3dpp.BackBufferCount	= num_buffers - 1;
	d3dpp.hDeviceWindow		= main_window;
	d3dpp.PresentationInterval	= D3DPRESENT_INTERVAL_IMMEDIATE;
	d3dpp.BackBufferFormat	= d3ddm.Format;

	for( i=0; i < sizeof(supported_formats) / sizeof(D3DFORMAT); i++) {
		hr = IDirect3D9_CheckDeviceFormat( d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 
										   D3DUSAGE_DYNAMIC, D3DRTYPE_TEXTURE, 
										   supported_formats[i] );
		if( !FAILED(hr) ) {
			layer_format = supported_formats[i];
			break;
		}
	}
	if(!layer_format)
		osd_error("DirectX: No supported pixel formats found !");

	if(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) {
		flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	} else {
		flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	hr = IDirect3D9_CreateDevice( d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, main_window, 
								  flags, &d3dpp, &device );
	if(FAILED(hr))
		osd_error("IDirect3D9_CreateDevice failed.");

	for( i=0; i<4; i++) {
		hr = D3DXCreateTexture( device, MODEL3_SCREEN_WIDTH, MODEL3_SCREEN_HEIGHT, 1,
								D3DUSAGE_DYNAMIC, layer_format, D3DPOOL_DEFAULT, &layer_data[i] );
		if(FAILED(hr))
			osd_error("D3DXCreateTexture failed.");
	}

	// Clear the buffers
	for( i=0; i<num_buffers; i++) {
		IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 0.0f, 0 );
		IDirect3DDevice9_Present( device, NULL, NULL, NULL, NULL );
	}

	memset(&viewport, 0, sizeof(D3DVIEWPORT9));
	viewport.X		= 0;
	viewport.Y		= 0;
	viewport.Width	= MODEL3_SCREEN_WIDTH;
	viewport.Height	= MODEL3_SCREEN_HEIGHT;
	viewport.MinZ	= 0.1f;
	viewport.MaxZ	= 100000.0f;

	IDirect3DDevice9_SetViewport( device, &viewport );

	hr = D3DXCreateSprite( device, &sprite );
	if(FAILED(hr))
		osd_error("D3DXCreateSprite failed.");

}

void osd_renderer_shutdown(void)
{
	if(d3d) {
		IDirect3D9_Release(d3d);
		d3d = NULL;
	}
}

void osd_renderer_reset(void)
{

}

void osd_renderer_update_frame(void)
{
	/* called to process all the data, _after_ */
	/* the tilegen put layer data in the rendering */
	/* buffer. */
	RECT src_rect = { 0, 0, MODEL3_SCREEN_WIDTH - 1, MODEL3_SCREEN_HEIGHT - 1 };

	IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 0.0f, 0 );

	IDirect3DDevice9_BeginScene( device );
	sprite->lpVtbl->Begin(sprite);
	sprite->lpVtbl->Draw(sprite, layer_data[2], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF);
	sprite->lpVtbl->Draw(sprite, layer_data[0], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF);
	sprite->lpVtbl->End(sprite);
	IDirect3DDevice9_EndScene( device );

	if(m3_config.stretch) {
		IDirect3DDevice9_Present( device, &src_rect, NULL, NULL, NULL );
	} else {
		IDirect3DDevice9_Present( device, NULL, NULL, NULL, NULL );
	}
}

UINT osd_renderer_get_layer_depth(void)
{
	switch(layer_format)
	{
		case D3DFMT_A1R5G5B5:
			return 16;
		case D3DFMT_A8R8G8B8:
			return 32;
		case D3DFMT_A8B8G8R8:
			return 32;
		default:
			return 0;
	}
}

void osd_renderer_get_layer_buffer(UINT layer_num, UINT8 **buffer, UINT *pitch)
{
	D3DLOCKED_RECT		locked_rect;
	HRESULT hr;

	buffer = NULL;
	*pitch = 0;

	hr = IDirect3DTexture9_LockRect( layer_data[layer_num], 0, &locked_rect, NULL, D3DLOCK_DISCARD );
	if( !FAILED(hr) ) {
        *buffer = (UINT8 *) locked_rect.pBits;
        *pitch  = locked_rect.Pitch / (osd_renderer_get_layer_depth() / 8);
	}
}

void osd_renderer_free_layer_buffer(UINT layer_num)
{
	IDirect3DTexture9_UnlockRect( layer_data[layer_num], 0 );
}
