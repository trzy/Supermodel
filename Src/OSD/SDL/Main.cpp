/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2023 The Supermodel Team
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
 * Main.cpp
 *
 * Main program driver for the SDL port.
 *
 * Bugs and Issues to Address
 * --------------------------
 * - -gfx-state crashes when ENABLE_DEBUGGER is also enabled:
 *
 *    #0  0x00007ff6586392a0 in CSoundBoard::GetDSB (this=0x1128) at Src/Model3/SoundBoard.cpp:552
 *    #1  0x00007ff6587072cf in Debugger::CSupermodelDebugger::CreateDSBCPUDebug (model3=0x0) at Src/Debugger/SupermodelDebugger.cpp:269
 *    #2  0x00007ff6587076e9 in Debugger::CSupermodelDebugger::AddCPUs (this=0x185da75df40) at Src/Debugger/SupermodelDebugger.cpp:361
 *    #3  0x00007ff6586f91a6 in Debugger::CDebugger::Attach (this=0x185da75df40) at Src/Debugger/Debugger.cpp:263
 *    #4  0x00007ff658705312 in Debugger::CConsoleDebugger::Attach (this=0x185da75df40) at Src/Debugger/ConsoleDebugger.cpp:2707
 *    #5  0x00007ff65862c6fc in Supermodel (game=..., rom_set=0x6cc8dff0e0, Model3=0x185da7598d0, Inputs=0x185da752590, Outputs=0x0, Debugger=std::shared_ptr<Debugger::CDebugger> (use count 3, weak count 0) = {...})
 *        at Src/OSD/SDL/Main.cpp:978
 *    #6  0x00007ff658634ba3 in SDL_main (argc=3, argv=0x185da4e68a0) at Src/OSD/SDL/Main.cpp:2056
 *    #7  0x00007ff658720141 in main_getcmdline ()
 *    #8  0x00007ff6585b13b1 in __tmainCRTStartup () at C:/M/mingw-w64-crt-git/src/mingw-w64/mingw-w64-crt/crt/crtexe.c:321
 *    #9  0x00007ff6585b14e6 in mainCRTStartup () at C:/M/mingw-w64-crt-git/src/mingw-w64/mingw-w64-crt/crt/crtexe.c:202
 * - Need to address all the stale TO-DOs below and clear them out :)
 *
 * To Do Before Next Release
 * -------------------------
 * - Thoroughly test config system (do overrides work as expected? XInput
 *   force settings?)
 * - Remove all occurrences of "using namespace std" from Nik's code.
 * - Standardize variable naming (recently introduced vars_like_this should be
 *   converted back to varsLikeThis).
 * - Update save state file revision (strings > 1024 chars are now supported).
 * - Fix BlockFile.cpp to use fstream!
 * - Check to make sure save states use explicitly-sized types for 32/64-bit
 *   compatibility (i.e., size_t, int, etc. not allowed).
 * - Make sure quitting while paused works.
 * - Add UI keys for balance setting?
 * - 5.1 audio support?
 *
 * Compile-Time Options
 * --------------------
 * - SUPERMODEL_WIN32: Define this if compiling on Windows.
 * - SUPERMODEL_OSX: Define this if compiling on Mac OS X.
 * - SUPERMODEL_DEBUGGER: Enable the debugger.
 * - DEBUG: Debug mode (use with caution, produces large logs of game behavior)
 */

#include <new>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <memory>
#include <vector>
#include <algorithm>
#include <GL/glew.h>

#ifdef SUPERMODEL_WIN32
#include "DirectInputSystem.h"
#include "WinOutputs.h"
#endif

#include "Supermodel.h"
#include "Util/Format.h"
#include "Util/NewConfig.h"
#include "Util/ConfigBuilders.h"
#include "OSD/FileSystemPath.h"
#include "GameLoader.h"
#include "SDLInputSystem.h"
#include "SDLIncludes.h"
#include "Debugger/SupermodelDebugger.h"
#include "Graphics/Legacy3D/Legacy3D.h"
#include "Graphics/New3D/New3D.h"
#include "Model3/IEmulator.h"
#include "Model3/Model3.h"
#include "OSD/Audio.h"
#include "Graphics/New3D/VBO.h"
#include "Graphics/SuperAA.h"

#include <iostream>
#include "Util/BMPFile.h"

#include "Crosshair.h"

/******************************************************************************
 Global Run-time Config
******************************************************************************/

static Util::Config::Node s_runtime_config("Global");


/******************************************************************************
 Display Management
******************************************************************************/

SDL_Window *s_window = nullptr;

/*
 * Position and size of rectangular region within OpenGL display to render to.
 * Unlike the config tree, these end up containing the actual resolution (and
 * computed offsets within the viewport) that will be rendered based on what
 * was obtained from SDL.
 */
static unsigned  xOffset, yOffset;      // offset of renderer output within OpenGL viewport
static unsigned  xRes, yRes;            // renderer output resolution (can be smaller than GL viewport)
static unsigned  totalXRes, totalYRes;  // total resolution (the whole GL viewport)
static int aaValue = 1;                 // default is 1 which is no aa

/*
 * Crosshair stuff
 */
static CCrosshair* s_crosshair = nullptr;

static bool SetGLGeometry(unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio)
{
  // What resolution did we actually get?
  int actualWidth;
  int actualHeight;
  SDL_GetWindowSize(s_window, &actualWidth, &actualHeight);
  *totalXResPtr = actualWidth;
  *totalYResPtr = actualHeight;

  // If required, fix the aspect ratio of the resolution that the user passed to match Model 3 ratio
  float xRes = float(*xResPtr);
  float yRes = float(*yResPtr);
  if (keepAspectRatio)
  {
    float model3Ratio = float(496.0/384.0);
    if (yRes < (xRes/model3Ratio))
      xRes = yRes*model3Ratio;
    if (xRes < (yRes*model3Ratio))
      yRes = xRes/model3Ratio;
  }

  // Center the visible area
  *xOffsetPtr = (*xResPtr - (unsigned) xRes)/2;
  *yOffsetPtr = (*yResPtr - (unsigned) yRes)/2;

  // If the desired resolution is smaller than what we got, re-center again
  if (int(*xResPtr) < actualWidth)
    *xOffsetPtr += (actualWidth - *xResPtr)/2;
  if (int(*yResPtr) < actualHeight)
    *yOffsetPtr += (actualHeight - *yResPtr)/2;

  // OpenGL initialization
  glViewport(0,0,*xResPtr,*yResPtr);
  glClearColor(0.0,0.0,0.0,0.0);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  // Clear both buffers to ensure a black border
  for (int i = 0; i < 2; i++)
  {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    SDL_GL_SwapWindow(s_window);
  }

  // Write back resolution parameters
  *xResPtr = (unsigned) xRes;
  *yResPtr = (unsigned) yRes;

  UINT32 correction = (UINT32)(((yRes / 384.f) * 2.f) + 0.5f);

  glEnable(GL_SCISSOR_TEST);

  // Scissor box (to clip visible area)
  if (s_runtime_config["WideScreen"].ValueAsDefault<bool>(false))
  {
    glScissor(0* aaValue, correction* aaValue, *totalXResPtr * aaValue, (*totalYResPtr - (correction * 2)) * aaValue);
  }
  else
  {
      glScissor((*xOffsetPtr + correction) * aaValue, (*yOffsetPtr + correction) * aaValue, (*xResPtr - (correction * 2)) * aaValue, (*yResPtr - (correction * 2)) * aaValue);
  }
  return OKAY;
}

static void GLAPIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    printf("OGLDebug:: 0x%X: %s\n", id, message);
}

// In windows with an nvidia card (sorry not tested anything else) you can customise the resolution.
// This also allows you to set a totally custom refresh rate. Apparently you can drive most monitors at
// 57.5fps with no issues. Anyway this code will automatically pick up your custom refresh rate, and set it if it exists
// It it doesn't exist, then it'll probably just default to 60 or whatever your refresh rate is.
static void SetFullScreenRefreshRate()
{
    float refreshRateHz = std::abs(s_runtime_config["RefreshRate"].ValueAs<float>());

    if (refreshRateHz > 57.f && refreshRateHz < 58.f) {

        int display_in_use = 0; /* Only using first display */

        int display_mode_count = SDL_GetNumDisplayModes(display_in_use);
        if (display_mode_count < 1) {
            return;
        }

        for (int i = 0; i < display_mode_count; ++i) {

            SDL_DisplayMode mode;

            if (SDL_GetDisplayMode(display_in_use, i, &mode) != 0) {
                return;
            }

            if (SDL_BITSPERPIXEL(mode.format) >= 24 && mode.w == totalXRes && mode.h == totalYRes) {
                if (mode.refresh_rate == 57 || mode.refresh_rate == 58) {       // nvidia is fairly flexible in what refresh rate windows will show, so we can match either 57 or 58,
                    int result = SDL_SetWindowDisplayMode(s_window, &mode);     // both are totally non standard frequencies and shouldn't be set incorrectly
                    if (result == 0) {
                        printf("Custom fullscreen mode set: %ix%i@57.524 Hz\n", mode.w, mode.h);
                    }
                    break;
                }
            }
        }
    }
}

/*
 * CreateGLScreen():
 *
 * Creates an OpenGL display surface of the requested size. xOffset and yOffset
 * are used to return a display surface offset (for OpenGL viewport commands)
 * because the actual drawing area may need to be adjusted to preserve the
 * Model 3 aspect ratio. The new resolution will be passed back as well -- both
 * the adjusted viewable area resolution and the total resolution.
 *
 * NOTE: keepAspectRatio should always be true. It has not yet been tested with
 * the wide screen hack.
 */
