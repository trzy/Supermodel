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

//#define ENABLE_LIGHTING
//#define WIREFRAME
#define ENABLE_ALPHA_BLENDING

typedef struct {
	int tex_num;
	int vb_index;
	int flags;
} POLYGON_LIST;

typedef struct {
	float x, y, z;
	float nx, ny, nz;
	D3DCOLOR color;
	float u, v;
} VERTEX;

#define D3DFVF_VERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0))

static LPDIRECT3D9			d3d;
static LPDIRECT3DDEVICE9	device;
static LPDIRECT3DTEXTURE9	layer_data[4];
static LPD3DXSPRITE			sprite;

static LPDIRECT3DTEXTURE9	texture[4096];
static short texture_table[64*64];

static LPD3DXMATRIXSTACK		matrix_stack;

static D3DFORMAT supported_formats[] = { D3DFMT_A1R5G5B5,
										 D3DFMT_A8R8G8B8,
										 D3DFMT_A8B8G8R8 };

static D3DFORMAT layer_format = 0;
static char num_bits[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

extern HWND main_window;

static UINT32* list_ram;	// Display List ram at 0x8E000000
static UINT32* cull_ram;	// Culling ram at 0x8C000000
static UINT32* poly_ram;	// Polygon ram at 0x98000000
static UINT32* vrom;		// VROM
static UINT8* texture_sheet;

static UINT32 matrix_start;

static void traverse_node(UINT32*);
static void traverse_list(UINT32*);
static void render_scene(void);
static UINT32* get_address(UINT32);

/*
 * void osd_renderer_init(UINT8 *culling_ram_ptr, UINT8 *polygon_ram_ptr,
 *                        UINT8 *vrom_ptr);
 *
 * Initializes the renderer.
 *
 * Parameters:
 *      list_ram_ptr = Pointer to Real3D display list RAM
 *      cull_ram_ptr = Pointer to Real3D culling RAM
 *      poly_ram_ptr = Pointer to Real3D polygon RAM.
 *		texture_ram_ptr = Pointer to Real3D texture RAM
 *      vrom_ptr     = Pointer to VROM.
 */

void osd_renderer_init(UINT8 *list_ram_ptr, UINT8 *cull_ram_ptr,UINT8 *poly_ram_ptr, UINT8 *texture_ram_ptr,UINT8 *vrom_ptr)
{
	D3DPRESENT_PARAMETERS		d3dpp;
	D3DDISPLAYMODE				d3ddm;
	D3DCAPS9					caps;
	D3DDISPLAYMODE				display_mode;
	HRESULT hr;
	DWORD flags = 0;
	int i, num_buffers, width, height;

	list_ram	= (UINT32*)list_ram_ptr;
	cull_ram	= (UINT32*)cull_ram_ptr;
	poly_ram	= (UINT32*)poly_ram_ptr;
	vrom		= (UINT32*)vrom_ptr;
	texture_sheet = texture_ram_ptr;

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

	width	= 0;
	height	= 0;

	if(!m3_config.fullscreen) {
		width	= MODEL3_SCREEN_WIDTH;
		height	= MODEL3_SCREEN_HEIGHT;
	} else {
		// Check if the display mode specified in m3_config is available
		UINT modes = IDirect3D9_GetAdapterModeCount( d3d, D3DADAPTER_DEFAULT, d3ddm.Format );
		
		for( i=0; i<modes; i++) {
			hr = IDirect3D9_EnumAdapterModes( d3d, D3DADAPTER_DEFAULT, d3ddm.Format, i, &display_mode );
			if( FAILED(hr) )
				error("IDirect3D9_EnumAdapterModes failed.\n");

			// Check if this mode matches
			if(display_mode.Width == m3_config.width && display_mode.Height == m3_config.height) {
				width	= m3_config.width;
				height	= m3_config.height;
			}
		}
	}
	// Check if we have a valid display mode
	if(width == 0 || height == 0)
		error("DirectX error: Display mode %dx%d not available\n",m3_config.width, m3_config.height );

	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.Windowed			= m3_config.fullscreen ? FALSE : TRUE;
	d3dpp.SwapEffect		= m3_config.stretch ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferWidth	= width;
	d3dpp.BackBufferHeight	= height;
	d3dpp.BackBufferCount	= num_buffers - 1;
	d3dpp.hDeviceWindow		= main_window;
	d3dpp.PresentationInterval	= D3DPRESENT_INTERVAL_ONE;
	d3dpp.BackBufferFormat	= D3DFMT_A8R8G8B8;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24X8;	// FIXME: All cards don't support 24-bit Z-buffer

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
		error("IDirect3D9_CreateDevice failed: %s",DXGetErrorString9(hr));

	for( i=0; i<4; i++) {
		hr = D3DXCreateTexture( device, MODEL3_SCREEN_WIDTH, MODEL3_SCREEN_HEIGHT, 1,
								D3DUSAGE_DYNAMIC, layer_format, D3DPOOL_DEFAULT, &layer_data[i] );
		if(FAILED(hr))
			osd_error("D3DXCreateTexture failed.");
	}
	memset(texture,0,sizeof(texture));
	memset(texture_table,0,sizeof(texture_table));

	switch(layer_format)
	{
		case D3DFMT_A1R5G5B5:
			tilegen_set_layer_format(16,10,5,0,15);break;
		case D3DFMT_A8R8G8B8:
			tilegen_set_layer_format(32,16,8,0,24);break;
		case D3DFMT_A8B8G8R8:
			tilegen_set_layer_format(32,0,8,16,24);break;
	}

	// Clear the buffers
	for( i=0; i<num_buffers + 1; i++) {
		IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 0.0f, 0 );
		IDirect3DDevice9_Present( device, NULL, NULL, NULL, NULL );
	}

	hr = D3DXCreateSprite( device, &sprite );
	if(FAILED(hr))
		osd_error("D3DXCreateSprite failed.");
}

