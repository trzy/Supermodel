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

#include "model3.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr9.h>

#include "dx_render.h"

#define WIREFRAME				0
#define ENABLE_ALPHA_BLENDING	1
#define DRAW_STATS				1
#define ENABLE_MODEL_CACHING	0

typedef struct {
	float x, y, z;
	float nx, ny, nz;
	D3DCOLOR color;
	D3DCOLOR specular;
	float u, v;
} VERTEX;

typedef struct {
	float x,y,z,w;
	float u,v;
} VERTEX2D;

typedef struct {
	D3DXVECTOR4 direction;
	D3DXVECTOR4 diffuse;
	D3DXVECTOR4 ambient;
} LIGHT2;

static char num_bits[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

#define D3DFVF_VERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0))
#define D3DFVF_VERTEX2D (D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0))

static LPDIRECT3D9			d3d;
static LPDIRECT3DDEVICE9	device;
static LPDIRECT3DTEXTURE9	layer_data[4];
static LPD3DXSPRITE			sprite;
static LPD3DXFONT			font;
static LPD3DXMATRIXSTACK	matrix_stack;
static LPDIRECT3DVERTEXSHADER9 vertex_shader;

static LIGHT2				light;

static LPDIRECT3DVERTEXBUFFER9	vertex_buffer;
static LPDIRECT3DVERTEXBUFFER9	vertex_buffer_2d;

static LPDIRECT3DTEXTURE9	texture[4096];
static short texture_table[64*64*4];
static int tris = 0;

static D3DMATRIX coordinate_system;
static D3DMATRIX projection;

static D3DFORMAT supported_formats[] = { D3DFMT_A1R5G5B5,
										 D3DFMT_A8R8G8B8,
										 D3DFMT_A8B8G8R8 };

static D3DFORMAT layer_format = 0;

static HWND main_window;
static HFONT hfont;

static int width;
static int height;

static UINT32* list_ram;	// Display List ram at 0x8E000000
static UINT32* cull_ram;	// Culling ram at 0x8C000000
static UINT32* poly_ram;	// Polygon ram at 0x98000000
static UINT32* vrom;		// VROM
static UINT8* texture_sheet;

static LONGLONG counter_start, counter_end, counter_frequency;

typedef struct {
	int refcount;
	UINT32 address;
	int vertex_index;
	int length;
} HASHTABLE;

#define HASH_TABLE_SIZE 2097152
static HASHTABLE model_hash_table[HASH_TABLE_SIZE];
static int cached_vertices = 0;

static void init_model_cache(void)
{
	int i;
	for( i=0; i < HASH_TABLE_SIZE; i++ ) {
		model_hash_table[i].refcount = 0;
		model_hash_table[i].address = 0;
	}
}

static HASHTABLE* cache_model(UINT32* mem,UINT32 address)
{
	VERTEX* vb;
	VERTEX vert[4];
	VERTEX prev_vertex[4];
	int hash_index;
	int index, end, vertex;
	float fixed_point_scale;

	address >>= 2;
	address &= 0x3FFFFF;
	hash_index = address % (HASH_TABLE_SIZE - 1);
	if( model_hash_table[hash_index].refcount > 0 ) {
		if( address == model_hash_table[hash_index].address ) {
			return &model_hash_table[hash_index];
		} else {
			error("Direct3D error: Hash table collision: %08X, %08X\n",
				address,model_hash_table[hash_index].address);
		}
	}

	end = 0;
	index = 0;
	vertex = 0;

	if(m3_config.step == 0x10)
		fixed_point_scale = 32768.0f;
	else
		fixed_point_scale = 524288.0f;

	model_hash_table[hash_index].refcount++;
	model_hash_table[hash_index].address = address;
	model_hash_table[hash_index].vertex_index = cached_vertices;

	IDirect3DVertexBuffer9_Lock( vertex_buffer, cached_vertices * sizeof(VERTEX), 4000 * sizeof(VERTEX), (void**)&vb, D3DLOCK_DISCARD );

	do {
		UINT32 header[7];
		UINT32 entry[16];
		D3DCOLOR color, specular;
		int transparency, luminance;
		int i, num_vertices, num_old_vertices;
		int v2, v;
		int texture_x, texture_y, tex_num, depth, tex_width, tex_height;
		float nx, ny, nz, uv_scale;

		for( i=0; i<7; i++) {
			header[i] = BSWAP32(mem[index]);
			index++;
		}

		// Check if this is the last polygon
		if(header[1] & 0x4)
			end = 1;

		uv_scale = (header[1] & 0x40) ? 1.0f : 8.0f;

		if(header[6] & 0x800000) {
			color = 0xFF000000 | (header[4] >> 8);
		} else {
			color = (transparency << 24) | (header[4] >> 8);
		}
		if(header[6] & 0x10000)
			luminance = 0xFF;

		specular = (luminance << 24);

		// Polygon normal
		// Assuming 2.22 fixed-point. Is this correct ?
		nx = (float)((int)header[1] >> 8) / 4194304.0f;
		ny = (float)((int)header[2] >> 8) / 4194304.0f;
		nz = (float)((int)header[3] >> 8) / 4194304.0f;

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
				memcpy( &vert[v2], &prev_vertex[v], sizeof(VERTEX) );
				vert[v2].nx = nx;
				vert[v2].ny = ny;
				vert[v2].nz = nz;
				vert[v2].color = color;
				vert[v2].specular = specular;
				v2++;
			}
		}

		// Load vertex data
		for( i=0; i<(num_vertices - num_old_vertices) * 4; i++) {
			entry[i] = BSWAP32(mem[index]);
			index++;
		}

		// Load new vertices
		for( v=0; v < (num_vertices - num_old_vertices); v++) {
			int ix, iy, iz, xsize, ysize;
			UINT16 tx,ty;
			int v_index = v * 4;

			ix = entry[v_index];
			iy = entry[v_index + 1];
			iz = entry[v_index + 2];
			vert[v2].x = (float)(ix) / fixed_point_scale;
			vert[v2].y = (float)(iy) / fixed_point_scale;
			vert[v2].z = (float)(iz) / fixed_point_scale;
			tx = (INT16)((entry[v_index + 3] >> 16) & 0xFFFF);
			ty = (INT16)(entry[v_index + 3] & 0xFFFF);
			xsize = 32 << ((header[3] >> 3) & 0x7);
			ysize = 32 << (header[3] & 0x7);
			vert[v2].u = ((float)(tx) / uv_scale) / (float)xsize;
			vert[v2].v = ((float)(ty) / uv_scale) / (float)ysize;
			vert[v2].nx = nx;
			vert[v2].ny = ny;
			vert[v2].nz = nz;
			vert[v2].color = color;
			vert[v2].specular = specular;

			v2++;
		}

		memcpy( &vb[vertex + 0], &vert[0], sizeof(VERTEX) * 3 );
		vertex += 3;
		cached_vertices += 3;
		if( num_vertices != 3 ) {
			memcpy( &vb[vertex + 0], &vert[0], sizeof(VERTEX) );
			memcpy( &vb[vertex + 1], &vert[2], sizeof(VERTEX) );
			memcpy( &vb[vertex + 2], &vert[3], sizeof(VERTEX) );
			vertex += 3;
			cached_vertices += 3;
		}

		memcpy( prev_vertex, vert, sizeof(VERTEX) * 4 );
	} while(end == 0);

	model_hash_table[hash_index].length = cached_vertices - model_hash_table[hash_index].vertex_index;

	IDirect3DVertexBuffer9_Unlock( vertex_buffer );
	return &model_hash_table[hash_index];
}