static bool CreateGLScreen(bool coreContext, bool quadRendering, const std::string &caption, bool focusWindow, unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio, bool fullScreen)
{
  GLenum err;

  // Call only once per program session (this is because of issues with
  // DirectInput when the window is destroyed and a new one created). Use
  // ResizeGLScreen() to change resolutions instead.
  if (s_window != nullptr)
  {
    return ErrorLog("Internal error: CreateGLScreen() called more than once");
  }

  // Initialize video subsystem
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    return ErrorLog("Unable to initialize SDL video subsystem: %s\n", SDL_GetError());

  // Important GL attributes
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

  if (coreContext) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

      if (quadRendering) {
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
      }
      else {
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
          SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
      }
  }

  // Set video mode
  s_window = SDL_CreateWindow(caption.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, *xResPtr, *yResPtr, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | (fullScreen ? SDL_WINDOW_FULLSCREEN : 0));
  if (nullptr == s_window)
  {
    ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    return FAIL;
  }

  if (focusWindow)
  {
    SDL_RaiseWindow(s_window);
  }

  // Create OpenGL context
  SDL_GLContext context = SDL_GL_CreateContext(s_window);
  if (nullptr == context)
  {
    ErrorLog("Unable to create OpenGL context: %s\n", SDL_GetError());
    return FAIL;
  }

  // Set vsync
  SDL_GL_SetSwapInterval(s_runtime_config["VSync"].ValueAsDefault<bool>(false) ? 1 : 0);

  // Set the context as the current window context
  SDL_GL_MakeCurrent(s_window, context);

  // Initialize GLEW, allowing us to use features beyond OpenGL 1.2
  err = glewInit();
  if (GLEW_OK != err)
  {
    ErrorLog("OpenGL initialization failed: %s\n", glewGetErrorString(err));
    return FAIL;
  }

  // print some basic GPU info
  GLint profile = 0;
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);

  printf("GPU info: %s ", glGetString(GL_VERSION));

  if (profile & GL_CONTEXT_CORE_PROFILE_BIT) {
      printf("(core profile)");
  }

  if (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
      printf("(compatability profile)");
  }

  printf("\n\n");

  //glDebugMessageCallback(DebugCallback, NULL);
  //glDebugMessageControl(GL_DONT_CARE,GL_DONT_CARE,GL_DONT_CARE, 0, 0, GL_TRUE);
  //glEnable(GL_DEBUG_OUTPUT);

  return SetGLGeometry(xOffsetPtr, yOffsetPtr, xResPtr, yResPtr, totalXResPtr, totalYResPtr, keepAspectRatio);
}

static void DestroyGLScreen()
{
  if (s_window != nullptr)
  {
    SDL_GL_DeleteContext(SDL_GL_GetCurrentContext());
    SDL_DestroyWindow(s_window);
  }
}

static bool ResizeGLScreen(unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio, bool fullScreen)
{
  // Set full screen mode
  if (SDL_SetWindowFullscreen(s_window, fullScreen ? SDL_WINDOW_FULLSCREEN : 0) < 0)
  {
    ErrorLog("Unable to enter %s mode: %s\n", fullScreen ? "fullscreen" : "windowed", SDL_GetError());
    return FAIL;
  }

  return SetGLGeometry(xOffsetPtr, yOffsetPtr, xResPtr, yResPtr, totalXResPtr, totalYResPtr, keepAspectRatio);
}

/*
 * PrintGLInfo():
 *
 * Queries and prints OpenGL information. A full list of extensions can
 * optionally be printed.
 */
static void PrintGLInfo(bool createScreen, bool infoLog, bool printExtensions)
{
  unsigned xOffset, yOffset, xRes=496, yRes=384, totalXRes, totalYRes;
  if (createScreen)
  {
    if (OKAY != CreateGLScreen(false, false, "Supermodel - Querying OpenGL Information...", false, &xOffset, &yOffset, &xRes, &yRes, &totalXRes, &totalYRes, false, false))
    {
      ErrorLog("Unable to query OpenGL.\n");
      return;
    }
  }

  GLint value;
  if (infoLog)  InfoLog("OpenGL information:");
  else             puts("OpenGL information:\n");
  const GLubyte *str = glGetString(GL_VENDOR);
  if (infoLog)  InfoLog("  Vendor                   : %s", str);
  else           printf("  Vendor                   : %s\n", str);
  str = glGetString(GL_RENDERER);
  if (infoLog)  InfoLog("  Renderer                 : %s", str);
  else           printf("  Renderer                 : %s\n", str);
  str = glGetString(GL_VERSION);
  if (infoLog)  InfoLog("  Version                  : %s", str);
  else           printf("  Version                  : %s\n", str);
  str = glGetString(GL_SHADING_LANGUAGE_VERSION);
  if (infoLog)  InfoLog("  Shading Language Version : %s", str);
  else           printf("  Shading Language Version : %s\n", str);
  glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
  if (infoLog)  InfoLog("  Maximum Vertex Array Size: %d vertices", value);
  else           printf("  Maximum Vertex Array Size: %d vertices\n", value);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
  if (infoLog)  InfoLog("  Maximum Texture Size     : %d texels", value);
  else           printf("  Maximum Texture Size     : %d texels\n", value);
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
  if (infoLog)  InfoLog("  Maximum Vertex Attributes: %d", value);
  else           printf("  Maximum Vertex Attributes: %d\n", value);
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
  if (infoLog)  InfoLog("  Maximum Vertex Uniforms  : %d", value);
  else           printf("  Maximum Vertex Uniforms  : %d\n", value);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
  if (infoLog)  InfoLog("  Maximum Texture Img Units: %d", value);
  else           printf("  Maximum Texture Img Units: %d\n", value);
  if (printExtensions)
  {
    str = glGetString(GL_EXTENSIONS);
    char *strLocal = (char *) malloc((strlen((char *) str)+1)*sizeof(char));
    if (NULL == strLocal)
    {
      if (infoLog)  InfoLog("  Supported Extensions     : %s", str);
      else           printf("  Supported Extensions     : %s\n", str);
    }
    else
    {
      strcpy(strLocal, (char *) str);
      if (infoLog)  InfoLog("  Supported Extensions     : %s", (strLocal = strtok(strLocal, " \t\n")));
      else           printf("  Supported Extensions     : %s\n", (strLocal = strtok(strLocal, " \t\n")));
      char* strLocalTmp = strLocal;
      while ((strLocalTmp = strtok(NULL, " \t\n")) != NULL)
      {
        if (infoLog)  InfoLog("                             %s", strLocalTmp);
        else           printf("                             %s\n", strLocalTmp);
      }
    }
    free(strLocal);
  }
  if (infoLog)  InfoLog("");
  else      printf("\n");
}

#ifdef DEBUG
static void PrintBAT(unsigned regu, unsigned regl)
{
  uint32_t batu = ppc_read_spr(regu);
  uint32_t batl = ppc_read_spr(regl);
  uint32_t bepi = batu >> (31 - 14);
  uint32_t bl = (batu >> (31 - 29)) & 0x7ff;
  bool vs = batu & 2;
  bool vp = batu & 1;
  uint32_t brpn = batl >> (31 - 14);
  uint32_t wimg = (batl >> (31 - 28)) & 0xf;
  uint32_t pp = batl & 3;
  uint32_t size = (bl + 1) * 128 * 1024;
  uint32_t ea_base = bepi << (31 - 14);
  uint32_t ea_limit = ea_base + size - 1;
  uint32_t pa_base = brpn << (31 - 14);
  uint32_t pa_limit = pa_base + size - 1;
  printf("%08X-%08X -> %08X-%08X ", ea_base, ea_limit, pa_base, pa_limit);
  printf("%c%c%c%c ", (wimg&8)?'W':'-', (wimg&4)?'I':'-', (wimg&2)?'M':'-', (wimg&1)?'G':'-');
  printf("PP=");
  if (pp == 0)
    printf("NA");
  else if (pp == 2)
    printf("RW");
  else
    printf("RO");
  printf(" Vs=%d Vp=%d", vs, vp);
}
#endif

#ifdef DEBUG
static void DumpPPCRegisters(IBus *bus)
{
  for (int i = 0; i < 32; i += 4)
  {
    printf("R%d=%08X\tR%d=%08X\tR%d=%08X\tR%d=%08X\n",
           i + 0, ppc_get_gpr(i + 0),
           i + 1, ppc_get_gpr(i + 1),
           i + 2, ppc_get_gpr(i + 2),
           i + 3, ppc_get_gpr(i + 3));
  }
  printf("PC =%08X\n", ppc_get_pc());
  printf("LR =%08X\n", ppc_get_lr());
  printf("DBAT0U=%08X\tIBAT0U=%08X\n", ppc_read_spr(SPR603E_DBAT0U), ppc_read_spr(SPR603E_IBAT0U));
  printf("DBAT0L=%08X\tIBAT0L=%08X\n", ppc_read_spr(SPR603E_DBAT0L), ppc_read_spr(SPR603E_IBAT0L));
  printf("DBAT1U=%08X\tIBAT1U=%08X\n", ppc_read_spr(SPR603E_DBAT1U), ppc_read_spr(SPR603E_IBAT1U));
  printf("DBAT1L=%08X\tIBAT1L=%08X\n", ppc_read_spr(SPR603E_DBAT1L), ppc_read_spr(SPR603E_IBAT1L));
  printf("DBAT2U=%08X\tIBAT2U=%08X\n", ppc_read_spr(SPR603E_DBAT2U), ppc_read_spr(SPR603E_IBAT2U));
  printf("DBAT2L=%08X\tIBAT2L=%08X\n", ppc_read_spr(SPR603E_DBAT2L), ppc_read_spr(SPR603E_IBAT2L));
  printf("DBAT3U=%08X\tIBAT3U=%08X\n", ppc_read_spr(SPR603E_DBAT3U), ppc_read_spr(SPR603E_IBAT3U));
  printf("DBAT3L=%08X\tIBAT3L=%08X\n", ppc_read_spr(SPR603E_DBAT3L), ppc_read_spr(SPR603E_IBAT3L));
  for (int i = 0; i < 10; i++)
    printf("SR%d =%08X VSID=%06X\n", i, ppc_read_sr(i), ppc_read_sr(i) & 0x00ffffff);
  for (int i = 10; i < 16; i++)
    printf("SR%d=%08X VSID=%06X\n", i, ppc_read_sr(i), ppc_read_sr(i) & 0x00ffffff);
  printf("SDR1=%08X\n", ppc_read_spr(SPR603E_SDR1));
  printf("\n");
  printf("DBAT0: "); PrintBAT(SPR603E_DBAT0U, SPR603E_DBAT0L); printf("\n");
  printf("DBAT1: "); PrintBAT(SPR603E_DBAT1U, SPR603E_DBAT1L); printf("\n");
  printf("DBAT2: "); PrintBAT(SPR603E_DBAT2U, SPR603E_DBAT2L); printf("\n");
  printf("DBAT3: "); PrintBAT(SPR603E_DBAT3U, SPR603E_DBAT3L); printf("\n");
  printf("IBAT0: "); PrintBAT(SPR603E_IBAT0U, SPR603E_IBAT0L); printf("\n");
  printf("IBAT1: "); PrintBAT(SPR603E_IBAT1U, SPR603E_IBAT1L); printf("\n");
  printf("IBAT2: "); PrintBAT(SPR603E_IBAT2U, SPR603E_IBAT2L); printf("\n");
  printf("IBAT3: "); PrintBAT(SPR603E_IBAT3U, SPR603E_IBAT3L); printf("\n");
  printf("\n");
  /*
  printf("First PTEG:\n");
  uint32_t ptab = ppc_read_spr(SPR603E_SDR1) & 0xffff0000;
  for (int i = 0; i < 65536/8; i++)
  {
    uint64_t pte = bus->Read64(ptab + i*8);
    uint32_t vsid = (pte >> (32 + (31 - 24))) & 0x00ffffff;
    uint32_t rpn = pte & 0xfffff000;
    int wimg = (pte >> 3) & 0xf;
    bool v = pte & 0x8000000000000000ULL;
    printf("  %d: %016llX V=%d VSID=%06X RPN=%08X WIMG=%c%c%c%c\n", i, pte, v, vsid, rpn, (wimg&8)?'W':'-', (wimg&4)?'I':'-', (wimg&2)?'M':'-', (wimg&1)?'G':'-');
  }
  */
}
#endif