void save_file(char* filename, UINT8* src, INT size)
{
	FILE* file = fopen(filename,"wb");
	int i;
	for(i=0;i<size;i+=2) {
		UINT16 pix = src[i+1] << 8 | src[i];
		int r = ((pix >> 10) & 0x1F) << 3;
		int g = ((pix >> 5) & 0x1F) << 3;
		int b = (pix & 0x1F) << 3;
		fputc(r,file);
		fputc(g,file);
		fputc(b,file);
	}
	fclose(file);
}

void osd_renderer_shutdown(void)
{
	if(d3d) {
		IDirect3D9_Release(d3d);
		d3d = NULL;
	}
	save_file("texture1.raw",texture_sheet,0x800000);
}

void osd_renderer_reset(void)
{

}

static void set_viewport(INT x, INT y, INT width, INT height)
{
	D3DVIEWPORT9	viewport;
	memset(&viewport, 0, sizeof(D3DVIEWPORT9));
	viewport.X		= x;
	viewport.Y		= y;
	viewport.Width	= width;
	viewport.Height	= height;
	viewport.MinZ	= 0.1f;
	viewport.MaxZ	= 100000.0f;

	IDirect3DDevice9_SetViewport( device, &viewport );
}

void osd_renderer_update_frame(void)
{
	D3DMATRIX projection;
	D3DMATRIX world;
	D3DMATRIX view;
	RECT src_rect = { 0, 0, MODEL3_SCREEN_WIDTH - 1, MODEL3_SCREEN_HEIGHT - 1 };

	D3DXMatrixPerspectiveFovLH( &projection, D3DXToRadian(45.0f), 496.0f / 384.0f, 0.1f, 100000.0f );
	D3DXMatrixIdentity( &world );
	D3DXMatrixIdentity( &view );
	D3DXMatrixScaling( &view, 1.0f, 1.0f, -1.0f );

	IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0 );

	IDirect3DDevice9_BeginScene( device );
	IDirect3DDevice9_SetTransform( device, D3DTS_PROJECTION, &projection );
	IDirect3DDevice9_SetTransform( device, D3DTS_WORLD, &world );
	IDirect3DDevice9_SetTransform( device, D3DTS_VIEW, &view );

	IDirect3DDevice9_SetFVF( device, D3DFVF_VERTEX );
	IDirect3DDevice9_SetRenderState( device, D3DRS_CULLMODE, D3DCULL_NONE );

