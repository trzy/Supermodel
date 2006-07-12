/******************************************************************/
/* DirectX 9 Renderer                                             */
/******************************************************************/

#include "model3.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr9.h>

#define EXTERNAL_SHADERS	0

static float min_z;
static float max_z;


#if EXTERNAL_SHADERS

static LPCTSTR vs_filename = "vertex_shader.vs";
static LPCTSTR ps_filename = "pixel_shader.ps";
static LPCTSTR vs2d_filename = "vertex_shader_2d.vs";
static LPCTSTR ps2d_filename = "pixel_shader_2d.ps";

#else

#include "shaders.h"

#endif


D3DXMATRIX d3d_matrix_stack_get_top(void);
void d3d_matrix_stack_init(void);

typedef struct
{
	float x;
	float y;
	float width;
	float height;
} VIEWPORT_PARAMS;

typedef struct
{
	D3DXVECTOR4 sun_vector;
	D3DXVECTOR4 sun_params;
} LIGHTING_PARAMS;

#define MAX_VIEWPORTS		32
#define MAX_LIGHTS			32

static int current_viewport;
static int current_light;
static int num_viewports;
static int num_lights;

static VIEWPORT_PARAMS viewport_params[MAX_VIEWPORTS];
static LIGHTING_PARAMS lighting_params[MAX_LIGHTS];


#define MESH_STATIC		0
#define MESH_DYNAMIC	1

typedef struct
{
	D3DMATRIX transform;
	D3DMATRIX normal;
	
	int mesh_index;
	int viewport;
	int lighting;
} MESH;

typedef struct
{
	int vb_index;
	int num_vertices;
	int vb_index_alpha;
	int num_vertices_alpha;
} MESH_CACHE;

#define DYNAMIC_VB_SIZE			180000
#define STATIC_VB_SIZE			1000000

#define NUM_DYNAMIC_MESHES		2048
#define NUM_STATIC_MESHES		32768

#define DYNAMIC_MESH_BUFFER_SIZE		2048
#define STATIC_MESH_BUFFER_SIZE			65536

typedef struct
{
	float x, y, z;
	float u, v;
	float nx, ny, nz;
	float tx, ty, twidth, theight;
	D3DCOLOR color, color2;
} VERTEX;

D3DVERTEXELEMENT9 vertex_decl[] = 
{
    {0, 0,		D3DDECLTYPE_FLOAT3,		D3DDECLMETHOD_DEFAULT,	D3DDECLUSAGE_POSITION,	0},
    {0, 12,		D3DDECLTYPE_FLOAT2,		D3DDECLMETHOD_DEFAULT,	D3DDECLUSAGE_TEXCOORD,	0},
    {0, 20,		D3DDECLTYPE_FLOAT3,		D3DDECLMETHOD_DEFAULT,	D3DDECLUSAGE_NORMAL,	0},
	{0, 32,		D3DDECLTYPE_FLOAT4,		D3DDECLMETHOD_DEFAULT,	D3DDECLUSAGE_TEXCOORD,	1},
	{0, 48,		D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, 	D3DDECLUSAGE_COLOR,		0},
	{0, 52,		D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, 	D3DDECLUSAGE_COLOR,		1},
    D3DDECL_END()
};



typedef struct
{
	float x, y, z, w;
	float u, v;
} VERTEX_2D;

const DWORD VERTEX2D_FVF = (D3DFVF_XYZRHW | D3DFVF_TEX1 );

D3DVERTEXELEMENT9 vertex_decl_2d[] =
{
	{0, 0,		D3DDECLTYPE_FLOAT3,		D3DDECLMETHOD_DEFAULT,	D3DDECLUSAGE_POSITION,	0},
	{0, 12,		D3DDECLTYPE_FLOAT2,		D3DDECLMETHOD_DEFAULT,	D3DDECLUSAGE_TEXCOORD,	0},
	D3DDECL_END()
};



static LPDIRECT3D9					d3d;
static LPDIRECT3DDEVICE9			device;

static D3DCAPS9						device_caps;

static LPDIRECT3DTEXTURE9			texture;
static LPDIRECT3DVERTEXBUFFER9		dynamic_vb;
static LPDIRECT3DVERTEXBUFFER9		static_vb;
static LPDIRECT3DVERTEXBUFFER9		dynamic_vb_alpha;
static LPDIRECT3DVERTEXBUFFER9		static_vb_alpha;
static LPDIRECT3DVERTEXSHADER9		vshader;
static LPDIRECT3DPIXELSHADER9		pshader;
static LPDIRECT3DVERTEXDECLARATION9	vertex_declaration;

static LPDIRECT3DTEXTURE9			texture_2d[4];
static LPDIRECT3DTEXTURE9			palette_2d;
static LPDIRECT3DTEXTURE9			priority_2d[4];
static LPDIRECT3DVERTEXBUFFER9		vb_2d;
static LPDIRECT3DVERTEXSHADER9		vshader_2d;
static LPDIRECT3DPIXELSHADER9		pshader_2d;
static LPDIRECT3DVERTEXDECLARATION9	vertex_declaration_2d;

static LPD3DXMATRIXSTACK			matrix_stack;

static LPDIRECT3DTEXTURE9			lightgun_cursor;

static LPD3DXFONT font;

static D3DMATRIX world_matrix;
static D3DMATRIX view_matrix;
static D3DMATRIX projection_matrix;

static D3DVIEWPORT9 viewport;

static char num_bits[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

static HWND main_window;
static HFONT hfont;

static UINT32 matrix_start;

static MESH_CACHE *static_mesh_cache;
static MESH_CACHE *dynamic_mesh_cache;

static int static_mesh_cache_top = 0;
static int dynamic_mesh_cache_top = 0;

static int dynamic_mesh_cache_vertex_top = 0;
static int dynamic_mesh_cache_vertex_top_alpha = 0;
static int static_mesh_cache_vertex_top = 0;
static int static_mesh_cache_vertex_top_alpha = 0;

static MESH *static_mesh_buffer;
static int static_mesh_buffer_top = 0;
static MESH *dynamic_mesh_buffer;
static int dynamic_mesh_buffer_top = 0;

static int listdone = 0;

static D3DXVECTOR4 sun_vector;
static D3DXVECTOR4 sun_params;

static BOOL traverse_node(UINT32);
static BOOL traverse_list(UINT32);
static BOOL render_scene(void);
static UINT32* get_address(UINT32);


static UINT16 *static_mesh_index;


static void d3d_shutdown(void)
{
	if(d3d) {
		IDirect3D9_Release(d3d);
		d3d = NULL;
	}
}

BOOL d3d_pre_init(void)
{
	HRESULT hr;
	D3DDISPLAYMODE				d3ddm;
	D3DADAPTER_IDENTIFIER9		adapter_identifier;

	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d)
	{
		message(0, "Direct3DCreate9 failed.\n");
		return FALSE;
	}

	hr = IDirect3D9_GetAdapterDisplayMode(d3d, D3DADAPTER_DEFAULT, &d3ddm);
	if (FAILED(hr))
	{
		message(0, "d3d->GetAdapterDisplayMode failed.\n");
		return FALSE;
	}

	hr = IDirect3D9_GetDeviceCaps(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &device_caps);
	if (FAILED(hr))
	{
		message(0, "d3d->GetDeviceCaps failed.\n");
		return FALSE;
	}

	hr = IDirect3D9_GetAdapterIdentifier(d3d, D3DADAPTER_DEFAULT, 0, &adapter_identifier);
	if (FAILED(hr))
	{
		message(0, "d3d->GetAdapterIdentifier failed.\n");
		return FALSE;
	}

	message(0, "Video card: %s", adapter_identifier.Description);

	if (device_caps.VertexShaderVersion < D3DVS_VERSION(3,0))
	{
		message(0, "The video card doesn't support Vertex Shader 3.0\n");
		return FALSE;
	}
	if (device_caps.PixelShaderVersion < D3DPS_VERSION(3,0))
	{
		message(0, "The video card doesn't support Pixel Shader 3.0\n");
		return FALSE;
	}

	return TRUE;
}