static void SaveFrameBuffer(const std::string& file)
{
    std::shared_ptr<uint8_t> pixels(new uint8_t[totalXRes * totalYRes * 4], std::default_delete<uint8_t[]>());
    glReadPixels(0, 0, totalXRes, totalYRes, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
    Util::WriteSurfaceToBMP<Util::RGBA8>(file, pixels.get(), totalXRes, totalYRes, true);
}

void Screenshot()
{
    // Make a screenshot
    time_t now = std::time(nullptr);
    tm* ltm = std::localtime(&now);
    std::string file = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Screenshots)
        << "Screenshot_"
        << std::setfill('0') << std::setw(4) << (1900 + ltm->tm_year)
        << '-'
        << std::setw(2) << (1 + ltm->tm_mon)
        << '-'
        << std::setw(2) << ltm->tm_mday
        << "_("
        << std::setw(2) << ltm->tm_hour
        << '-'
        << std::setw(2) << ltm->tm_min
        << '-'
        << std::setw(2) << ltm->tm_sec
        << ").bmp";

    std::cout << "Screenshot created: " << file << std::endl;
    SaveFrameBuffer(file);
}

/******************************************************************************
 Render State Analysis
******************************************************************************/

#ifdef DEBUG

#include "Model3/Model3GraphicsState.h"
#include "OSD/SDL/PolyAnalysis.h"
#include <fstream>

static std::string s_gfxStatePath;
static const std::string k_gfxAnalysisPath = "GraphicsAnalysis/";

static std::string GetFileBaseName(const std::string &file)
{
  std::string base = file;
  size_t pos = file.find_last_of('/');
  if (pos != std::string::npos)
    base = file.substr(pos + 1);
  pos = file.find_last_of('\\');
  if (pos != std::string::npos)
    base = file.substr(pos + 1);
  return base;
}

static void TestPolygonHeaderBits(IEmulator *Emu)
{
  const static std::vector<uint32_t> unknownPolyBits
  {
    0xffffffff,
    0x000000ab, // actual color
    0x000000fc,
    0x000000c0,
    0x000000a0,
    0xffffff60,
    0xff0300ff  // contour, luminous, etc.
  };

  const std::vector<uint32_t> unknownCullingNodeBits
  {
    0xffffffff,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
  };

  GLint readBuffer;
  glGetIntegerv(GL_READ_BUFFER, &readBuffer);
  glReadBuffer(GL_FRONT);

  // Render separate image for each unknown bit
  s_runtime_config.Set("Debug/ForceFlushModels", true);
  for (int idx = 0; idx < 7; idx++)
  {
    for (int bit = 0; bit < 32; bit++)
    {
      uint32_t mask = 1 << bit;
      s_runtime_config.Set("Debug/HighlightPolyHeaderIdx", idx);
      s_runtime_config.Set("Debug/HighlightPolyHeaderMask", mask);
      if ((unknownPolyBits[idx] & mask))
      {
        Emu->RenderFrame();
        std::string file = Util::Format() << k_gfxAnalysisPath << GetFileBaseName(s_gfxStatePath) << "." << "poly" << "." << idx << "_" << Util::Hex(mask) << ".bmp";
        SaveFrameBuffer(file);
      }
    }
  }

  for (int idx = 0; idx < 10; idx++)
  {
    for (int bit = 0; bit < 32; bit++)
    {
      uint32_t mask = 1 << bit;
      s_runtime_config.Set("Debug/HighlightCullingNodeIdx", idx);
      s_runtime_config.Set("Debug/HighlightCullingNodeMask", mask);
      if ((unknownCullingNodeBits[idx] & mask))
      {
        Emu->RenderFrame();
        std::string file = Util::Format() << k_gfxAnalysisPath << GetFileBaseName(s_gfxStatePath) << "." << "culling" << "." << idx << "_" << Util::Hex(mask) << ".bmp";
        SaveFrameBuffer(file);
      }
    }
  }

  glReadBuffer(readBuffer);

  // Generate the HTML GUI
  std::string file = Util::Format() << k_gfxAnalysisPath << "_" << GetFileBaseName(s_gfxStatePath) << ".html";
  std::ofstream fs(file);
  if (!fs.good())
    ErrorLog("Unable to open '%s' for writing.", file.c_str());
  else
  {
    std::string contents = s_polyAnalysisHTMLPrologue;
    contents += "    var g_file_base_name = '" + GetFileBaseName(s_gfxStatePath) + "';\n";
    contents += "    var g_unknown_poly_bits = [" + std::string(Util::Format(",").Join(unknownPolyBits)) + "];\n";
    contents += "    var g_unknown_culling_bits = [" + std::string(Util::Format(",").Join(unknownCullingNodeBits)) + "];\n";
    contents += s_polyAnalysisHTMLEpilogue;
    fs << contents;
    printf("Produced: %s\n", file.c_str());
  }
}

#endif


/******************************************************************************
 Save States and NVRAM

 Save states and NVRAM use the same basic format. When anything changes that
 breaks compatibility with previous versions of Supermodel, the save state
 and NVRAM version numbers must be incremented as needed.

 Header block name: "Supermodel Save State" or "Supermodel NVRAM State"
 Data: Save state file version (4-byte integer), ROM set ID (up to 9 bytes,
 including terminating \0).

 Different subsystems output their own blocks.
******************************************************************************/

static const int STATE_FILE_VERSION = 3;  // save state file version
static const int NVRAM_FILE_VERSION = 0;  // NVRAM file version
static unsigned s_saveSlot = 0;           // save state slot #

static void SaveState(IEmulator *Model3)
{
  CBlockFile  SaveState;

  std::string file_path = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Saves) << Model3->GetGame().name << ".st" << s_saveSlot;
  if (OKAY != SaveState.Create(file_path, "Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    ErrorLog("Unable to save state to '%s'.", file_path.c_str());
    return;
  }

  // Write file format version and ROM set ID to header block
  int32_t fileVersion = STATE_FILE_VERSION;
  SaveState.Write(&fileVersion, sizeof(fileVersion));
  SaveState.Write(Model3->GetGame().name);

  // Save state
  Model3->SaveState(&SaveState);
  SaveState.Close();
  printf("Saved state to '%s'.\n", file_path.c_str());
  DebugLog("Saved state to '%s'.\n", file_path.c_str());
}

static void LoadState(IEmulator *Model3, std::string file_path = std::string())
{
  CBlockFile  SaveState;

  // Generate file path
  if (file_path.empty())
    file_path = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Saves) << Model3->GetGame().name << ".st" << s_saveSlot;

  // Open and check to make sure format is correct
  if (OKAY != SaveState.Load(file_path))
  {
    ErrorLog("Unable to load state from '%s'.", file_path.c_str());
    return;
  }

  if (OKAY != SaveState.FindBlock("Supermodel Save State"))
  {
    ErrorLog("'%s' does not appear to be a valid save state file.", file_path.c_str());
    return;
  }

  int32_t fileVersion;
  SaveState.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != STATE_FILE_VERSION)
  {
    ErrorLog("'%s' is incompatible with this version of Supermodel.", file_path.c_str());
    return;
  }

  // Load
  Model3->LoadState(&SaveState);
  SaveState.Close();
  printf("Loaded state from '%s'.\n", file_path.c_str());
  DebugLog("Loaded state from '%s'.\n", file_path.c_str());
}

