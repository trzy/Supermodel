/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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
 * Model2Video.h
 *
 * The Model 2 3D pipeline, ported from MAME (src/mame/sega/model2_v.cpp,
 * model2rd.ipp; R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew
 * Daniels):
 *
 *   geometrizer    simulated (HLE): MAME's code, written from the disassembly
 *                  of a 2B game's geometrizer program; the original and 2A
 *                  boards have it in an undumped internal ROM
 *   z-sort, clip   MAME's understanding of the hardware (4.12 z buckets)
 *   rasterizer     MAME's software rasterizer: textured (mipmapped,
 *                  bilinear, microtextures) and flat polygons, drawn front to
 *                  back with a fill buffer, into a 512x512 image
 *
 * The code is MAME's with Supermodel glue (the board's memory through
 * pointers, a scanline polygon filler in place of MAME's poly_manager).
 *
 * Based on original work by R. Belmont, Olivier Galibert, ElSemi, Angelo
 * Salese and Matthew Daniels in MAME. Portions Copyright (c) R. Belmont,
 * Olivier Galibert, ElSemi, Angelo Salese and Matthew Daniels, used under the
 * BSD 3-Clause License: see Docs/README.txt.
 */

#ifndef INCLUDED_MODEL2VIDEO_H
#define INCLUDED_MODEL2VIDEO_H

#include "Types.h"
#include <cstdint>
#include <memory>
#include <functional>
#include <vector>

class CBlockFile;

namespace Model2
{
  // A vertex of the 3D for the GPU (Model1Render's Model 2 pass): triangles in
  // drawing order (back to front), with what MAME's rasterizer works from
  struct GPUVertex
  {
    float     x, y, w;          // homogeneous native screen coordinates: x / w, y / w is the
                                // position (496x384, y down), w = z (the GPU clips w < 0)
    float     u, v;             // texture coordinates (interpolated perspective correct)
    float     clip[4];          // the polygon's viewport: x1, y1, x2, y2 (native, inclusive)
    uint32_t  header01;         // texture header words 0 (low) and 1
    uint32_t  header23;         // texture header words 2 and 3
    uint32_t  lumaLod;          // luma (bits 0-7), texture LOD (signed, bits 16-31)
  };

  class CVideo
  {
  public:
    // The board's memory (must stay valid)
    struct Memory
    {
      const uint32_t *bufferRAM = nullptr;   // 0x8000 words
      const uint16_t *paletteRAM = nullptr;  // 0x2000 words
      const uint16_t *colorXlat = nullptr;   // 0x6000 words
      const uint8_t  *lumaRAM = nullptr;     // 0x8000 bytes
      const uint32_t *textureRAM0 = nullptr; // 0x80000 words
      const uint32_t *textureRAM1 = nullptr;
      const uint8_t  *gamma = nullptr;       // 256 entries
      const uint32_t *polygonROM = nullptr;  // power of 2 words
      uint32_t        polygonROMWords = 0;
      const uint16_t *textureROM = nullptr;  // power of 2 words
      uint32_t        textureROMWords = 0;
    };

    void Init(const Memory &memory);
    void Reset();

    // At vblank (when the board starts a frame): walks the display list in
    // buffer RAM from readAddress and sorts the frame's polygons
    void GeoParse(uint32_t readAddress);

    // Model 2B with the geometrizer's program run (a SHARC): its output goes
    // to the z-sort/rasterizer input instead (MAME's model2_3d_push, which its
    // simulation also feeds), a frame at a time
    void RasterFrameStart();
    void RasterPush(uint32_t data);

    // Debugging aid: called with every word the rasterizer input gets
    std::function<void(uint32_t)> m_pushTrace;

    // Draws the 3D over dest (496x384, 0xAARRGGBB): only pixels a polygon
    // covered are written. Draws the polygons of the last GeoParse(); later
    // calls redraw the same image (MAME's m_render_done).
    void Render(uint32_t *dest, int width, int height);

    // The same frame for the GPU: the polygons as triangles, back to front
    // (the reverse of MAME's front-to-back order with a fill buffer, so the
    // result is the same without one)
    const std::vector<GPUVertex> &BuildGPU();

    // BuildGPU()'s output: quads, 4 vertices each (GL_LINES_ADJACENCY; a
    // triangle repeats its last vertex), interpolated across all 4 corners as
    // the hardware does; or triangles, 3 vertices each (a fan per polygon)
    void SetGPUQuads(bool quads) { m_gpuQuads = quads; m_render_done = false; }

    // Last Bronx (MAME's m_geo_mesh): the simulated geometrizer's opcodes 1-3
    // are mesh commands
    void SetGeoMesh(bool on) { m_geoMeshGame = on; m_geo_mesh = on; }

    // Top Skater: a second matrix (commands 1B, 1C) and compact object
    // records (command 11)
    void SetGeoTopSkater(bool on) { m_geoTopSkater = on; }
    bool GPUQuads() const { return m_gpuQuads; }

    // Wide screen: viewports at the screen's left and right edges reach this
    // many native pixels further out, for clipping and for BuildGPU() (the
    // renderer draws there). 0 = the 4:3 picture. Not for Render().
    void SetWideExtension(int nativePixels);

    // Tilemap chip sync registers (MAME horizontal_sync_w / vertical_sync_w)
    void HorizontalSync(uint16_t data);
    void VerticalSync(uint16_t data);
    void ZClipWrite(uint32_t data);
    uint32_t PolygonCount() const;