BOOL d3d_init(HWND main_window)
{
	D3DPRESENT_PARAMETERS		d3dpp;
	LPD3DXBUFFER 		vsh, psh, errors;
	HRESULT hr;
	DWORD flags = 0;
	int width, height;
	int i;

	if (m3_config.fullscreen)
	{
		width	= m3_config.width;
		height	= m3_config.height;
	}
	else
	{
		width = 496;
		height = 384;
	}

	// Check if we have a valid display mode
	if (width == 0 || height == 0)
		return FALSE;

	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.Windowed					= m3_config.fullscreen ? FALSE : TRUE;
	d3dpp.SwapEffect				= m3_config.stretch ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_FLIP;
	d3dpp.BackBufferWidth			= width;
	d3dpp.BackBufferHeight			= height;
	d3dpp.BackBufferCount			= m3_config.stretch ? 1 : 2;
	d3dpp.hDeviceWindow				= main_window;
	d3dpp.PresentationInterval		= m3_config.fullscreen ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	d3dpp.BackBufferFormat			= D3DFMT_A8R8G8B8;
	d3dpp.EnableAutoDepthStencil	= TRUE;
	d3dpp.AutoDepthStencilFormat	= D3DFMT_D24X8;

	if (device_caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
	{
		flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	} 
	else
	{
		flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, main_window, flags, &d3dpp, &device);
	if(FAILED(hr))
	{
		return FALSE;
	}
		
	// create vertex buffers for dynamic vertex data
	hr = IDirect3DDevice9_CreateVertexBuffer(device,
			DYNAMIC_VB_SIZE * sizeof(VERTEX), D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &dynamic_vb, NULL);
	if (FAILED(hr))
	{
		message(0, "d3d->CreateVertexBuffer failed\n");
		return FALSE;
	}
	hr = IDirect3DDevice9_CreateVertexBuffer(device,
			DYNAMIC_VB_SIZE * sizeof(VERTEX), D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &dynamic_vb_alpha, NULL);
	if (FAILED(hr))
	{
		message(0, "d3d->CreateVertexBuffer failed\n");
		return FALSE;
	}
	
	// create vertex buffers for static vertex data
	hr = IDirect3DDevice9_CreateVertexBuffer(device,
			STATIC_VB_SIZE * sizeof(VERTEX), 0, 0, D3DPOOL_MANAGED, &static_vb, NULL);
	if (FAILED(hr))
	{
		message(0, "d3d->CreateVertexBuffer failed\n");
		return FALSE;
	}
	hr = IDirect3DDevice9_CreateVertexBuffer(device,
			STATIC_VB_SIZE * sizeof(VERTEX), 0, 0, D3DPOOL_MANAGED, &static_vb_alpha, NULL);
	if (FAILED(hr))
	{
		message(0, "d3d->CreateVertexBuffer failed\n");
		return FALSE;
	}
	
#if EXTERNAL_SHADERS
	// create vertex shader
	hr = D3DXAssembleShaderFromFile(vs_filename, NULL, NULL, 0, &vsh, &errors);
	if (FAILED(hr))
	{
		message(0, "Direct3D error: %s", errors->lpVtbl->GetBufferPointer(errors));
		return FALSE;
	}
	hr = IDirect3DDevice9_CreateVertexShader(device, (DWORD*)vsh->lpVtbl->GetBufferPointer(vsh), &vshader);
	if (FAILED(hr))
	{
		message(0, "Direct3D error: IDirect3DDevice9_CreateVertexShader failed.");
		return FALSE;
	}
	
	// create pixel shader
	hr = D3DXAssembleShaderFromFile(ps_filename, NULL, NULL, 0, &psh, &errors);
	if (FAILED(hr))
	{
		message(0, "Direct3D error: %s", errors->lpVtbl->GetBufferPointer(errors));
		return FALSE;
	}
	hr = IDirect3DDevice9_CreatePixelShader(device, (DWORD*)psh->lpVtbl->GetBufferPointer(psh), &pshader);
	if (FAILED(hr))
	{
		message(0, "Direct3D error: IDirect3DDevice9_CreatePixelShader failed.");
		return FALSE;
	}
#else
	// create vertex shader
	hr = IDirect3DDevice9_CreateVertexShader(device, (DWORD*)vertex_shader_source, &vshader);
	if (FAILED(hr))
	{
		message(0, "Direct3D error: IDirect3DDevice9_CreateVertexShader failed.");
		return FALSE;
	}
	
	// create pixel shader
	hr = IDirect3DDevice9_CreatePixelShader(device, (DWORD*)pixel_shader_source, &pshader);
	if (FAILED(hr))
	{
		message(0, "Direct3D error: IDirect3DDevice9_CreatePixelShader failed.");
		return FALSE;
	}
#endif
	
	// create vertex declarations
	hr = IDirect3DDevice9_CreateVertexDeclaration(device, vertex_decl, &vertex_declaration);
	if (FAILED(hr))
	{
		message(0, "IDirect3DDevice9_CreateVertexDeclaration failed.");
		return FALSE;
	}
	
	// create textures
	hr = IDirect3DDevice9_CreateTexture(device, 2048, 4096, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL);
	if (FAILED(hr))
	{
		message(0, "IDirect3DDevice9_CreateTexture failed.");
		return FALSE;
	}
	
	dynamic_mesh_buffer = (MESH*)malloc(sizeof(MESH) * DYNAMIC_MESH_BUFFER_SIZE);
	static_mesh_buffer = (MESH*)malloc(sizeof(MESH) * STATIC_MESH_BUFFER_SIZE);
	
	dynamic_mesh_cache = (MESH_CACHE*)malloc(sizeof(MESH_CACHE) * NUM_DYNAMIC_MESHES);
	static_mesh_cache = (MESH_CACHE*)malloc(sizeof(MESH_CACHE) * NUM_STATIC_MESHES);
	
	static_mesh_index = (UINT16*)malloc(sizeof(UINT16) * 16777216);
	for (i=0; i < 16777216; i++)
	{
		static_mesh_index[i] = 0xffff;
	}

	{
		/////////////////////////////
		// 2D layers               //
		/////////////////////////////

		VERTEX_2D *vb;
	
		// create vertex shader and pixel shader for 2D
#if EXTERNAL_SHADERS
		// create vertex shader
		hr = D3DXAssembleShaderFromFile(vs2d_filename, NULL, NULL, 0, &vsh, &errors);
		if (FAILED(hr))
		{
			message(0, "Direct3D error: %s", errors->lpVtbl->GetBufferPointer(errors));
			return FALSE;
		}
		hr = IDirect3DDevice9_CreateVertexShader(device, (DWORD*)vsh->lpVtbl->GetBufferPointer(vsh), &vshader_2d);
		if (FAILED(hr))
		{
			message(0, "Direct3D error: IDirect3DDevice9_CreateVertexShader failed.");
			return FALSE;
		}
	
		// create pixel shader
		hr = D3DXAssembleShaderFromFile(ps2d_filename, NULL, NULL, 0, &psh, &errors);
		if (FAILED(hr))
		{
			message(0, "Direct3D error: %s", errors->lpVtbl->GetBufferPointer(errors));
			return FALSE;
		}
		hr = IDirect3DDevice9_CreatePixelShader(device, (DWORD*)psh->lpVtbl->GetBufferPointer(psh), &pshader_2d);
		if (FAILED(hr))
		{
			message(0, "Direct3D error: IDirect3DDevice9_CreatePixelShader failed.");
			return FALSE;
		}
#else
		// create vertex shader
		hr = IDirect3DDevice9_CreateVertexShader(device, (DWORD*)vertex_shader_2d_source, &vshader_2d);
		if (FAILED(hr))
		{
			message(0, "Direct3D error: IDirect3DDevice9_CreateVertexShader failed.");
			return FALSE;
		}
	
		// create pixel shader
		hr = IDirect3DDevice9_CreatePixelShader(device, (DWORD*)pixel_shader_2d_source, &pshader_2d);
		if (FAILED(hr))
		{
			message(0, "Direct3D error: IDirect3DDevice9_CreatePixelShader failed.");
			return FALSE;
		}
#endif

		// create vertex declarations
		hr = IDirect3DDevice9_CreateVertexDeclaration(device, vertex_decl, &vertex_declaration_2d);
		if (FAILED(hr))
		{
			message(0, "IDirect3DDevice9_CreateVertexDeclaration failed.");
			return FALSE;
		}
	
		// create textures for 2d layers
		for (i=0; i < 4; i++)
		{
			hr = IDirect3DDevice9_CreateTexture(device,
				512, 512, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8L8, D3DPOOL_DEFAULT, &texture_2d[i], NULL);
			if (FAILED(hr))
			{
				message(0, "IDirect3DDevice9_CreateTexture failed.");
				return FALSE;
			}

			// priority
			hr = IDirect3DDevice9_CreateTexture(device,
				1, 512, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &priority_2d[i], NULL);
			if (FAILED(hr))
			{
				message(0, "IDirect3DDevice9_CreateTexture failed.");
				return FALSE;
			}
		}

		// create texture for palette
		hr = IDirect3DDevice9_CreateTexture(device,
			256, 256, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &palette_2d, NULL);
		if (FAILED(hr))
		{
			message(0, "IDirect3DDevice9_CreateTexture failed.");
			return FALSE;
		}



		// create vertex buffer for 2d layers
		hr = IDirect3DDevice9_CreateVertexBuffer(device, 4 * sizeof(VERTEX_2D), 0, 0, D3DPOOL_MANAGED, &vb_2d, NULL);
		if (FAILED(hr))
		{
			message(0, "d3d->CreateVertexBuffer failed\n");
			return FALSE;
		}

		// fill in the vertex data
		IDirect3DVertexBuffer9_Lock(vb_2d, 0, 0, (void **)&vb, D3DLOCK_DISCARD);

		vb[0].x = 0.0f;		vb[0].y = 0.0f;		vb[0].z = 1.0f;		vb[0].w = 1.0f;
		vb[0].u = 0.0f;		vb[0].v = 0.0f;
		vb[1].x = 512.0f;	vb[1].y = 0.0f;		vb[1].z = 1.0f;		vb[1].w = 1.0f;
		vb[1].u = 1.0f;		vb[1].v = 0.0f;
		vb[2].x = 512.0f;	vb[2].y = 512.0f;	vb[2].z = 1.0f;		vb[2].w = 1.0f;
		vb[2].u = 1.0f;		vb[2].v = 1.0f;
		vb[3].x = 0.0f;		vb[3].y = 512.0f;	vb[3].z = 1.0f;		vb[3].w = 1.0f;
		vb[3].u = 0.0f;		vb[3].v = 1.0f;

		IDirect3DVertexBuffer9_Unlock(vb_2d);
	}
	
	D3DXCreateMatrixStack(0, &matrix_stack);
	d3d_matrix_stack_init();


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
	if (hfont == NULL)
	{
		message(0, "Direct3D error: Couldn't create font.");
		return FALSE;
	}

	// Create D3D font
	hr = D3DXCreateFont(device, 14, 0, FW_BOLD, 1, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
						DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Terminal", &font);
	if (FAILED(hr))
	{
		message(0, "Direct3D error: D3DXCreateFont failed.");
		return FALSE;
	}

	atexit(d3d_shutdown);

	// set minimum and maximum Z
	if (m3_config.step == 0x10)
	{
		min_z = 10.0f;
		max_z = 500000.0f;
	}
	else
	{
		min_z = 0.1f;
		max_z = 100000.0f;
	}

	return TRUE;
}

static void set_viewport(int x, int y, int width, int height)
{
	D3DVIEWPORT9	viewport;
	
	memset(&viewport, 0, sizeof(D3DVIEWPORT9));
	viewport.X		= x;
	viewport.Y		= y;
	viewport.Width	= width;
	viewport.Height	= height;
	viewport.MinZ	= min_z;
	viewport.MaxZ	= max_z;

	IDirect3DDevice9_SetViewport(device, &viewport);
}

void osd_renderer_draw_layer(int layer, UINT32 color_offset, int x, int y, BOOL top)
{
	int i;
	D3DXVECTOR4 scroll_pos;
	float co[4];
	co[0] = 0.0f;
	co[1] = (((float)((color_offset >> 16) & 0xff) / 255.0f) - 0.5f) * 2.0f;
	co[2] = (((float)((color_offset >>  8) & 0xff) / 255.0f) - 0.5f) * 2.0f;
	co[3] = (((float)((color_offset >>  0) & 0xff) / 255.0f) - 0.5f) * 2.0f;

	scroll_pos.x = (float)(x) / 512.0f;
	scroll_pos.y = (float)(y) / 512.0f;
	scroll_pos.z = top ? 1.0f : 0.0f;

	set_viewport(0, 0, 496, 384);

	IDirect3DDevice9_BeginScene(device);

	IDirect3DDevice9_SetStreamSourceFreq(device, 0, 1);
	IDirect3DDevice9_SetStreamSource(device, 0, vb_2d, 0, sizeof(VERTEX_2D));
	IDirect3DDevice9_SetStreamSourceFreq(device, 1, 1);
	IDirect3DDevice9_SetStreamSource(device, 1, NULL, 0, NULL);

	IDirect3DDevice9_SetFVF(device, VERTEX2D_FVF);
//	IDirect3DDevice9_SetVertexDeclaration(device, vertex_declaration_2d);
	IDirect3DDevice9_SetVertexShader(device, vshader_2d);
	IDirect3DDevice9_SetPixelShader(device, pshader_2d);

	// set color offset
	IDirect3DDevice9_SetPixelShaderConstantF(device, 8, (float *)co, 1);
	IDirect3DDevice9_SetPixelShaderConstantF(device, 9, (float *)&scroll_pos, 1);

	IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE);
	IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, D3DZB_FALSE);
	IDirect3DDevice9_SetRenderState(device, D3DRS_FILLMODE, D3DFILL_SOLID);

	IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHATESTENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHAREF, (DWORD)0x00000000);
	IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHAFUNC, D3DCMP_GREATER);

	IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	IDirect3DDevice9_SetSamplerState(device, 1, D3DSAMP_MINFILTER, D3DTEXF_NONE);
	IDirect3DDevice9_SetSamplerState(device, 1, D3DSAMP_MAGFILTER, D3DTEXF_NONE);
	IDirect3DDevice9_SetSamplerState(device, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	IDirect3DDevice9_SetSamplerState(device, 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	IDirect3DDevice9_SetSamplerState(device, 2, D3DSAMP_MINFILTER, D3DTEXF_NONE);
	IDirect3DDevice9_SetSamplerState(device, 2, D3DSAMP_MAGFILTER, D3DTEXF_NONE);
	IDirect3DDevice9_SetSamplerState(device, 2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	IDirect3DDevice9_SetSamplerState(device, 2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	
	IDirect3DDevice9_SetTexture(device, 0, texture_2d[layer]);
	IDirect3DDevice9_SetTexture(device, 1, palette_2d);
	IDirect3DDevice9_SetTexture(device, 2, priority_2d[layer]);

	IDirect3DDevice9_DrawPrimitive(device, D3DPT_TRIANGLEFAN, 0, 2);

	IDirect3DDevice9_EndScene(device);
}

void osd_renderer_blit(void)
{
	if (m3_config.stretch)
	{
		RECT src_rect = {0, 0, 496, 384};
		IDirect3DDevice9_Present(device, &src_rect, NULL, NULL, NULL);
	}
	else
	{
		IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);
	}
}

void osd_renderer_clear(BOOL fbuf, BOOL zbuf)
{
	int flags = 0;

	set_viewport(0, 0, m3_config.width, m3_config.height);

	if (fbuf)
	{
		flags |= D3DCLEAR_TARGET;
	}
	if (zbuf)
	{
		flags |= D3DCLEAR_ZBUFFER;
	}

	IDirect3DDevice9_Clear(device, 0, NULL, flags, 0x00000000, 1.0f, 0);
}

void osd_renderer_get_layer_buffer(int layer_num, UINT8 **buffer, int *pitch)
{
	D3DLOCKED_RECT locked_rect;
	HRESULT hr;

	*buffer = NULL;
	*pitch = 0;

	hr = IDirect3DTexture9_LockRect(texture_2d[layer_num], 0, &locked_rect, NULL, D3DLOCK_DISCARD);
	if (!FAILED(hr))
	{
        *buffer = (UINT8 *)locked_rect.pBits;
        *pitch  = locked_rect.Pitch / 2;
	}
}

void osd_renderer_free_layer_buffer(UINT layer_num)
{
	IDirect3DTexture9_UnlockRect(texture_2d[layer_num], 0);
}

void osd_renderer_get_palette_buffer(UINT32 **buffer, int *width, int *pitch)
{
	D3DLOCKED_RECT locked_rect;
	HRESULT hr;

	*buffer = NULL;
	*pitch = 0;
	*width = 0;

	hr = IDirect3DTexture9_LockRect(palette_2d, 0, &locked_rect, NULL, D3DLOCK_DISCARD);
	if (!FAILED(hr))
	{
		*buffer = (UINT32*)locked_rect.pBits;
		*pitch = locked_rect.Pitch / 4;
		*width = 256;
	}
}

void osd_renderer_free_palette_buffer(void)
{
	IDirect3DTexture9_UnlockRect(palette_2d, 0);
}

void osd_renderer_get_priority_buffer(int layer_num, UINT8 **buffer, int *pitch)
{
	D3DLOCKED_RECT locked_rect;
	HRESULT hr;

	*buffer = NULL;
	*pitch = 0;

	hr = IDirect3DTexture9_LockRect(priority_2d[layer_num], 0, &locked_rect, NULL, D3DLOCK_DISCARD);
	if (!FAILED(hr))
	{
		*buffer = (UINT8*)locked_rect.pBits;
		*pitch = locked_rect.Pitch;
	}
}

void osd_renderer_free_priority_buffer(int layer_num)
{
	IDirect3DTexture9_UnlockRect(priority_2d[layer_num], 0);
}

void osd_renderer_draw_text(int x, int y, const char* string, DWORD color, BOOL shadow)
{
	RECT rect = { x, y, 496-1, 384-1 };
	RECT rect_s = { x+1, y+1, 496-1, 384-1 };
	if (shadow)
	{
		font->lpVtbl->DrawText(font, NULL, string, -1, &rect_s, 0, 0xFF000000);
	}

	font->lpVtbl->DrawText(font, NULL, string, -1, &rect, 0, color);
}


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
	memcpy( &view_matrix, m, sizeof(MATRIX) );
	view_matrix._22 = -view_matrix._22;
}

static void cache_model(VERTEX *vb, VERTEX *vb_alpha, UINT32 *src, UINT32 address, int *verts, int *verts_alpha)
{
	float fixed_point_scale;
	VERTEX vertex[4];
	VERTEX prev_vertex[4];
	int end, index, vb_index, vb_index_alpha;
	int polygon_index = 0;
	float uv_scale;

	int polygon = 0;

	int normal_num = 0;

	*verts = 0;
	*verts_alpha = 0;

	//UINT32 *src = get_address( address );

	if(src == NULL)
		return FALSE;

	if(m3_config.step == 0x10)
		fixed_point_scale = 32768.0f;
	else
		fixed_point_scale = 524288.0f;

	memset(prev_vertex, 0, sizeof(prev_vertex));
	end = 0;
	index = 0;
	vb_index = 0;
	vb_index_alpha = 0;

	do {
		UINT32 header[7];
		UINT32 entry[16];
		D3DCOLOR color, color2;
		int transparency;
		int i, num_vertices, num_old_vertices;
		int v2, v;
		int texture_format;
		int texture_x, texture_y, tex_width, tex_height;
		float nx, ny, nz;
		float dot;
		int polygon_id;

		for( i=0; i<7; i++) {
			header[i] = src[index];
			index++;
		}

		uv_scale = (header[1] & 0x40) ? 1.0f : 8.0f;

		polygon_id = (header[0] >> 10) & 0x3fff;

		if (polygon == 0 && (header[0] & 0xf) != 0)
			return FALSE;

		if (header[6] == 0 /*|| header[0] & 0x300*/)
			return FALSE;

		if ((header[0] & 0x300) == 0x300)
		{
			//printf("Polygon = %08X %08X %08X %08X %08X %08X %08X (address = %08X)\n", header[0], header[1], header[2], header[3], header[4], header[5], header[6], address);
		}

		// Check if this is the last polygon
		if(header[1] & 0x4)
			end = 1;

		transparency = ((header[6] >> 18) & 0x1F) << 3;
		if (header[6] & 0x800000)
		{
			color = 0xFF000000 | ((header[4] >> 8) & 0xffffff);
		}
		else
		{
			color = (transparency << 24) | ((header[4] >> 8) & 0xffffff);
		}

		if (header[6] & 0x10000)
		{
			color2 = 0xffff0000 | (((header[6] >> 11) & 0x1F) << 3) << 16;
		}
		else
		{
			color2 = 0;
		}

		if (header[6] & 0x4000000)
		{
			// texture enable
			color2 |= 0x0000ff00;
		}

		texture_x	= 32 * (((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 0x1));
		texture_y	= 32 * ((header[5] & 0x1F) | ((header[4] >> 1) & 0x20));
		tex_width	= 32 << ((header[3] >> 3) & 0x7);
		tex_height	= 32 << (header[3] & 0x7);
		
		texture_format	= ((header[6] >> 7) & 0x7) ? 0 : 1;
		if (texture_format == 0)	texture_y += 2048;

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
				//vertex[v2].nx = prev_vertex[v].nx;
				//vertex[v2].ny = prev_vertex[v].ny;
				//vertex[v2].nz = prev_vertex[v].nz;
				vertex[v2].color	= color;
				vertex[v2].color2	= color2;
				vertex[v2].tx		= texture_x;
				vertex[v2].ty		= texture_y;
				vertex[v2].twidth	= tex_width;
				vertex[v2].theight	= tex_height;
				v2++;
			}
		}

		// Load vertex data
		for( i=0; i<(num_vertices - num_old_vertices) * 4; i++) {
			entry[i] = src[index];
			index++;
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

			if ((ix & 0xf0000000) == 0x70000000 ||
				(iy & 0xf0000000) == 0x70000000 ||
				(iz & 0xf0000000) == 0x70000000)
				return FALSE;

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
			vertex[v2].u		= ((float)(tx) / uv_scale) / (float)xsize;
			vertex[v2].v		= ((float)(ty) / uv_scale) / (float)ysize;
			vertex[v2].nx		= nx;
			vertex[v2].ny		= ny;
			vertex[v2].nz		= nz;
			vertex[v2].color	= color;
			vertex[v2].color2	= color2;
			vertex[v2].tx		= texture_x;
			vertex[v2].ty		= texture_y;
			vertex[v2].twidth	= tex_width;
			vertex[v2].theight	= tex_height;
			v2++;

			if (header[0] & 0x300)
			{
				//printf("   Vert: %08X %08X %08X %08X\n", entry[v_index+0], entry[v_index+1], entry[v_index+2], entry[v_index+3]);
			}
		}	
		
		{
			D3DXVECTOR3 e1, e2, cross, n;
			e1.x = vertex[1].x - vertex[0].x;
			e1.y = vertex[1].y - vertex[0].y;
			e1.z = vertex[1].z - vertex[0].z;
			e2.x = vertex[2].x - vertex[0].x;
			e2.y = vertex[2].y - vertex[0].y;
			e2.z = vertex[2].z - vertex[0].z;
			
			n.x = nx;
			n.y = ny;
			n.z = nz;
			
			D3DXVec3Cross(&cross, &e2, &e1);
			D3DXVec3Normalize(&cross, &cross);
			dot = D3DXVec3Dot(&n, &cross);
		}

		// Transparent polygons
		// Translucent texture (ARGB4444)
		if((header[6] & 0x800000) == 0 || header[6] & 0x1 /* || header[6] & 0x80000000*/)
		{
			if (dot >= 0)
			{
				memcpy(&vb_alpha[vb_index_alpha+0], &vertex[0], sizeof(VERTEX));
				memcpy(&vb_alpha[vb_index_alpha+1], &vertex[1], sizeof(VERTEX));
				memcpy(&vb_alpha[vb_index_alpha+2], &vertex[2], sizeof(VERTEX));
			}
			else
			{
				memcpy(&vb_alpha[vb_index_alpha+0], &vertex[2], sizeof(VERTEX));
				memcpy(&vb_alpha[vb_index_alpha+1], &vertex[1], sizeof(VERTEX));
				memcpy(&vb_alpha[vb_index_alpha+2], &vertex[0], sizeof(VERTEX));
			}
			vb_index_alpha += 3;
		
			if (num_vertices > 3)
			{
				if (dot >= 0)
				{
					memcpy(&vb_alpha[vb_index_alpha+0], &vertex[0], sizeof(VERTEX));
					memcpy(&vb_alpha[vb_index_alpha+1], &vertex[2], sizeof(VERTEX));
					memcpy(&vb_alpha[vb_index_alpha+2], &vertex[3], sizeof(VERTEX));
				}
				else
				{
					memcpy(&vb_alpha[vb_index_alpha+0], &vertex[3], sizeof(VERTEX));
					memcpy(&vb_alpha[vb_index_alpha+1], &vertex[2], sizeof(VERTEX));
					memcpy(&vb_alpha[vb_index_alpha+2], &vertex[0], sizeof(VERTEX));
				}
				vb_index_alpha += 3;
			}
		}
		else
		{
			if (dot >= 0)
			{
				memcpy(&vb[vb_index+0], &vertex[0], sizeof(VERTEX));
				memcpy(&vb[vb_index+1], &vertex[1], sizeof(VERTEX));
				memcpy(&vb[vb_index+2], &vertex[2], sizeof(VERTEX));
			}
			else
			{
				memcpy(&vb[vb_index+0], &vertex[2], sizeof(VERTEX));
				memcpy(&vb[vb_index+1], &vertex[1], sizeof(VERTEX));
				memcpy(&vb[vb_index+2], &vertex[0], sizeof(VERTEX));
			}
			vb_index += 3;
		
			if (num_vertices > 3)
			{
				if (dot >= 0)
				{
					memcpy(&vb[vb_index+0], &vertex[0], sizeof(VERTEX));
					memcpy(&vb[vb_index+1], &vertex[2], sizeof(VERTEX));
					memcpy(&vb[vb_index+2], &vertex[3], sizeof(VERTEX));
				}
				else
				{
					memcpy(&vb[vb_index+0], &vertex[3], sizeof(VERTEX));
					memcpy(&vb[vb_index+1], &vertex[2], sizeof(VERTEX));
					memcpy(&vb[vb_index+2], &vertex[0], sizeof(VERTEX));
				}
				vb_index += 3;
			}
		}
		

		// Copy current vertex as previous vertex
		memcpy(prev_vertex, vertex, sizeof(VERTEX) * 4);

		polygon++;
		
	} while(end == 0);

	*verts = vb_index / 3;
	*verts_alpha = vb_index_alpha / 3;
}

