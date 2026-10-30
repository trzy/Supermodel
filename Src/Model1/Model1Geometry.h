// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/mame/sega/model1_v.cpp).

/*
 * Model1Geometry.h
 *
 * The Sega Model 1 video board: the geometrizer (display list walker,
 * transformation, lighting, clipping, projection and z-sorting), as simulated
 * by MAME.
 *
 * MAME rasterizes the sorted quads into a 496x384 bitmap. Here the output is
 * the list of screen-space quads instead, kept in drawing order, with their
 * floating point coordinates so the renderer can draw them at any resolution.
 * The hardware draws them with the painter's algorithm (no depth buffer), and
 * so does the renderer. MAME's software rasterizer is kept as well (Rasterize())
 * to produce the exact MAME image for comparison.
 */

#ifndef INCLUDED_MODEL1GEOMETRY_H
#define INCLUDED_MODEL1GEOMETRY_H

#include "Types.h"
#include <cstdint>
#include <vector>

class CBlockFile;

namespace Model1
{
  // Native screen size
  constexpr int SCREEN_WIDTH  = 496;
  constexpr int SCREEN_HEIGHT = 384;

  // Color flag: draw only every other pixel (checkerboard), the hardware's
  // translucency
  constexpr uint32_t MOIRE = 0x01000000;

  // A quad in native screen coordinates (x right, y down, in pixels)
  struct ScreenQuad
  {
    float     x[4], y[4];     // projected position
    int32_t   sx[4], sy[4];   // the same, truncated to integers as MAME does (used by Rasterize())
    uint32_t  color;          // 0x00RRGGBB, plus MOIRE
    float     z;              // sort key
    uint32_t  flags, adr;     // polygon flags word and address (debugging)
    bool      line;           // wireframe: a degenerate quad with two distinct screen points, drawn as a line from vertex lineA to lineB
    uint8_t   lineA, lineB;
  };

  // Quads sharing one viewport, in drawing order (farthest first)
  struct Batch
  {
    int x1, y1, x2, y2;       // viewport clip rectangle, inclusive, native pixels
    uint32_t first, count;    // range in Frame::quads
  };

  struct Frame
  {
    std::vector<ScreenQuad> quads;
    std::vector<Batch>      below;  // objects (command 0x01) and direct polygons, drawn below the HUD tilemaps
    std::vector<Batch>      above;  // objects (command 0x41), drawn above the HUD tilemaps

    void Clear()
    {
      quads.clear();
      below.clear();
      above.clear();
    }
  };

  class CGeometry
  {
  public:
    enum RenderPass { RENDER_BELOW_HUD, RENDER_ABOVE_HUD };

    // Memory shared with the board. The display lists are 0x8000 16-bit words
    // each, the polygon ROM is 32-bit words, palette RAM is 0x2000 words and
    // the color translation RAM is 0x6000 words.
    void AttachMemory(const uint16_t *displayList0, const uint16_t *displayList1, const uint32_t *polyROM, uint32_t polyROMWords, const uint16_t *paletteRAM, const uint16_t *colorXlat);

    // List control register (0x680000-0x680003)
    uint16_t ReadListCtl(unsigned offset);
    void WriteListCtl(unsigned offset, uint16_t data, uint16_t mask);

    // Walks the current display list and produces the quads (MAME's
    // tgp_render() for both passes). frameNumber selects the blink phase.
    void Render(Frame *frame, uint64_t frameNumber);

    // Start of vertical blanking (MAME's screen_vblank_model1(): tgp_scan()
    // and end_frame())
    void VBlank(uint64_t frameNumber);

    // MAME's software rasterizer: draws the batches into a 496x384 bitmap of
    // 0x00RRGGBB pixels
    static void Rasterize(const Frame &frame, const std::vector<Batch> &batches, uint32_t *bitmap);

    // Model 1 palette RAM word to 0x00RRGGBB
    static uint32_t PaletteToRGB(uint16_t v);

    void Reset();
    void SaveState(CBlockFile *SaveState);
    void LoadState(CBlockFile *SaveState);

    CGeometry();

  private:
    struct spoint_t
    {
      int32_t x = 0, y = 0;
    };

    struct point_t
    {
      float x = 0, y = 0, z = 0;
      float xx = 0, yy = 0;
      float fx = 0, fy = 0;   // screen position before truncation (Supermodel addition)
      spoint_t s;
    };

    struct quad_t
    {
      quad_t() { }
      quad_t(int ccol, float cz, point_t *p0, point_t *p1, point_t *p2, point_t *p3)
        : p{ p0, p1, p2, p3 }, z(cz), col(ccol)
      {
      }

      int compare(const quad_t *other) const;

      point_t *p[4] = { nullptr, nullptr, nullptr, nullptr };
      float z = 0;
      int col = 0;
      uint32_t flags = 0;   // polygon flags word (Supermodel addition, for debugging)
      uint32_t adr = 0;     // polygon address (Supermodel addition, for debugging)
    };