/*
 * BOOL init_d3d_renderer(HWND hWnd, int width, int height)
 *
 * Initializes the renderer.
 *
 * Parameters:
 *		HWND hWnd = Window handle
 *		int width = Width of the framebuffer
 *		int height = Height of the framebuffer
 */

BOOL d3d_init(HWND hWnd)
{
	D3DPRESENT_PARAMETERS		d3dpp;
	D3DDISPLAYMODE				d3ddm;
	D3DCAPS9					caps;
	D3DDISPLAYMODE				display_mode;
	HRESULT hr;
	DWORD flags = 0;
	LPD3DXBUFFER vsh, errors;
	int i, num_buffers;

	main_window = hWnd;

    atexit(d3d_shutdown);

	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if( !d3d ) {
		error("Direct3D error: Direct3DCreate9 failed.");
		return FALSE;
	}

	hr = IDirect3D9_GetAdapterDisplayMode( d3d, D3DADAPTER_DEFAULT, &d3ddm );
	if( FAILED(hr) ) {
		error("Direct3D error: IDirect3D9_GetAdapterDisplayMode failed.");
		return FALSE;
	}

	hr = IDirect3D9_GetDeviceCaps( d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps );
	if( FAILED(hr) ) {
		error("Direct3D error: IDirect3D9_GetDeviceCaps failed.");
		return FALSE;
	}

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
		INT modes = IDirect3D9_GetAdapterModeCount( d3d, D3DADAPTER_DEFAULT, d3ddm.Format );
		
		for( i=0; i<modes; i++) {
			hr = IDirect3D9_EnumAdapterModes( d3d, D3DADAPTER_DEFAULT, d3ddm.Format, i, &display_mode );
			if( FAILED(hr) )
				error("Direct3D error: IDirect3D9_EnumAdapterModes failed.\n");

			// Check if this mode matches
			if(display_mode.Width == m3_config.width && display_mode.Height == m3_config.height) {
				width	= m3_config.width;
				height	= m3_config.height;
			}
		}
	}
	// Check if we have a valid display mode
	if(width == 0 || height == 0) {
		error("Direct3D error: Display mode %dx%d not available\n",m3_config.width, m3_config.height );
		return FALSE;
	}

	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.Windowed					= m3_config.fullscreen ? FALSE : TRUE;
	d3dpp.SwapEffect				= m3_config.stretch ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferWidth			= width;
	d3dpp.BackBufferHeight			= height;
	d3dpp.BackBufferCount			= num_buffers - 1;
	d3dpp.hDeviceWindow				= main_window;
	d3dpp.PresentationInterval		= D3DPRESENT_INTERVAL_ONE;
	d3dpp.BackBufferFormat			= d3ddm.Format;
	d3dpp.EnableAutoDepthStencil	= TRUE;
	d3dpp.AutoDepthStencilFormat	= D3DFMT_D24X8;	// FIXME: All cards don't support 24-bit Z-buffer

	for( i=0; i < sizeof(supported_formats) / sizeof(D3DFORMAT); i++) {
		hr = IDirect3D9_CheckDeviceFormat( d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 
										   D3DUSAGE_DYNAMIC, D3DRTYPE_TEXTURE, 
										   supported_formats[i] );
		if( !FAILED(hr) ) {
			layer_format = supported_formats[i];
			break;
		}
	}
	if( !layer_format ) {
		error("Direct3D error: No supported pixel formats found !");
		return FALSE;
	}

	if( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) {

		if(caps.VertexShaderVersion < D3DVS_VERSION(1,1))
			flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		else
			flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;

	} else {
		flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	hr = IDirect3D9_CreateDevice( d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, main_window, 
								  flags, &d3dpp, &device );
	if( FAILED(hr) ) {
		error("Direct3D error: IDirect3D9_CreateDevice failed: %s",DXGetErrorString9(hr));
		return FALSE;
	}

	for( i=0; i<4; i++) {
		hr = D3DXCreateTexture( device, MODEL3_SCREEN_WIDTH, MODEL3_SCREEN_HEIGHT, 1,
								D3DUSAGE_DYNAMIC, layer_format, D3DPOOL_DEFAULT, &layer_data[i] );
		if( FAILED(hr) ) {
			error("Direct3D error: D3DXCreateTexture failed.");
			return FALSE;
		}
	}
	memset( texture, 0, sizeof(texture) );
	memset( texture_table, 0, sizeof(texture_table) );

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
	for( i=0; i < num_buffers + 1; i++ ) {
		IDirect3DDevice9_Clear( device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 0.0f, 0 );
		IDirect3DDevice9_Present( device, NULL, NULL, NULL, NULL );
	}

	hr = D3DXCreateSprite( device, &sprite );
	if( FAILED(hr) ) {
		error("Direct3D error: D3DXCreateSprite failed.");
		return FALSE;
	}

	// Create font for the on-screen display
	hfont = CreateFont( 14, 0,					// Width, Height
						0, 0,					// Escapement, Orientation
						FW_BOLD,				// Font weight
						FALSE, FALSE,			// Italic, underline
						FALSE,					// Strikeout
						ANSI_CHARSET,			// Charset
						OUT_DEFAULT_PRECIS,		// Precision
						CLIP_DEFAULT_PRECIS,	// Clip precision
						DEFAULT_QUALITY,		// Output quality
						DEFAULT_PITCH |			// Pitch and family
						FF_DONTCARE,
						"Terminal" );
	if( hfont == NULL ) {
		error("Direct3D error: Couldn't create font.");
		return FALSE;
	}

	// Create D3D font
	hr = D3DXCreateFont( device, hfont, &font );
	if( FAILED(hr) ) {
		error("Direct3D error: D3DXCreateFont failed.");
		return FALSE;
	}

	QueryPerformanceFrequency((LARGE_INTEGER*)&counter_frequency);

	D3DXCreateMatrixStack(0, &matrix_stack);
	d3d_matrix_stack_init();

#if ENABLE_MODEL_CACHING 1
	init_model_cache();
	hr = IDirect3DDevice9_CreateVertexBuffer( device, 1000000 * sizeof(VERTEX), 
		0, D3DFVF_VERTEX, D3DPOOL_MANAGED, &vertex_buffer, NULL );
	if( FAILED(hr) ) {
		error("Direct3D error: IDirect3DDevice9_CreateVertexBuffer failed.");
		return FALSE;
	}

	// Create vertex buffer for 2D
	{
		VERTEX2D v2d[6];
		VOID* v2d_buffer;
		hr = IDirect3DDevice9_CreateVertexBuffer( device, 6 * sizeof(VERTEX2D),
			0, D3DFVF_VERTEX2D, D3DPOOL_MANAGED, &vertex_buffer_2d, NULL );
		if( FAILED(hr) ) {
			error("Direct3D error: IDirect3DDevice9_CreateVertexBuffer failed.");
			return FALSE;
		}
		v2d[0].x = 0.0f;	v2d[0].y = 0.0f;	v2d[0].z = 0.0f;	v2d[0].w = 1.0f;
		v2d[0].u = 0.0f;	v2d[0].v = 0.0f;
		v2d[1].x = 495.0f;	v2d[1].y = 0.0f;	v2d[1].z = 0.0f;	v2d[1].w = 1.0f;
		v2d[1].u = 1.0f;	v2d[1].v = 0.0f;
		v2d[2].x = 495.0f;	v2d[2].y = 383.0f;	v2d[2].z = 0.0f;	v2d[2].w = 1.0f;
		v2d[2].u = 1.0f;	v2d[2].v = 1.0f;
		v2d[3].x = 0.0f;	v2d[3].y = 0.0f;	v2d[3].z = 0.0f;	v2d[3].w = 1.0f;
		v2d[3].u = 0.0f;	v2d[3].v = 0.0f;
		v2d[4].x = 495.0f;	v2d[4].y = 383.0f;	v2d[4].z = 0.0f;	v2d[4].w = 1.0f;
		v2d[4].u = 1.0f;	v2d[4].v = 1.0f;
		v2d[5].x = 0.0f;	v2d[5].y = 383.0f;	v2d[5].z = 0.0f;	v2d[5].w = 1.0f;
		v2d[5].u = 0.0f;	v2d[5].v = 1.0f;

		IDirect3DVertexBuffer9_Lock( vertex_buffer_2d, 0, 0, &v2d_buffer, 0 );
		memcpy( v2d_buffer, v2d, sizeof(VERTEX2D) * 6 );
		IDirect3DVertexBuffer9_Unlock( vertex_buffer_2d );
	}
#endif

	// Create the vertex shader
	hr = D3DXAssembleShaderFromFile( "m3/win32/dx_vertex_shader.vs", NULL, NULL, 0, &vsh, &errors );
	if( FAILED(hr) ) {
		error("Direct3D error: %s",errors->lpVtbl->GetBufferPointer(errors) );
		return FALSE;
	}
	hr = IDirect3DDevice9_CreateVertexShader( device, (DWORD*)vsh->lpVtbl->GetBufferPointer(vsh), &vertex_shader );
	if( FAILED(hr) ) {
		error("Direct3D error: IDirect3DDevice9_CreateVertexShader failed.");
		return FALSE;
	}

	return TRUE;
}