#ifdef ENABLE_LIGHTING
	IDirect3DDevice9_SetRenderState( device, D3DRS_LIGHTING, TRUE );
#else
	IDirect3DDevice9_SetRenderState( device, D3DRS_LIGHTING, FALSE );
#endif

	IDirect3DDevice9_SetRenderState( device, D3DRS_ZENABLE, D3DZB_USEW );

	set_viewport( 0, 0, MODEL3_SCREEN_WIDTH, MODEL3_SCREEN_HEIGHT );
	sprite->lpVtbl->Draw(sprite, layer_data[3], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF);
	sprite->lpVtbl->Draw(sprite, layer_data[2], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF);

#ifdef ENABLE_ALPHA_BLENDING
	IDirect3DDevice9_SetRenderState( device, D3DRS_ALPHABLENDENABLE, TRUE );
	IDirect3DDevice9_SetRenderState( device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	IDirect3DDevice9_SetRenderState( device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
#endif

	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	IDirect3DDevice9_SetTextureStageState( device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
	
#ifdef WIREFRAME
	IDirect3DDevice9_SetRenderState( device, D3DRS_FILLMODE, D3DFILL_WIREFRAME );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
#endif

	render_scene();

	set_viewport( 0, 0, MODEL3_SCREEN_WIDTH, MODEL3_SCREEN_HEIGHT );
	sprite->lpVtbl->Draw(sprite, layer_data[1], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF);
	sprite->lpVtbl->Draw(sprite, layer_data[0], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF);
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

	*buffer = NULL;
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

void osd_renderer_remove_textures(UINT x, UINT y, UINT w, UINT h)
{
	// Update texture table
	// Removes the overwritten texture from cache
	int i,j;
	for( j = y/32; j < (y+h)/32; j++) {
		for( i = x/32; i < (x+w)/32; i++) {
			int tex_num = texture_table[(j*64) + i];
			if(texture[tex_num] != NULL) {
				IDirect3DTexture9_Release( texture[tex_num] );
				texture[tex_num] = NULL;
			}
			texture_table[(j*64) + i] = 0;
		}
	}
}

static int cache_texture(int texture_x, int texture_y, int tex_width, int tex_height, int depth)
{
	D3DFORMAT format;
	int i,j,x,y;
	int texture_num;
	HRESULT hr;
	D3DLOCKED_RECT locked_rect;
	UINT pitch;
	UINT16* ptr, *src;
	int texture_depth;

	i = 0;
	texture_num = -1;
	while(i < 4096 && texture_num < 0) {
		if(texture[i] == NULL) {
			texture_num = i;
		}
		i++;
	}
	if(texture_num == -1)
		error("DirectX: No available textures !");

	texture_depth = (depth >> 8) & 0x3;
	switch(texture_depth)
	{
		case 0: format = D3DFMT_A1R5G5B5;break;
		case 1: format = D3DFMT_A4R4G4B4;break;	// Unknown
		case 2: format = D3DFMT_A8L8;break;
		case 3: format = D3DFMT_A4R4G4B4;break;
	}

	hr = D3DXCreateTexture( device, tex_width, tex_height, 1, 0, format, D3DPOOL_MANAGED,
							&texture[texture_num] );
	if( FAILED(hr) )
		error("D3DXCreateTexture failed.");

	hr = IDirect3DTexture9_LockRect( texture[texture_num], 0, &locked_rect, NULL, 0 );
	if( FAILED(hr) )
		error("IDirect3DTexture9_LockRect failed.");

	ptr = (UINT16*)locked_rect.pBits;
	pitch = locked_rect.Pitch / 2;

	src = &texture_sheet[(texture_y * 2048 + texture_x) * 2];

	switch(texture_depth)
	{
		case 0:	// ARGB1555
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					ptr[t_index + x] = src[s_index + x] ^ 0x8000;
				}
			}
			break;
		case 1:	// Unknown, 4-bit texture ???
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					ptr[t_index + x] = (x + y) % 2 ? 0xFC00 : 0x1F;
				}
			}
			break;
		case 2:	// A8L8
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = ((src[s_index + (x/2)] >> ((x & 0x1) ? 8 : 0)) & 0xFF);
					ptr[t_index + x] = pix << 8 | pix;
				}
			}
			break;
		case 3:	// ARGB4444
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = src[s_index + x];
					UINT16 a = ((pix & 0xF) << 12);
					UINT16 b = ((pix & 0xF0) >> 4);
					UINT16 g = ((pix & 0xF00) >> 4);
					UINT16 r = ((pix & 0xF000) >> 4);
					ptr[t_index + x] = a | r | g | b;
				}
			}
			break;
	}

	IDirect3DTexture9_UnlockRect( texture[texture_num], 0 );
	
	// Update texture table
	for( j = texture_y/32; j < (texture_y + tex_height)/32; j++) {
		for( i = texture_x/32; i < (texture_x + tex_width)/32; i++) {
			texture_table[(j*64) + i] = texture_num;
		}
	}
	return texture_num;
}