void osd_renderer_draw_model(UINT32 *mem, UINT32 address, int dynamic)
{
	int num_triangles, num_triangles_alpha;
	VERTEX *vb, *vb_alpha;
	D3DMATRIX world_view_proj, world_view;
	D3DMATRIX matrix = d3d_matrix_stack_get_top();
	
	// Make the world-view matrix
	//D3DXMatrixMultiply(&world_view, &matrix, &view_matrix);
	D3DXMatrixTranspose(&world_view, &matrix);
	
	// Make the world-view-projection matrix
	D3DXMatrixMultiply(&world_view_proj, &matrix, &view_matrix);
	D3DXMatrixMultiply(&world_view_proj, &world_view_proj, &projection_matrix);
	D3DXMatrixTranspose(&world_view_proj, &world_view_proj);
	
	if (dynamic)
	{
		int vert_index = dynamic_mesh_cache_vertex_top;
		int vert_index_alpha = dynamic_mesh_cache_vertex_top_alpha;
		
		IDirect3DVertexBuffer9_Lock(dynamic_vb, 0, 0, (void **)&vb, D3DLOCK_DISCARD);
		IDirect3DVertexBuffer9_Lock(dynamic_vb_alpha, 0, 0, (void **)&vb_alpha, D3DLOCK_DISCARD);
		cache_model(&vb[vert_index], &vb_alpha[vert_index_alpha], mem, address, &num_triangles, &num_triangles_alpha);
		IDirect3DVertexBuffer9_Unlock(dynamic_vb_alpha);
		IDirect3DVertexBuffer9_Unlock(dynamic_vb);

		dynamic_mesh_cache[dynamic_mesh_cache_top].vb_index = vert_index;
		dynamic_mesh_cache[dynamic_mesh_cache_top].vb_index_alpha = vert_index_alpha;
		dynamic_mesh_cache[dynamic_mesh_cache_top].num_vertices = num_triangles * 3;
		dynamic_mesh_cache[dynamic_mesh_cache_top].num_vertices_alpha = num_triangles_alpha * 3;
		
		dynamic_mesh_cache_vertex_top += num_triangles * 3;
		dynamic_mesh_cache_vertex_top_alpha += num_triangles_alpha * 3;
		
		memcpy(&dynamic_mesh_buffer[dynamic_mesh_buffer_top].normal, &world_view, sizeof(D3DMATRIX));
		memcpy(&dynamic_mesh_buffer[dynamic_mesh_buffer_top].transform, &world_view_proj, sizeof(D3DMATRIX));
	
		dynamic_mesh_buffer[dynamic_mesh_buffer_top].lighting = current_light;
		dynamic_mesh_buffer[dynamic_mesh_buffer_top].viewport = current_viewport;
		
		dynamic_mesh_buffer[dynamic_mesh_buffer_top].mesh_index = dynamic_mesh_cache_top;
		
		dynamic_mesh_cache_top++;
		if (dynamic_mesh_cache_top >= NUM_DYNAMIC_MESHES)
		{
			printf("dynamic mesh cache overflow!\n");
			exit(1);
		}
		
		dynamic_mesh_buffer_top++;
		if (dynamic_mesh_buffer_top >= DYNAMIC_MESH_BUFFER_SIZE)
		{
			printf("dynamic mesh buffer overflow!\n");
			exit(1);
		}

		if (dynamic_mesh_cache_vertex_top >= DYNAMIC_VB_SIZE)
		{
			printf("dynamic vertex cache overflow!\n");
			exit(1);
		}
		if (dynamic_mesh_cache_vertex_top_alpha >= DYNAMIC_VB_SIZE)
		{
			printf("dynamic vertex cache alpha overflow!\n");
			exit(1);
		}
	}
	else
	{
		UINT16 mesh_index = static_mesh_index[address & 0xffffff];
		
		if (mesh_index == 0xffff)
		{
			// cache new mesh
			int vert_index = static_mesh_cache_vertex_top;
			int vert_index_alpha = static_mesh_cache_vertex_top_alpha;

			int voffset = vert_index * sizeof(VERTEX);
			int voffset_alpha = vert_index_alpha * sizeof(VERTEX);
			int vsize = 10000 * sizeof(VERTEX);
		
			IDirect3DVertexBuffer9_Lock(static_vb, voffset, vsize, (void **)&vb, 0);
			IDirect3DVertexBuffer9_Lock(static_vb_alpha, voffset_alpha, vsize, (void **)&vb_alpha, 0);
			cache_model(&vb[0], &vb_alpha[0], mem, address, &num_triangles, &num_triangles_alpha);
			IDirect3DVertexBuffer9_Unlock(static_vb_alpha);
			IDirect3DVertexBuffer9_Unlock(static_vb);

			static_mesh_cache[static_mesh_cache_top].vb_index = vert_index;
			static_mesh_cache[static_mesh_cache_top].vb_index_alpha = vert_index_alpha;
			static_mesh_cache[static_mesh_cache_top].num_vertices = num_triangles * 3;
			static_mesh_cache[static_mesh_cache_top].num_vertices_alpha = num_triangles_alpha * 3;
		
			static_mesh_cache_vertex_top += num_triangles * 3;
			static_mesh_cache_vertex_top_alpha += num_triangles_alpha * 3;
		
			static_mesh_index[address & 0xffffff] = static_mesh_cache_top;
			mesh_index = static_mesh_cache_top;
			
			static_mesh_cache_top++;
			if (static_mesh_cache_top >= NUM_STATIC_MESHES)
			{
				printf("static mesh cache overflow!\n");
				exit(1);
			}

			if (static_mesh_cache_vertex_top >= STATIC_VB_SIZE)
			{
				printf("static vertex cache overflow!\n");
				exit(1);
			}
			if (static_mesh_cache_vertex_top_alpha >= STATIC_VB_SIZE)
			{
				printf("static vertex cache alpha overflow!\n");
				exit(1);
			}
		}
			
		memcpy(&static_mesh_buffer[static_mesh_buffer_top].normal, &world_view, sizeof(D3DMATRIX));
		memcpy(&static_mesh_buffer[static_mesh_buffer_top].transform, &world_view_proj, sizeof(D3DMATRIX));

		static_mesh_buffer[static_mesh_buffer_top].lighting = current_light;
		static_mesh_buffer[static_mesh_buffer_top].viewport = current_viewport;
			
		static_mesh_buffer[static_mesh_buffer_top].mesh_index = mesh_index;
		
		static_mesh_buffer_top++;
		if (static_mesh_buffer_top >= STATIC_MESH_BUFFER_SIZE)
		{
			printf("static mesh buffer overflow!\n");
			exit(1);
		}
	}
}