static void SaveNVRAM(IEmulator *Model3)
{
  CBlockFile  NVRAM;

  std::string file_path = Util::Format() << FileSystemPath::GetPath(FileSystemPath::NVRAM) << Model3->GetGame().name << ".nv";
  if (OKAY != NVRAM.Create(file_path, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    ErrorLog("Unable to save NVRAM to '%s'. Make sure directory exists!", file_path.c_str());
    return;
  }

  // Write file format version and ROM set ID to header block
  int32_t fileVersion = NVRAM_FILE_VERSION;
  NVRAM.Write(&fileVersion, sizeof(fileVersion));
  NVRAM.Write(Model3->GetGame().name);

  // Save NVRAM
  Model3->SaveNVRAM(&NVRAM);
  NVRAM.Close();
  DebugLog("Saved NVRAM to '%s'.\n", file_path.c_str());
}

static void LoadNVRAM(IEmulator *Model3)
{
  CBlockFile  NVRAM;

  // Generate file path
  std::string file_path = Util::Format() << FileSystemPath::GetPath(FileSystemPath::NVRAM) << Model3->GetGame().name << ".nv";

  // Open and check to make sure format is correct
  if (OKAY != NVRAM.Load(file_path))
  {
    //ErrorLog("Unable to restore NVRAM from '%s'.", filePath);
    return;
  }

  if (OKAY != NVRAM.FindBlock("Supermodel NVRAM State"))
  {
    ErrorLog("'%s' does not appear to be a valid NVRAM file.", file_path.c_str());
    return;
  }

  int32_t fileVersion;
  NVRAM.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != NVRAM_FILE_VERSION)
  {
    ErrorLog("'%s' is incompatible with this version of Supermodel.", file_path.c_str());
    return;
  }

  // Load
  Model3->LoadNVRAM(&NVRAM);
  NVRAM.Close();
  DebugLog("Loaded NVRAM from '%s'.\n", file_path.c_str());
}


/*
static void PrintGLError(GLenum error)
{
  switch (error)
  {
  case GL_INVALID_ENUM:       printf("invalid enum\n"); break;
  case GL_INVALID_VALUE:      printf("invalid value\n"); break;
  case GL_INVALID_OPERATION:  printf("invalid operation\n"); break;
  case GL_STACK_OVERFLOW:     printf("stack overflow\n"); break;
  case GL_STACK_UNDERFLOW:    printf("stack underflow\n"); break;
  case GL_OUT_OF_MEMORY:      printf("out of memory\n"); break;
  case GL_TABLE_TOO_LARGE:    printf("table too large\n"); break;
  case GL_NO_ERROR:           break;
  default:                    printf("unknown error\n"); break;
  }
}
*/

/******************************************************************************
 Video Callbacks
******************************************************************************/

static CInputs *videoInputs = NULL;
static uint32_t currentInputs = 0;

bool BeginFrameVideo()
{
  return true;
}

void EndFrameVideo()
{
  // Show crosshairs for light gun games
  if (videoInputs)
    s_crosshair->Update(currentInputs, videoInputs, xOffset, yOffset, xRes, yRes);

  // Swap the buffers
  SDL_GL_SwapWindow(s_window);
}


/******************************************************************************
 Frame Timing
******************************************************************************/

static uint64_t s_perfCounterFrequency = 0;

static uint64_t GetDesiredRefreshRateMilliHz()
{
  // The refresh rate is expressed as mHz (millihertz -- Hz * 1000) in order to
  // be expressable as an integer. E.g.: 57.524 Hz -> 57524 mHz.
  float refreshRateHz = std::abs(s_runtime_config["RefreshRate"].ValueAs<float>());
  uint64_t refreshRateMilliHz = uint64_t(1000.0f * refreshRateHz);
  return refreshRateMilliHz;
}

static void SuperSleepUntil(uint64_t target)
{
  uint64_t time = SDL_GetPerformanceCounter();

  // If we're ahead of the target, we're done
  if (time > target)
  {
    return;
  }

  // Compute the whole number of millis to sleep. Because OS sleep is not accurate,
  // we actually sleep for one less and will spin-wait for the final millisecond.
  int32_t numWholeMillisToSleep = int32_t((target - time) * 1000 / s_perfCounterFrequency);
  numWholeMillisToSleep -= 1;
  if (numWholeMillisToSleep > 0)
  {
    SDL_Delay(numWholeMillisToSleep);
  }

  // Spin until requested time
  volatile uint64_t now;
  int32_t remain;
  do
  {
    now = SDL_GetPerformanceCounter();
    remain = int32_t((target - now));
  } while (remain>0);
}


/******************************************************************************
 Main Program Loop
******************************************************************************/