void d3d_shutdown(void)
{
	if( d3d ) {
		IDirect3D9_Release(d3d);
		d3d = NULL;
	}
}

static D3DMATRIX matrix_to_d3d( const MATRIX m )
{
	D3DMATRIX c;
	memcpy( &c, m, sizeof(MATRIX) );

	return c;
}

static void draw_text( int x, int y, char* string, DWORD color, BOOL shadow )
{
	RECT rect = { x, y, width-1, height-1 };
	RECT rect_s = { x+1, y+1, width-1, height-1 };
	if( shadow )
		font->lpVtbl->DrawText( font, string, -1, &rect_s, 0, 0xFF000000 );

	font->lpVtbl->DrawText( font, string, -1, &rect, 0, color );
}



////////////////////////////
//   Renderer Interface   //
////////////////////////////

/*
 * void osd_renderer_set_memory(UINT8 *culling_ram_8e_ptr,
 *                              UINT8 *culling_ram_8c_ptr,
 *                              UINT8 *polygon_ram_ptr,
 *                              UINT8 *texture_ram_ptr,
 *                              UINT8 *vrom_ptr);
 *
 * Receives the Real3D memory regions.
 *
 * Currently, this function checks the Model 3 stepping and configures the
 * renderer appropriately.
 *
 * Parameters:
 *      culling_ram_8e_ptr = Pointer to Real3D culling RAM at 0x8E000000.
 *      culling_ram_8c_ptr = Pointer to Real3D culling RAM at 0x8C000000.
 *      polygon_ram_ptr    = Pointer to Real3D polygon RAM.
 *      texture_ram_ptr    = Pointer to Real3D texture RAM.
 *      vrom_ptr           = Pointer to VROM.
 */