    void SaveState(CBlockFile *SaveState);
    void LoadState(CBlockFile *SaveState);

    CVideo();
    ~CVideo();

    // MAME's types (public for the file's static helpers)
    struct poly_vertex
    {
      float x = 0, y = 0;
      float p[3] = { 0, 0, 0 };
      poly_vertex() = default;
      poly_vertex(float x_, float y_) : x(x_), y(y_) { }
    };
    struct plane;
    struct texture_parameter;
    struct polygon;
    struct quad_m2;
    struct raster_state;
    struct geo_state;
    struct m2_poly_extra_data;
    struct extent_t;
    struct rectangle;
    // Supermodel: what the geometrizer reads its input through (MAME: a raw
    // pointer): an index into a memory, wrapping within it like the hardware's
    // addresses, so a bad count can't read past the end
    struct WPtr
    {
      const uint32_t *base = nullptr;
      uint32_t mask = 0;
      uint32_t index = 0;
      WPtr() = default;
      WPtr(const uint32_t *b, uint32_t m, uint32_t i) : base(b), mask(m), index(i) { }
      uint32_t operator*() const { return base[index & mask]; }
      uint32_t operator[](uint32_t i) const { return base[(index + i) & mask]; }
      WPtr &operator++() { index++; return *this; }
      WPtr operator++(int) { WPtr t = *this; index++; return t; }
      WPtr &operator+=(uint32_t n) { index += n; return *this; }
    };

  private:
    typedef void (CVideo::*render_delegate)(int32_t scanline, const extent_t &extent, const m2_poly_extra_data &object, int threadid);

    // MAME's model2_state / model2_renderer members used by the code
    std::unique_ptr<raster_state> m_raster;
    std::unique_ptr<geo_state> m_geo;
    const uint32_t *m_bufferram = nullptr;
    const uint16_t *m_palram = nullptr;
    const uint16_t *m_colorxlat = nullptr;
    const uint8_t  *m_lumaram = nullptr;
    const uint32_t *m_textureram0 = nullptr;
    const uint32_t *m_textureram1 = nullptr;
    const uint8_t  *m_gamma_table = nullptr;
    uint32_t m_geo_read_start_address = 0;
    bool     m_geo_mesh = false;
    bool     m_geoTopSkater = false;
    bool     m_render_done = false;
    int16_t  m_crtc_xoffset = 0, m_crtc_yoffset = 0;
    int16_t  m_xoffs = 90, m_yoffs = -8;      // the renderer's copies
    int      m_wideExtension = 0;             // see SetWideExtension()
    bool     m_geoMeshGame = false;           // see SetGeoMesh()
    void     ComputeClipPlanes();
    int      WideLeft(int x1) const;
    int      WideRight(int x2) const;
    std::vector<uint32_t> m_destmap;          // 512x512
    std::vector<uint8_t>  m_fillmap;          // 512x512
    std::vector<GPUVertex> m_gpuVertices;
    bool m_gpuQuads = true;     // BuildGPU()'s output
    render_delegate m_render_callbacks[4];

    void RenderPolygon(const rectangle &clip, render_delegate callback, const poly_vertex *v, int count, const m2_poly_extra_data &extra);

    // MAME
    void render_frame_start();
    void geo_parse();
    void render_polygons();
    WPtr geo_process_command(geo_state &geo, uint32_t opcode, WPtr input, bool *end_code);
    WPtr geo_nop(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_object_data(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_direct_data(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_window_data(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_texture_data(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_polygon_data(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_texture_parameters(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_mode(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_zsort_mode(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_focal_distance(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_light_source(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_matrix_write(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_mesh_mode(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_mesh_draw(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_mesh_flush(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_translate_write(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_data_mem_push(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_test(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_end(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_dummy(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_log_data(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_lod(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_code_upload(geo_state &geo, uint32_t opcode, WPtr input);
    WPtr geo_code_jump(geo_state &geo, uint32_t opcode, WPtr input);
    void geo_parse_np_ns(geo_state &geo, WPtr input, uint32_t count);
    void geo_parse_np_s(geo_state &geo, WPtr input, uint32_t count);
    void geo_parse_nn_ns(geo_state &geo, WPtr input, uint32_t count, bool compact);
    void geo_parse_nn_s(geo_state &geo, WPtr input, uint32_t count, bool compact);
    WPtr geo_matrix2_copy(geo_state &geo, uint32_t opcode, WPtr input);
    void model2_3d_push(raster_state *raster, uint32_t input);
    template <unsigned NumVerts> void model2_3d_process_polygon(raster_state *raster, uint32_t attr);
    void model2_3d_project(polygon *poly);
    void model2_3d_render(polygon *poly, const rectangle &cliprect);
    uint16_t float_to_zval(float floatval, int32_t z_adjust);
    bool check_culling(raster_state *raster, uint32_t attr, float min_z, float max_z);
    template <bool Translucent> void draw_scanline_solid(int32_t scanline, const extent_t &extent, const m2_poly_extra_data &object, int threadid);
    template <bool Translucent> void draw_scanline_tex(int32_t scanline, const extent_t &extent, const m2_poly_extra_data &object, int threadid);
    template <bool Translucent> uint32_t fetch_bilinear_texel(const m2_poly_extra_data &object, int32_t miplevel, int32_t fu, int32_t fv);
    static uint16_t get_texel(uint32_t base_x, uint32_t base_y, int x, int y, const uint32_t *sheet);
  };
}

#endif  // INCLUDED_MODEL2VIDEO_H