#ifdef SUPERMODEL_DEBUGGER
int Supermodel(const Game &game, ROMSet *rom_set, IEmulator *Model3, CInputs *Inputs, COutputs *Outputs, std::shared_ptr<Debugger::CDebugger> Debugger)
{
  std::shared_ptr<CLogger> oldLogger;
#else
int Supermodel(const Game &game, ROMSet *rom_set, IEmulator *Model3, CInputs *Inputs, COutputs *Outputs)
{
#endif // SUPERMODEL_DEBUGGER
  std::string initialState = s_runtime_config["InitStateFile"].ValueAs<std::string>();
  uint64_t    prevFPSTicks;
  unsigned    fpsFramesElapsed;
  bool        gameHasLightguns = false;
  bool        quit = false;
  bool        paused = false;
  bool        dumpTimings = false;

  // Initialize and load ROMs
  if (OKAY != Model3->Init())
    return 1;
  if (Model3->LoadGame(game, *rom_set))
    return 1;
  *rom_set = ROMSet();  // free up this memory we won't need anymore

  // Load NVRAM
  LoadNVRAM(Model3);

  // Set the video mode
  char baseTitleStr[128];
  char titleStr[128];
  totalXRes = xRes = s_runtime_config["XResolution"].ValueAs<unsigned>();
  totalYRes = yRes = s_runtime_config["YResolution"].ValueAs<unsigned>();
  sprintf(baseTitleStr, "Supermodel - %s", game.title.c_str());
  SDL_SetWindowTitle(s_window, baseTitleStr);
  SDL_SetWindowSize(s_window, totalXRes, totalYRes);

  int xpos = s_runtime_config["WindowXPosition"].Exists() ? s_runtime_config["WindowXPosition"].ValueAs<int>() : SDL_WINDOWPOS_CENTERED;
  int ypos = s_runtime_config["WindowYPosition"].Exists() ? s_runtime_config["WindowYPosition"].ValueAs<int>() : SDL_WINDOWPOS_CENTERED;
  SDL_SetWindowPosition(s_window, xpos, ypos);

  if (s_runtime_config["BorderlessWindow"].ValueAs<bool>())
  {
    SDL_SetWindowBordered(s_window, SDL_FALSE);
  }

  SetFullScreenRefreshRate();

  bool stretch = s_runtime_config["Stretch"].ValueAs<bool>();
  bool fullscreen = s_runtime_config["FullScreen"].ValueAs<bool>();
  if (OKAY != ResizeGLScreen(&xOffset, &yOffset ,&xRes, &yRes, &totalXRes, &totalYRes, !stretch, fullscreen))
    return 1;

  // Info log GL information
  PrintGLInfo(false, true, false);

  // Initialize audio system
  SetAudioType(game.audio);
  if (OKAY != OpenAudio(s_runtime_config))
    return 1;

  // Hide mouse if fullscreen, enable crosshairs for gun games
  Inputs->GetInputSystem()->SetMouseVisibility(!s_runtime_config["FullScreen"].ValueAs<bool>());
  gameHasLightguns = !!(game.inputs & (Game::INPUT_GUN1|Game::INPUT_GUN2));
  gameHasLightguns |= game.name == "lostwsga";
  currentInputs = game.inputs;
  if (gameHasLightguns)
    videoInputs = Inputs;
  else
    videoInputs = NULL;

  // Attach the inputs to the emulator
  Model3->AttachInputs(Inputs);

  // Attach the outputs to the emulator
  if (Outputs != NULL)
    Model3->AttachOutputs(Outputs);

  // Frame timing
  s_perfCounterFrequency = SDL_GetPerformanceFrequency();
  uint64_t perfCountPerFrame = s_perfCounterFrequency * 1000 / GetDesiredRefreshRateMilliHz();
  uint64_t nextTime = 0;

  // Initialize the renderers
  SuperAA* superAA = new SuperAA(aaValue);
  superAA->Init(totalXRes, totalYRes);  // pass actual frame sizes here
  CRender2D *Render2D = new CRender2D(s_runtime_config);
  IRender3D *Render3D = s_runtime_config["New3DEngine"].ValueAs<bool>() ? ((IRender3D *) new New3D::CNew3D(s_runtime_config, Model3->GetGame().name)) : ((IRender3D *) new Legacy3D::CLegacy3D(s_runtime_config));

  if (OKAY != Render2D->Init(xOffset*aaValue, yOffset*aaValue, xRes*aaValue, yRes*aaValue, totalXRes*aaValue, totalYRes*aaValue, superAA->GetTargetID()))
    goto QuitError;
  if (OKAY != Render3D->Init(xOffset*aaValue, yOffset*aaValue, xRes*aaValue, yRes*aaValue, totalXRes*aaValue, totalYRes*aaValue, superAA->GetTargetID()))
    goto QuitError;
 
  Model3->AttachRenderers(Render2D,Render3D, superAA);

  // Reset emulator
  Model3->Reset();

  // Load initial save state if requested
  if (initialState.length() > 0)
    LoadState(Model3, initialState);

#ifdef SUPERMODEL_DEBUGGER
  // If debugger was supplied, set it as logger and attach it to system
  oldLogger = GetLogger();
  if (Debugger != NULL)
  {
    SetLogger(Debugger);
    Debugger->Attach();
  }
#endif // SUPERMODEL_DEBUGGER

  // Emulate!
  fpsFramesElapsed = 0;
  prevFPSTicks = SDL_GetPerformanceCounter();
  quit = false;
  paused = false;
  dumpTimings = false;
#ifdef DEBUG
  if (dynamic_cast<CModel3GraphicsState *>(Model3))
  {
    TestPolygonHeaderBits(Model3);
    quit = true;
  }
#endif
  while (!quit)
  {
    // Render if paused, otherwise run a frame
    if (paused)
      Model3->RenderFrame();
    else
      Model3->RunFrame();

    // Poll the inputs
    if (!Inputs->Poll(&game, xOffset, yOffset, xRes, yRes))
      quit = true;

#ifdef SUPERMODEL_DEBUGGER
    bool processUI = true;
    if (Debugger != NULL)
    {
      Debugger->Poll();

      // Check if debugger requests exit or pause
      if (Debugger->CheckExit())
      {
        quit = true;
        processUI = false;
      }
      else if (Debugger->CheckPause())
      {
        paused = true;
        processUI = false;
      }
    }
    if (processUI)
    {
#endif // SUPERMODEL_DEBUGGER

    // Check UI controls
    if (Inputs->uiExit->Pressed())
    {
      // Quit emulator
      quit = true;
    }
    else if (Inputs->uiReset->Pressed())
    {
      if (!paused)
      {
        Model3->PauseThreads();
        SetAudioEnabled(false);
      }

      // Reset emulator
      Model3->Reset();

#ifdef SUPERMODEL_DEBUGGER
      // If debugger was supplied, reset it too
      if (Debugger != NULL)
        Debugger->Reset();
#endif // SUPERMODEL_DEBUGGER

      if (!paused)
      {
        Model3->ResumeThreads();
        SetAudioEnabled(true);
      }

      puts("Model 3 reset.");
    }
    else if (Inputs->uiPause->Pressed())
    {
      // Toggle emulator paused flag
      paused = !paused;

      if (paused)
      {
        Model3->PauseThreads();
        SetAudioEnabled(false);
        sprintf(titleStr, "%s (Paused)", baseTitleStr);
        SDL_SetWindowTitle(s_window, titleStr);
      }
      else
      {
        Model3->ResumeThreads();
        SetAudioEnabled(true);
        SDL_SetWindowTitle(s_window, baseTitleStr);
      }

      // Send paused value as output
      if (Outputs != NULL)
        Outputs->SetValue(OutputPause, paused);
    }
    else if (Inputs->uiFullScreen->Pressed())
    {
      // Toggle emulator fullscreen
      s_runtime_config.Get("FullScreen").SetValue(!s_runtime_config["FullScreen"].ValueAs<bool>());

      // Delete renderers and recreate them afterwards since GL context will most likely be lost when switching from/to fullscreen
      delete Render2D;
      delete Render3D;
      Render2D = nullptr;
      Render3D = nullptr;

      // Resize screen
      totalXRes = xRes = s_runtime_config["XResolution"].ValueAs<unsigned>();
      totalYRes = yRes = s_runtime_config["YResolution"].ValueAs<unsigned>();
      bool stretch = s_runtime_config["Stretch"].ValueAs<bool>();
      bool fullscreen = s_runtime_config["FullScreen"].ValueAs<bool>();
      if (OKAY != ResizeGLScreen(&xOffset,&yOffset,&xRes,&yRes,&totalXRes,&totalYRes,!stretch,fullscreen))
        goto QuitError;

      // Recreate renderers and attach to the emulator
      superAA->Init(totalXRes, totalYRes);
      Render2D = new CRender2D(s_runtime_config);
      Render3D = s_runtime_config["New3DEngine"].ValueAs<bool>() ? ((IRender3D *) new New3D::CNew3D(s_runtime_config, Model3->GetGame().name)) : ((IRender3D *) new Legacy3D::CLegacy3D(s_runtime_config));
      if (OKAY != Render2D->Init(xOffset * aaValue, yOffset * aaValue, xRes * aaValue, yRes * aaValue, totalXRes * aaValue, totalYRes * aaValue, superAA->GetTargetID()))
        goto QuitError;
      if (OKAY != Render3D->Init(xOffset * aaValue, yOffset * aaValue, xRes * aaValue, yRes * aaValue, totalXRes * aaValue, totalYRes * aaValue, superAA->GetTargetID()))
        goto QuitError;

      Model3->AttachRenderers(Render2D, Render3D, superAA);

      Render3D->UploadTextures(0, 0, 0, 2048, 2048);    // sync texture memory

      Inputs->GetInputSystem()->SetMouseVisibility(!s_runtime_config["FullScreen"].ValueAs<bool>());
    }
    else if (Inputs->uiSaveState->Pressed())
    {
      if (!paused)
      {
        Model3->PauseThreads();
        SetAudioEnabled(false);
      }

      // Save game state
      SaveState(Model3);

      if (!paused)
      {
        Model3->ResumeThreads();
        SetAudioEnabled(true);
      }
    }
    else if (Inputs->uiChangeSlot->Pressed())
    {
      // Change save slot
      ++s_saveSlot;
      s_saveSlot %= 10; // clamp to [0,9]
      printf("Save slot: %d\n", s_saveSlot);
    }
    else if (Inputs->uiLoadState->Pressed())
    {
      if (!paused)
      {
        Model3->PauseThreads();
        SetAudioEnabled(false);
      }

      // Load game state
      LoadState(Model3);

#ifdef SUPERMODEL_DEBUGGER
      // If debugger was supplied, reset it after loading state
      if (Debugger != NULL)
        Debugger->Reset();
#endif // SUPERMODEL_DEBUGGER

      if (!paused)
      {
        Model3->ResumeThreads();
        SetAudioEnabled(true);
      }
    }
    else if (Inputs->uiMusicVolUp->Pressed())
    {
      // Increase music volume by 10%
      if (!Model3->GetGame().mpeg_board.empty())
      {
        int vol = (std::min)(200, s_runtime_config["MusicVolume"].ValueAs<int>() + 10);
        s_runtime_config.Get("MusicVolume").SetValue(vol);
        printf("Music volume: %d%%", vol);
        if (200 == vol)
          puts(" (maximum)");
        else
          printf("\n");
      }
      else
        puts("This game does not have an MPEG music board.");
    }
    else if (Inputs->uiMusicVolDown->Pressed())
    {
      // Decrease music volume by 10%
      if (!Model3->GetGame().mpeg_board.empty())
      {
        int vol = (std::max)(0, s_runtime_config["MusicVolume"].ValueAs<int>() - 10);
        s_runtime_config.Get("MusicVolume").SetValue(vol);
        printf("Music volume: %d%%", vol);
        if (0 == vol)
          puts(" (muted)");
        else
          printf("\n");
      }
      else
        puts("This game does not have an MPEG music board.");
    }
    else if (Inputs->uiSoundVolUp->Pressed())
    {
      // Increase sound volume by 10%
    int vol = (std::min)(200, s_runtime_config["SoundVolume"].ValueAs<int>() + 10);
      s_runtime_config.Get("SoundVolume").SetValue(vol);
      printf("Sound volume: %d%%", vol);
      if (200 == vol)
        puts(" (maximum)");
      else
        printf("\n");
    }
    else if (Inputs->uiSoundVolDown->Pressed())
    {
      // Decrease sound volume by 10%
      int vol = (std::max)(0, s_runtime_config["SoundVolume"].ValueAs<int>() - 10);
      s_runtime_config.Get("SoundVolume").SetValue(vol);
      printf("Sound volume: %d%%", vol);
      if (0 == vol)
        puts(" (muted)");
      else
        printf("\n");
    }
#ifdef SUPERMODEL_DEBUGGER
    else if (Inputs->uiDumpInpState->Pressed())
    {
      // Dump input states
      Inputs->DumpState(&game);
    }
    else if (Inputs->uiDumpTimings->Pressed())
    {
      dumpTimings = !dumpTimings;
    }
#endif
    else if (Inputs->uiSelectCrosshairs->Pressed() && gameHasLightguns)
    {
      int crosshairs = (s_runtime_config["Crosshairs"].ValueAs<unsigned>() + 1) & 3;
      s_runtime_config.Get("Crosshairs").SetValue(crosshairs);
      switch (crosshairs)
      {
      case 0: puts("Crosshairs disabled.");             break;
      case 3: puts("Crosshairs enabled.");              break;
      case 1: puts("Showing Player 1 crosshair only."); break;
      case 2: puts("Showing Player 2 crosshair only."); break;
      }
    }
    else if (Inputs->uiClearNVRAM->Pressed())
    {
      // Clear NVRAM
      Model3->ClearNVRAM();
      puts("NVRAM cleared.");
    }
    else if (Inputs->uiToggleFrLimit->Pressed())
    {
      // Toggle frame limiting
      s_runtime_config.Get("Throttle").SetValue(!s_runtime_config["Throttle"].ValueAs<bool>());
      printf("Frame limiting: %s\n", s_runtime_config["Throttle"].ValueAs<bool>() ? "On" : "Off");
    }
    else if (Inputs->uiScreenshot->Pressed())
    {
      // Make a screenshot
      Screenshot();
    }
#ifdef SUPERMODEL_DEBUGGER
      else if (Debugger != NULL && Inputs->uiEnterDebugger->Pressed())
      {
        // Break execution and enter debugger
        Debugger->ForceBreak(true);
      }
    }
#endif // SUPERMODEL_DEBUGGER

    // Refresh rate (frame limiting)
    if (paused || s_runtime_config["Throttle"].ValueAs<bool>())
    {
        SuperSleepUntil(nextTime);
        nextTime = SDL_GetPerformanceCounter() + perfCountPerFrame;
    }

    // Measure frame rate
    uint64_t currentFPSTicks = SDL_GetPerformanceCounter();
    if (s_runtime_config["ShowFrameRate"].ValueAs<bool>())
    {
      fpsFramesElapsed += 1;
      uint64_t measurementTicks = currentFPSTicks - prevFPSTicks;
      if (measurementTicks >= s_perfCounterFrequency) // update FPS every 1 second (s_perfCounterFrequency is how many perf ticks in one second)
      {
        float fps = float(fpsFramesElapsed) / (float(measurementTicks) / float(s_perfCounterFrequency));
        sprintf(titleStr, "%s - %1.3f FPS%s", baseTitleStr, fps, paused ? " (Paused)" : "");
        SDL_SetWindowTitle(s_window, titleStr);
        prevFPSTicks = currentFPSTicks;   // reset tick count
        fpsFramesElapsed = 0;             // reset frame count
      }
    }

    if (dumpTimings && !paused)
    {
      CModel3 *M = dynamic_cast<CModel3 *>(Model3);
      if (M)
        M->DumpTimings();
    }
  }

  // Make sure all threads are paused before shutting down
  Model3->PauseThreads();

#ifdef SUPERMODEL_DEBUGGER
  // If debugger was supplied, detach it from system and restore old logger
  if (Debugger != NULL)
  {
    Debugger->Detach();
    SetLogger(oldLogger);
  }
#endif // SUPERMODEL_DEBUGGER

  // Save NVRAM
  SaveNVRAM(Model3);

  // Close audio
  CloseAudio();

  // Shut down renderers
  delete Render2D;
  delete Render3D;
  delete superAA;

  return 0;

  // Quit with an error
QuitError:
  delete Render2D;
  delete Render3D;
  delete superAA;

  return 1;
}


/******************************************************************************
 Entry Point and Command Line Procesing
******************************************************************************/

static const std::string s_analysisPath = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Analysis);
static const std::string s_configFilePath = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Config) << "Supermodel.ini";
static const std::string s_gameXMLFilePath = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Config) << "Games.xml";
static const std::string s_logFilePath = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Log) << "Supermodel.log";

