// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/mame/sega/model1_v.cpp).

/*
 * Model1Geometry.cpp
 *
 * Model 1 geometrizer TGP simulation (see Model1Geometry.h). The display list
 * walk, transformation, lighting, clipping, projection and sorting follow
 * MAME's model1_v.cpp. Differences, all at the output end:
 *
 *  - draw_objects() appends the sorted quads to a Frame (with the projected
 *    floating point coordinates) instead of filling a bitmap. The software
 *    fill is kept in Rasterize() and gives MAME's image from the same quads.
 *  - Point and quad storage is bounded (MAME allocates 2M points and 1M quads
 *    up front); on overflow the rest of the list is dropped for that frame.
 *  - Reads of the display lists, polygon memory and TGP RAM are kept inside
 *    their arrays (MAME indexes them unchecked).
 *  - The debug camera (vxx/vyy/vzz/ayy, keys in MAME) is fixed at zero.
 */

#include "Model1Geometry.h"
#include "BlockFile.h"
#include "OSD/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace Model1
{
  enum { FRAC_SHIFT = 16 };

  static constexpr size_t MAX_POINTS = 1 << 19;
  static constexpr size_t MAX_QUADS = 1 << 18;
  static constexpr uint32_t TGP_RAM_WORDS = 0x100000 - 0x40000;
  static constexpr uint32_t POLY_RAM_WORDS = 0x400000;
  static constexpr uint32_t POLY_RAM_SAVED_WORDS = 0x40000;  // as MAME's save state

  static inline float u2f(uint32_t v)
  {
    float f;
    memcpy(&f, &v, sizeof(f));
    return f;
  }

  static inline int pal5bit(int bits)
  {
    bits &= 0x1f;
    return (bits << 3) | (bits >> 2);
  }

  static inline float dot(const float *a, float bx, float by, float bz)
  {
    return a[0] * bx + a[1] * by + a[2] * bz;
  }


  /******************************************************************************
   Memory access
  ******************************************************************************/

  uint32_t CGeometry::readi(int adr) const
  {
    return m_display_list_current[(adr + 0) & 0x7fff] | (m_display_list_current[(adr + 1) & 0x7fff] << 16);
  }

  int16_t CGeometry::readi16(int adr) const
  {
    return m_display_list_current[(adr + 0) & 0x7fff];
  }

  float CGeometry::readf(int adr) const
  {
    return u2f(readi(adr));
  }

  uint32_t CGeometry::polyi(bool ram, uint32_t adr) const
  {
    if (ram)
      return m_poly_ram[adr & (POLY_RAM_WORDS - 1)];
    return m_poly_rom_words ? m_poly_rom[adr % m_poly_rom_words] : 0;
  }

  float CGeometry::polyf(bool ram, uint32_t adr) const
  {
    return u2f(polyi(ram, adr));
  }

  uint16_t CGeometry::tgp_ram_r(uint32_t adr) const
  {
    uint32_t i = adr - 0x40000;
    return i < TGP_RAM_WORDS ? m_tgp_ram[i] : 0;
  }

  void CGeometry::tgp_ram_w(uint32_t adr, uint16_t data)
  {
    uint32_t i = adr - 0x40000;
    if (i < TGP_RAM_WORDS)
      m_tgp_ram[i] = data;
  }

  void CGeometry::poly_ram_w(uint32_t adr, uint32_t data)
  {
    uint32_t i = adr - 0x800000;
    if (i < POLY_RAM_WORDS)
      m_poly_ram[i] = data;
  }

  uint32_t CGeometry::PaletteToRGB(uint16_t v)
  {
    int r = pal5bit((v >> 0) & 0x1f);
    int g = pal5bit((v >> 5) & 0x1f);
    int b = pal5bit((v >> 10) & 0x1f);
    // Bit 15 is an intensity/shade bit, not unused as the plain xBGR_555 decode
    // assumes.  Almost every entry has it set (full brightness); a handful clear
    // it to render dimmed (e.g. Star Wars Arcade's grey menu backdrop pen 0x7777).
    // Halve the channels when it is clear so those pens stop reading too bright.
    if (!(v & 0x8000))
    {
      r >>= 1;
      g >>= 1;
      b >>= 1;
    }
    return (r << 16) | (g << 8) | b;
  }


  /******************************************************************************
   View
  ******************************************************************************/

  void CGeometry::view_t::transform_point(point_t *p) const
  {
    point_t q = *p;
    float xx = translation[0] * q.x + translation[3] * q.y + translation[6] * q.z + translation[ 9] + vxx;
        p->y = translation[1] * q.x + translation[4] * q.y + translation[7] * q.z + translation[10] + vyy;
    float zz = translation[2] * q.x + translation[5] * q.y + translation[8] * q.z + translation[11] + vzz;
    p->x = ayyc * xx - ayys * zz;
    p->z = ayys * xx + ayyc * zz;
  }

  void CGeometry::view_t::transform_vector(vec3 &p) const
  {
    vec3 q(p);
    p.x = q.x * translation[0] + q.y * translation[3] + q.z * translation[6];
    p.y = q.x * translation[1] + q.y * translation[4] + q.z * translation[7];
    p.z = q.x * translation[2] + q.y * translation[5] + q.z * translation[8];
  }

  float CGeometry::view_determinant(const point_t *p1, const point_t *p2, const point_t *p3) const
  {
    float x1 = p2->x - p1->x;
    float y1 = p2->y - p1->y;
    float z1 = p2->z - p1->z;
    float x2 = p3->x - p1->x;
    float y2 = p3->y - p1->y;
    float z2 = p3->z - p1->z;

    return p1->x * (y1 * z2 - y2 * z1) + p1->y * (z1 * x2 - z2 * x1) + p1->z * (x1 * y2 - x2 * y1);
  }

  void CGeometry::view_t::project_point(point_t *p) const
  {
    p->xx = p->x / p->z;
    p->yy = p->y / p->z;
    p->fx = xc + (p->xx * zoomx + viewx);
    p->fy = yc - (p->yy * zoomy + viewy);
    p->s.x = (int32_t) p->fx;
    p->s.y = (int32_t) p->fy;
  }

  void CGeometry::view_t::project_point_direct(point_t *p) const
  {
    p->xx = p->x /*/ p->z*/;
    p->yy = p->y /*/ p->z*/;
    p->fx = xc + p->xx;
    p->fy = yc - p->yy;
    p->s.x = (int32_t) p->fx;
    p->s.y = (int32_t) p->fy;
  }

  // xe = xc + (x/z * zm + tx)
  // xe < x1 => xc + (x/z * zm + tx) < x1
  //         => x/z < (x1-xc-tx)/zm
  //         => x < z*(x1-xc-tx)/zm

  // ye = yc - (y/z * zm + ty)
  // ye < y1 => yc - (y/z * zm + ty) < y1
  //         => -y/z < (y1-yc+ty)/zm
  //         => y > -z*(y1-yc+ty)/zm

  // xf = zf*a
  // xx = x1*t+x2(1-t); zz = z1*t+z2(1-t)
  // => x1*t+x2(1-t) = z1*t*a+z2*(1-t)*a
  // => t*(x1-x2+a*(z2-z1) = -x2+a*z2
  // => t = (z2*a-x2)/((z2-z1)*a-(x2-x1))

  void CGeometry::view_t::recompute_frustum()
  {
    a_left   = ( x1 - xc - viewx) / zoomx;
    a_right  = ( x2 - xc - viewx) / zoomx;
    a_bottom = (-y1 + yc - viewy) / zoomy;
    a_top    = (-y2 + yc - viewy) / zoomy;
  }

  void CGeometry::view_t::init_translation_matrix()
  {
    memset(translation, 0, sizeof(translation));
    translation[0] = 1.0;
    translation[4] = 1.0;
    translation[8] = 1.0;
  }

  void CGeometry::view_t::set_viewport(float xcenter, float ycenter, float xl, float xr, float yb, float yt)
  {
    xc = (int) xcenter;
    yc = (int) ycenter;
    x1 = (int) xl;
    x2 = (int) xr;
    y1 = (int) yb;
    y2 = (int) yt;

    recompute_frustum();
  }

  void CGeometry::view_t::set_lightparam(int index, float diffuse, float ambient, float specular, int power)
  {
    lightparams[index & 0xff].d = diffuse;
    lightparams[index & 0xff].a = ambient;
    lightparams[index & 0xff].s = specular;
    lightparams[index & 0xff].p = power;
  }

  void CGeometry::view_t::set_zoom(float x, float y)
  {
    zoomx = x;
    zoomy = y;

    recompute_frustum();
  }

  // The geometrizer DSPs use the light vector as sent, without normalizing it
  // (VF sends one of length ~0.91, so it is lit ~9% darker than a unit vector)
  void CGeometry::view_t::set_light_direction(float x, float y, float z)
  {
    light.x = x;
    light.y = y;
    light.z = z;
  }

  void CGeometry::view_t::set_translation_matrix(float *mat)
  {
    for (int i = 0; i < 12; i++)
    {
      translation[i] = mat[i];
    }
  }

  void CGeometry::view_t::set_view_translation(float x, float y)
  {
    viewx = x;
    viewy = y;

    recompute_frustum();
  }


  /******************************************************************************
   Clipping
  ******************************************************************************/

  bool CGeometry::fclip_isc_bottom(view_t *view, point_t *p)
  {
    return p->y > (p->z * view->a_bottom);
  }

  void CGeometry::fclip_clip_bottom(view_t *view, point_t *pt, point_t *p1, point_t *p2)
  {
    float t = (p2->z * view->a_bottom-p2->y) / ((p2->z - p1->z) * view->a_bottom - (p2->y - p1->y));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
  }

  bool CGeometry::fclip_isc_top(view_t *view, point_t *p)
  {
    return p->y < (p->z * view->a_top);
  }

  void CGeometry::fclip_clip_top(view_t *view, point_t *pt, point_t *p1, point_t *p2)
  {
    float t = (p2->z * view->a_top - p2->y) / ((p2->z - p1->z) * view->a_top - (p2->y - p1->y));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
  }

  bool CGeometry::fclip_isc_left(view_t *view, point_t *p)
  {
    return p->x < (p->z * view->a_left);
  }

  void CGeometry::fclip_clip_left(view_t *view, point_t *pt, point_t *p1, point_t *p2)
  {
    float t = (p2->z * view->a_left - p2->x) / ((p2->z - p1->z) * view->a_left - (p2->x - p1->x));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
  }

  bool CGeometry::fclip_isc_right(view_t *view, point_t *p)
  {
    return p->x > (p->z * view->a_right);
  }

  void CGeometry::fclip_clip_right(view_t *view, point_t *pt, point_t *p1, point_t *p2)
  {
    float t = (p2->z * view->a_right - p2->x) / ((p2->z - p1->z) * view->a_right - (p2->x - p1->x));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
  }

  // A scratch point for when storage is exhausted, so callers never get null
  CGeometry::point_t *CGeometry::alloc_point()
  {
    if (m_numPoints >= MAX_POINTS - 1)
    {
      m_overflow = true;
      return &m_pointdb[MAX_POINTS - 1];
    }
    return &m_pointdb[m_numPoints++];
  }

  void CGeometry::push_quad(const quad_t &q)
  {
    if (m_numQuads >= MAX_QUADS)
    {
      m_overflow = true;
      return;
    }
    m_quaddb[m_numQuads++] = q;
  }

  void CGeometry::fclip_push_quad_next(int level, quad_t &q, point_t *p1, point_t *p2, point_t *p3, point_t *p4)
  {
    quad_t cquad(q.col, q.z, p1, p2, p3, p4);
    cquad.flags = q.flags;
    cquad.adr = q.adr;
    fclip_push_quad(level+1, cquad);
  }

  void CGeometry::fclip_push_quad(int level, quad_t &q)
  {
    view_t *view = &m_view;

    if (level == 4)
    {
      push_quad(q);
      return;
    }

    bool is_out[4];
    for (int i = 0; i < 4; i++)
    {
      is_out[i] = m_clipfn[level].m_isclipped(view, q.p[i]);
    }

    // No clipping
    if(!is_out[0] && !is_out[1] && !is_out[2] && !is_out[3])
    {
      fclip_push_quad(level+1, q);
      return;
    }

    // Full clipping
    if(is_out[0] && is_out[1] && is_out[2] && is_out[3])
      return;

    // Find n so that point n is clipped and n-1 isn't
    int i;
    for (i = 0; i < 4; i++)
      if(is_out[i] && !is_out[(i-1)&3])
        break;

    point_t* pt[4];
    bool is_out2[4];
    for (int j = 0; j < 4; j++)
    {
      pt[j] = q.p[(i + j) & 3];
      is_out2[j] = is_out[(i + j) & 3];
    }

    // At this point, pt[0] is clipped out and pt[3] isn't.  Test for the 4 possible cases
    if (is_out2[1])
    {
      if (is_out2[2])
      {
        // pt 0,1,2 clipped out, one triangle left
        point_t* pi1 = alloc_point();
        m_clipfn[level].m_clip(view, pi1, pt[2], pt[3]);
        point_t* pi2 = alloc_point();
        m_clipfn[level].m_clip(view, pi2, pt[3], pt[0]);
        fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
      }
      else
      {
        // pt 0,1 clipped out, one quad left
        point_t* pi1 = alloc_point();
        m_clipfn[level].m_clip(view, pi1, pt[1], pt[2]);
        point_t* pi2 = alloc_point();
        m_clipfn[level].m_clip(view, pi2, pt[3], pt[0]);
        fclip_push_quad_next(level, q, pi1, pt[2], pt[3], pi2);
      }
    }
    else
    {
      if (is_out2[2])
      {
        // pt 0,2 clipped out, shouldn't happen, two triangles
        point_t* pi1 = alloc_point();
        m_clipfn[level].m_clip(view, pi1, pt[0], pt[1]);
        point_t* pi2 = alloc_point();
        m_clipfn[level].m_clip(view, pi2, pt[1], pt[2]);
        fclip_push_quad_next(level, q, pi1, pt[1], pi2, pi2);
        pi1 = alloc_point();
        m_clipfn[level].m_clip(view, pi1, pt[2], pt[3]);
        pi2 = alloc_point();
        m_clipfn[level].m_clip(view, pi2, pt[3], pt[0]);
        fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
      }
      else
      {
        // pt 0 clipped out, one decagon left, split in quad+tri
        point_t* pi1 = alloc_point();
        m_clipfn[level].m_clip(view, pi1, pt[0], pt[1]);
        point_t* pi2 = alloc_point();
        m_clipfn[level].m_clip(view, pi2, pt[3], pt[0]);
        fclip_push_quad_next(level, q, pi1, pt[1], pt[2], pt[3]);
        fclip_push_quad_next(level, q, pt[3], pi2, pi1, pi1);
      }
    }
  }


  /******************************************************************************
   Objects
  ******************************************************************************/

  float CGeometry::min4f(float a, float b, float c, float d)
  {
    float m = a;
    if(b<m)
      m = b;
    if(c<m)
      m = c;
    if(d<m)
      m = d;
    return m;
  }

  float CGeometry::max4f(float a, float b, float c, float d)
  {
    float m = a;
    if (b > m)
      m = b;
    if (c > m)
      m = c;
    if (d > m)
      m = d;
    return m;
  }

  // Light parameters are uploaded as 8-bit integers and kept as fractions of
  // 255; the geometrizers do their luminance sums in integer counts
  static int lightparam_count(float v)
  {
    return (int) std::lround(v * 255.0f);
  }

  // Specular luminance in counts (0-255 scale, not clamped)
  int CGeometry::compute_specular(const vec3 &normal, const vec3 &light, float diffuse, int lmode) const
  {
    if (!m_view.spec_enable)
      return 0;

    // the 315-5571 forwards only 3 bits of the power field
    const lightparam_t &lp = m_view.lightparams[lmode];
    int power = lp.p & 7;
    int scale = lightparam_count(lp.s);
    if (power == 0 || scale <= 0)
      return 0;

    // z component of the reflected light vector: R.z = 2*(N.L)*N.z - L.z,
    // raised to a power of two selected by the power field (315-5572: 1 = s,
    // 2-3 = s^2, 4-6 = s^4, 7 = s^8) and scaled by the specular parameter
    float s = (2.0f * diffuse * normal.z) - light.z;
    if (s <= 0.0f)
      return 0;
    if (power >= 2)
      s *= s;
    if (power >= 4)
      s *= s;
    if (power >= 7)
      s *= s;
    return (int) (s * (float) scale);
  }

  // Color of a polygon from its tile word and luminance (0-0x3f), through the
  // palette and the color translation RAM
  uint32_t CGeometry::color_from_tex(uint16_t tex_data, int lumval) const
  {
    int color = m_paletteram16[0x1000 | (tex_data & 0x3ff)];
    int r = (color >> 0x0) & 0x1f;
    int g = (color >> 0x5) & 0x1f;
    int b = (color >> 0xA) & 0x1f;
    // bits [11:10] of the tile word select a colour mode: mode 01 blinks by
    // rotating the BGR channels on alternate frames.
    if (((tex_data >> 10) & 3) == 1 && (m_frameNumber & 1))
    {
      // rotate b -> g -> r -> b
      int tmp = b;
      b = r;
      r = g;
      g = tmp;
    }
    r = (m_color_xlat[(r << 8) | lumval | 0x0] >> 3) & 0x1f;
    g = (m_color_xlat[(g << 8) | lumval | 0x2000] >> 3) & 0x1f;
    b = (m_color_xlat[(b << 8) | lumval | 0x4000] >> 3) & 0x1f;
    return (pal5bit(r) << 16) | (pal5bit(g) << 8) | (pal5bit(b) << 0);
  }

  // old_z is the flat-z "reuse" register (poly zmode 0 = keep previous poly's
  // sort z). It persists ACROSS objects within a display-list walk: NetMerc's
  // garage door wings (all-zmode-0 dynamic objects) must inherit the ~325k z of
  // the preceding object so the z~40k wall occludes them (real-hardware
  // capture); resetting per object made them sort at z=0 (nearest) and paint
  // through the wall.
  void CGeometry::push_object(uint32_t tex_adr, uint32_t poly_adr, uint32_t size, float &old_z)
  {
    // Protect against bad data when attacking a super destroyer
    if(tex_adr == 0xffffffff || size >= 0x1000000)
      return;

    bool ram = (poly_adr & 0x800000) != 0;
    poly_adr &= 0x7fffff;

    if (!size)
      size = 0xffffffff;

    point_t *old_p0 = alloc_point();
    point_t *old_p1 = alloc_point();

    old_p0->x = polyf(ram, poly_adr + 0);
    old_p0->y = polyf(ram, poly_adr + 1);
    old_p0->z = polyf(ram, poly_adr + 2);
    old_p1->x = polyf(ram, poly_adr + 3);
    old_p1->y = polyf(ram, poly_adr + 4);
    old_p1->z = polyf(ram, poly_adr + 5);
    m_view.transform_point(old_p0);
    m_view.transform_point(old_p1);

    if (old_p0->z > 0)
    {
      m_view.project_point(old_p0);
    }
    else
    {
      old_p0->s.x = old_p0->s.y = 0;
      old_p0->fx = old_p0->fy = 0;
    }

    if (old_p1->z > 0)
    {
      m_view.project_point(old_p1);
    }
    else
    {
      old_p1->s.x = old_p1->s.y = 0;
      old_p1->fx = old_p1->fy = 0;
    }

    poly_adr += 6;

    for (uint32_t i = 0; i < size; i++)
    {
      if (m_overflow)
        break;

      uint32_t flags = polyi(ram, poly_adr);

      int type = flags & 3;
      if (!type)
        break;

      if (flags & 0x00001000)
        tex_adr++;
      // Flags bit 22 selects the second light parameter bank. netmerc uploads
      // two banks (0x00-0x0c organic/terrain, 0x80-0x9e metal) and mixes
      // polygons from both within a single display list; the other games
      // never set the bit. In the Model 2 GEO attribute word this same bit
      // is the top bit of the texture parameter index.
      int lightmode = ((flags >> 17) & 15) | ((flags & 0x00400000) ? 0x80 : 0);

      point_t *p0 = alloc_point();
      point_t *p1 = alloc_point();

      vec3 vn;
      vn.x = polyf(ram, poly_adr + 1);
      vn.y = polyf(ram, poly_adr + 2);
      vn.z = polyf(ram, poly_adr + 3);
      p0->x = polyf(ram, poly_adr + 4);
      p0->y = polyf(ram, poly_adr + 5);
      p0->z = polyf(ram, poly_adr + 6);
      p1->x = polyf(ram, poly_adr + 7);
      p1->y = polyf(ram, poly_adr + 8);
      p1->z = polyf(ram, poly_adr + 9);

      if (type == 2)
      {
        p1->x = p0->x;
        p1->y = p0->y;
        p1->z = p0->z;
      }

      int link = (flags >> 8) & 3;

      m_view.transform_vector(vn);

      m_view.transform_point(p0);
      m_view.transform_point(p1);
      if (p0->z > 0)
      {
        m_view.project_point(p0);
      }
      else
      {
        p0->s.x = p0->s.y = 0;
        p0->fx = p0->fy = 0;
      }

      if (p1->z > 0)
      {
        m_view.project_point(p1);
      }
      else
      {
        p1->s.x = p1->s.y = 0;
        p1->fx = p1->fy = 0;
      }

      // Modes 2/3 (command 7 bit 1): the 315-5571 ignores the stored normal
      // (VF and Wing War store other data there) and computes the face normal
      // from the two previous vertices and a new one. Flags bits 23-30 select
      // it for quads: 1-2 = first new vertex, 3 = second, others give (1,0,0).
      // Triangles always use their new vertex. The 5572 normalizes it.
      if (m_view.computed_normals)
      {
        int edges = (flags >> 23) & 0xff;
        if (type != 2 && (edges == 0 || edges > 3))
        {
          vn.x = 1.0f;
          vn.y = 0.0f;
          vn.z = 0.0f;
        }
        else
        {
          const point_t *c = (type != 2 && edges == 3) ? p1 : p0;
          float ax = old_p0->x - c->x, ay = old_p0->y - c->y, az = old_p0->z - c->z;
          float bx = old_p1->x - c->x, by = old_p1->y - c->y, bz = old_p1->z - c->z;
          vn.x = ay * bz - az * by;
          vn.y = az * bx - ax * bz;
          vn.z = ax * by - ay * bx;
          float len = std::sqrt(vn.x * vn.x + vn.y * vn.y + vn.z * vn.z);
          if (len > 0.0f)
          {
            vn.x /= len;
            vn.y /= len;
            vn.z /= len;
          }
        }
      }

      bool front = link && view_determinant(old_p1, old_p0, p0) <= 0;
      if (link && ((flags & 0x00004000) || front))
      {
        quad_t cquad;
        cquad.flags = flags;
        cquad.adr = poly_adr | (ram ? 0x800000 : 0);

        // the normal is used as stored and transformed, not renormalized
        // (the 5571s only normalize the normals they compute themselves)

        cquad.p[0] = old_p1;
        cquad.p[1] = old_p0;
        cquad.p[2] = p0;
        cquad.p[3] = p1;

        switch ((flags >> 10) & 3)
        {
          case 0:
            cquad.z = old_z;
            break;
          case 1:
            cquad.z = old_z = min4f(old_p1->z, old_p0->z, p0->z, p1->z);
            break;
          case 2:
            cquad.z = old_z = max4f(old_p1->z, old_p0->z, p0->z, p1->z);
            break;
          case 3:
          default:
            cquad.z = 0.0;
            break;
        }

        {
          // Luminance as the 315-5572 geometrizers compute it: integer counts,
          // trunc(diffuse * N.L) clamped at 0, plus ambient and specular, only
          // the total clamped to 255. A double-sided polygon seen from behind
          // is lit with the dot product's sign flipped.
          const lightparam_t &lp = m_view.lightparams[lightmode];
          float dif = vn.x * m_view.light.x + vn.y * m_view.light.y + vn.z * m_view.light.z;
          int diffuse = (int) ((front ? dif : -dif) * (float) lightparam_count(lp.d));
          int lum = std::max(0, diffuse) + lightparam_count(lp.a) + compute_specular(vn, m_view.light, dif, lightmode);
          lum = std::min(lum, 255);
          uint16_t tex_data = tgp_ram_r(tex_adr);

          // The video board indexes the colour translation RAM with the top 6
          // bits: the games load it as a 64-step ramp per colour value (VF:
          // step 32 = the palette colour, steps 64-255 the unshaded colour)
          int lumval = lum >> 2;

          // bit 10 of the tile word marks a flat UI element (radar blips, HUD
          // text): draw unlit at full intensity instead of shading it
          if (tex_data & 0x400)
            lumval = 0x3f;

          cquad.col = (int) color_from_tex(tex_data, lumval);
        }

        if (flags & 0x00002000)
          cquad.col |= MOIRE;

        fclip_push_quad(0, cquad);
      }

      poly_adr += 10;
      switch (link) {
        case 0:
        case 2:
          old_p0 = p0;
          old_p1 = p1;
          break;
        case 1:
          old_p1 = p0;
          break;
        case 3:
          old_p0 = p1;
          break;
      }
    }
  }

  int CGeometry::push_direct(int list_offset)
  {
    uint32_t tex_adr = readi(list_offset + 2);

    point_t *old_p0 = alloc_point();
    point_t *old_p1 = alloc_point();

    old_p0->x = readf(list_offset + 2 + 4);
    old_p0->y = readf(list_offset + 2 + 6);
    old_p0->z = readf(list_offset + 2 + 8);
    old_p1->x = readf(list_offset + 2 + 12);
    old_p1->y = readf(list_offset + 2 + 14);
    old_p1->z = readf(list_offset + 2 + 16);

    // Direct (2D-projected) polys are screen-space: project_point_direct only
    // offsets x/y by the viewport centre and ignores z, so project every vertex
    // unconditionally.  Guarding on z > 0 and snapping behind-"camera" vertices
    // to (0,0) spawns huge spurious triangles radiating from the screen origin
    // (the bogus red cross over the SWA cockpit once direct polys draw on top).
    m_view.project_point_direct(old_p0);
    m_view.project_point_direct(old_p1);

    list_offset += 18;

    for (;;) {
      uint32_t flags = readi(list_offset + 2);

      int type = flags & 3;
      if (!type)
        break;

      if (flags & 0x00001000)
        tex_adr++;

      // list+2+2 is luminosity
      // list+2+4 is 0?
      // list+2+12 is z?

      point_t *p0 = alloc_point();
      point_t *p1 = alloc_point();

      uint32_t lum = readi(list_offset + 2 + 2);

      float z = 0;
      if (type == 2)
      {
        p0->x = readf(list_offset + 2 + 6);
        p0->y = readf(list_offset + 2 + 8);
        p0->z = readf(list_offset + 2 + 10);
        z = p0->z;
        *p1 = *p0;
        list_offset += 12;
      }
      else
      {
        p0->x = readf(list_offset + 2 + 6);
        p0->y = readf(list_offset + 2 + 8);
        p0->z = readf(list_offset + 2 + 10);
        p1->x = readf(list_offset + 2 + 14);
        p1->y = readf(list_offset + 2 + 16);
        p1->z = readf(list_offset + 2 + 18);
        z = readf(list_offset + 2 + 12);
        list_offset += 20;
      }

      int link = (flags >> 8) & 3;

      m_view.project_point_direct(p0);
      m_view.project_point_direct(p1);

      if (link)
      {
        quad_t cquad;
        cquad.flags = flags;
        cquad.adr = 0x80000000 | (uint32_t) list_offset;
        cquad.p[0] = old_p1;
        cquad.p[1] = old_p0;
        cquad.p[2] = p0;
        cquad.p[3] = p1;
        cquad.z = z;
        {
          int lumval = (int) (((float)(lum >> 24)) * 2.0f);
          lumval >>= 2; //there must be a luma translation table somewhere
          if (lumval > 0x3f) lumval = 0x3f;
          else if (lumval < 0) lumval = 0;
          // as MAME: direct polygons don't use the blink mode
          uint16_t tex_data = tgp_ram_r(tex_adr) & ~0x0c00;
          cquad.col = (int) color_from_tex(tex_data, lumval);
        }
        bool skip = false;
        if (flags & 0x00002000)
        {
          cquad.col |= MOIRE;
          // A moiré (stipple-translucent) direct poly whose flat colour resolves
          // to the transparent texture pen (index 0x1000 -> black) is a "fade
          // toward backdrop" overlay, not a black fill.  Painting the pen
          // literally gives a black/scene checkerboard (the Star Wars Arcade
          // gunner 2nd-viewport overlay); the arcade alternates backdrop/scene.
          // Paint the backdrop pen (palette[0]) so the stipple reads
          // backdrop/scene.
          if ((tgp_ram_r(tex_adr) & 0x3ff) == 0)
            cquad.col = (int) (MOIRE | PaletteToRGB(m_paletteram16[0]));

          // wingwar's display list carries a constant degenerate moiré record
          // (all four vertices on one point) that the flat-quad filler would
          // paint as a stray dot at the viewport centre; not seen on hardware.
          if (cquad.p[0]->s.x == cquad.p[1]->s.x && cquad.p[1]->s.x == cquad.p[2]->s.x && cquad.p[2]->s.x == cquad.p[3]->s.x &&
            cquad.p[0]->s.y == cquad.p[1]->s.y && cquad.p[1]->s.y == cquad.p[2]->s.y && cquad.p[2]->s.y == cquad.p[3]->s.y)
            skip = true;
        }

        // Direct polys are screen-space - project_point_direct only offsets x/y by
        // the viewport centre and ignores z.  fclip_push_quad's frustum clip tests
        // vertices against z-scaled planes (y > z*a_bottom, ...), which for a direct
        // poly with small/zero z collapses every plane onto the origin and culls the
        // whole poly (e.g. the SWA FIGHTER CONTROL console moiré rect, all z = 0).
        // Skip the frustum clip and queue the quad directly; fill_quad still scissors
        // it to the viewport rectangle.
        if (!skip)
          push_quad(cquad);
      }

      switch (link) {
      case 0:
      case 2:
        old_p0 = p0;
        old_p1 = p1;
        break;
      case 1:
        old_p1 = p0;
        break;
      case 3:
        old_p0 = p1;
        break;
      }
    }
    list_offset += 4;
    return list_offset;
  }

  int CGeometry::skip_direct(int list_offset) const
  {
    list_offset += 18;

    while (true) {
      uint32_t flags = readi(list_offset + 2);

      int type = flags & 3;
      if (!type)
        break;

      if (type == 2)
        list_offset += 12;
      else
        list_offset += 20;
    }
    list_offset += 4;
    return list_offset;
  }


  /******************************************************************************
   Sorting and output
  ******************************************************************************/

  int CGeometry::quad_t::compare(const quad_t *other) const
  {
    float z2 = other->z;

    if (z < z2)
      return +1;
    if (z > z2)
      return -1;

    if (this - other < 0)
      return -1;

    return +1;
  }

  void CGeometry::sort_quads()
  {
    const size_t count = m_numQuads;
    for (size_t i = 0; i < count; i++)
    {
      m_quadind[i] = &m_quaddb[i];
    }
    // MAME uses qsort with a comparison that never returns 0 (ties are broken by
    // address), so the result doesn't depend on the sort algorithm
    std::sort(m_quadind.begin(), m_quadind.begin() + count, [](const quad_t *a, const quad_t *b) { return a->compare(b) < 0; });
  }

  // Appends the sorted quads to the frame, as one batch clipped to the current
  // viewport (MAME's draw_quads() clips the viewport to the screen)
  void CGeometry::emit_quads(Frame *frame, RenderPass pass)
  {
    Batch batch;
    batch.x1 = std::max(m_view.x1, 0);
    batch.x2 = std::min(m_view.x2, SCREEN_WIDTH - 1);
    batch.y1 = std::max(m_view.y1, 0);
    batch.y2 = std::min(m_view.y2, SCREEN_HEIGHT - 1);
    batch.first = (uint32_t) frame->quads.size();
    batch.count = (uint32_t) m_numQuads;

    for (size_t i = 0; i < m_numQuads; i++)
    {
      const quad_t &q = *m_quadind[i];
      ScreenQuad sq;
      for (int j = 0; j < 4; j++)
      {
        sq.x[j] = q.p[j]->fx;
        sq.y[j] = q.p[j]->fy;
        sq.sx[j] = q.p[j]->s.x;
        sq.sy[j] = q.p[j]->s.y;
      }
      int color = q.col;
      if (color < 0)
        color = -1 - color;
      sq.color = (uint32_t) color;
      sq.z = q.z;
      sq.flags = q.flags;
      sq.adr = q.adr;

      // A wireframe primitive arrives as a degenerate quad with only two distinct
      // screen vertices (A,A,B,B); it is drawn as a line so near-horizontal wires
      // keep their full span instead of collapsing to one pixel per row.
      sq.line = false;
      sq.lineA = sq.lineB = 0;
      {
        int ax = sq.sx[0], ay = sq.sy[0];
        int bx = ax, by = ay, ndist = 1, b = 0;
        for (int k = 1; k < 4; k++)
        {
          int vx = sq.sx[k], vy = sq.sy[k];
          if (vx == ax && vy == ay)
            continue;
          if (ndist == 1)
          {
            bx = vx;
            by = vy;
            b = k;
            ndist = 2;
          }
          else if (vx != bx || vy != by)
          {
            ndist = 3;
            break;
          }
        }
        if (ndist == 2)
        {
          sq.line = true;
          sq.lineA = 0;
          sq.lineB = (uint8_t) b;
        }
      }

      frame->quads.push_back(sq);
    }

    if (batch.count)
    {
      if (pass == RENDER_BELOW_HUD)
        frame->below.push_back(batch);
      else
        frame->above.push_back(batch);
    }
  }

  void CGeometry::draw_objects(Frame *frame, RenderPass pass)
  {
    if (m_numQuads)
    {
      sort_quads();
      emit_quads(frame, pass);
    }

    m_numQuads = 0;
    m_numPoints = 0;
  }


  /******************************************************************************
   Display list
  ******************************************************************************/

  void CGeometry::set_current_render_list()
  {
    if(!(m_listctl[0] & 4))
      m_listctl[0] = (m_listctl[0] & ~0x40) | (m_listctl[0] & 8 ? 0x40 : 0);
    m_display_list_current = m_listctl[0] & 0x40 ? m_display_list1 : m_display_list0;
  }

  void CGeometry::end_frame(uint64_t frameNumber)
  {
    if((m_listctl[0] & 4) && (frameNumber & 1))
      m_listctl[0] ^= 0x40;
  }

  uint16_t CGeometry::ReadListCtl(unsigned offset)
  {
    if(!offset)
      return m_listctl[0] | 0x30;
    else
      return m_listctl[1];
  }

  void CGeometry::WriteListCtl(unsigned offset, uint16_t data, uint16_t mask)
  {
    uint16_t &r = m_listctl[offset & 1];
    r = (r & ~mask) | (data & mask);
  }

  void CGeometry::tgp_render(Frame *frame, RenderPass pass)
  {
    m_render_done = 1;
    if ((m_listctl[1] & 0x1f) == 0x1f)
    {
      set_current_render_list();

      m_view.init_translation_matrix();
      float old_z = 0;
      m_overflow = false;

      int list_offset = 0;
      for (int guard = 0; guard < 0x10000; guard++) {
        int type = readi(list_offset + 0);

        switch (type)
        {
        case 0:
          list_offset += 2;
          break;
        case 1:
          // command 0x01 = object drawn below the cat1 HUD tilemaps
          if (pass == RENDER_BELOW_HUD)
            push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), old_z);
          list_offset += 8;
          break;
        case 0x41:
          // command 0x41 = object drawn above the HUD (e.g. SWA radar blips)
          if (pass == RENDER_ABOVE_HUD)
            push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), old_z);
          list_offset += 8;
          break;
        case 2:
        {
          // direct (2D-projected) polys; push them in the below-HUD pass so
          // they share the per-viewport z-sort with the 0x01 objects
          if (pass == RENDER_BELOW_HUD)
            list_offset = push_direct(list_offset);
          else
            list_offset = skip_direct(list_offset);
          break;
        }
        case 3:
        {
          draw_objects(frame, pass);

          float xc = readi16(list_offset + 4);
          float yc = (float) (383 - (readi16(list_offset + 6) - 39));
          float x1 = readi16(list_offset + 8);
          float y2 = (float) (383 - (readi16(list_offset + 10) - 39));
          float x2 = readi16(list_offset + 12);
          float y1 = (float) (383 - (readi16(list_offset + 14) - 39));

          m_view.set_viewport(xc, yc, x1, x2, y1, y2);

          list_offset += 16;
          break;
        }
        case 4:
        {
          int adr = readi(list_offset + 2);
          int len = readi(list_offset + 4) + 1;
          for (int i = 0; i < len; i++)
          {
            tgp_ram_w(adr + i, readi16(list_offset + 6 + 2 * i));
          }
          list_offset += 6 + len * 2;
          break;
        }
        case 5:
        {
          int adr = readi(list_offset + 2);
          int len = readi(list_offset + 4);
          for (int i = 0; i < len; i++)
          {
            poly_ram_w(adr + i, readi(list_offset + 2 * i + 6));
          }
          list_offset += 6 + len * 2;
          break;
        }
        case 6:
        {
          int adr = readi(list_offset + 2);
          int len = readi(list_offset + 4);
          for (int i = 0; i < len; i++)
          {
            int v = readi(list_offset + 6 + i * 2);
            float diffuse = (float(v & 0xff)) / 255.0f;
            float ambient = (float((v >> 8) & 0xff)) / 255.0f;
            float specular = (float((v >> 16) & 0xff)) / 255.0f;
            int power = (v >> 24) & 0xff;
            m_view.set_lightparam(i + adr, diffuse, ambient, specular, power);
          }
          list_offset += 6 + len * 2;
          break;
        }
        case 7:
          // Mode word: bit 0 enables the specular term (netmerc toggles it
          // mid-list, on for the enemy mechs and off for terrain/cockpit).
          // Bit 1 selects face normals computed from the polygon's vertices
          // instead of the stored ones (315-5571 modes 2/3); VF and Wing War
          // switch it per object. Both persist across frames, as in the DSPs.
          m_view.spec_enable = (readi(list_offset + 2) & 1) != 0;
          m_view.computed_normals = (readi(list_offset + 2) & 2) != 0;
          list_offset += 4;
          break;
        case 8:
          list_offset += 4;
          break;
        case 9:
          m_view.set_zoom(readf(list_offset + 2) * 4, readf(list_offset + 4) * 4);
          list_offset += 6;
          break;
        case 0xa:
          m_view.set_light_direction(readf(list_offset + 2), readf(list_offset + 4), readf(list_offset + 6));
          list_offset += 8;
          break;
        case 0xb:
        {
          float mat[12];
          for (int i = 0; i < 12; i++)
          {
            mat[i] = readf(list_offset + 2 + 2 * i);
          }
          m_view.set_translation_matrix(mat);
          list_offset += 26;
          break;
        }
        case 0xc:
          m_view.set_view_translation(readf(list_offset + 2), readf(list_offset + 4));
          list_offset += 6;
          break;
        case 0xf:
          //case -1:
          goto end;
        default:
          DebugLog("Model 1 video: unknown display list command %d\n", type);
          goto end;
        }
      }
    end:
      draw_objects(frame, pass);
      if (m_overflow)
        DebugLog("Model 1 video: point or quad storage exhausted, part of the frame was dropped\n");
    }
  }

  void CGeometry::tgp_scan()
  {
    if (!m_render_done && (m_listctl[1] & 0x1f) == 0x1f)
    {
      set_current_render_list();
      // Skip everything but the data uploads
      int list_offset = 0;
      for (int guard = 0; guard < 0x10000; guard++)
      {
        int type = readi(list_offset + 0);
        switch (type) {
          case 0:
            list_offset += 2;
            break;
          case 1:
          case 0x41:
            list_offset += 8;
            break;
          case 2:
            list_offset = skip_direct(list_offset);
            break;
          case 3:
            list_offset += 16;
            break;
          case 4:
          {
            int adr = readi(list_offset + 2);
            int len = readi(list_offset + 4) + 1;
            for (int i = 0; i<len; i++)
            {
              tgp_ram_w(adr + i, readi16(list_offset + 6 + 2 * i));
            }
            list_offset += 6 + len * 2;
            break;
          }
          case 5:
          {
            int adr = readi(list_offset + 2);
            int len = readi(list_offset + 4);
            for (int i = 0; i < len; i++)
            {
              poly_ram_w(adr + i, readi(list_offset + 2 * i + 6));
            }
            list_offset += 6 + len * 2;
            break;
          }
          case 6:
          {
            int adr = readi(list_offset + 2);
            int len = readi(list_offset + 4);
            for (int i = 0; i<len; i++)
            {
              int v = readi(list_offset + 6 + i * 2);
              lightparam_t &lp = m_view.lightparams[(i + adr) & 0xff];
              lp.d = (float(v & 0xff)) / 255.0f;
              lp.a = (float((v >> 8) & 0xff)) / 255.0f;
              lp.s = (float((v >> 16) & 0xff)) / 255.0f;
              lp.p = (v >> 24) & 0xff;
            }
            list_offset += 6 + len * 2;
            break;
          }
          case 7:
            list_offset += 4;
            break;
          case 8:
            list_offset += 4;
            break;
          case 9:
            list_offset += 6;
            break;
          case 0xa:
            list_offset += 8;
            break;
          case 0xb:
            list_offset += 26;
            break;
          case 0xc:
            list_offset += 6;
            break;
          case 0xf:
          case -1:
            goto end;
          default:
            goto end;
        }
      }
    end:
      ;
    }
    m_render_done = 0;
  }

  void CGeometry::Render(Frame *frame, uint64_t frameNumber)
  {
    m_frameNumber = frameNumber;
    m_view.ayyc = std::cos(m_view.ayy);
    m_view.ayys = std::sin(m_view.ayy);
    frame->Clear();
    // the list is walked once per pass: start both from the same mode
    bool spec_enable = m_view.spec_enable, computed_normals = m_view.computed_normals;
    tgp_render(frame, RENDER_BELOW_HUD);
    m_view.spec_enable = spec_enable;
    m_view.computed_normals = computed_normals;
    tgp_render(frame, RENDER_ABOVE_HUD);
  }

  void CGeometry::VBlank(uint64_t frameNumber)
  {
    tgp_scan();
    end_frame(frameNumber);
  }


  /******************************************************************************
   MAME's software rasterizer
  ******************************************************************************/

  namespace
  {
    struct Bitmap
    {
      uint32_t *pix;
      uint32_t *row(int y) const { return pix + y * SCREEN_WIDTH; }
    };

    struct ClipRect
    {
      int x1, y1, x2, y2;
    };

    void draw_hline(const Bitmap &bitmap, int x1, int x2, int y, int color)
    {
      uint32_t *const base = bitmap.row(y);
      while(x1 <= x2)
      {
        base[x1] = color;
        x1++;
      }
    }

    void draw_hline_moired(const Bitmap &bitmap, int x1, int x2, int y, int color)
    {
      uint32_t *const base = bitmap.row(y);
      while(x1 <= x2)
      {
        if(!((x1^y) & 1))
        {
          base[x1] = color;
        }
        x1++;
      }
    }

    void fill_slope(const Bitmap &bitmap, const ClipRect *view, int color, int32_t x1, int32_t x2, int32_t sl1, int32_t sl2, int32_t y1, int32_t y2, int32_t *nx1, int32_t *nx2)
    {
      if(y1 > view->y2)
      {
        return;
      }

      if (y2 <= view->y1)
      {
        int delta = y2 - y1;
        *nx1 = x1 + delta * sl1;
        *nx2 = x2 + delta * sl2;
        return;
      }

      if (y2 > view->y2)
      {
        y2 = view->y2 + 1;
      }

      if (y1 < view->y1)
      {
        int delta = view->y1 - y1;
        x1 += delta * sl1;
        x2 += delta * sl2;
        y1 = view->y1;
      }

      if (x1 > x2 || (x1 == x2 && sl1 > sl2))
      {
        int32_t t = x1;
        x1 = x2;
        x2 = t;

        t = sl1;
        sl1 = sl2;
        sl2 = t;

        int32_t *tp = nx1;
        nx1 = nx2;
        nx2 = tp;
      }

      while(y1 < y2)
      {
        if(y1 >= view->y1)
        {
          int xx1 = x1>>FRAC_SHIFT;
          int xx2 = x2>>FRAC_SHIFT;
          if(xx1 <= view->x2 || xx2 >= view->x1)
          {
            if(xx1 < view->x1)
            {
              xx1 = view->x1;
            }
            if(xx2 > view->x2)
            {
              xx2 = view->x2;
            }

            if(color & MOIRE)
            {
              draw_hline_moired(bitmap, xx1, xx2, y1, color);
            }
            else
            {
              draw_hline(bitmap, xx1, xx2, y1, color);
            }
          }
        }

        x1 += sl1;
        x2 += sl2;
        y1++;
      }
      *nx1 = x1;
      *nx2 = x2;
    }

    void fill_line(const Bitmap &bitmap, const ClipRect *view, int color, int32_t y, int32_t x1, int32_t x2)
    {
      int xx1 = x1>>FRAC_SHIFT;
      int xx2 = x2>>FRAC_SHIFT;

      if(y > view->y2 || y < view->y1)
        return;

      if(xx1 <= view->x2 || xx2 >= view->x1) {
        if(xx1 < view->x1)
          xx1 = view->x1;
        if(xx2 > view->x2)
          xx2 = view->x2;

        if(color & MOIRE)
          draw_hline_moired(bitmap, xx1, xx2, y, color);
        else
          draw_hline(bitmap, xx1, xx2, y, color);
      }
    }

    // Draw a solid line between two screen points (integer Bresenham).  The
    // endpoints come from the unclipped projection and can be garbage, so clip
    // the segment to the viewport first (Liang-Barsky).
    void draw_wireframe_line(const Bitmap &bitmap, const ClipRect &clip, int x1, int y1, int x2, int y2, uint32_t color, bool moire)
    {
      const double lx1 = x1, ly1 = y1;
      const double cdx = double(x2) - lx1, cdy = double(y2) - ly1;
      const double p[4] = { -cdx, cdx, -cdy, cdy };
      const double q[4] = { lx1 - clip.x1, clip.x2 - lx1, ly1 - clip.y1, clip.y2 - ly1 };
      double t0 = 0.0, t1 = 1.0;
      for (int i = 0; i < 4; i++)
      {
        if (p[i] == 0.0)
        {
          if (q[i] < 0.0)
            return; // parallel to this edge and outside it
        }
        else
        {
          const double t = q[i] / p[i];
          if (p[i] < 0.0)
            t0 = std::max(t0, t);
          else
            t1 = std::min(t1, t);
        }
      }
      if (t0 > t1)
        return; // entirely outside the viewport

      if (t1 < 1.0)
      {
        x2 = int(std::lround(lx1 + t1 * cdx));
        y2 = int(std::lround(ly1 + t1 * cdy));
      }
      if (t0 > 0.0)
      {
        x1 = int(std::lround(lx1 + t0 * cdx));
        y1 = int(std::lround(ly1 + t0 * cdy));
      }

      int dx = std::abs(x2 - x1), dy = std::abs(y2 - y1);
      int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
      int err = dx - dy;
      for (;;)
      {
        if (x1 >= 0 && x1 < SCREEN_WIDTH && y1 >= 0 && y1 < SCREEN_HEIGHT)
          if (!moire || !((x1 ^ y1) & 1))
            bitmap.row(y1)[x1] = color;
        if (x1 == x2 && y1 == y2)
          break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
      }
    }

    void fill_quad(const Bitmap &bitmap, const ClipRect *view, const ScreenQuad &q)
    {
      struct { int32_t x, y; } p[8];
      int color = (int) q.color;

      if (q.line)
      {
        draw_wireframe_line(bitmap, *view, q.sx[q.lineA], q.sy[q.lineA], q.sx[q.lineB], q.sy[q.lineB], color & ~MOIRE, (color & MOIRE) != 0);
        return;
      }

      for (int i = 0; i < 4; i++)
      {
        p[i].x = p[i+4].x = q.sx[i] << FRAC_SHIFT;
        p[i].y = p[i+4].y = q.sy[i];
      }

      int pmin = 0;
      int pmax = 0;
      for (int i = 1; i < 4; i++)
      {
        if(p[i].y < p[pmin].y)
        {
          pmin = i;
        }
        if(p[i].y > p[pmax].y)
        {
          pmax = i;
        }
      }

      int32_t cury = p[pmin].y;
      int32_t limy = p[pmax].y;

      int32_t x1, x2;
      if (cury == limy)
      {
        x1 = p[0].x;
        x2 = p[0].x;
        for (int i = 1; i < 4; i++)
        {
          if (p[i].x < x1)
          {
            x1 = p[i].x;
          }
          if (p[i].x > x2)
          {
            x2 = p[i].x;
          }
        }
        fill_line(bitmap, view, color, cury, x1, x2);
        return;
      }

      if(cury > view->y2)
      {
        return;
      }
      if(limy <= view->y1)
      {
        return;
      }

      if(limy > view->y2)
      {
        limy = view->y2;
      }

      int ps1 = pmin+4;
      int ps2 = pmin;

      int32_t sl1, sl2;

      // MAME jumps into the loop at "startup"; the same flow without the goto
      while(p[ps1-1].y == cury)
      {
        ps1--;
      }
      while(p[ps2+1].y == cury)
      {
        ps2++;
      }
      x1 = p[ps1].x;
      x2 = p[ps2].x;
      sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
      sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);

      for(;;)
      {
        if (p[ps1 - 1].y == p[ps2 + 1].y)
        {
          fill_slope(bitmap, view, color, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2);
          cury = p[ps1 - 1].y;
          if(cury >= limy)
          {
            break;
          }
          ps1--;
          ps2++;

          while(p[ps1-1].y == cury)
          {
            ps1--;
          }
          while(p[ps2+1].y == cury)
          {
            ps2++;
          }
          x1 = p[ps1].x;
          x2 = p[ps2].x;
          sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
          sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
        }
        else if (p[ps1 - 1].y < p[ps2 + 1].y)
        {
          fill_slope(bitmap, view, color, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2);
          cury = p[ps1 - 1].y;
          if(cury >= limy)
          {
            break;
          }
          ps1--;
          while(p[ps1 - 1].y == cury)
          {
            ps1--;
          }
          x1 = p[ps1].x;
          sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
        }
        else
        {
          fill_slope(bitmap, view, color, x1, x2, sl1, sl2, cury, p[ps2 + 1].y, &x1, &x2);
          cury = p[ps2 + 1].y;
          if(cury >= limy)
          {
            break;
          }
          ps2++;
          while(p[ps2 + 1].y == cury)
          {
            ps2++;
          }
          x2 = p[ps2].x;
          sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
        }
      }
      if(cury == limy)
      {
        fill_line(bitmap, view, color, cury, x1, x2);
      }
    }
  }

  void CGeometry::Rasterize(const Frame &frame, const std::vector<Batch> &batches, uint32_t *pixels)
  {
    Bitmap bitmap { pixels };
    for (const Batch &b : batches)
    {
      ClipRect view { b.x1, b.y1, b.x2, b.y2 };
      for (uint32_t i = 0; i < b.count; i++)
        fill_quad(bitmap, &view, frame.quads[b.first + i]);
    }
  }


  /******************************************************************************
   State
  ******************************************************************************/

  void CGeometry::AttachMemory(const uint16_t *displayList0, const uint16_t *displayList1, const uint32_t *polyROM, uint32_t polyROMWords, const uint16_t *paletteRAM, const uint16_t *colorXlat)
  {
    m_display_list0 = displayList0;
    m_display_list1 = displayList1;
    m_poly_rom = polyROM;
    m_poly_rom_words = polyROMWords;
    m_paletteram16 = paletteRAM;
    m_color_xlat = colorXlat;
    m_display_list_current = m_display_list0;
  }

  void CGeometry::Reset()
  {
    m_listctl[0] = m_listctl[1] = 0;
    m_render_done = false;
    m_numPoints = 0;
    m_numQuads = 0;
    m_display_list_current = m_display_list0;
  }

  void CGeometry::SaveState(CBlockFile *SaveState)
  {
    SaveState->NewBlock("Model 1 Geometry", __FILE__);
    SaveState->Write(m_tgp_ram.data(), TGP_RAM_WORDS * sizeof(uint16_t));
    SaveState->Write(m_poly_ram.data(), POLY_RAM_SAVED_WORDS * sizeof(uint32_t));
    SaveState->Write(m_listctl, sizeof(m_listctl));
    // MAME doesn't save the view (it's rebuilt by each display list walk), but
    // the light parameters persist between frames
    for (auto &lp : m_view.lightparams)
    {
      SaveState->Write(&lp.a, sizeof(lp.a));
      SaveState->Write(&lp.d, sizeof(lp.d));
      SaveState->Write(&lp.s, sizeof(lp.s));
      SaveState->Write(&lp.p, sizeof(lp.p));
    }
  }

  void CGeometry::LoadState(CBlockFile *SaveState)
  {
    if (Result::OKAY != SaveState->FindBlock("Model 1 Geometry"))
    {
      ErrorLog("Unable to load Model 1 geometry state. Save state file is corrupt.");
      return;
    }
    SaveState->Read(m_tgp_ram.data(), TGP_RAM_WORDS * sizeof(uint16_t));
    SaveState->Read(m_poly_ram.data(), POLY_RAM_SAVED_WORDS * sizeof(uint32_t));
    SaveState->Read(m_listctl, sizeof(m_listctl));
    for (auto &lp : m_view.lightparams)
    {
      SaveState->Read(&lp.a, sizeof(lp.a));
      SaveState->Read(&lp.d, sizeof(lp.d));
      SaveState->Read(&lp.s, sizeof(lp.s));
      SaveState->Read(&lp.p, sizeof(lp.p));
    }
  }

  CGeometry::CGeometry()
  {
    m_pointdb.resize(MAX_POINTS);
    m_quaddb.resize(MAX_QUADS);
    m_quadind.resize(MAX_QUADS);
    m_tgp_ram.assign(TGP_RAM_WORDS, 0);
    m_poly_ram.assign(POLY_RAM_WORDS, 0);

    m_clipfn[0].m_isclipped = &CGeometry::fclip_isc_bottom;
    m_clipfn[0].m_clip = &CGeometry::fclip_clip_bottom;
    m_clipfn[1].m_isclipped = &CGeometry::fclip_isc_top;
    m_clipfn[1].m_clip = &CGeometry::fclip_clip_top;
    m_clipfn[2].m_isclipped = &CGeometry::fclip_isc_left;
    m_clipfn[2].m_clip = &CGeometry::fclip_clip_left;
    m_clipfn[3].m_isclipped = &CGeometry::fclip_isc_right;
    m_clipfn[3].m_clip = &CGeometry::fclip_clip_right;
  }
}
