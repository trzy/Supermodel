/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

 /*
  * Render2D.cpp
  *
  * Implementation of the CRender2D class: OpenGL tile generator graphics.
  *
  * To-Do List
  * ----------
  * - Is there a universal solution to the 'ROLLING START' scrolling bug (Scud
  *   Race) and the scrolling text during Magical Truck Adventure's attract
  *   mode? To fix Scud Race, either the stencil mask or the h-scroll value must
  *   be shifted by 16 pixels. Magical Truck Adventure is similar but opposite.
  *   Perhaps this is a function of timing registers accessed via JTAG?
  * - Is there a better way to handle the overscan regions in wide screen mode?
  *   Is clearing two thin viewports better than one big clear?
  * - Are v-scroll values 9 or 10 bits? (Does it matter?) Lost World seems to
  *   have some scrolling issues.
  * - A proper shut-down function is needed! OpenGL might not be available when
  *   the destructor for this class is called.
  *
  * Tile Generator Hardware Overview
  * --------------------------------
  *
  * Model 3's medium resolution tile generator hardware appears to be derived
  * from the Model 2 and System 24 chipset, but is much simpler. It consists of
  * four 64x64 tile layers, comprised of 8x8 pixel tiles, with configurable
  * priorities. There may be additional features but so far, no known Model 3
  * games use them.
  *
  * VRAM is comprised of 1 MB for tile data and an additional 128 KB for the
  * palette (each color occupies 32 bits). The four tilemap layers are referred
  * to as: A (0), A' (1), B (2), and B' (3). Palette RAM may be located on a
  * separate RAM IC.
  *
  * Registers
  * ---------

	 0xF1180020:         -------- -------- -------- -------- ?
						 -------- -------- x------- -------- Layer 3 bitdepth (0 = 8-bit, 1 = 4-bit)
						 -------- -------- -x------ -------- Layer 2 bitdepth (0 = 8-bit, 1 = 4-bit)
						 -------- -------- --x----- -------- Layer 1 bitdepth (0 = 8-bit, 1 = 4-bit)
						 -------- -------- ---x---- -------- Layer 0 bitdepth (0 = 8-bit, 1 = 4-bit)
						 -------- -------- ----x--- -------- Layer 3 priority (0 = below 3D, 1 = above 3D)
						 -------- -------- -----x-- -------- Layer 2 priority (0 = below 3D, 1 = above 3D)
						 -------- -------- ------x- -------- Layer 1 priority (0 = below 3D, 1 = above 3D)
						 -------- -------- -------x -------- Layer 0 priority (0 = below 3D, 1 = above 3D)

	 0xF1180040:                                             Layer 0 & 1 modulation
						 -------- xxxxxxxx -------- -------- Red component
						 -------- -------- xxxxxxxx -------- Green component
						 -------- -------- -------- xxxxxxxx Blue component

	 0xF1180044:                                             Layer 2 & 3 modulation
						 -------- xxxxxxxx -------- -------- Red component
						 -------- -------- xxxxxxxx -------- Green component
						 -------- -------- -------- xxxxxxxx Blue component

	 0xF1180060:         x------- -------- -------- -------- Layer 0 enable
						 -------x xxxxxxxx -------- -------- Layer 0 Y scroll position
						 -------- -------- x------- -------- Layer 0 X line scroll enable
						 -------- -------- -------x xxxxxxxx Layer 0 X scroll position

	 0xF1180064:         x------- -------- -------- -------- Layer 1 enable
						 -------x xxxxxxxx -------- -------- Layer 1 Y scroll position
						 -------- -------- x------- -------- Layer 1 X line scroll enable
						 -------- -------- -------x xxxxxxxx Layer 1 X scroll position

	 0xF1180068:         x------- -------- -------- -------- Layer 2 enable
						 -------x xxxxxxxx -------- -------- Layer 2 Y scroll position
						 -------- -------- x------- -------- Layer 2 X line scroll enable
						 -------- -------- -------x xxxxxxxx Layer 2 X scroll position

	 0xF118006C:         x------- -------- -------- -------- Layer 3 enable
						 -------x xxxxxxxx -------- -------- Layer 3 Y scroll position
						 -------- -------- x------- -------- Layer 3 X line scroll enable
						 -------- -------- -------x xxxxxxxx Layer 3 X scroll position

  *
  * VRAM Memory Map
  * ---------------
  *
  * The lower 1 MB of VRAM is used for storing tiles, per-line horizontal scroll
  * values, and the stencil mask, which determines which of each pair of layers
  * is displayed on a given line and column.
  *
  *    00000-F5FFF   Tile pattern data
  *    F6000-F63FF   Layer A horizontal scroll table (512 lines)
  *    F6400-F67FF   Layer A' horizontal scroll table
  *    F6800-F6BFF   Layer B horizontal scroll table
  *    F6C00-F6FFF   Layer B' horizontal scroll table
  *    F7000-F77FF   Mask table (assuming 4 bytes per line, 512 lines)
  *    F7800-F7FFF   ?
  *    F8000-F9FFF   Layer A name table
  *    FA000-FBFFF   Layer A' name table
  *    FC000-FDFFF   Layer B name table
  *    FE000-FFFFF   Layer B' name table
  *
  * Tiles may actually address the entire 1 MB space, although in practice,
  * that would conflict with the other fixed memory regions.
  *
  * Palette
  * -------
  *
  * The palette stores 32768 colors. Each entry is a little endian 32-bit word.
  * The upper 16 bits are unused and the lower 16 bits contain the color:
  *
  *    15                 0
  *     tbbb bbgg gggr rrrr
  *
  * The 't' bit is for transparency. When set, pixels of that color are
  * transparent, unless they are the bottom-most layer.
  *
  * Tile Name Table and Pattern Layout
  * ----------------------------------
  *
  * The name table is a 64x64 array of 16-bit words serving as indices for tile
  * pattern data and the palette. The first 64 words correspond to the first
  * row of tiles, the next 64 to the second row, etc. Although 64x64 entries
  * describes a 512x512 pixel screen, only the upper-left 62x48 tiles are
  * visible when the vertical and horizontal scroll values are 0. Scrolling
  * moves the 496x384 pixel 'window' around, with individual wrapping of the
  * two axes.
  *
  * The data is actually arranged in 32-bit chunks in little endian format, so
  * that tiles 0, 1, 2, and 3 will be stored as 1, 0, 3, 2. Fetching two name
  * table entries as a single 32-bit word places the left tile in the high 16
  * bits and the right tile in the low 16 bits.
  *
  * The format of a name table entry in 4-bit color mode is:
  *
  *    15                 0
  *     jkpp pppp pppp iiii
  *
  * The pattern index is '0ppp pppp pppi iiij'. Multiplying by 32 yields the
  * offset in VRAM at which the tile pattern data is stored. Note that the MSB
  * of the name table entry becomes the LSB of the pattern index. This allows
  * for 32768 4-bit tile patterns, each occupying 32 bytes, which means the
  * whole 1 MB VRAM space can be addressed.
  *
  * The 4-bit pattern data is stored as 8 32-bit words. Each word stores a row
  * of 8 pixels:
  *
  *    31                                     0
  *     aaaa bbbb cccc dddd eeee ffff gggg hhhh
  *
  * 'a' is the left-most pixel data. These 4-bit values are combined with bits
  * from the name table to form a palette index, which determines the final
  * color. For example, for pixel 'a', the 15-bit color index is:
  *
  *      14                0
  *     kpp pppp pppp aaaa
  *
  * Note that index bits are re-used to form the palette index, meaning that
  * the pattern address partly determines the color.
  *
  * In 8-bit color mode, the name table entry looks like:
  *
  *    15                 0
  *     ?ppp pppp iiii iiii
  *
  * The low 15 'p' and 'i' bits together form the pattern index, which must be
  * multiplied by 64 to get the offset. The pattern data now consists of 16 32-
  * bit words, each containing four 8-bit pixels:
  *
  *    31                                     0
  *     aaaa aaaa bbbb bbbb cccc cccc dddd dddd
  *
  * 'a' is the left-most pixel. Each line is therefore comprised of two 32-bit
  * words. The palette index for pixel 'a' is now formed from:
  *
  *    14                0
  *     ppp pppp aaaa aaaa
  *
  * Stencil Mask
  * ------------
  *
  * For any pixel position, there are in fact only two visible layers, despite
  * there being four defined layers. The layers are grouped in pairs: A (the
  * 'primary' layer) and A' (the 'alternate') form one pair, and B and B' form
  * the other. Only one of the primary or alternate layers from each group may
  * be visible at a given position. The 'stencil mask' controls this.
  *
  * The mask table is a bit field organized into 512 (or 384?) lines with each
  * bit controlling four columns (32 pixels). The mask does not appear to be
  * affected by scrolling -- that is, it does not scroll with the underlying
  * tiles, which do so independently. The mask remains fixed.
  *
  * Each mask entry is a little endian 32-bit word. The high 16 bits control
  * A/A' and the low 16 bits control B/B'. Each word controls an entire line
  * (32 pixels per bit, 512 pixels per 16-bit line mask, where the first 16
  * pixels are allocated to the overscan region.) If a bit is set to 1, the
  * pixel from the primary layer is used, otherwise the alternate layer is
  * used when the mask is 0. It is important to remember that the layers may
  * have been scrolled independently. The mask operates on the final resultant
  * two pixels that are determined for each location.
  *
  * Example of a line mask:
  *
  *    31                  15                 0
  *     0111 0000 0000 1111 0000 0000 1111 1111
  *
  * These settings would display layer A' for the first 32 pixels of the line,
  * followed by layer A for the next 96 pixels, A' for the subsequent 256
  * pixels, and A for the final 128 pixels. The first 256 pixels of the line
  * would display layer B' and the second 256 pixels would be from layer B.
  *
  * The stencil mask does not affect layer priorities, which are managed
  * separately regardless of mask settings.
  *
  * Scrolling
  * ---------
  *
  * Each of the four layers can be scrolled independently. Vertical scroll
  * values are stored in the appropriate scroll register and horizontal scroll
  * values can be sourced either from the register (in which case the entire
  * layer will be scrolled uniformly) or from a table in VRAM (which contains
  * independent values for each line).
  *
  * The scroll registers are laid out as:
  *
  *    31                                     0
  *     e??? ???y yyyy yyyy h??? ??xx xxxx xxxx
  *
  * The 'e' bit enables the layer when set. The 'y' bits comprise a vertical
  * scroll value in pixels. The 'x' bits form a horizontal scroll value. If 'h'
  * is set, then the VRAM table (line-by-line scrolling) is used, otherwise the
  * 'x' values are applied to every line. It is also possible that the scroll
  * values use more or less bits, but probably no more than 1.
  *
  * Each line must be wrapped back to the beginning of the same line. Likewise,
  * vertical scrolling wraps around back to the top of the tilemap.
  *
  * The horizontal scroll table is a series of 16-bit little endian words, one
  * for each line beginning at 0. It appears all the values can be used for
  * scrolling (no control bits have been observed). The number of bits actually
  * used by the hardware is irrelevant -- wrapping has the effect of making
  * higher order bits unimportant.
  *
  * Layer Priorities
  * ----------------
  *
  * The layer control register (0x20) contains 4 bits that appear to control
  * layer priorities. It is assumed that the 3D graphics, output by the Real3D
  * pixel processors independently of the tile generator, constitute their own
  * 'layer' and that the 2D tilemaps appear in front or behind. There may be a
  * specific function for each priority bit or the field may be interpreted as a
  * single 4-bit value denoting preset layer orders.
  *
  * Color Offsets
  * -------------
  *
  * Color offsets can be applied to the final RGB color value of every pixel.
  * This is used for effects such as fading to a certain color, lightning (Lost
  * World), etc. The current best guess is that the two registers control each
  * pair (A/A' and B/B') of layers. The format appears to be:
  *
  *    31                                     0
  *     ???? ???? rrrr rrrr gggg gggg bbbb bbbb
  *
  * Where 'r', 'g', and 'b' appear to be signed 8-bit color offsets. Because
  * they exceed the color resolution of the palette, they must be scaled
  * appropriately.
  *
  * Color offset registers are handled in TileGen.cpp. Two palettes are computed
  * -- one for A/A' and another for B/B'. These are passed to the renderer.
  */