    struct vec3
    {
      float x = 0, y = 0, z = 0;
    };

    struct lightparam_t
    {
      float a = 0;
      float d = 0;
      float s = 0;
      int p = 0;
    };

    class view_t
    {
    public:
      void init_translation_matrix();

      void set_viewport(float xcenter, float ycenter, float xl, float xr, float yb, float yt);
      void set_lightparam(int index, float diffuse, float ambient, float specular, int power);
      void set_zoom(float x, float y);
      void set_light_direction(float x, float y, float z);
      void set_translation_matrix(float *mat);
      void set_view_translation(float x, float y);

      void project_point(point_t *p) const;
      void project_point_direct(point_t *p) const;

      void transform_vector(vec3 &p) const;
      void transform_point(point_t *p) const;

      void recompute_frustum();

      int xc = 0, yc = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
      float zoomx = 0, zoomy = 0, viewx = 0, viewy = 0;
      float a_bottom = 0, a_top = 0, a_left = 0, a_right = 0;
      float vxx = 0, vyy = 0, vzz = 0, ayy = 0, ayyc = 0, ayys = 0;
      float translation[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
      vec3 light;
      lightparam_t lightparams[256];
      bool spec_enable = false;
    };

    typedef bool (*isclipped_fn)(view_t *, point_t *);
    typedef void (*clip_fn)(view_t *, point_t *, point_t *, point_t *);
    struct clipper_t
    {
      isclipped_fn m_isclipped = nullptr;
      clip_fn m_clip = nullptr;
    };

    // Display list access
    uint32_t readi(int adr) const;
    int16_t readi16(int adr) const;
    float readf(int adr) const;
    float polyf(bool ram, uint32_t adr) const;
    uint32_t polyi(bool ram, uint32_t adr) const;
    uint16_t tgp_ram_r(uint32_t adr) const;
    void tgp_ram_w(uint32_t adr, uint16_t data);
    void poly_ram_w(uint32_t adr, uint32_t data);

    float view_determinant(const point_t *p1, const point_t *p2, const point_t *p3) const;

    static bool fclip_isc_bottom(view_t *, point_t *);
    static bool fclip_isc_top(view_t *, point_t *);
    static bool fclip_isc_left(view_t *, point_t *);
    static bool fclip_isc_right(view_t *, point_t *);
    static void fclip_clip_bottom(view_t *, point_t *, point_t *, point_t *);
    static void fclip_clip_top(view_t *, point_t *, point_t *, point_t *);
    static void fclip_clip_left(view_t *, point_t *, point_t *, point_t *);
    static void fclip_clip_right(view_t *, point_t *, point_t *, point_t *);

    void tgp_render(Frame *frame, RenderPass pass);
    void tgp_scan();

    void sort_quads();
    void emit_quads(Frame *frame, RenderPass pass);
    void fclip_push_quad_next(int level, quad_t &q, point_t *p1, point_t *p2, point_t *p3, point_t *p4);
    void fclip_push_quad(int level, quad_t &q);
    point_t *alloc_point();
    void push_quad(const quad_t &q);

    static float min4f(float a, float b, float c, float d);
    static float max4f(float a, float b, float c, float d);
    float compute_specular(const vec3 &normal, const vec3 &light, float diffuse, int lmode) const;

    int push_direct(int list_offset);
    int skip_direct(int list_offset) const;
    void push_object(uint32_t tex_adr, uint32_t poly_adr, uint32_t size, float &old_z);
    void draw_objects(Frame *frame, RenderPass pass);

    void set_current_render_list();
    void end_frame(uint64_t frameNumber);

    uint32_t color_from_tex(uint16_t tex_data, int lumval) const;

    // Shared memory
    const uint16_t  *m_display_list0 = nullptr;
    const uint16_t  *m_display_list1 = nullptr;
    const uint32_t  *m_poly_rom = nullptr;
    uint32_t        m_poly_rom_words = 0;
    const uint16_t  *m_paletteram16 = nullptr;
    const uint16_t  *m_color_xlat = nullptr;

    // Video board state
    view_t                m_view;
    std::vector<point_t>  m_pointdb;
    size_t                m_numPoints = 0;
    std::vector<quad_t>   m_quaddb;
    size_t                m_numQuads = 0;
    std::vector<quad_t *> m_quadind;
    bool                  m_overflow = false;

    uint16_t  m_listctl[2] = { 0, 0 };
    bool      m_render_done = false;
    const uint16_t *m_display_list_current = nullptr;
    uint64_t  m_frameNumber = 0;

    std::vector<uint16_t> m_tgp_ram;
    std::vector<uint32_t> m_poly_ram;

    clipper_t m_clipfn[4];
  };
}

#endif  // INCLUDED_MODEL1GEOMETRY_H
