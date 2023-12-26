#ifndef INCLUDED_IRENDER3D_H
#define INCLUDED_IRENDER3D_H

#include <cstdint>

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
  virtual void SetStepping(int stepping) = 0;
  virtual bool Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget) = 0;
  virtual void SetSunClamp(bool enable) = 0;
  virtual void SetSignedShade(bool enable) = 0;
  virtual float GetLosValue(int layer) = 0;

  virtual ~IRender3D()
  {
  }
};

#endif  // INCLUDED_IRENDER3D_H