void osd_renderer_set_memory(UINT8 *culling_ram_8e_ptr,
                             UINT8 *culling_ram_8c_ptr,
                             UINT8 *polygon_ram_ptr, UINT8 *texture_ram_ptr,
                             UINT8 *vrom_ptr)
{
    list_ram = (UINT32*)culling_ram_8e_ptr;
    cull_ram = (UINT32*)culling_ram_8c_ptr;
    poly_ram = (UINT32*)polygon_ram_ptr;
    texture_sheet = texture_ram_ptr;
    vrom = vrom_ptr;
}

/*
 * void osd_renderer_set_viewport(const VIEWPORT *vp);
 *
 * Sets up a viewport. Enables Z-buffering.
 *
 * Parameters:
 *      vp = Viewport and projection parameters.
 */

void osd_renderer_set_viewport(const VIEWPORT* vp)
{
	D3DVIEWPORT9	viewport;

	float fov = D3DXToRadian( (float)(vp->up + vp->down) );
	float aspect_ratio = (float)((float)vp->width / (float)vp->height);

	memset(&viewport, 0, sizeof(D3DVIEWPORT9));
	viewport.X		= vp->x;
	viewport.Y		= vp->y;
	viewport.Width	= vp->width;
	viewport.Height	= vp->height;
	viewport.MinZ	= 0.1f;
	viewport.MaxZ	= 100000.0f;

	IDirect3DDevice9_SetViewport( device, &viewport );

	D3DXMatrixPerspectiveFovLH( &projection, fov, aspect_ratio, 0.1f, 100000.0f );
	IDirect3DDevice9_SetTransform( device, D3DTS_PROJECTION, &projection );
}

void osd_renderer_set_color_offset( BOOL is_enabled, FLOAT32 r, FLOAT32 g, FLOAT32 b )
{

}

/******************************************************************/
/* Viewport and Projection                                        */
/******************************************************************/

/*
 * void osd_renderer_set_coordinate_system(const MATRIX m);
 *
 * Applies the coordinate system matrix and makes adjustments so that the
 * Model 3 coordinate system is properly handled.
 *
 * Parameters:
 *      m = Matrix.
 */

void osd_renderer_set_coordinate_system( const MATRIX m )
{
	memcpy( &coordinate_system, m, sizeof(MATRIX) );
	coordinate_system._22 = -coordinate_system._22;
}

