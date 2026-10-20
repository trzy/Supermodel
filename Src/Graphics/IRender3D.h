#ifndef INCLUDED_IRENDER3D_H
#define INCLUDED_IRENDER3D_H

#include <cstdint>
#include "Types.h"
#include "Game.h"

/*
 * IRender3D:
 *
 * Interface (abstract base class) for Real3D rendering engine.
 */
class IRender3D
{
public:
  virtual void RenderFrame(void) = 0;
  virtual void BeginFrame(void) = 0;
  virtual void EndFrame(void) = 0;
  virtual void UploadTextures(unsigned level, unsigned x, unsigned y, unsigned width, unsigned height) = 0;
  virtual void AttachMemory(const uint32_t *cullingRAMLoPtr, const uint32_t *cullingRAMHiPtr, const uint32_t *polyRAMPtr, const uint32_t *vromPtr, const uint16_t *textureRAMPtr) = 0;

  /*
   * SetStepping(stepping):
   *
   * Sets the hardware stepping. It sets the data formats (culling node size,
   * fixed-point formats) and some behavior. The Real3D Pro-1000 uses the
   * Model 3 Step 1.0 formats. Call it before AttachMemory().
   */
  virtual void SetStepping(Game::Stepping stepping) = 0;

  virtual Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget) = 0;
  virtual void SetSunClamp(bool enable) = 0;
  virtual void SetBlockCulling(bool enable) = 0;
  virtual float GetLosValue(int layer) = 0;

  /*
   * SetVROMIsPolygonRAM(enable):
   *
   * Tells the renderer that there is no VROM (Real3D Pro-1000). The VROM
   * pointer then points to polygon RAM, which is larger, so model addresses
   * at 0x100000 and above are writable polygon RAM. The default is false
   * (Model 3). A renderer that does not support this can ignore it.
   */
  virtual void SetVROMIsPolygonRAM(bool enable)
  {
  }

  /*
   * SetScreenSize(width, height):
   *
   * Set the native display coordinate range that viewport coordinates use.
   * Model 3 uses 496x384. Pro-1000 uses values from the selected sync file
   * (for example, 636x480 for sync file 1). Call this before Init(), or
   * call it at any time when the sync file changes. Renderers that do not
   * need this can ignore it.
   */
  virtual void SetScreenSize(float width, float height)
  {
  }

  virtual ~IRender3D()
  {
  }
};

#endif  // INCLUDED_IRENDER3D_H