void osd_renderer_begin_3d_scene(void)
{
	dynamic_mesh_buffer_top = 0;
	static_mesh_buffer_top = 0;
	
	dynamic_mesh_cache_top = 0;
	dynamic_mesh_cache_vertex_top = 0;
	dynamic_mesh_cache_vertex_top_alpha = 0;

	num_lights = 0;
	num_viewports = 0;
}

void osd_renderer_end_3d_scene(void)
{
	int i;
	int end = 0;
	int index = 0;
	float mipmap_lod_bias;

	int selected_viewport = -1;
	int selected_lighting = -1;

	IDirect3DDevice9_BeginScene(device);

	IDirect3DDevice9_SetVertexDeclaration(device, vertex_declaration);
	IDirect3DDevice9_SetVertexShader(device, vshader);
	IDirect3DDevice9_SetPixelShader(device, pshader);	
	IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE);
	IDirect3DDevice9_SetRenderState(device, D3DRS_FILLMODE, D3DFILL_SOLID);
	IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, D3DZB_USEW);

	IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHATESTENABLE, FALSE);

	IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHABLENDENABLE, FALSE);
//	IDirect3DDevice9_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
//	IDirect3DDevice9_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice9_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
	
	IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MAXMIPLEVEL, 0);
	//IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	
	//mipmap_lod_bias = -4.0f;
	//IDirect3DDevice9_SetSamplerState( device, 0, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)(&mipmap_lod_bias));
	
	IDirect3DDevice9_SetTexture(device, 0, texture);
	
	IDirect3DDevice9_SetStreamSourceFreq(device, 0, 1);
	IDirect3DDevice9_SetStreamSource(device, 0, static_vb, 0, sizeof(VERTEX));

	set_viewport(0, 0, 496, 384);
	
	// render static meshes
	for (i=0; i < static_mesh_buffer_top; i++)
	{
		D3DMATRIX *transform, *normal;
		int num_triangles, vb_index;
		
		int mesh_index = static_mesh_buffer[i].mesh_index;
		vb_index = static_mesh_cache[mesh_index].vb_index;
		num_triangles = static_mesh_cache[mesh_index].num_vertices / 3;

		if (num_triangles > 0)
		{
			if (selected_viewport != static_mesh_buffer[i].viewport)
			{
				D3DVIEWPORT9 vp;
				selected_viewport = static_mesh_buffer[i].viewport;
				vp.X		= viewport_params[selected_viewport].x;
				vp.Y		= viewport_params[selected_viewport].y;
				vp.Width	= viewport_params[selected_viewport].width;
				vp.Height	= viewport_params[selected_viewport].height;
				vp.MinZ		= min_z;
				vp.MaxZ		= max_z;
				IDirect3DDevice9_SetViewport(device, &vp);
			}
		
			transform = &static_mesh_buffer[i].transform;
			normal = &static_mesh_buffer[i].normal;
		
			IDirect3DDevice9_SetVertexShaderConstantF(device, 0, (float*)transform, 4);
			IDirect3DDevice9_SetVertexShaderConstantF(device, 4, (float*)normal, 4);
		
			if (selected_lighting != static_mesh_buffer[i].lighting)
			{
				selected_lighting = static_mesh_buffer[i].lighting;
				IDirect3DDevice9_SetVertexShaderConstantF(device, 16, (float*)&lighting_params[selected_lighting].sun_vector, 1);
				IDirect3DDevice9_SetVertexShaderConstantF(device, 17, (float*)&lighting_params[selected_lighting].sun_params, 1);
			}
		
			IDirect3DDevice9_DrawPrimitive(device, D3DPT_TRIANGLELIST, vb_index, num_triangles);
		}
	}
	
	IDirect3DDevice9_SetStreamSourceFreq(device, 0, 1);
	IDirect3DDevice9_SetStreamSource(device, 0, dynamic_vb, 0, sizeof(VERTEX));

	// render dynamic meshes
	for (i=0; i < dynamic_mesh_buffer_top; i++)
	{
		D3DMATRIX *transform, *normal;
		int num_triangles, vb_index;
	
		int mesh_index = dynamic_mesh_buffer[i].mesh_index;
		vb_index = dynamic_mesh_cache[mesh_index].vb_index;
		num_triangles = dynamic_mesh_cache[mesh_index].num_vertices / 3;

		if (num_triangles > 0)
		{

			if (selected_viewport != dynamic_mesh_buffer[i].viewport)
			{
				D3DVIEWPORT9 vp;
				selected_viewport = dynamic_mesh_buffer[i].viewport;
				vp.X		= viewport_params[selected_viewport].x;
				vp.Y		= viewport_params[selected_viewport].y;
				vp.Width	= viewport_params[selected_viewport].width;
				vp.Height	= viewport_params[selected_viewport].height;
				vp.MinZ		= min_z;
				vp.MaxZ		= max_z;
				IDirect3DDevice9_SetViewport(device, &vp);
			}
			
			transform = &dynamic_mesh_buffer[i].transform;
			normal = &dynamic_mesh_buffer[i].normal;
		
			IDirect3DDevice9_SetVertexShaderConstantF(device, 0, (float*)transform, 4);
			IDirect3DDevice9_SetVertexShaderConstantF(device, 4, (float*)normal, 4);

			if (selected_lighting != dynamic_mesh_buffer[i].lighting)
			{
				selected_lighting = dynamic_mesh_buffer[i].lighting;
				IDirect3DDevice9_SetVertexShaderConstantF(device, 16, (float*)&lighting_params[selected_lighting].sun_vector, 1);
				IDirect3DDevice9_SetVertexShaderConstantF(device, 17, (float*)&lighting_params[selected_lighting].sun_params, 1);
			}
					
			IDirect3DDevice9_DrawPrimitive(device, D3DPT_TRIANGLELIST, vb_index, num_triangles);
		}
	}

	// render alpha polys
	IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice9_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice9_SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE);

	IDirect3DDevice9_SetStreamSourceFreq(device, 0, 1);
	IDirect3DDevice9_SetStreamSource(device, 0, dynamic_vb_alpha, 0, sizeof(VERTEX));

	// render dynamic meshes
	//for (i=dynamic_mesh_buffer_top-1; i >= 0; i--)
	for (i=0; i < dynamic_mesh_buffer_top; i++)
	{
		D3DMATRIX *transform, *normal;
		int num_triangles, vb_index;
	
		int mesh_index = dynamic_mesh_buffer[i].mesh_index;
		vb_index = dynamic_mesh_cache[mesh_index].vb_index_alpha;
		num_triangles = dynamic_mesh_cache[mesh_index].num_vertices_alpha / 3;

		if (num_triangles > 0)
		{
			if (selected_viewport != dynamic_mesh_buffer[i].viewport)
			{
				D3DVIEWPORT9 vp;
				selected_viewport = dynamic_mesh_buffer[i].viewport;
				vp.X		= viewport_params[selected_viewport].x;
				vp.Y		= viewport_params[selected_viewport].y;
				vp.Width	= viewport_params[selected_viewport].width;
				vp.Height	= viewport_params[selected_viewport].height;
				vp.MinZ		= min_z;
				vp.MaxZ		= max_z;
				IDirect3DDevice9_SetViewport(device, &vp);
			}
		
			transform = &dynamic_mesh_buffer[i].transform;
			normal = &dynamic_mesh_buffer[i].normal;
		
			IDirect3DDevice9_SetVertexShaderConstantF(device, 0, (float*)transform, 4);
			IDirect3DDevice9_SetVertexShaderConstantF(device, 4, (float*)normal, 4);

			if (selected_lighting != dynamic_mesh_buffer[i].lighting)
			{
				selected_lighting = dynamic_mesh_buffer[i].lighting;
				IDirect3DDevice9_SetVertexShaderConstantF(device, 16, (float*)&lighting_params[selected_lighting].sun_vector, 1);
				IDirect3DDevice9_SetVertexShaderConstantF(device, 17, (float*)&lighting_params[selected_lighting].sun_params, 1);
			}
					
			IDirect3DDevice9_DrawPrimitive(device, D3DPT_TRIANGLELIST, vb_index, num_triangles);
		}
	}

	IDirect3DDevice9_SetStreamSourceFreq(device, 0, 1);
	IDirect3DDevice9_SetStreamSource(device, 0, static_vb_alpha, 0, sizeof(VERTEX));

	// render static meshes
	//for (i=static_mesh_buffer_top-1; i >= 0; i--)
	for (i=0; i < static_mesh_buffer_top; i++)
	{
		D3DMATRIX *transform, *normal;
		int num_triangles, vb_index;
		
		int mesh_index = static_mesh_buffer[i].mesh_index;
		vb_index = static_mesh_cache[mesh_index].vb_index_alpha;
		num_triangles = static_mesh_cache[mesh_index].num_vertices_alpha / 3;

		if (num_triangles > 0)
		{
			if (selected_viewport != static_mesh_buffer[i].viewport)
			{
				D3DVIEWPORT9 vp;
				selected_viewport = static_mesh_buffer[i].viewport;
				vp.X		= viewport_params[selected_viewport].x;
				vp.Y		= viewport_params[selected_viewport].y;
				vp.Width	= viewport_params[selected_viewport].width;
				vp.Height	= viewport_params[selected_viewport].height;
				vp.MinZ		= min_z;
				vp.MaxZ		= max_z;
				IDirect3DDevice9_SetViewport(device, &vp);
			}
		
			transform = &static_mesh_buffer[i].transform;
			normal = &static_mesh_buffer[i].normal;
		
			IDirect3DDevice9_SetVertexShaderConstantF(device, 0, (float*)transform, 4);
			IDirect3DDevice9_SetVertexShaderConstantF(device, 4, (float*)normal, 4);
		
			if (selected_lighting != static_mesh_buffer[i].lighting)
			{
				selected_lighting = static_mesh_buffer[i].lighting;
				IDirect3DDevice9_SetVertexShaderConstantF(device, 16, (float*)&lighting_params[selected_lighting].sun_vector, 1);
				IDirect3DDevice9_SetVertexShaderConstantF(device, 17, (float*)&lighting_params[selected_lighting].sun_params, 1);
			}
		
			IDirect3DDevice9_DrawPrimitive(device, D3DPT_TRIANGLELIST, vb_index, num_triangles);
		}
	}

	IDirect3DDevice9_EndScene(device);
}