void osd_renderer_set_light( int light_num, LIGHT* param )
{
	D3DXVECTOR4 direction;
	D3DXVECTOR4 diffuse, ambient;

	switch(param->type)
	{
		case LIGHT_PARALLEL:
			direction.x = param->u * coordinate_system._11;
			direction.y = param->v * coordinate_system._22;
			direction.z = -param->w * coordinate_system._33;
			direction.w = 1.0f;
			
			diffuse.x = 1.0f * param->diffuse_intensity;	// R
			diffuse.y = 1.0f * param->diffuse_intensity;	// G
			diffuse.z = 1.0f * param->diffuse_intensity;	// B
			diffuse.w = 1.0f;

			ambient.x = 1.0f * param->ambient_intensity;	// R
			ambient.y = 1.0f * param->ambient_intensity;	// G
			ambient.z = 1.0f * param->ambient_intensity;	// B
			ambient.w = 1.0f;

			memcpy( &light.direction, &direction, sizeof(D3DXVECTOR4));
			memcpy( &light.diffuse, &diffuse, sizeof(D3DXVECTOR4));
			memcpy( &light.ambient, &ambient, sizeof(D3DXVECTOR4));

			IDirect3DDevice9_SetVertexShaderConstantF( device, 32, (float*)&direction, 4 );
			IDirect3DDevice9_SetVertexShaderConstantF( device, 33, (float*)&diffuse, 4 );
			IDirect3DDevice9_SetVertexShaderConstantF( device, 34, (float*)&ambient, 4 );
			
			break;

		default:
			error("Direct3D error: Unsupported light type: %d",param->type);
	}
}

void osd_renderer_set_fog( float fog_density, UINT32 fog_color )
{
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGTABLEMODE, D3DFOG_EXP );
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGDENSITY, *(DWORD*)&fog_density );
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGCOLOR, fog_color);
	IDirect3DDevice9_SetRenderState( device, D3DRS_FOGENABLE, TRUE );
}

void osd_renderer_begin(void)
{
	float mipmap_bias;
	tris = 0;

	IDirect3DDevice9_SetRenderState( device, D3DRS_LIGHTING, FALSE );

#if ENABLE_ALPHA_BLENDING 1
	IDirect3DDevice9_SetRenderState( device, D3DRS_ALPHABLENDENABLE, TRUE );
	IDirect3DDevice9_SetRenderState( device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	IDirect3DDevice9_SetRenderState( device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
#endif

#if ENABLE_MODEL_CACHING 1
	IDirect3DDevice9_SetStreamSource( device, 0, vertex_buffer, 0, sizeof(VERTEX) );
#endif
	IDirect3DDevice9_SetFVF( device, D3DFVF_VERTEX );
	IDirect3DDevice9_SetVertexShader( device, vertex_shader );
	IDirect3DDevice9_SetRenderState( device, D3DRS_CULLMODE, D3DCULL_NONE );

	IDirect3DDevice9_SetRenderState( device, D3DRS_ZENABLE, D3DZB_USEW );

	IDirect3DDevice9_BeginScene( device );

#if WIREFRAME 1
	IDirect3DDevice9_SetRenderState( device, D3DRS_FILLMODE, D3DFILL_WIREFRAME );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
#endif

	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
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
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
	mipmap_bias = -0.5f;
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)(&mipmap_bias));
}

void osd_renderer_end(void)
{
	IDirect3DDevice9_EndScene( device );
}

void osd_renderer_clear( BOOL fbuf, BOOL zbuf )
{
	DWORD flags = 0;
	if( fbuf )
		flags |= D3DCLEAR_TARGET;
	if( zbuf )
		flags |= D3DCLEAR_ZBUFFER;

	IDirect3DDevice9_Clear( device, 0, NULL, flags, 0x00000000, 1.0f, 0 );
}

void osd_renderer_blit(void)
{
	char string[200];
	double fps;
	RECT src_rect = { 0, 0, 495, 383 };

	QueryPerformanceCounter((LARGE_INTEGER*)&counter_end);
	fps = 1.0 / ((double)(counter_end - counter_start) / (double)counter_frequency);
	counter_start = counter_end;

#if DRAW_STATS 1
	sprintf(string,"FPS: %3.3f",fps);
	draw_text( 4, 1, string, RGB_GREEN, TRUE );
	sprintf(string,"Tris: %d",tris);
	draw_text( 4, 15, string, RGB_GREEN, TRUE );
	sprintf(string,"Cached vertices: %d",cached_vertices);
	draw_text( 4, 29, string, RGB_GREEN, TRUE );
	sprintf(string,"Light 0: X: %3.6f, Y: %3.6f, Z: %3.6f",
		light.direction.x, light.direction.y, light.direction.z );
	draw_text( 4, 43, string, RGB_GREEN, TRUE );
	sprintf(string,"Light 0: Ambient Intensity: %3.6f, Diffuse Intensity: %3.6f",
		light.ambient.x, light.diffuse.x );
	draw_text( 4, 57, string, RGB_GREEN, TRUE );

#endif

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

void osd_renderer_draw_layer(UINT layer_num)
{
	VIEWPORT v;
	v.x = 0;		v.y = 0;
	v.width = 496;	v.height = 384;
	v.up = 0;		v.down = 1;
	v.left = 0;		v.right = 1;
	osd_renderer_set_viewport( &v );

	layer_num &= 0x3;
#if ENABLE_MODEL_CACHING 1
#if WIREFRAME 1
	IDirect3DDevice9_SetRenderState( device, D3DRS_FILLMODE, D3DFILL_SOLID );
#endif
	IDirect3DDevice9_SetStreamSource( device, 0, vertex_buffer_2d, 0, sizeof(VERTEX2D) );
	IDirect3DDevice9_SetFVF( device, D3DFVF_VERTEX2D );

	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

	IDirect3DDevice9_SetTexture( device, 0, layer_data[layer_num] );
	IDirect3DDevice9_DrawPrimitive( device, D3DPT_TRIANGLELIST, 0, 2 );

	IDirect3DDevice9_SetFVF( device, D3DFVF_VERTEX );

	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );

	IDirect3DDevice9_SetTexture( device, 0, NULL );
	IDirect3DDevice9_SetStreamSource( device, 0, vertex_buffer, 0, sizeof(VERTEX) );
#if WIREFRAME 1
	IDirect3DDevice9_SetRenderState( device, D3DRS_FILLMODE, D3DFILL_WIREFRAME );
#endif
#else
	sprite->lpVtbl->Draw( sprite, layer_data[layer_num], NULL, NULL, NULL, 0.0f, NULL, 0xFFFFFFFF );
#endif
}