static int polylist_compare(const void *arg1, const void *arg2)
{
	POLYGON_LIST *a = (POLYGON_LIST*)arg1;
	POLYGON_LIST *b = (POLYGON_LIST*)arg2;

	if(a->tex_num < b->tex_num) {
		return -1;
	} else if(a->tex_num == b->tex_num) {
		return 0;
	} else {
		return 1;
	}
}

static void render_model(UINT32 address, BOOL little_endian)
{
	float fixed_point_scale;
	VERTEX vertex[4];
	VERTEX prev_vertex[4];
	int end, index, polygon_index, vb_index, prev_texture, i, page;
	VERTEX  *vb;
	HRESULT hr;
	UINT32* src;
	float uv_scale;

	src = get_address( address );
	if(src == NULL)
		error("render_model: invalid address %08X",address);

	if(m3_config.step == 0x10)
		fixed_point_scale = 32768.0f;
	else
		fixed_point_scale = 524288.0f;

	memset(prev_vertex, 0, sizeof(prev_vertex));
	end = 0;
	index = 0;
	polygon_index = 0;
	vb_index = 0;

	do {
		UINT32 header[7];
		UINT32 entry[16];
		D3DCOLOR color;
		int transparency;
		int i, num_vertices, num_old_vertices;
		int v2, v;
		int texture_x, texture_y, tex_num, depth, tex_width, tex_height;
		float nx, ny, nz;

		if(little_endian) {
			for( i=0; i<7; i++) {
				header[i] = src[index];
				index++;
			}
		} else {
			for( i=0; i<7; i++) {
				header[i] = BSWAP32(src[index]);
				index++;
			}
		}

		uv_scale = (header[1] & 0x40) ? 1.0f : 8.0f;

		if(header[0] == 0)
			return;

		// Check if this is the last polygon
		if(header[1] & 0x4)
			end = 1;

		transparency = ((header[6] >> 18) & 0x1F) << 3;
		if(header[6] & 0x800000) {
			color = 0xFF000000 | (header[4] >> 8);
		} else {
			color = (transparency << 24) | (header[4] >> 8);
		}

		// Polygon normal
		// Assuming 2.22 fixed-point. Is this correct ?
		nx = (float)(header[1] >> 8) / 4194304.0f;
		ny = (float)(header[2] >> 8) / 4194304.0f;
		nz = (float)(header[3] >> 8) / 4194304.0f;

		// If bit 0x40 set this is a quad, otherwise a triangle
		if(header[0] & 0x40)
			num_vertices = 4;
		else
			num_vertices = 3;

		// How many vertices are reused
		num_old_vertices = num_bits[header[0] & 0xF];

		// Load reused vertices
		v2 = 0;
		for( v=0; v<4; v++) {
			if( header[0] & (1 << v) ) {
				memcpy( &vertex[v2], &prev_vertex[v], sizeof(VERTEX) );
				vertex[v2].nx = nx;
				vertex[v2].ny = ny;
				vertex[v2].nz = nz;
				vertex[v2].color = color;
				v2++;
			}
		}

		// Load vertex data
		if(little_endian) {
			for( i=0; i<(num_vertices - num_old_vertices) * 4; i++) {
				entry[i] = src[index];
				index++;
			}
		} else {
			for( i=0; i<(num_vertices - num_old_vertices) * 4; i++) {
				entry[i] = BSWAP32(src[index]);
				index++;
			}
		}

		// Load new vertices
		for( v=0; v < (num_vertices - num_old_vertices); v++) {
			int ix, iy, iz;
			UINT16 tx,ty;
			int xsize, ysize;
			int v_index = v * 4;

			ix = entry[v_index];
			iy = entry[v_index + 1];
			iz = entry[v_index + 2];

			vertex[v2].x = (float)(ix) / fixed_point_scale;
			vertex[v2].y = (float)(iy) / fixed_point_scale;
			vertex[v2].z = (float)(iz) / fixed_point_scale;

			tx = (UINT16)((entry[v_index + 3] >> 16) & 0xFFFF);
			ty = (UINT16)(entry[v_index + 3] & 0xFFFF);
			xsize = 32 << ((header[3] >> 3) & 0x7);
			ysize = 32 << (header[3] & 0x7);

			// Convert texture coordinates from 13.3 fixed-point to float
			// Tex coords need to be divided by texture size because D3D wants them in
			// range 0...1
			vertex[v2].u = ((float)(tx) / uv_scale) / (float)xsize;
			vertex[v2].v = ((float)(ty) / uv_scale) / (float)ysize;
			vertex[v2].nx = nx;
			vertex[v2].ny = ny;
			vertex[v2].nz = nz;
			vertex[v2].color = color;

			v2++;
		}

		// Get the texture number from texture table
		page = (header[4] & 0x40) ? 1 : 0;
		texture_x = ((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 0x1);
		texture_y = (header[5] & 0x1F) | (page ? 0x20 : 0);
		depth = header[6];
		tex_width	= 32 << ((header[3] >> 3) & 0x7);
		tex_height	= 32 << (header[3] & 0x7);

		tex_num = texture_table[texture_y * 64 + texture_x];
		if(tex_num == 0)
			tex_num = cache_texture(texture_x * 32,texture_y * 32, tex_width, tex_height, depth);
		if((header[6] & 0x4000000) == 0)
			tex_num = -1;

		// Calculate normals
		{
			D3DXVECTOR3 v1,v2,v3;
			D3DXVECTOR3 n1,n2;
			D3DXVECTOR3 r1;
			v1.x = vertex[0].x; v1.y = vertex[0].y; v1.z = vertex[0].z;
			v2.x = vertex[1].x; v2.y = vertex[1].y; v2.z = vertex[1].z;
			v3.x = vertex[2].x; v3.y = vertex[2].y; v3.z = vertex[2].z;
			D3DXVec3Subtract( &n1, &v2, &v1 );
			D3DXVec3Subtract( &n2, &v3, &v2 );
			D3DXVec3Cross( &r1, &n1, &n2 );
			D3DXVec3Normalize( &r1, &r1 );
			for( i=0; i<4; i++) {
				vertex[i].nx = r1.x;
				vertex[i].ny = r1.y;
				vertex[i].nz = r1.z;
			}
		}

		if(tex_num)
			IDirect3DDevice9_SetTexture( device, 0, texture[tex_num] );
		else
			IDirect3DDevice9_SetTexture( device, 0, NULL );

		if(header[2] & 0x2)
			IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
		else
			IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
		if(header[2] & 0x1)
			IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
		else
			IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

		if(header[6] & 0x80000000) {
			IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		} else {
			IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
		}

		if(num_vertices == 3) {
			IDirect3DDevice9_DrawPrimitiveUP( device, D3DPT_TRIANGLELIST, 1, &vertex, sizeof(VERTEX) );
		} else {
			IDirect3DDevice9_DrawPrimitiveUP( device, D3DPT_TRIANGLEFAN, 2, &vertex, sizeof(VERTEX) );
		}
		// Copy current vertex as previous vertex
		memcpy( prev_vertex, vertex, sizeof(VERTEX) * 4 );
	} while(end == 0);
}

static void load_matrix(int address, D3DMATRIX *matrix)
{
	int index = matrix_start + ((address & 0xFFF) * 12);

	matrix->_11 = *(float*)(&list_ram[index + 3]);
	matrix->_21 = *(float*)(&list_ram[index + 6]);
	matrix->_31 = *(float*)(&list_ram[index + 9]);
	matrix->_12 = *(float*)(&list_ram[index + 4]);
	matrix->_22 = *(float*)(&list_ram[index + 7]);
	matrix->_32 = *(float*)(&list_ram[index + 10]);
	matrix->_13 = *(float*)(&list_ram[index + 5]);
	matrix->_23 = *(float*)(&list_ram[index + 8]);
	matrix->_33 = *(float*)(&list_ram[index + 11]);
	matrix->_14 = *(float*)(&list_ram[index + 0]);
	matrix->_24 = *(float*)(&list_ram[index + 1]);
	matrix->_34 = *(float*)(&list_ram[index + 2]);
	matrix->_41 = 0.0f;
	matrix->_42 = 0.0f;
	matrix->_43 = 0.0f;
	matrix->_44 = 1.0f;
	D3DXMatrixTranspose( matrix, matrix );
}

static void load_coord_sys(int address, D3DMATRIX *matrix)
{
	int index = matrix_start + (address * 12);

	matrix->_11 = *(float*)(&list_ram[index + 6]);
	matrix->_12 = *(float*)(&list_ram[index + 7]);
	matrix->_13 = *(float*)(&list_ram[index + 8]);
	matrix->_21 = *(float*)(&list_ram[index + 9]);
	matrix->_22 = -*(float*)(&list_ram[index + 10]);
	matrix->_23 = *(float*)(&list_ram[index + 11]);
	matrix->_31 = *(float*)(&list_ram[index + 3]);
	matrix->_32 = *(float*)(&list_ram[index + 4]);
	matrix->_33 = *(float*)(&list_ram[index + 5]);
	matrix->_41 = *(float*)(&list_ram[index + 0]);
	matrix->_42 = *(float*)(&list_ram[index + 1]);
	matrix->_43 = *(float*)(&list_ram[index + 2]);
	matrix->_14 = 0.0f;
	matrix->_24 = 0.0f;
	matrix->_34 = 0.0f;
	matrix->_44 = 1.0f;
}

static UINT32* get_address(UINT32 address)
{
	UINT32 a = address & 0x1FFFFFF;
	
	//printf("address: %08X\n",address);
	if(a < 0x80000) {
		return (UINT32*)(&cull_ram[a]);
	}
	else if(a >= 0x800000 && a < 0x840000) {
		return (UINT32*)(&list_ram[a - 0x800000]);
	}
	else if(a >= 0x1000000 && a < 0x1080000) {
		return (UINT32*)(&poly_ram[a - 0x1000000]);
	}
	else if(a >= 0x1800000 && a < 0x1FFFFFF) {
		return (UINT32*)(&vrom[a - 0x1800000]);
	}
	else {
		return 0;
	}
}

static void traverse_pointer_list(UINT32 address)
{
	int i;
	UINT32* list;
	list = get_address( address );
	if(list == NULL)
		error("traverse_pointer_list: invalid address %08X",address);

	for( i=0; i<2; i++) {
		UINT32 link = list[i];

		traverse_node( link );
	}
}

static void traverse_list(UINT32 address)
{
	int end = 0;
	int index = 0;
	UINT32* list;
	list = get_address( address );
	if(list == NULL)
		error("traverse_list: invalid address %08X",address);

	do {
		UINT32 link = list[index];
		int type = (link >> 24) & 0xFF;

		// Test for invalid links
		if(link == 0x800800 || (link & 0xFFFFFF) == 0)
			return;

		// Check if this is the final node of the list
		if((type & 0x2) == 0x2)
			end = 1;
			
		traverse_node( link & 0xFFFFFF );

		index++;
	} while(end == 0);
}

static void traverse_node(UINT32 address)
{
	int i;
	float x, y, z;
	int offset = 0;
	BOOL pushed = FALSE;
	UINT32 matrix_address;
	D3DMATRIX matrix, *world_matrix;

	UINT32 link,next;
	UINT32* node_ram;
	node_ram = get_address( address );
	if(node_ram == NULL)
		error("traverse_node: invalid address %08X",address);

	if(m3_config.step == 0x10)
		offset = 2;

	matrix_address = node_ram[0x3 - offset];

	matrix_stack->lpVtbl->Push( matrix_stack );
	pushed = TRUE;

	if((matrix_address & 0xFFF) != 0) {
		load_matrix(matrix_address & 0xFFF, &matrix);
		matrix_stack->lpVtbl->MultMatrixLocal( matrix_stack, &matrix );
	}
	
	x = *(float*)(&node_ram[0x4 - offset]);
	y = *(float*)(&node_ram[0x5 - offset]);
	z = *(float*)(&node_ram[0x6 - offset]);
	matrix_stack->lpVtbl->TranslateLocal( matrix_stack, x, y, z );

	world_matrix = matrix_stack->lpVtbl->GetTop( matrix_stack );
	IDirect3DDevice9_SetTransform( device, D3DTS_WORLD, world_matrix );

	// Check both links
	for( i=0; i<2; i++) {
		UINT32 link = node_ram[0x7 + i - offset];
		int type = (link >> 24) & 0xFF;
		
		// Test for valid links
		if(link != 0x1000000 && link != 0x800800 && link != 0) {
			UINT16 scale = (node_ram[0x9 + i - offset] >> 16) & 0xFFFF;

			switch(type)
			{
			case 0:		// Node
				if(node_ram[0] & 0x8) {
					// Scud Race weird node
					traverse_pointer_list( link );
				} else {
					traverse_node( link );
				}
				break;
			case 1:		// Model
			case 3:
				if(link & 0x800000) {
					render_model( link, FALSE );
				} else {
					render_model( link, TRUE );
				}
				break;
			case 4:		// List
				traverse_list( link );
				break;
			default:
				error("traverse_node: Unknown type %d, %08X",type,link);
			}
		}
		if(pushed) {
			matrix_stack->lpVtbl->Pop( matrix_stack );
			pushed = FALSE;
		}
	}
}

static void traverse_main_tree(UINT32 address, int priority)
{
	int index = 0;
	UINT32 next,link;
	UINT32* node_ram;
	D3DMATERIAL9 material;
	D3DCOLORVALUE ambient;
	D3DCOLORVALUE diffuse;
	D3DCOLORVALUE material_color;
	D3DXVECTOR3 sun_vector;
	D3DLIGHT9 sun;
	D3DMATRIX projection;
	D3DMATRIX matrix;
	UINT8 amb;
	float sun_diffuse_color, sun_ambient_color;
	int tree_priority;
	float fog_color, fog_density;

	node_ram = get_address( address );
	if(node_ram == NULL)
		error("traverse_main_tree: invalid address %08X",address);

	memset(&sun, 0, sizeof(sun));
	memset(&material, 0, sizeof(material));

#ifdef ENABLE_LIGHTING
	sun_diffuse_color = *(float*)&node_ram[index + 7];
	amb = (node_ram[index + 36] >> 8) & 0xFF;
	sun_ambient_color = (float)amb / 255.0f;
	sun_vector.y = *(float*)&node_ram[index + 6];
	sun_vector.x = -*(float*)&node_ram[index + 5];
	sun_vector.z = -*(float*)&node_ram[index + 4];

	diffuse.r = diffuse.g = diffuse.b = sun_diffuse_color;
	ambient.r = ambient.g = ambient.b = sun_ambient_color;
	material.Diffuse = diffuse;
	material.Ambient = ambient;
	IDirect3DDevice9_SetMaterial( device, &material );

	// Set up sun light
	sun.Type = D3DLIGHT_DIRECTIONAL;
	sun.Ambient = ambient;
	sun.Diffuse = diffuse;
	sun.Direction = sun_vector;

	IDirect3DDevice9_SetLight( device, 0, &sun );
	IDirect3DDevice9_LightEnable( device, 0, TRUE );
#endif
	
	next = node_ram[index + 1];
	link = node_ram[index + 2];
	matrix_start = node_ram[index + 22] & 0xFFFF;


	// Set coordinate system
	load_coord_sys( 0, &matrix );
	IDirect3DDevice9_SetTransform( device, D3DTS_VIEW, &matrix );

	// Set viewport and projection
	{
		float l1,l2,t1,t2,r1,r2,b1,b2;
		float fov_x, fov_y;
		int i;
		INT x = (node_ram[index + 26] & 0xFFFF);
		INT y = (node_ram[index + 26] >> 16) & 0xFFFF;
		INT width = node_ram[index + 20] & 0xFFFF;
		INT height = (node_ram[index + 20] >> 16) & 0xFFFF;

		set_viewport( x >> 4, y >> 4, width >> 2, height >> 2 );
		
		l1 = asin(*(float*)(&node_ram[index + 12]));
		l2 = acos(*(float*)(&node_ram[index + 13]));
		t1 = asin(*(float*)(&node_ram[index + 14]));
		t2 = acos(*(float*)(&node_ram[index + 15]));
		r1 = asin(*(float*)(&node_ram[index + 16]));
		r2 = acos(*(float*)(&node_ram[index + 17]));
		b1 = asin(*(float*)(&node_ram[index + 18]));
		b2 = acos(*(float*)(&node_ram[index + 19]));

		fov_x = l1 + r1;
		fov_y = t1 + b1;

		D3DXMatrixPerspectiveFovLH( &projection,fov_y, (float)width / (float)height, 0.1f, 100000.0f );
		IDirect3DDevice9_SetTransform( device, D3DTS_PROJECTION, &projection );
	}
	
	// Set up fog
	
	fog_density = *(float*)&node_ram[index + 35];
	fog_color = node_ram[index + 34];
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGENABLE, TRUE );
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGTABLEMODE, D3DFOG_EXP );
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGDENSITY, *(DWORD*)&fog_density );
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGCOLOR, fog_color );

	// Traverse to the node if the priority is correct
	tree_priority = (node_ram[0] >> 3) & 0x3;
	if(tree_priority == priority) {
		if(((link >> 24) & 0xFF) == 0) {
			traverse_node( link );
		}
	}

	// Go to next tree node if valid link
	if((next & 0x800000) != 0) {
		traverse_main_tree( next, priority );
	}
}

static void render_scene(void)
{
	int end = 0;
	int index = 0;
	HRESULT hr;

	hr = D3DXCreateMatrixStack( 0, &matrix_stack );
	if(FAILED(hr))
		osd_error("D3DXCreateMatrixStack failed.");

	matrix_stack->lpVtbl->LoadIdentity( matrix_stack );

	IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0 );
	traverse_main_tree( 0x800000, 0 );
	IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0 );
	traverse_main_tree( 0x800000, 1 );
	IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0 );
	traverse_main_tree( 0x800000, 2 );
	IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0 );
	traverse_main_tree( 0x800000, 3 );

	matrix_stack->lpVtbl->Release( matrix_stack );
}