void osd_renderer_set_light( int light_num, LIGHT* param )
{
	switch(param->type)
	{
		case LIGHT_PARALLEL:
			lighting_params[num_lights].sun_vector.x = -param->u * view_matrix._11;
			lighting_params[num_lights].sun_vector.y = param->v * view_matrix._22;
			lighting_params[num_lights].sun_vector.z = -param->w * view_matrix._33;
			lighting_params[num_lights].sun_vector.w = 1.0f;

			lighting_params[num_lights].sun_params.x = param->diffuse_intensity;
			lighting_params[num_lights].sun_params.y = param->ambient_intensity;
			break;

		default:
			error("Direct3D error: Unsupported light type: %d",param->type);
	}

	current_light = num_lights;
	
	num_lights++;
	if (num_lights >= MAX_LIGHTS)
	{
		error("Too many lights!");
	}
}

void osd_renderer_set_viewport(const VIEWPORT* vp)
{
	float fov = D3DXToRadian( (float)(vp->up + vp->down) );
	float aspect_ratio = (float)((float)vp->width / (float)vp->height);

	float x = vp->x;
	float y = vp->y;
	float w = vp->width;
	float h = vp->height;

	viewport_params[num_viewports].x = x;
	viewport_params[num_viewports].y = y;
	viewport_params[num_viewports].width = w;
	viewport_params[num_viewports].height = h;

	current_viewport = num_viewports;

	num_viewports++;
	if (num_viewports >= MAX_VIEWPORTS)
	{
		error("Too many viewports!");
	}

	D3DXMatrixPerspectiveFovLH(&projection_matrix, fov, aspect_ratio, min_z, max_z);
}