#include "Render2D.h"

#include "Supermodel.h"
#include "Shader.h"
#include "Shaders2D.h" // fragment and vertex shaders

/******************************************************************************
 Frame Display Functions
******************************************************************************/

// Set up viewport and OpenGL state for 2D rendering (sets up blending function but disables blending)
void CRender2D::Setup2D(bool isBottom)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // alpha of 1.0 is opaque, 0 is transparent

	// Disable Z-buffering
	glDisable(GL_DEPTH_TEST);

	// Clear everything if requested or just overscan areas for wide screen mode
	if (isBottom)
	{
		if (m_aaTarget) {
			glBindFramebuffer(GL_FRAMEBUFFER, m_aaTarget);	// set target if needed
		}

		glClearColor(0.0, 0.0, 0.0, 0.0);
		glViewport	(0, 0, m_totalXPixels, m_totalYPixels);
		glDisable	(GL_SCISSOR_TEST);							// scissor is enabled to fix the 2d/3d miss match problem
		glClear		(GL_COLOR_BUFFER_BIT);						// we want to clear outside the scissored areas so must disable it
		glEnable	(GL_SCISSOR_TEST);

		if (m_aaTarget) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);			// restore target if needed
		}
	}

	// Set up the viewport and orthogonal projection
	bool stretchBottom = m_config["WideBackground"].ValueAs<bool>() && isBottom;
	if (!stretchBottom)
	{
		glViewport(m_xOffset - m_correction, m_yOffset + m_correction, m_xPixels, m_yPixels); //Preserve aspect ratio of tile layer by constraining and centering viewport
	}
}