// Create and configure inputs
static bool ConfigureInputs(CInputs *Inputs, Util::Config::Node *fileConfig, Util::Config::Node *runtimeConfig, const Game &game, bool configure)
{
  static const char configFileComment[] = {
    ";\n"
    "; Supermodel Configuration File\n"
    ";\n"
  };

  Inputs->LoadFromConfig(*runtimeConfig);

  // If the user wants to configure the inputs, do that now
  if (configure)
  {
    std::string title("Supermodel - ");
    if (game.name.empty())
      title.append("Configuring Default Inputs...");
    else
      title.append(Util::Format() << "Configuring Inputs for: " << game.title);
    SDL_SetWindowTitle(s_window, title.c_str());

    // Extract the relevant INI section (which will be the global section if no
    // game was specified, otherwise the game's node) in the file config, which
    // will be written back to disk
    Util::Config::Node *fileConfigRoot = game.name.empty() ? fileConfig : fileConfig->TryGet(game.name);
    if (fileConfigRoot == nullptr)
    {
      fileConfigRoot = &fileConfig->Add(game.name);
    }

    // Configure the inputs
    if (Inputs->ConfigureInputs(game, xOffset, yOffset, xRes, yRes))
    {
      // Write input configuration and input system settings to config file
      Inputs->StoreToConfig(fileConfigRoot);
      Util::Config::WriteINIFile(s_configFilePath, *fileConfig, configFileComment);

      // Also save to runtime configuration in case we proceed and play
      Inputs->StoreToConfig(runtimeConfig);
    }
    else
      puts("Configuration aborted...");
    puts("");
  }

  return OKAY;
}

// Print game list
static void PrintGameList(const std::string &xml_file, const std::map<std::string, Game> &games)
{
  if (games.empty())
  {
    puts("No games defined.");
    return;
  }
  printf("Games defined in %s:\n", xml_file.c_str());
  puts("");
  puts("    ROM Set         Title");
  puts("    -------         -----");
  for (auto &v: games)
  {
    const Game &game = v.second;
    printf("    %s", game.name.c_str());
    for (size_t i = game.name.length(); i < 9; i++)  // pad for alignment (no game ID should be more than 9 letters)
      printf(" ");
    if (!game.version.empty())
      printf("       %s (%s)\n", game.title.c_str(), game.version.c_str());
    else
      printf("       %s\n", game.title.c_str());
  }
}

static void LogConfig(const Util::Config::Node &config)
{
  InfoLog("Runtime configuration:");
  for (auto &child: config)
  {
    if (child.Empty())
      InfoLog("  %s=<empty>", child.Key().c_str());
    else
      InfoLog("  %s=%s", child.Key().c_str(), child.ValueAs<std::string>().c_str());
  }
  InfoLog("");
}

static Util::Config::Node DefaultConfig()
{
  Util::Config::Node config("Global");
  config.Set("GameXMLFile", s_gameXMLFilePath);
  config.Set("InitStateFile", "");
  // CModel3
  config.Set("MultiThreaded", true);
  config.Set("GPUMultiThreaded", true);
  // 2D and 3D graphics engines
  config.Set("MultiTexture", false);
  config.Set("VertexShader", "");
  config.Set("FragmentShader", "");
  config.Set("VertexShaderFog", "");
  config.Set("FragmentShaderFog", "");
  config.Set("VertexShader2D", "");
  config.Set("FragmentShader2D", "");
  // CSoundBoard
  config.Set("EmulateSound", true);
  config.Set("Balance", "0.0");
  config.Set("BalanceLeftRight", "0.0");
  config.Set("BalanceFrontRear", "0.0");
  config.Set("NbSoundChannels", "4");
  config.Set("SoundFreq", "57.6"); // 60.0f? 57.524160f?
  // CDSB
  config.Set("EmulateDSB", true);
  config.Set("SoundVolume", "100");
  config.Set("MusicVolume", "100");
  // Other sound options
  config.Set("LegacySoundDSP", false); // New config option for games that do not play correctly with MAME's SCSP sound core.
  // CDriveBoard
  config.Set("ForceFeedback", false);
  // Platform-specific/UI
  config.Set("New3DEngine", true);
  config.Set("QuadRendering", false);
  config.Set("XResolution", "496");
  config.Set("YResolution", "384");
  config.SetEmpty("WindowXPosition");
  config.SetEmpty("WindowYPosition");
  config.Set("FullScreen", false);
  config.Set("BorderlessWindow", false);
  config.Set("Supersampling", 1);
  config.Set("WideScreen", false);
  config.Set("Stretch", false);
  config.Set("WideBackground", false);
  config.Set("VSync", true);
  config.Set("Throttle", true);
  config.Set("RefreshRate", 60.0f);
  config.Set("ShowFrameRate", false);
  config.Set("Crosshairs", int(0));
  config.Set("CrosshairStyle", "vector");
  config.Set("FlipStereo", false);
#ifdef SUPERMODEL_WIN32
  config.Set("InputSystem", "dinput");
  // DirectInput ForceFeedback
  config.Set("DirectInputConstForceLeftMax", "100");
  config.Set("DirectInputConstForceRightMax", "100");
  config.Set("DirectInputSelfCenterMax", "100");
  config.Set("DirectInputFrictionMax", "100");
  config.Set("DirectInputVibrateMax", "100");
  // XInput ForceFeedback
  config.Set("XInputConstForceThreshold", "30");
  config.Set("XInputConstForceMax", "100");
  config.Set("XInputVibrateMax", "100");
  config.Set("XInputStereoVibration", true);
  // SDL ForceFeedback
  config.Set("SDLConstForceMax", "100");
  config.Set("SDLSelfCenterMax", "100");
  config.Set("SDLFrictionMax", "100");
  config.Set("SDLVibrateMax", "100");
  config.Set("SDLConstForceThreshold", "30");
#ifdef NET_BOARD
  // NetBoard
  config.Set("Network", false);
  config.Set("SimulateNet", true);
  config.Set("PortIn", unsigned(1970));
  config.Set("PortOut", unsigned(1971));
  config.Set("AddressOut", "127.0.0.1");
#endif
#else
  config.Set("InputSystem", "sdl");
  // SDL ForceFeedback
  config.Set("SDLConstForceMax", "100");
  config.Set("SDLSelfCenterMax", "100");
  config.Set("SDLFrictionMax", "100");
  config.Set("SDLVibrateMax", "100");
  config.Set("SDLConstForceThreshold", "30");
#endif
  config.Set("Outputs", "none");
  config.Set("DumpTextures", false);
  return config;
}

static void Title(void)
{
  puts("Supermodel: A Sega Model 3 Arcade Emulator (Version " SUPERMODEL_VERSION ")");
  puts("Copyright 2003-2023 by The Supermodel Team");
}

static void Help(void)
{
  Util::Config::Node defaultConfig = DefaultConfig();
  puts("Usage: Supermodel <romset> [options]");
  puts("ROM set must be a valid ZIP file containing a single game.");
  puts("");
  puts("General Options:");
  puts("  -?, -h, -help, --help   Print this help text");
  puts("  -print-games            List supported games and quit");
  printf("  -game-xml-file=<file>   ROM set definition file [Default: %s]\n", s_gameXMLFilePath.c_str());
  printf("  -log-output=<outputs>   Log output destination(s) [Default: %s]\n", s_logFilePath.c_str());
  puts("  -log-level=<level>      Logging threshold [Default: info]");
  puts("");
  puts("Core Options:");
  puts("  -ppc-frequency=<mhz>    PowerPC frequency (default varies by stepping)");
  puts("  -no-threads             Disable multi-threading entirely");
  puts("  -gpu-multi-threaded     Run graphics rendering in separate thread [Default]");
  puts("  -no-gpu-thread          Run graphics rendering in main thread");
  puts("  -load-state=<file>      Load save state after starting");
  puts("");
  puts("Video Options:");
  puts("  -res=<x>,<y>            Resolution [Default: 496,384]");
  puts("  -ss=<n>                 Supersampling (range 1-8)");
  puts("  -window-pos=<x>,<y>     Window position [Default: centered]");
  puts("  -window                 Windowed mode [Default]");
  puts("  -borderless             Windowed mode with no border");
  puts("  -fullscreen             Full screen mode");
  puts("  -wide-screen            Expand 3D field of view to screen width");
  puts("  -wide-bg                When wide-screen mode is enabled, also expand the 2D");
  puts("                          background layer to screen width");
  puts("  -stretch                Fit viewport to resolution, ignoring aspect ratio");
  puts("  -no-throttle            Disable frame rate lock");
  puts("  -vsync                  Lock to vertical refresh rate [Default]");
  puts("  -no-vsync               Do not lock to vertical refresh rate");
  puts("  -true-hz                Use true Model 3 refresh rate of 57.524 Hz");
  puts("  -show-fps               Display frame rate in window title bar");
  puts("  -crosshairs=<n>         Crosshairs configuration for gun games:");
  puts("                          0=none [Default], 1=P1 only, 2=P2 only, 3=P1 & P2");
  puts("  -crosshair-style=<s>    Crosshair style: vector or bmp. [Default: vector]");
  puts("  -new3d                  New 3D engine by Ian Curtis [Default]");
  puts("  -quad-rendering         Enable proper quad rendering");
  puts("  -legacy3d               Legacy 3D engine (faster but less accurate)");
  puts("  -multi-texture          Use 8 texture maps for decoding (legacy engine)");
  puts("  -no-multi-texture       Decode to single texture (legacy engine) [Default]");
  puts("  -vert-shader=<file>     Load Real3D vertex shader for 3D rendering");
  puts("  -frag-shader=<file>     Load Real3D fragment shader for 3D rendering");
  puts("  -vert-shader-fog=<file> Load Real3D scroll fog vertex shader (new engine)");
  puts("  -frag-shader-fog=<file> Load Real3D scroll fog fragment shader (new engine)");
  puts("  -vert-shader-2d=<file>  Load tile map vertex shader");
  puts("  -frag-shader-2d=<file>  Load tile map fragment shader");
  puts("  -print-gl-info          Print OpenGL driver information and quit");
  puts("");
  puts("Audio Options:");
  puts("  -sound-volume=<vol>     Volume of SCSP-generated sound in %, applies only");
  puts("                          when Digital Sound Board is present [Default: 100]");
  puts("  -music-volume=<vol>     Digital Sound Board volume in % [Default: 100]");
  puts("  -balance=<bal>          Relative front/rear balance in % [Default: 0]");
  puts("  -channels=<c>           Number of sound channels to use on host [Default: 4]");
  puts("  -flip-stereo            Swap left and right audio channels");
  puts("  -no-sound               Disable sound board emulation (sound effects)");
  puts("  -no-dsb                 Disable Digital Sound Board (MPEG music)");
  puts("  -new-scsp               New SCSP engine based on MAME [Default]");
  puts("  -legacy-scsp            Legacy SCSP engine by ElSemi");
  puts("");
#ifdef NET_BOARD
  puts("Net Options:");
  puts("  -no-net                 Disable net board [Default]");
  puts("  -net                    Enable net board");
  puts("  -simulate-netboard      Simulate the net board [Default]");
  puts("  -emulate-netboard       Emulate the net board (requires -no-threads)");
  puts("");
#endif
  puts("Input Options:");
  puts("  -force-feedback         Enable force feedback (DirectInput, XInput)");
  puts("  -config-inputs          Configure keyboards, mice, and game controllers");
#ifdef SUPERMODEL_WIN32
  printf("  -input-system=<s>       Input system [Default: %s]\n", defaultConfig["InputSystem"].ValueAs<std::string>().c_str());
  printf("  -outputs=<s>            Outputs [Default: %s]\n", defaultConfig["Outputs"].ValueAs<std::string>().c_str());
#endif
  puts("  -print-inputs           Prints current input configuration");
  puts("");
  puts("Debug Options:");
  puts("  -dump-textures          Write textures to bitmap image files on exit");
#ifdef SUPERMODEL_DEBUGGER
  puts("  -disable-debugger       Completely disable debugger functionality");
  puts("  -enter-debugger         Enter debugger at start of emulation");
#endif // SUPERMODEL_DEBUGGER
#ifdef DEBUG
  puts("  -gfx-state=<file>       Produce graphics analysis for save state (works only");
  puts("                          with the legacy 3D engine and requires a");
  puts("                          GraphicsAnalysis directory to exist)");
#endif
  puts("");
}