////////////////////////
//    Matrix Stack    //
////////////////////////

static D3DMATRIX matrix_to_d3d( const MATRIX m )
{
	D3DMATRIX c;
	memcpy( &c, m, sizeof(MATRIX) );

	return c;
}

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

	if (stack_ptr >= 256)
	{
		error("Matrix stack overflow\n");
	}
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

// Mipmap starting positions for each level
static const int mipmap_xpos[] =
{ 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
static const int mipmap_ypos[] =
{ 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023, 0 };

void renderer_upload_texture(int x, int y, int u, int v, int width, int height, void *data, int mip_level)
{
	HRESULT hr;
	int i, j;
	D3DLOCKED_RECT rect;
	UINT16 *s = (UINT16*)data;
	UINT32 *d;
	int pitch;
	RECT texture_rect;

	if (mip_level > 3)
		return;
	
	/*hr = IDirect3DTexture9_LockRect(texture, 0, &rect, NULL, 0);
	d = (UINT32*)rect.pBits;
	pitch = rect.Pitch / 4;
	
	for (j=y; j < y+height; j++)
	{
		int index = j * pitch;
		int u = x;
		for (i=x; i < x+width; i++)
		{
			UINT16 c = s[(j * 2048) + u];
			int r = ((c >> 10) & 0x1f) << 3;
			int g = ((c >>  5) & 0x1f) << 3;
			int b = ((c >>  0) & 0x1f) << 3;
			d[index+i] = 0xff000000 | (r << 16) | (g << 8) | (b);
			u++;
		}
	}
	
	for (j=y; j < y+height; j++)
	{
		int index = (j+2048) * pitch;
		int u = x;
		for (i=x; i < x+width; i++)
		{
			UINT16 c = s[(j * 2048) + u];
			int r = ((c >> 12) & 0xf) << 4;
			int g = ((c >>  8) & 0xf) << 4;
			int b = ((c >>  4) & 0xf) << 4;
			int a = ((c >>  0) & 0xf) << 4;
			d[index+i] = (a << 24) | (r << 16) | (g << 8) | (b);
			u++;
		}
	}
	
	IDirect3DTexture9_UnlockRect(texture, 0);*/

	u >>= mip_level;
	v >>= mip_level;
	width >>= mip_level;
	height >>= mip_level;

	texture_rect.left	= u;
	texture_rect.top	= v;
	texture_rect.right	= u + width;
	texture_rect.bottom	= v + height;

	hr = IDirect3DTexture9_LockRect(texture, mip_level, &rect, &texture_rect, 0);
	if (!FAILED(hr))
	{
		d = (UINT32*)rect.pBits;
		pitch = rect.Pitch / 4;
	
		for (j=0; j < height; j++)
		{
			int index = j * pitch;
			for (i=0; i < width; i++)
			{
				UINT16 c = s[((j+y) * 2048) + x + i];
				int r = ((c >> 10) & 0x1f) << 3;
				int g = ((c >>  5) & 0x1f) << 3;
				int b = ((c >>  0) & 0x1f) << 3;
				int a = (c & 0x8000) ? 0 : 0xff;
				d[index+i] = (a << 24) | (r << 16) | (g << 8) | (b);
			}
		}
	}
	IDirect3DTexture9_UnlockRect(texture, mip_level);

	texture_rect.left	= u;
	texture_rect.top	= v + (2048 >> mip_level);
	texture_rect.right	= u + width;
	texture_rect.bottom	= v + (2048 >> mip_level) + height;
	
	hr = IDirect3DTexture9_LockRect(texture, mip_level, &rect, &texture_rect, 0);
	if (!FAILED(hr))
	{
		d = (UINT32*)rect.pBits;
		pitch = rect.Pitch / 4;
		for (j=0; j < height; j++)
		{
			int index = j * pitch;
			for (i=0; i < width; i++)
			{
				UINT16 c = s[((j+y) * 2048) + x + i];
				int r = ((c >> 12) & 0xf) << 4;
				int g = ((c >>  8) & 0xf) << 4;
				int b = ((c >>  4) & 0xf) << 4;
				int a = ((c >>  0) & 0xf) << 4;
				d[index+i] = (a << 24) | (r << 16) | (g << 8) | (b);
			}
		}
	}
	IDirect3DTexture9_UnlockRect(texture, mip_level);
}

void osd_renderer_invalidate_textures(UINT x, UINT y, int u, int v, UINT w, UINT h, UINT8 *texture_sheet, int miplevel)
{
	renderer_upload_texture(x, y, u, v, w, h, texture_sheet, miplevel);
}

UINT32 osd_renderer_get_features(void)
{
	return RENDERER_FEATURE_PALETTE | RENDERER_FEATURE_PRIORITY;
}