void CRender2D::DrawSurface(GLuint textureID)
{
	if (m_aaTarget) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_aaTarget);	// set target if needed
	}

	m_drawShader.EnableShader();

	glEnable			(GL_BLEND);
	glBindVertexArray	(m_vao);
	glActiveTexture		(GL_TEXTURE0); // texture unit 0
	glBindTexture		(GL_TEXTURE_2D, textureID);
	glDrawArrays		(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray	(0);
	glDisable			(GL_BLEND);

	m_drawShader.DisableShader();

	if (m_aaTarget) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);			// restore target if needed
	}
}

void CRender2D::BeginFrame(void)
{
}

void CRender2D::PreRenderFrame(void)
{
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 512);	// skip the non viewable data

	for (int i = 0; i < 2; i++) {
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 496, 384, GL_RGBA, GL_UNSIGNED_BYTE, m_drawBuffers[i]->data);
	}

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void CRender2D::RenderFrameBottom(void)
{
	Setup2D(true);
	DrawSurface(m_textureIDs[0]);
}

void CRender2D::RenderFrameTop(void)
{
	Setup2D(false);
	DrawSurface(m_textureIDs[1]);
}

void CRender2D::EndFrame(void)
{
}


/******************************************************************************
 Emulation Callbacks
******************************************************************************/

