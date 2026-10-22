/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * NoSDL.h (libretro)
 *
 * Replaces OSD/SDL/SDLIncludes.h in the libretro core when it is built
 * without LIBRETRO_NET. Makefiles/Rules.inc includes this file before each
 * C++ file (-include). It has the same include guard as OSD/SDL/SDLIncludes.h,
 * so that header is then empty. The core does not link SDL; this file gives
 * the few SDL and SDL_net functions that the emulator code calls:
 *
 *  - SDL_ShowSimpleMessageBox(): writes the message to the log.
 *  - SDL_LoadBMP(), SDL_FreeSurface(): uncompressed 32-bit BMP files only (the
 *    crosshair bitmaps). The crosshair bitmaps are optional in the libretro
 *    core: if a file cannot be read, a transparent 1x1 image is returned so
 *    that the vector crosshairs still work.
 *  - SDL_GetDisplayDPI(): the frontend scales the output, so the standard
 *    96 DPI.
 *  - SDL_SwapBE16(), SDL_SwapBE32().
 *  - SDL_net: every function fails, so the network server cannot start. The
 *    libretro core gives no network server to the emulator in this build.
 */

// Same include guard as OSD/SDL/SDLIncludes.h (see above)
#ifndef INCLUDED_SDLINCLUDES_H
#define INCLUDED_SDLINCLUDES_H

#include "OSD/Logger.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

/******************************************************************************
 SDL
******************************************************************************/

#define SDL_MESSAGEBOX_ERROR 0x10

inline int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message, void *window)
{
  (void) flags;
  (void) window;
  ErrorLog("%s: %s", title, message);
  return 0;
}

inline Uint16 SDL_SwapBE16(Uint16 x)
{
  uint8_t b[2];
  memcpy(b, &x, 2);
  return Uint16((b[0] << 8) | b[1]);
}

inline Uint32 SDL_SwapBE32(Uint32 x)
{
  uint8_t b[4];
  memcpy(b, &x, 4);
  return (Uint32(b[0]) << 24) | (Uint32(b[1]) << 16) | (Uint32(b[2]) << 8) | Uint32(b[3]);
}

inline int SDL_GetDisplayDPI(int displayIndex, float *ddpi, float *hdpi, float *vdpi)
{
  (void) displayIndex;
  if (ddpi) *ddpi = 96.0f;
  if (hdpi) *hdpi = 96.0f;
  if (vdpi) *vdpi = 96.0f;
  return 0;
}

struct SDL_Surface
{
  int w = 0;
  int h = 0;
  void *pixels = nullptr;
  std::vector<uint8_t> data;  // pixels: BGRA rows, top row first
};

inline SDL_Surface *SDL_LoadBMP32(const char *file)
{
  FILE *fp = fopen(file, "rb");
  if (!fp)
    return nullptr;
  std::vector<uint8_t> data;
  uint8_t buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    data.insert(data.end(), buffer, buffer + n);
  fclose(fp);

  auto le16 = [&data](size_t offset) -> uint32_t { return data[offset] | (data[offset + 1] << 8); };
  auto le32 = [&data](size_t offset) -> uint32_t { return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (uint32_t(data[offset + 3]) << 24); };

  if (data.size() < 54 || data[0] != 'B' || data[1] != 'M')
    return nullptr;
  uint32_t pixelOffset = le32(10);
  int32_t w = int32_t(le32(18));
  int32_t h = int32_t(le32(22));
  uint32_t bpp = le16(28);
  uint32_t compression = le32(30);
  if (bpp != 32 || (compression != 0 && compression != 3) || w <= 0 || h == 0)
    return nullptr;
  bool bottomUp = h > 0;
  if (h < 0)
    h = -h;
  size_t rowBytes = size_t(w) * 4;
  if (pixelOffset + rowBytes * size_t(h) > data.size())
    return nullptr;

  SDL_Surface *surface = new SDL_Surface();
  surface->w = w;
  surface->h = h;
  surface->data.resize(rowBytes * size_t(h));
  for (int32_t y = 0; y < h; y++)
  {
    int32_t srcRow = bottomUp ? (h - 1 - y) : y;
    memcpy(surface->data.data() + rowBytes * size_t(y), data.data() + pixelOffset + rowBytes * size_t(srcRow), rowBytes);
  }
  surface->pixels = surface->data.data();
  return surface;
}

inline SDL_Surface *SDL_LoadBMP(const char *file)
{
  SDL_Surface *surface = SDL_LoadBMP32(file);
  if (surface)
    return surface;
  InfoLog("'%s' not found: bitmap crosshairs are not available (copy Assets/*.bmp to system/supermodel/Assets).", file);
  surface = new SDL_Surface();
  surface->w = 1;
  surface->h = 1;
  surface->data.assign(4, 0);   // transparent
  surface->pixels = surface->data.data();
  return surface;
}

inline void SDL_FreeSurface(SDL_Surface *surface)
{
  delete surface;
}

/******************************************************************************
 SDL_net (no network)
******************************************************************************/

struct IPaddress
{
  Uint32 host;
  Uint16 port;
};

typedef struct _TCPsocket *TCPsocket;
typedef struct _SDLNet_SocketSet *SDLNet_SocketSet;

inline int SDLNet_Init(void) { return -1; }
inline void SDLNet_Quit(void) {}
inline const char *SDLNet_GetError(void) { return "this libretro core is built without network support (LIBRETRO_NET=1)"; }
inline int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port) { (void) address; (void) host; (void) port; return -1; }
inline TCPsocket SDLNet_TCP_Open(IPaddress *ip) { (void) ip; return nullptr; }
inline TCPsocket SDLNet_TCP_Accept(TCPsocket server) { (void) server; return nullptr; }
inline void SDLNet_TCP_Close(TCPsocket sock) { (void) sock; }
inline int SDLNet_TCP_Send(TCPsocket sock, const void *data, int len) { (void) sock; (void) data; (void) len; return -1; }
inline int SDLNet_TCP_Recv(TCPsocket sock, void *data, int maxlen) { (void) sock; (void) data; (void) maxlen; return -1; }
inline IPaddress *SDLNet_TCP_GetPeerAddress(TCPsocket sock) { (void) sock; return nullptr; }
inline SDLNet_SocketSet SDLNet_AllocSocketSet(int maxsockets) { (void) maxsockets; return nullptr; }
inline void SDLNet_FreeSocketSet(SDLNet_SocketSet set) { (void) set; }
inline int SDLNet_CheckSockets(SDLNet_SocketSet set, Uint32 timeout) { (void) set; (void) timeout; return -1; }
inline int SDLNet_TCP_AddSocket(SDLNet_SocketSet set, TCPsocket sock) { (void) set; (void) sock; return -1; }
inline int SDLNet_TCP_DelSocket(SDLNet_SocketSet set, TCPsocket sock) { (void) set; (void) sock; return -1; }
inline int SDLNet_SocketReady(TCPsocket sock) { (void) sock; return 0; }

#endif  // INCLUDED_SDLINCLUDES_H