static void draw_uncached_model(UINT32* mem, UINT32 address, BOOL little_endian)
{
	float fixed_point_scale;
	VERTEX vertex[4];
	VERTEX prev_vertex[4];
	int end, index, polygon_index, vb_index, prev_texture, i, page;
	HRESULT hr;
	float uv_scale;

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
		D3DCOLOR color, specular;
		int transparency;
		int luminance = 0, spec;
		int i, num_vertices, num_old_vertices;
		int v2, v;
		int texture_x, texture_y, tex_num, depth, tex_width, tex_height;
		float nx, ny, nz;

		if(little_endian) {
			for( i=0; i<7; i++) {
				header[i] = mem[index];
				index++;
			}
		} else {
			for( i=0; i<7; i++) {
				header[i] = BSWAP32(mem[index]);
				index++;
			}
		}

		if( header[6] == 0 )
			return;

		uv_scale = (header[1] & 0x40) ? 1.0f : 8.0f;

		// Check if this is the last polygon
		if(header[1] & 0x4)
			end = 1;

		transparency = ((header[6] >> 18) & 0x1F) << 3;
		if(header[6] & 0x800000) {
			color = 0xFF000000 | (header[4] >> 8);
		} else {
			color = (transparency << 24) | (header[4] >> 8);
		}
		if(header[6] & 0x10000)
			luminance = 0xFF;
			//luminance = (((header[6] >> 11) & 0x1F) << 3);

		spec = (header[0] >> 26) & 0x3F;
		specular = (luminance << 24) | spec << 16;

		// Polygon normal
		// Assuming 2.22 fixed-point. Is this correct ?
		nx = (float)((int)header[1] >> 8) / 4194304.0f;
		ny = (float)((int)header[2] >> 8) / 4194304.0f;
		nz = (float)((int)header[3] >> 8) / 4194304.0f;

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
				vertex[v2].specular = specular;
				v2++;
			}
		}

		// Load vertex data
		if(little_endian) {
			for( i=0; i<(num_vertices - num_old_vertices) * 4; i++) {
				entry[i] = mem[index];
				index++;
			}
		} else {
			for( i=0; i<(num_vertices - num_old_vertices) * 4; i++) {
				entry[i] = BSWAP32(mem[index]);
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

			tx = (INT16)((entry[v_index + 3] >> 16) & 0xFFFF);
			ty = (INT16)(entry[v_index + 3] & 0xFFFF);
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
			vertex[v2].specular = specular;

			v2++;
		}

		// Get the texture number from texture table
		page = (header[4] & 0x40) ? 1 : 0;
		texture_x = ((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 0x1);
		texture_y = (header[5] & 0x1F) | (page ? 0x20 : 0);
		depth = (header[6] >> 7) & 0x7;
		tex_width	= 32 << ((header[3] >> 3) & 0x7);
		tex_height	= 32 << (header[3] & 0x7);

		tex_num = texture_table[texture_y * 64 + texture_x];
		if(tex_num == 0)
			tex_num = cache_texture(texture_x * 32,texture_y * 32, tex_width, tex_height, depth);
		if((header[6] & 0x4000000) == 0)
			tex_num = -1;

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

		// Transparent polygons
		if((header[6] & 0x800000) == 0 || header[6] & 0x1) {
			IDirect3DDevice9_SetRenderState( device, D3DRS_ZWRITEENABLE, FALSE );
		} else {
			IDirect3DDevice9_SetRenderState( device, D3DRS_ZWRITEENABLE, TRUE );
		}

		// Translucent texture (ARGB4444)
		if(header[6] & 0x80000000 || header[6] & 0x1) {
			IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		} else {
			IDirect3DDevice9_SetTextureStageState( device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
		}

		if(num_vertices == 3) {
			tris++;
			IDirect3DDevice9_DrawPrimitiveUP( device, D3DPT_TRIANGLELIST, 1, &vertex, sizeof(VERTEX) );
		} else {
			tris+=2;
			IDirect3DDevice9_DrawPrimitiveUP( device, D3DPT_TRIANGLEFAN, 2, &vertex, sizeof(VERTEX) );
		}
		// Copy current vertex as previous vertex
		memcpy( prev_vertex, vertex, sizeof(VERTEX) * 4 );
	} while(end == 0);
}

void osd_renderer_draw_model(UINT32 *mem, UINT32 address, BOOL little_endian)
{
	HASHTABLE *h;
	MATRIX m;
	D3DMATRIX world_view, world_view_proj ;
	D3DMATRIX world = d3d_matrix_stack_get_top( &m );
	
	// Make the world-view matrix
	D3DXMatrixTranspose( &world_view, &world );
	IDirect3DDevice9_SetVertexShaderConstantF( device, 4, (float*)&world_view, 4 );

	// Make the world-view-projection matrix
	D3DXMatrixMultiply( &world_view_proj, &world, &coordinate_system );
	D3DXMatrixMultiply( &world_view_proj, &world_view_proj, &projection );
	D3DXMatrixTranspose( &world_view_proj, &world_view_proj );
	IDirect3DDevice9_SetVertexShaderConstantF( device, 0, (float*)&world_view_proj, 4 );

#if ENABLE_MODEL_CACHING 1
	if(little_endian)
		return;

	h = cache_model(mem, address);
	IDirect3DDevice9_DrawPrimitive( device, D3DPT_TRIANGLELIST, h->vertex_index, h->length / 3 );
#else
	draw_uncached_model(mem, address, little_endian);
#endif
}

/******************************************************************/
/* Texture Management                                             */
/******************************************************************/

/*
 * void osd_renderer_invalidate_textures(UINT x, UINT y, UINT w, UINT h);
 *
 * Invalidates all textures that are within the rectangle in texture memory
 * defined by the parameters.
 *
 * Parameters:
 *      x = Texture pixel X coordinate in texture memory.
 *      y = Y coordinate.
 *      w = Width of rectangle in pixels.
 *      h = Height.
 */

void osd_renderer_invalidate_textures(UINT x, UINT y, UINT w, UINT h)
{
	// Update texture table
	// Removes the overwritten texture from cache
	UINT i,j;
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

static void upload_tex(void* s, void* d, int tex_width, 
					   int tex_height, int texture_depth, int pitch)
{
	int x,y;
	switch(texture_depth)
	{
		case 0:	// ARGB1555
		{
			UINT16* ptr = (UINT16*)d;
			UINT16* src = (UINT16*)s;
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					ptr[t_index + x] = src[s_index + x] ^ 0x8000;
				}
			}
			break;
		}
		case 1:	// 4-bit, field 0x000F
		{
			UINT8* ptr = (UINT8*)d;
			UINT16* src = (UINT16*)s;
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = src[s_index + x];
					ptr[t_index + x] = ((pix >> 0) & 0xF) << 4;
				}
			}
			break;
		}
		case 2:	// 4-bit, field 0x00F0
		{
			UINT8* ptr = (UINT8*)d;
			UINT16* src = (UINT16*)s;
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = src[s_index + x];
					ptr[t_index + x] = ((pix >> 4) & 0xF) << 4;
				}
			}
			break;
		}
		case 3:	// 4-bit, field 0x0F00
		{
			UINT8* ptr = (UINT8*)d;
			UINT16* src = (UINT16*)s;
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = src[s_index + x];
					ptr[t_index + x] = ((pix >> 8) & 0xF) << 4;
				}
			}
			break;
		}
		case 4:	// A4L4
		{
			UINT8* ptr = (UINT8*)d;
			UINT8* src = (UINT8*)s;
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = ((src[s_index + (x/2)] >> ((x & 0x1) ? 8 : 0)) & 0xFF);
					ptr[t_index + x] = ((pix >> 4) & 0xF) << 12 | 
									   ((pix & 0xF) << 4);
				}
			}
			break;
		}
		case 5:	// A8
		{
			UINT8* ptr = (UINT8*)d;
			UINT8* src = (UINT8*)s;
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = ((src[s_index + (x/2)] >> ((x & 0x1) ? 8 : 0)) & 0xFF);
					ptr[t_index + x] = pix;
				}
			}
			break;
		}
		case 6:	// 4-bit, field 0xF000
		{
			UINT8* ptr = (UINT8*)d;
			UINT16* src = (UINT16*)s;
			for( y=0; y < tex_height; y++) {
				UINT32 s_index = y * 2048;
				UINT32 t_index = y * pitch;
				for( x=0; x < tex_width; x++) {
					UINT16 pix = src[s_index + x];
					ptr[t_index + x] = ((pix >> 12) & 0xF) << 4;
				}
			}
			break;
		}
		case 7:	// ARGB4444
		{
			UINT16* ptr = (UINT16*)d;
			UINT16* src = (UINT16*)s;
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
	}
}