struct ParsedCommandLine
{
  Util::Config::Node config = Util::Config::Node("CommandLine");
  std::vector<std::string> rom_files;
  bool error = false;
  bool print_help = false;
  bool print_games = false;
  bool print_gl_info = false;
  bool config_inputs = false;
  bool print_inputs = false;
  bool disable_debugger = false;
  bool enter_debugger = false;
#ifdef DEBUG
  std::string gfx_state;
#endif

  ParsedCommandLine()
  {
    // Logging is special: it is only parsed from the command line and
    // therefore, defaults are needed early
    config.Set("LogOutput", s_logFilePath.c_str());
    config.Set("LogLevel", "info");
  }
};

static ParsedCommandLine ParseCommandLine(int argc, char **argv)
{
  ParsedCommandLine cmd_line;
  const std::map<std::string, std::string> valued_options
  { // -option=value
    { "-game-xml-file",         "GameXMLFile"             },
    { "-load-state",            "InitStateFile"           },
    { "-ppc-frequency",         "PowerPCFrequency"        },
    { "-crosshairs",            "Crosshairs"              },
    { "-crosshair-style",       "CrosshairStyle"          },
    { "-vert-shader",           "VertexShader"            },
    { "-frag-shader",           "FragmentShader"          },
    { "-vert-shader-fog",       "VertexShaderFog"         },
    { "-frag-shader-fog",       "FragmentShaderFog"       },
    { "-vert-shader-2d",        "VertexShader2D"          },
    { "-frag-shader-2d",        "FragmentShader2D"        },
    { "-sound-volume",          "SoundVolume"             },
    { "-music-volume",          "MusicVolume"             },
    { "-balance",               "Balance"                 },
    { "-channels", 	            "NbSoundChannels"         },
    { "-soundfreq",             "SoundFreq"               },
    { "-input-system",          "InputSystem"             },
    { "-outputs",               "Outputs"                 },
    { "-log-output",            "LogOutput"               },
    { "-log-level",             "LogLevel"                }
  };
  const std::map<std::string, std::pair<std::string, bool>> bool_options
  { // -option
    { "-threads",             { "MultiThreaded",    true } },
    { "-no-threads",          { "MultiThreaded",    false } },
    { "-gpu-multi-threaded",  { "GPUMultiThreaded", true } },
    { "-no-gpu-thread",       { "GPUMultiThreaded", false } },
    { "-window",              { "FullScreen",       false } },
    { "-fullscreen",          { "FullScreen",       true } },
    { "-borderless",          { "BorderlessWindow", true } },
    { "-no-wide-screen",      { "WideScreen",       false } },
    { "-wide-screen",         { "WideScreen",       true } },
    { "-stretch",             { "Stretch",          true } },
    { "-no-stretch",          { "Stretch",          false } },
    { "-wide-bg",             { "WideBackground",   true } },
    { "-no-wide-bg",          { "WideBackground",   false } },
    { "-no-multi-texture",    { "MultiTexture",     false } },
    { "-multi-texture",       { "MultiTexture",     true } },
    { "-throttle",            { "Throttle",         true } },
    { "-no-throttle",         { "Throttle",         false } },
    { "-vsync",               { "VSync",            true } },
    { "-no-vsync",            { "VSync",            false } },
    { "-show-fps",            { "ShowFrameRate",    true } },
    { "-no-fps",              { "ShowFrameRate",    false } },
    { "-new3d",               { "New3DEngine",      true } },
    { "-quad-rendering",      { "QuadRendering",    true } },
    { "-legacy3d",            { "New3DEngine",      false } },
    { "-no-flip-stereo",      { "FlipStereo",       false } },
    { "-flip-stereo",         { "FlipStereo",       true } },
    { "-sound",               { "EmulateSound",     true } },
    { "-no-sound",            { "EmulateSound",     false } },
    { "-dsb",                 { "EmulateDSB",       true } },
    { "-no-dsb",              { "EmulateDSB",       false } },
    { "-legacy-scsp",         { "LegacySoundDSP",   true } },
    { "-new-scsp",            { "LegacySoundDSP",   false } },
#ifdef NET_BOARD
    { "-net",                 { "Network",       true } },
    { "-no-net",              { "Network",       false } },
    { "-simulate-netboard",   { "SimulateNet",   true } },
    { "-emulate-netboard",    { "SimulateNet",   false } },
#endif
    { "-no-force-feedback",   { "ForceFeedback",    false } },
    { "-force-feedback",      { "ForceFeedback",    true } },
    { "-dump-textures",       { "DumpTextures",     true } },
  };
  for (int i = 1; i < argc; i++)
  {
    std::string arg(argv[i]);
    if (arg[0] == '-')
    {
      // First, check maps
      size_t idx_equals = arg.find_first_of('=');
      if (idx_equals != std::string::npos)
      {
        std::string option(arg.begin(), arg.begin() + idx_equals);
        std::string value(arg.begin() + idx_equals + 1, arg.end());
        if (value.length() == 0)
        {
          ErrorLog("Argument to '%s' cannot be blank.", option.c_str());
          cmd_line.error = true;
          continue;
        }
        auto it = valued_options.find(option);
        if (it != valued_options.end())
        {
          const std::string &config_key = it->second;
          cmd_line.config.Set(config_key, value);
          continue;
        }
      }
      else
      {
        auto it = bool_options.find(arg);
        if (it != bool_options.end())
        {
          const std::string &config_key = it->second.first;
          bool value = it->second.second;
          cmd_line.config.Set(config_key, value);
          continue;
        }
        else if (valued_options.find(arg) != valued_options.end())
        {
          ErrorLog("'%s' requires an argument.", argv[i]);
          cmd_line.error = true;
          continue;
        }
      }
      // Fell through -- handle special cases
      if (arg == "-?" || arg == "-h" || arg == "-help" || arg == "--help")
        cmd_line.print_help = true;
      else if (arg == "-print-games")
        cmd_line.print_games = true;
      else if (arg == "-res" || arg.find("-res=") == 0)
      {
        std::vector<std::string> parts = Util::Format(arg).Split('=');
        if (parts.size() != 2)
        {ErrorLog("'-res' requires both a width and height (e.g., '-res=496,384').");
          cmd_line.error = true;
        }
        else
        {
          unsigned  x, y;
          if (2 == sscanf(&argv[i][4],"=%u,%u", &x, &y))
          {
            std::string xres = Util::Format() << x;
            std::string yres = Util::Format() << y;
            cmd_line.config.Set("XResolution", xres);
            cmd_line.config.Set("YResolution", yres);
          }
          else
          {
            ErrorLog("'-res' requires both a width and height (e.g., '-res=496,384').");
            cmd_line.error = true;
          }
        }
      }
      else if (arg == "-window-pos" || arg.find("-window-pos=") == 0)
      {
          std::vector<std::string> parts = Util::Format(arg).Split('=');
          if (parts.size() != 2)
          {
              ErrorLog("'-window-pos' requires both an X and Y position (e.g., '-window-pos=10,0').");
              cmd_line.error = true;
          }
          else
          {
              int xpos, ypos;
              if (2 == sscanf(&argv[i][11], "=%d,%d", &xpos, &ypos))
              {
                  cmd_line.config.Set("WindowXPosition", xpos);
                  cmd_line.config.Set("WindowYPosition", ypos);
              }
              else
              {
                  ErrorLog("'-window-pos' requires both an X and Y position (e.g., '-window-pos=10,0').");
                  cmd_line.error = true;
              }
          }
      }
      else if (arg == "-ss" || arg.find("-ss=") == 0) {

          std::vector<std::string> parts = Util::Format(arg).Split('=');

          if (parts.size() != 2)
          {
              ErrorLog("'-ss' requires an integer argument (e.g., '-ss=2').");
              cmd_line.error = true;
          }
          else {

              try {
                  int val = std::stoi(parts[1]);
                  val = std::clamp(val, 1, 8);

                  cmd_line.config.Set("Supersampling", val);
              }
              catch (...) {
                  ErrorLog("'-ss' requires an integer argument (e.g., '-ss=2').");
                  cmd_line.error = true;
              }
          }
      }
      else if (arg == "-true-hz")
        cmd_line.config.Set("RefreshRate", 57.524f);
      else if (arg == "-print-gl-info")
        cmd_line.print_gl_info = true;
      else if (arg == "-config-inputs")
        cmd_line.config_inputs = true;
      else if (arg == "-print-inputs")
        cmd_line.print_inputs = true;
#ifdef SUPERMODEL_DEBUGGER
      else if (arg == "-disable-debugger")
        cmd_line.disable_debugger = true;
      else if (arg == "-enter-debugger")
        cmd_line.enter_debugger = true;
#endif
#ifdef DEBUG
      else if (arg == "-gfx-state" || arg.find("-gfx-state=") == 0)
      {
        std::vector<std::string> parts = Util::Format(arg).Split('=');
        if (parts.size() != 2)
        {
          ErrorLog("'-gfx-state' requires a file name.");
          cmd_line.error = true;
        }
        else
          cmd_line.gfx_state = parts[1];
      }
#endif
      else
      {
        ErrorLog("Ignoring unrecognized option: %s", argv[i]);
        cmd_line.error = true;
      }
    }
    else
      cmd_line.rom_files.emplace_back(arg);
  }
  return cmd_line;
}