// Deprecated
void CRender2D::WriteVRAM(unsigned addr, uint32_t data)
{
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CRender2D::AttachRegisters(const uint32_t* regPtr)
{
}

void CRender2D::AttachPalette(const uint32_t* palPtr[2])
{
}

void CRender2D::AttachVRAM(const uint8_t* vramPtr)
{
}

void CRender2D::AttachDrawBuffers(std::shared_ptr<TileGenBuffer> bottom, std::shared_ptr<TileGenBuffer> top)
{
	m_drawBuffers[0] = bottom;
	m_drawBuffers[1] = top;
}

Result CRender2D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget, UpscaleMode upscaleMode)
{
	// Resolution
	m_xPixels		= xRes;
	m_yPixels		= yRes;
	m_xOffset		= xOffset;
	m_yOffset		= yOffset;
	m_totalXPixels	= totalXRes;
	m_totalYPixels	= totalYRes;
	m_correction	= (UINT32)(((yRes / 384.) * 2.) + 0.5);		// for some reason the 2d layer is 2 pixels off the 3D
	m_aaTarget		= aaTarget;
	m_upscaleMode	= (m_xPixels == 496 && m_yPixels == 384) ? UpscaleMode::Nearest : upscaleMode;

	std::string um = "#define UPSCALEMODE " + std::to_string((int)m_upscaleMode) + '\n';

	m_drawShader.LoadShaders(s_vertexShader, (std::string(s_fragmentShaderHeader) + um + s_fragmentShader).c_str());
	m_drawShader.GetUniformLocationMap("tex1");
	// init uniform memory
	m_drawShader.EnableShader();
	glUniform1i(m_drawShader.uniformLocMap["tex1"], 0);	// texture bound to texture unit 0
	m_drawShader.DisableShader();

	// allocate storage
	for (auto& t : m_textureIDs) {
		glBindTexture(GL_TEXTURE_2D, t);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_upscaleMode == UpscaleMode::Nearest ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_upscaleMode == UpscaleMode::Nearest ? GL_NEAREST : GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 496, 384, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}

	return Result::OKAY;
}

CRender2D::CRender2D(const Util::Config::Node& config)
	: m_config(config),
	m_vao(0)
{
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	// no states needed since we do it in the shader
	glBindVertexArray(0);

	// create textures
	glGenTextures(2, m_textureIDs);
}

CRender2D::~CRender2D(void)
{
	if (m_vao) {
		glDeleteVertexArrays(1, &m_vao);
		m_vao = 0;
	}

	for (auto& t : m_textureIDs) {
		glDeleteTextures(1, &t);
		t = 0;
	}
}