static const int mipmap_x[12] =
{ 0,1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
static const int mipmap_y[12] =
{ 0,512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023, 0 };
static const int mipmap_size[7] = { 1, 2, 4, 8, 16, 32, 64 };

static int cache_texture(int texture_x, int texture_y, int tex_width, 
						 int tex_height, int texture_depth)
{
	D3DFORMAT format;
	int i,j,x,y;
	int texture_num;
	HRESULT hr;
	D3DLOCKED_RECT locked_rect;
	UINT pitch;
	UINT pitch8;
	void* src;
	int plane, width, height;
	int mipmap_num, size, mip_y;
	int page = (texture_y & 0x400) ? 1 : 0;
	mip_y = texture_y & 0x3FF;

	if(tex_width > 512 || tex_height > 512)
		return -1;

	width = tex_width;
	height = tex_height;

	size = min(width,height);
	if(size == 32)
		mipmap_num = 3;
	else if(size == 64)
		mipmap_num = 4;
	else if(size == 128)
		mipmap_num = 5;
	else if(size == 256)
		mipmap_num = 6;
	else if(size == 512)
		mipmap_num = 7;

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

	switch(texture_depth)
	{
		case 0: format = D3DFMT_A1R5G5B5;break;	// 16-bit A1R5G5B5
		case 1: format = D3DFMT_A8;break;		// 4-bit, field 0x000F
		case 2: format = D3DFMT_A8;break;		// 4-bit, field 0x00F0
		case 3: format = D3DFMT_A8;break;		// 4-bit, field 0x0F00
		case 4: format = D3DFMT_A8L8;break;		// 8-bit A4L8
		case 5: format = D3DFMT_A8;break;		// 8-bit grayscale
		case 6: format = D3DFMT_A8;break;		// 4-bit, field 0xF000
		case 7: format = D3DFMT_A4R4G4B4;break;	// 16-bit A4R4G4B4
	}

	switch(texture_depth)
	{
		case 1: plane = 3;break;
		case 2: plane = 2;break;
		case 3: plane = 1;break;
		default: plane = 0; break;
	}

	hr = D3DXCreateTexture( device, tex_width, tex_height, mipmap_num, 0, format, D3DPOOL_MANAGED,
							&texture[texture_num] );
	if( FAILED(hr) )
		error("D3DXCreateTexture failed.");

	for( i=0; i < mipmap_num; i++ ) {
		int mwidth = width / mipmap_size[i];
		int mheight = height / mipmap_size[i];
		int mx = mipmap_x[i] + (texture_x / mipmap_size[i]);
		int my = mipmap_y[i] + (mip_y / mipmap_size[i]);
		if(page)
			my += 1024;

		hr = IDirect3DTexture9_LockRect( texture[texture_num], i, &locked_rect, NULL, 0 );
		if( FAILED(hr) )
			error("IDirect3DTexture9_LockRect failed.");

		pitch = locked_rect.Pitch;
		if( texture_depth == 0 || texture_depth == 7 )
			pitch /= 2;

		src = &texture_sheet[(my * 2048 + mx) * 2];

		upload_tex( src, locked_rect.pBits, mwidth, mheight, texture_depth, pitch );

		IDirect3DTexture9_UnlockRect( texture[texture_num], i );
	}
	
	// Update texture table
	for( j = texture_y/32; j < (texture_y + tex_height)/32; j++) {
		for( i = texture_x/32; i < (texture_x + tex_width)/32; i++) {
			texture_table[(j*64) + i] = texture_num;
		}
	}
	return texture_num;
}



////////////////////////
//    Matrix Stack    //
////////////////////////

static int stack_ptr = 0;

static void d3d_matrix_stack_init(void)
{
	matrix_stack->lpVtbl->LoadIdentity(matrix_stack);
}

static D3DXMATRIX d3d_matrix_stack_get_top(void)
{
	D3DXMATRIX *m = matrix_stack->lpVtbl->GetTop(matrix_stack);
	return *m;
}

/*
 * void osd_renderer_push_matrix(void);
 *
 * Pushes a matrix on to the stack. The matrix pushed is the former top of the
 * stack.
 */

void osd_renderer_push_matrix(void)
{
	stack_ptr++;
	matrix_stack->lpVtbl->Push(matrix_stack);
}

/*
 * void osd_renderer_pop_matrix(void);
 *
 * Pops a matrix off the top of the stack.
 */

void osd_renderer_pop_matrix(void)
{
	stack_ptr--;
	if( stack_ptr >= 0)
		matrix_stack->lpVtbl->Pop(matrix_stack);
}

/*
 * void osd_renderer_multiply_matrix(MATRIX m);
 *
 * Multiplies the top of the matrix stack by the specified matrix
 *
 * Parameters:
 *      m = Matrix to multiply.
 */

void osd_renderer_multiply_matrix( MATRIX m )
{
	D3DMATRIX x = matrix_to_d3d( m );
	matrix_stack->lpVtbl->MultMatrixLocal(matrix_stack, &x );
}

/*
 * void osd_renderer_translate_matrix(float x, float y, float z);
 *
 * Translates the top of the matrix stack.
 *
 * Parameters:
 *      x = Translation along X axis.
 *      y = Y axis.
 *      z = Z axis.
 */

void osd_renderer_translate_matrix( float x, float y, float z )
{
	//stack->lpVtbl->TranslateLocal(stack, x, y, z );
	D3DMATRIX t;
	t._11 = 1.0f; t._12 = 0.0f; t._13 = 0.0f; t._14 = 0.0f;
	t._21 = 0.0f; t._22 = 1.0f; t._23 = 0.0f; t._24 = 0.0f;
	t._31 = 0.0f; t._32 = 0.0f; t._33 = 1.0f; t._34 = 0.0f;
	t._41 = x;	  t._42 = y;    t._43 = z;    t._44 = 1.0f;
	matrix_stack->lpVtbl->MultMatrixLocal(matrix_stack,&t);
}