/*
 * main(argc, argv):
 *
 * Program entry point.
 */

int main(int argc, char **argv)
{
  Title();
  if (argc <= 1)
  {
    Help();
    return 0;
  }

  // Before command line is parsed, console logging only
  SetLogger(std::make_shared<CConsoleErrorLogger>());

  // Load config and parse command line
  auto cmd_line = ParseCommandLine(argc, argv);
  if (cmd_line.error)
  {
    return 1;
  }

  // Create logger as specified by command line
  auto logger = CreateLogger(cmd_line.config);
  if (!logger)
  {
    ErrorLog("Unable to initialize logging system.");
    return 1;
  }
  SetLogger(logger);
  InfoLog("Supermodel Version " SUPERMODEL_VERSION);
  InfoLog("Started as:");
  for (int i = 0; i < argc; i++)
    InfoLog("  argv[%d] = %s", i, argv[i]);

  // Finish processing command line
  if (cmd_line.print_help)
  {
    Help();
    return 0;
  }
  if (cmd_line.print_gl_info)
  {
    // We must exit after this because CreateGLScreen() is used
    PrintGLInfo(true, false, false);
    return 0;
  }
#ifdef DEBUG
  s_gfxStatePath.assign(cmd_line.gfx_state);
#endif
  bool print_games = cmd_line.print_games;
  bool rom_specified = !cmd_line.rom_files.empty();
  if (!rom_specified && !print_games && !cmd_line.config_inputs && !cmd_line.print_inputs)
  {
    ErrorLog("No ROM file specified.");
    return 0;
  }

  // Load game and resolve run-time config
  Game game;
  ROMSet rom_set;
  Util::Config::Node fileConfig("Global");
  {
    Util::Config::Node fileConfigWithDefaults("Global");
    Util::Config::Node config3("Global");
    Util::Config::Node config4("Global");
    Util::Config::FromINIFile(&fileConfig, s_configFilePath);
    Util::Config::MergeINISections(&fileConfigWithDefaults, DefaultConfig(), fileConfig); // apply .ini file's global section over defaults
    Util::Config::MergeINISections(&config3, fileConfigWithDefaults, cmd_line.config);    // apply command line overrides
    if (rom_specified || print_games)
    {
      std::string xml_file = config3["GameXMLFile"].ValueAs<std::string>();
      GameLoader loader(xml_file);
      if (print_games)
      {
        PrintGameList(xml_file, loader.GetGames());
        return 0;
      }
      if (loader.Load(&game, &rom_set, *cmd_line.rom_files.begin()))
        return 1;
      Util::Config::MergeINISections(&config4, config3, fileConfig[game.name]);   // apply game-specific config
    }
    else
      config4 = config3;
    Util::Config::MergeINISections(&s_runtime_config, config4, cmd_line.config);  // apply command line overrides once more
  }
  LogConfig(s_runtime_config);

  // Initialize SDL (individual subsystems get initialized later)
  if (SDL_Init(0) != 0)
  {
    ErrorLog("Unable to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }

  // Begin initializing various subsystems...
  int exitCode = 0;
  IEmulator *Model3 = nullptr;
  CInputSystem *InputSystem = nullptr;
  CInputs *Inputs = nullptr;
  COutputs *Outputs = nullptr;
#ifdef SUPERMODEL_DEBUGGER
  std::shared_ptr<Debugger::CSupermodelDebugger> Debugger;
#endif // SUPERMODEL_DEBUGGER
  std::string selectedInputSystem = s_runtime_config["InputSystem"].ValueAs<std::string>();

  aaValue = s_runtime_config["Supersampling"].ValueAs<int>();

  // Create a window
  xRes = 496;
  yRes = 384;
  if (OKAY != CreateGLScreen(s_runtime_config["New3DEngine"].ValueAs<bool>(), s_runtime_config["QuadRendering"].ValueAs<bool>(),"Supermodel", false, &xOffset, &yOffset, &xRes, &yRes, &totalXRes, &totalYRes, false, false))
  {
    exitCode = 1;
    goto Exit;
  }

  // Create Crosshair
  s_crosshair = new CCrosshair(s_runtime_config);
  if (s_crosshair->Init() != OKAY)
  {
      ErrorLog("Unable to load bitmap crosshair texture\n");
      exitCode = 1;
      goto Exit;
  }

  // Create Model 3 emulator
#ifdef DEBUG
  Model3 = s_gfxStatePath.empty() ? static_cast<IEmulator *>(new CModel3(s_runtime_config)) : static_cast<IEmulator *>(new CModel3GraphicsState(s_runtime_config, s_gfxStatePath));
#else
  Model3 = new CModel3(s_runtime_config);
#endif

  // Create input system
  if (selectedInputSystem == "sdl")
    InputSystem = new CSDLInputSystem(s_runtime_config);
#ifdef SUPERMODEL_WIN32
  else if (selectedInputSystem == "dinput")
    InputSystem = new CDirectInputSystem(s_runtime_config, s_window, false, false);
  else if (selectedInputSystem == "xinput")
    InputSystem = new CDirectInputSystem(s_runtime_config, s_window, false, true);
  else if (selectedInputSystem == "rawinput")
    InputSystem = new CDirectInputSystem(s_runtime_config, s_window, true, false);
#endif // SUPERMODEL_WIN32
  else
  {
    ErrorLog("Unknown input system: %s\n", selectedInputSystem.c_str());
    exitCode = 1;
    goto Exit;
  }

  // Create inputs from input system (configuring them if required)
  Inputs = new CInputs(InputSystem);
  if (!Inputs->Initialize())
  {
    ErrorLog("Unable to initalize inputs.\n");
    exitCode = 1;
    goto Exit;
  }

  // NOTE: fileConfig is passed so that the global section is used for input settings
  // and because this function may write out a new config file, which must preserve
  // all sections. We don't want to pollute the output with built-in defaults.
  if (ConfigureInputs(Inputs, &fileConfig, &s_runtime_config, game, cmd_line.config_inputs))
  {
    exitCode = 1;
    goto Exit;
  }

  if (cmd_line.print_inputs)
  {
    Inputs->PrintInputs(NULL);
    InputSystem->PrintSettings();
  }

  if (!rom_specified)
    goto Exit;

  // Create outputs
#ifdef SUPERMODEL_WIN32
  {
    std::string outputs = s_runtime_config["Outputs"].ValueAs<std::string>();
    if (outputs == "none")
      Outputs = NULL;
    else if (outputs == "win")
      Outputs = new CWinOutputs();
    else
    {
      ErrorLog("Unknown outputs: %s\n", outputs.c_str());
      exitCode = 1;
      goto Exit;
    }
  }
#endif // SUPERMODEL_WIN32

  // Initialize outputs
  if (Outputs != NULL && !Outputs->Initialize())
  {
    ErrorLog("Unable to initialize outputs.\n");
    exitCode = 1;
    goto Exit;
  }

#ifdef SUPERMODEL_DEBUGGER
  // Create Supermodel debugger unless debugging is disabled
  if (!cmd_line.disable_debugger)
  {
    Debugger = std::make_shared<Debugger::CSupermodelDebugger>(dynamic_cast<CModel3 *>(Model3), Inputs, logger);
    // If -enter-debugger option was set force debugger to break straightaway
    if (cmd_line.enter_debugger)
      Debugger->ForceBreak(true);
  }
  // Fire up Supermodel with debugger
  exitCode = Supermodel(game, &rom_set, Model3, Inputs, Outputs, Debugger);
#else
  // Fire up Supermodel
  exitCode = Supermodel(game, &rom_set, Model3, Inputs, Outputs);
#endif // SUPERMODEL_DEBUGGER
  delete Model3;

Exit:
  if (Inputs != NULL)
    delete Inputs;
  if (InputSystem != NULL)
    delete InputSystem;
  if (Outputs != NULL)
    delete Outputs;
  if (s_crosshair != NULL)
      delete s_crosshair;
  DestroyGLScreen();
  SDL_Quit();

  if (exitCode)
    InfoLog("Program terminated due to an error.");
  else
    InfoLog("Program terminated normally.");

  return exitCode;
}
