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

typedef struct {
	int tex_num;
	int vb_index;
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
static D3DVIEWPORT9			viewport;

static LPDIRECT3DVERTEXBUFFER9	vertex_buffer;
static POLYGON_LIST polygon_list[32768];

static LPDIRECT3DTEXTURE9	texture[4096];
static short texture_table[64*64];

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

static void traverse_node(UINT32*);
static void render_scene(void);

/*
 * void osd_renderer_init(UINT8 *culling_ram_ptr, UINT8 *polygon_ram_ptr,
 *                        UINT8 *vrom_ptr);
 *
 * Initializes the renderer.
 *
 * Parameters:
 *      list_ram_ptr = Pointer to Real3D display list RAM
 *      poly_ram_ptr = Pointer to Real3D polygon RAM.
 *      vrom_ptr     = Pointer to VROM.
 */

void osd_renderer_init(UINT8 *list_ram_ptr, UINT8 *poly_ram_ptr, UINT8 *vrom_ptr)
{
	D3DPRESENT_PARAMETERS		d3dpp;
	D3DDISPLAYMODE				d3ddm;
	D3DCAPS9					caps;
	HRESULT hr;
	DWORD flags = 0;
	int i, num_buffers, width, height;

	list_ram	= (UINT32*)list_ram_ptr;
	poly_ram	= (UINT32*)poly_ram_ptr;
	vrom		= (UINT32*)vrom_ptr;

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
		osd_error("IDirect3D9_CreateDevice failed.");

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

	// Create vertex buffer
	hr = IDirect3DDevice9_CreateVertexBuffer( device, sizeof(VERTEX) * 32768, D3DUSAGE_DYNAMIC,
											  D3DFVF_VERTEX, D3DPOOL_DEFAULT, &vertex_buffer, NULL );
	if(FAILED(hr))
		osd_error("IDirect3DDevice9_CreateVertexBuffer failed.");
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
	D3DMATRIX projection;
	D3DMATRIX world;
	D3DMATRIX view;
	RECT src_rect = { 0, 0, MODEL3_SCREEN_WIDTH - 1, MODEL3_SCREEN_HEIGHT - 1 };

	D3DXMatrixPerspectiveFovLH( &projection, D3DXToRadian(30.0f), 496.0f / 384.0f, 0.1f, 100000.0f );
	D3DXMatrixIdentity( &world );
	D3DXMatrixIdentity( &view );
	//D3DXMatrixScaling( &view, 1.5f, 1.5f, 1.0f );


	IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0 );

	IDirect3DDevice9_BeginScene( device );
	IDirect3DDevice9_SetTransform( device, D3DTS_PROJECTION, &projection );
	IDirect3DDevice9_SetTransform( device, D3DTS_WORLD, &world );
	IDirect3DDevice9_SetTransform( device, D3DTS_VIEW, &view );

	IDirect3DDevice9_SetFVF( device, D3DFVF_VERTEX );
	IDirect3DDevice9_SetStreamSource( device, 0, vertex_buffer, 0, sizeof(VERTEX) );
	IDirect3DDevice9_SetRenderState( device, D3DRS_CULLMODE, D3DCULL_NONE );
	IDirect3DDevice9_SetRenderState( device, D3DRS_LIGHTING, FALSE );
	IDirect3DDevice9_SetRenderState( device, D3DRS_ZENABLE, D3DZB_USEW );

	sprite->lpVtbl->Draw(sprite, layer_data[2], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF);

	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	IDirect3DDevice9_SetTextureStageState( device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

#if 0
	IDirect3DDevice9_SetRenderState( device, D3DRS_FILLMODE, D3DFILL_WIREFRAME );
#endif

	render_scene();

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

static void decode_texture16(UINT16* ptr, UINT16* src, int width, int height)
{
	int i, j, x, y;
	int index = 0;

	for( j=0; j<height; j+=8 ) {
		for( i=0; i<width; i+=8 ) {

			// Decode tile
			for( y=0; y<8; y++) {
				for( x=0; x<8; x++) {
					int x2 = ((x / 2) * 4) + (x & 0x1);
					int y2 = ((y / 2) * 16) + ((y & 0x1) * 2);
					UINT16 pix = src[index + x2 + y2];

					ptr[((j+y) * width) + i + x] = pix;
				}
			}
			index += 64;
		}
	}
}

static void decode_texture8(UINT8 *ptr, UINT8 *src, int width, int height)
{
	int i, j, x, y;
	int index = 0;

	for( j=0; j<height; j+=8 ) {
		for( i=0; i<width; i+=8 ) {

			// Decode tile
			for( y=0; y<8; y++) {
				for( x=0; x<8; x++) {
					int x2 = ((x / 2) * 4) + (x & 0x1);
					int y2 = ((y / 2) * 16) + ((y & 0x1) * 2);
					UINT8 pix = src[index + x2 + y2];

					ptr[((j+y) * width) + i + x] = pix;
				}
			}
			index += 64;
		}
	}
}

void osd_renderer_upload_texture(UINT8 *src)
{
	UINT32 header[2];
	int texture_num, i, j;
	int width, height, xpos, ypos, type, depth;
	D3DLOCKED_RECT	locked_rect;
	HRESULT hr;
	UINT8 *ptr;
	D3DFORMAT	format = 0;

	// Find a free D3D texture
	texture_num = -1;
	i = 0;
	while( i < 4096 && texture_num == -1) {
		if(texture[i] == NULL) {
			texture_num = i;
		}
		i++;
	}
	if(texture_num < 0) {
		osd_error("DirectX Error: No free textures available !\n");
	}

	// Get the texture information from the header
	header[0] = *(UINT32*)(&src[0]);
	header[1] = *(UINT32*)(&src[4]);
	
	width	= 32 << ((header[1] >> 14) & 0x7);
	height	= 32 << ((header[1] >> 17) & 0x7);
	xpos	= (header[1] & 0x3F) * 32;
	ypos	= ((header[1] >> 7) & 0x1F) * 32;
	type	= (header[1] >> 24) & 0xFF;
	depth	= (header[1] & 0x800000) ? 1 : 0;

	if(type != 1)		// TODO: Handle mipmaps
		return;

	format = (depth) ? D3DFMT_A1R5G5B5 : D3DFMT_L8;

	// Create D3D texture
	hr = D3DXCreateTexture( device, width, height, 1, 0, format, D3DPOOL_MANAGED,
							&texture[texture_num] );
	if( FAILED(hr) )
		osd_error("D3DXCreateTexture failed.\n");
	
	hr = IDirect3DTexture9_LockRect( texture[texture_num], 0, &locked_rect, NULL, 0 );
	if( FAILED(hr) )
		osd_error("IDirect3DTexture9_LockRect failed.\n");

	ptr = (UINT8*)locked_rect.pBits;

	// Decode the texture
	if(depth) {
		decode_texture16( (UINT16*)&ptr[0], (UINT16*)&src[8], width, height );
	} else {
		decode_texture8( ptr, &src[8], width, height );
	}
	IDirect3DTexture9_UnlockRect( texture[texture_num], 0 );

	// Update texture table
	for( j = ypos/32; j < (ypos+height)/32; j++) {
		for( i = xpos/32; i < (xpos+width)/32; i++) {
			texture_table[(j*64) + i] = texture_num;
			// TODO: Remove overwritten textures
		}
	}
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

static void render_model(UINT32 *src)
{
	VERTEX vertex[4];
	VERTEX prev_vertex[4];
	int end, index, polygon_index, vb_index, prev_texture, i;
	VERTEX  *vb;
	HRESULT hr;

	memset(prev_vertex, 0, sizeof(prev_vertex));
	end = 0;
	index = 0;
	polygon_index = 0;
	vb_index = 0;

	hr = IDirect3DVertexBuffer9_Lock( vertex_buffer, 0, 0, (void**)&vb, D3DLOCK_DISCARD );
	if(FAILED(hr))
		osd_error("IDirect3DVertexBuffer9_Lock failed.");

	do {
		UINT32 header[7];
		D3DCOLOR color;
		int i, num_vertices, num_old_vertices;
		int v2, v;
		int texture_x, texture_y, tex_num;
		float nx, ny, nz;

		for( i=0; i<7; i++) {
			header[i] = src[index];
			index++;
		}

		// Check if this is the last polygon
		if(header[1] & 0x4)
			end = 1;

		color = 0xFF000000 | (header[4] >> 8);

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

		// Load new vertices
		for( v=0; v < (num_vertices - num_old_vertices); v++) {
			int ix, iy, iz;
			short tx,ty;
			int xsize, ysize;

			ix = src[index];
			iy = src[index + 1];
			iz = src[index + 2];
			index += 3;

			vertex[v2].x = (float)(ix) / 524288.0f;
			vertex[v2].y = (float)(iy) / 524288.0f;
			vertex[v2].z = (float)(iz) / 524288.0f;

			tx = (short)((src[index] >> 16) & 0xFFFF);
			ty = (short)(src[index] & 0xFFFF);
			xsize = 32 << ((header[3] >> 3) & 0x7);
			ysize = 32 << (header[3] & 0x7);
			index++;

			// Convert texture coordinates from 13.3 fixed-point to float
			// Tex coords need to be divided by texture size because D3D wants them in
			// range 0...1
			vertex[v2].u = ((float)(tx) / 8.0f) / (float)xsize;
			vertex[v2].v = ((float)(ty) / 8.0f) / (float)ysize;
			
			vertex[v2].nx = nx;
			vertex[v2].ny = ny;
			vertex[v2].nz = nz;
			vertex[v2].color = color;

			v2++;
		}

		// Get the texture number from texture table
		texture_x = ((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 0x1);
		texture_y = (header[5] & 0x1F);
		tex_num = texture_table[texture_y * 64 + texture_x];
		if((header[6] & 0x4000000) == 0)
			tex_num = -1;

		if(num_vertices == 3) {
			polygon_list[polygon_index].tex_num = tex_num;
			polygon_list[polygon_index].vb_index = vb_index;
			
			memcpy(&vb[vb_index], &vertex[0], sizeof(VERTEX) * 3);
			
			polygon_index++;
			vb_index += 3;
		} else {
			polygon_list[polygon_index].tex_num = tex_num;
			polygon_list[polygon_index].vb_index = vb_index;
			polygon_list[polygon_index+1].tex_num = tex_num;
			polygon_list[polygon_index+1].vb_index = vb_index + 3;

			memcpy(&vb[vb_index], &vertex[0], sizeof(VERTEX) * 3);
			memcpy(&vb[vb_index+3], &vertex[2], sizeof(VERTEX) * 2);
			memcpy(&vb[vb_index+5], &vertex[0], sizeof(VERTEX));
			
			polygon_index += 2;
			vb_index += 6;
		}
		// Copy current vertex as previous vertex
		memcpy( prev_vertex, vertex, sizeof(VERTEX) * 4 );
	} while(end == 0);

	IDirect3DVertexBuffer9_Unlock( vertex_buffer );
	// Sort the polygons per texture
	qsort(polygon_list, polygon_index, sizeof(POLYGON_LIST), polylist_compare);

	IDirect3DDevice9_SetTexture( device, 0, NULL );
	prev_texture = -1;

	for( i=0; i<polygon_index; i++) {
		if(polygon_list[i].tex_num > prev_texture)
			IDirect3DDevice9_SetTexture( device, 0, texture[polygon_list[i].tex_num] );
		IDirect3DDevice9_DrawPrimitive( device, D3DPT_TRIANGLELIST, polygon_list[i].vb_index, 1 );
	}
}

// render_model_vrom
//
// Renders a model in vrom
// TODO: Cache vrom models !

static void render_model_vrom(UINT32 *src)
{
	VERTEX vertex[4];
	VERTEX prev_vertex[4];
	int end, index, polygon_index, vb_index, prev_texture, i;
	VERTEX  *vb;
	HRESULT hr;

	memset(prev_vertex, 0, sizeof(VERTEX) * 4);
	end = 0;
	index = 0;
	polygon_index = 0;
	vb_index = 0;

	hr = IDirect3DVertexBuffer9_Lock( vertex_buffer, 0, 0, (void**)&vb, D3DLOCK_DISCARD );
	if(FAILED(hr))
		osd_error("IDirect3DVertexBuffer9_Lock failed.");

	do {
		UINT32 header[7];
		D3DCOLOR color;
		int i, num_vertices, num_old_vertices;
		int v2, v;
		int texture_x, texture_y, tex_num;
		float nx, ny, nz;

		for( i=0; i<7; i++) {
			header[i] = BSWAP32(src[index]);
			index++;
		}

		// Check if this is the last polygon
		if(header[1] & 0x4)
			end = 1;

		color = 0xFF000000 | (header[4] >> 8);

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

		// Load new vertices
		for( v=0; v < (num_vertices - num_old_vertices); v++) {
			int ix, iy, iz;
			short tx,ty;
			int xsize, ysize;

			ix = BSWAP32(src[index]);
			iy = BSWAP32(src[index + 1]);
			iz = BSWAP32(src[index + 2]);
			index += 3;

			vertex[v2].x = (float)(ix) / 524288.0f;
			vertex[v2].y = (float)(iy) / 524288.0f;
			vertex[v2].z = (float)(iz) / 524288.0f;

			tx = (short)((BSWAP32(src[index]) >> 16) & 0xFFFF);
			ty = (short)(BSWAP32(src[index]) & 0xFFFF);
			xsize = 32 << ((header[3] >> 3) & 0x7);
			ysize = 32 << (header[3] & 0x7);
			index++;

			// Convert texture coordinates from 13.3 fixed-point to float
			// Tex coords need to be divided by texture size because D3D wants them in
			// range 0...1
			vertex[v2].u = ((float)(tx) / 8.0f) / (float)xsize;
			vertex[v2].v = ((float)(ty) / 8.0f) / (float)ysize;

			vertex[v2].nx = nx;
			vertex[v2].ny = ny;
			vertex[v2].nz = nz;
			vertex[v2].color = color;
			
			v2++;
		}

		// Get the texture number from texture table
		texture_x = ((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 0x1);
		texture_y = (header[5] & 0x1F);
		tex_num = texture_table[texture_y * 64 + texture_x];
		if((header[6] & 0x4000000) == 0)
			tex_num = -1;

		if(num_vertices == 3) {
			polygon_list[polygon_index].tex_num = tex_num;
			polygon_list[polygon_index].vb_index = vb_index;
			
			memcpy(&vb[vb_index], &vertex[0], sizeof(VERTEX) * 3);
			
			polygon_index++;
			vb_index += 3;
		} else {
			polygon_list[polygon_index].tex_num = tex_num;
			polygon_list[polygon_index].vb_index = vb_index;
			polygon_list[polygon_index+1].tex_num = tex_num;
			polygon_list[polygon_index+1].vb_index = vb_index + 3;

			memcpy(&vb[vb_index], &vertex[0], sizeof(VERTEX) * 3);
			memcpy(&vb[vb_index+3], &vertex[2], sizeof(VERTEX) * 2);
			memcpy(&vb[vb_index+5], &vertex[0], sizeof(VERTEX));
			
			polygon_index += 2;
			vb_index += 6;
		}
		// Copy current vertex as previous vertex
		memcpy( prev_vertex, vertex, sizeof(VERTEX) * 4 );
	} while(end == 0);

	IDirect3DVertexBuffer9_Unlock( vertex_buffer );
	// Sort the polygons per texture
	qsort(polygon_list, polygon_index, sizeof(POLYGON_LIST), polylist_compare);

	IDirect3DDevice9_SetTexture( device, 0, NULL );
	prev_texture = -1;

	for( i=0; i<polygon_index; i++) {
		if(polygon_list[i].tex_num > prev_texture)
			IDirect3DDevice9_SetTexture( device, 0, texture[polygon_list[i].tex_num] );
		IDirect3DDevice9_DrawPrimitive( device, D3DPT_TRIANGLELIST, polygon_list[i].vb_index, 1 );
	}
}

static void load_matrix(int address)
{
	D3DMATRIX matrix;
	int index = 0x2000 + (address * 12);	// FIXME: Is the base fixed ?

	matrix._11 = *(float*)(&list_ram[index + 3]);
	matrix._12 = *(float*)(&list_ram[index + 6]);
	matrix._13 = *(float*)(&list_ram[index + 9]);
	matrix._21 = *(float*)(&list_ram[index + 4]);
	matrix._22 = *(float*)(&list_ram[index + 7]);
	matrix._23 = *(float*)(&list_ram[index + 10]);
	matrix._31 = *(float*)(&list_ram[index + 5]);
	matrix._32 = *(float*)(&list_ram[index + 8]);
	matrix._33 = *(float*)(&list_ram[index + 11]);
	matrix._41 = *(float*)(&list_ram[index + 0]);
	matrix._42 = *(float*)(&list_ram[index + 1]);
	matrix._43 = *(float*)(&list_ram[index + 2]);
	matrix._14 = 0.0f;
	matrix._24 = 0.0f;
	matrix._34 = 0.0f;
	matrix._44 = 1.0f;

	IDirect3DDevice9_SetTransform( device, D3DTS_WORLD, &matrix );
}

static void traverse_list(UINT32* list)
{
	int end = 0;
	int index = 0;
	do {
		UINT32 link = list[index];
		int type = (link >> 24) & 0xFF;

		// Test for invalid links
		if(link == 0x800800 || (link & 0xFFFFFF) == 0)
			return;

		// Check if this is the final node of the list
		if(type == 0x2)
			end = 1;

		if(link & 0x800000) {
			traverse_node( &list_ram[link & 0x7FFFFF] );  // Link is in 0x8E000000
		} else {
			traverse_node( &cull_ram[link & 0x7FFFFF] );  // Link is in 0x8C000000
		}
		index++;
	} while(end == 0);
}

static void traverse_node(UINT32* node_ram)
{
	int i;
	float x, y, z;
	UINT32 matrix_address = node_ram[0x3];
	
	if(((matrix_address >> 24) & 0xFF) == 0x20) {
		load_matrix(matrix_address & 0x7FFFFF);
	}

	// This is some kind of position. 
	x = *(float*)(&node_ram[0x4]);
	y = *(float*)(&node_ram[0x5]);
	z = *(float*)(&node_ram[0x6]);

	// Check both links
	// TODO: Scud Race has some special nodes which are bigger
	//       What are the last two words for ?
	for( i=0; i<2; i++) {
		UINT32 link = node_ram[0x7 + i];
		int type = (link >> 24) & 0xFF;
		
		// Test for valid links
		if(link != 0x1000000 && link != 0x800800 && link != 0) {

			switch(type)
			{
				case 0:		// Another node
					if(link & 0x800000) {
						traverse_node( &list_ram[link & 0x7FFFFF] );
					} else {
						traverse_node( &cull_ram[link & 0x7FFFFF] );
					}
					break;
				case 1:		// Model
					if(link & 0x800000) {
						render_model_vrom( &vrom[link & 0x7FFFFF] );
					} else {
						render_model( &poly_ram[link & 0x7FFFFF] );
					}
					break;
				case 4:		// List of nodes
					if(link & 0x800000) {
						traverse_list( &list_ram[link & 0x7FFFFF] );
					} else {
						traverse_list( &cull_ram[link & 0x7FFFFF] );
					}
					break;
				default:
					error("Error: traverse_node: invalid node link %08X\n",link);
			}
		}
	}
}

static void render_scene(void)
{
	int end = 0;
	int index = 0;
	do {
		UINT32 next = list_ram[index + 1];
		UINT32 link = list_ram[index + 2];
		
		// Test for valid link
		if(link & 0x800000 && ((link >> 24) & 0xFF) == 0) {
			traverse_node( &list_ram[link & 0x7FFFFF] );
		}

		// Check for end
		if((next & 0x800000) == 0)
			end = 1;

		index = next & 0x7FFFFF;
	} while(end == 0);
}