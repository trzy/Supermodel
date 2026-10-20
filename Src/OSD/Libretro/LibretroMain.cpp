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
 * LibretroMain.cpp
 *
 * libretro API implementation: the libretro counterpart of OSD/SDL/Main.cpp.
 *
 * - Video: OpenGL rendered through the frontend's hardware context (core
 *   profile 4.1 for the new 3D engine, compatibility profile for the legacy
 *   engine). Supermodel's renderers draw to the default framebuffer
 *   (framebuffer 0); glBindFramebuffer() is redirected so that framebuffer 0
 *   is the frontend's framebuffer.
 * - Audio: the sound board runs once per emulated frame (735 samples);
 *   the sample rate reported to the frontend is 735 x refresh rate so audio
 *   and video stay in step at both 60 and 57.524 Hz.
 * - Input: see LibretroInputSystem.h and LibretroInputs.cpp.
 * - Options: see LibretroOptions.cpp.
 * - Save states: Supermodel's save state format, kept in memory.
 * - NVRAM: Supermodel's .nv files in <save>/supermodel/NVRAM.
 */

#include <GL/glew.h>

#include "libretro.h"
#include "LibretroAudio.h"
#include "LibretroInputSystem.h"
#include "LibretroInputs.h"
#include "LibretroOptions.h"
#include "LibretroPaths.h"

#include "Supermodel.h"
#include "BlockFile.h"
#include "GameLoader.h"
#include "OSD/Audio.h"
#include "OSD/FileSystemPath.h"
#include "OSD/Video.h"
#include "OSD/Outputs.h"
#include "Model3/IEmulator.h"
#include "Model3/Model3.h"
#include "Pro1000/Pro1000.h"
#include "Graphics/IRender2D.h"
#include "Graphics/Render2D.h"
#include "Graphics/Pro1000Render2D.h"
#include "Graphics/New3D/New3D.h"
#ifndef SUPERMODEL_OSX
#include "Graphics/Legacy3D/Legacy3D.h"
#endif
#include "Graphics/SuperAA.h"
#include "Sound/MPEG/MpegAudio.h"
#include "Util/ConfigBuilders.h"
#include "Crosshair.h"
#ifdef SUPERMODEL_LIBRETRO_NET
#include "NetOutputs.h"
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef SUPERMODEL_LIBRETRO_EMBEDDED_GAMES_XML
#include "GamesXML.inc"   // generated from Config/Games.xml by bin2c
#endif

/******************************************************************************
 Frontend Interface
******************************************************************************/

static retro_environment_t s_environ = nullptr;
static retro_video_refresh_t s_videoRefresh = nullptr;
static retro_audio_sample_t s_audioSample = nullptr;
static retro_audio_sample_batch_t s_audioSampleBatch = nullptr;
static retro_input_poll_t s_inputPoll = nullptr;
static retro_input_state_t s_inputState = nullptr;
static retro_log_printf_t s_log = nullptr;

static retro_hw_render_callback s_hwRender;

/******************************************************************************
 Logging
******************************************************************************/

class CLibretroLogger : public CLogger
{
public:
  void DebugLog(const char *fmt, va_list vl) override
  {
#ifdef DEBUG
    Output(RETRO_LOG_DEBUG, fmt, vl);
#else
    (void) fmt;
    (void) vl;
#endif
  }

  void InfoLog(const char *fmt, va_list vl) override
  {
    Output(RETRO_LOG_INFO, fmt, vl);
  }

  void ErrorLog(const char *fmt, va_list vl) override
  {
    Output(RETRO_LOG_ERROR, fmt, vl);
  }

private:
  static void Output(retro_log_level level, const char *fmt, va_list vl)
  {
    char message[4096];
    vsnprintf(message, sizeof(message), fmt, vl);

    // Supermodel messages may or may not end with a newline
    size_t len = strlen(message);
    while (len > 0 && (message[len - 1] == '\n' || message[len - 1] == '\r'))
      message[--len] = '\0';
    if (len == 0)
      return;

    if (s_log)
      s_log(level, "[Supermodel] %s\n", message);
    else
      fprintf(stderr, "[Supermodel] %s\n", message);
  }
};

/******************************************************************************
 Emulator State
******************************************************************************/

static Util::Config::Node s_config("Global");
static Game s_game;
static IEmulator *s_emu = nullptr;
static std::shared_ptr<CLibretroInputSystem> s_inputSystem;
static CInputs *s_inputs = nullptr;
static COutputs *s_outputs = nullptr;

static SuperAA *s_superAA = nullptr;
static IRender2D *s_render2D = nullptr;
static IRender3D *s_render3D = nullptr;
static CCrosshair *s_crosshair = nullptr;
static bool s_renderersReady = false;
static bool s_emulatorReset = false;   // Reset() is done once renderers exist

// Display geometry (as computed by SetGLGeometry() in the SDL build)
static unsigned s_xOffset = 0, s_yOffset = 0;   // offset of image within frame
static unsigned s_xRes = 496, s_yRes = 384;     // image size
static unsigned s_totalXRes = 496, s_totalYRes = 384;  // frame size
static int s_aaValue = 1;
static float s_refreshRate = 60.0f;
static std::string s_crosshairStyle;

static bool s_gameHasLightguns = false;
static unsigned s_portDevice[CLibretroInputSystem::NUM_PORTS] =
  { RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD };

static std::vector<int16_t> s_audioBuffer;
static size_t s_stateSize = 0;
static std::vector<uint8_t> s_pendingState;  // state loaded before the renderers existed

static const int STATE_FILE_VERSION = 6;  // must match OSD/SDL/Main.cpp
static const int NVRAM_FILE_VERSION = 0;  // must match OSD/SDL/Main.cpp

/******************************************************************************
 OpenGL Integration
******************************************************************************/

extern "C" void *glewLibretroGetProcAddress(const char *name)
{
  if (!s_hwRender.get_proc_address)
    return nullptr;
  return (void *) s_hwRender.get_proc_address(name);
}

static PFNGLBINDFRAMEBUFFERPROC s_glBindFramebuffer = nullptr;

// Framebuffer 0 is the frontend's framebuffer
static void GLAPIENTRY LibretroBindFramebuffer(GLenum target, GLuint framebuffer)
{
  if (framebuffer == 0)
    framebuffer = (GLuint) s_hwRender.get_current_framebuffer();
  s_glBindFramebuffer(target, framebuffer);
}

static bool InitGL()
{
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK)
  {
    ErrorLog("OpenGL initialization failed: %s", (const char *) glewGetErrorString(err));
    return false;
  }
  // GLEW may leave a harmless GL error behind
  while (glGetError() != GL_NO_ERROR)
    ;

  // Redirect binds of the default framebuffer to the frontend's framebuffer
  if (__glewBindFramebuffer != LibretroBindFramebuffer)
  {
    s_glBindFramebuffer = __glewBindFramebuffer;
    __glewBindFramebuffer = LibretroBindFramebuffer;
  }

  InfoLog("OpenGL information:");
  InfoLog("  Vendor                   : %s", (const char *) glGetString(GL_VENDOR));
  InfoLog("  Renderer                 : %s", (const char *) glGetString(GL_RENDERER));
  InfoLog("  Version                  : %s", (const char *) glGetString(GL_VERSION));
  InfoLog("  Shading Language Version : %s", (const char *) glGetString(GL_SHADING_LANGUAGE_VERSION));
  return true;
}

static void ComputeGeometry()
{
  s_totalXRes = s_config["XResolution"].ValueAs<unsigned>();
  s_totalYRes = s_config["YResolution"].ValueAs<unsigned>();
  bool keepAspectRatio = !s_config["Stretch"].ValueAs<bool>();

  // Same computation as SetGLGeometry() in OSD/SDL/Main.cpp, for a window of
  // exactly the requested size
  float xResF = float(s_totalXRes);
  float yResF = float(s_totalYRes);
  if (keepAspectRatio)
  {
    float model3Ratio = float(496.0 / 384.0);
    if (yResF < (xResF / model3Ratio))
      xResF = yResF * model3Ratio;
    if (xResF < (yResF * model3Ratio))
      yResF = xResF / model3Ratio;
  }
  s_xOffset = (s_totalXRes - (unsigned) xResF) / 2;
  s_yOffset = (s_totalYRes - (unsigned) yResF) / 2;
  s_xRes = (unsigned) xResF;
  s_yRes = (unsigned) yResF;
}

// Global GL state the renderers expect (set once per frame because the
// frontend may change it between frames)
static void SetGLState()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, s_totalXRes, s_totalYRes);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);

  UINT32 correction = (UINT32) (((s_yRes / 384.) * 2.) + 0.5);  // due to the 2D layer compensation (2 pixels off)
  glEnable(GL_SCISSOR_TEST);
  if (s_config["WideScreen"].ValueAsDefault<bool>(false))
    glScissor(0 * s_aaValue, correction * s_aaValue, s_totalXRes * s_aaValue, (s_totalYRes - (correction * 2)) * s_aaValue);
  else
    glScissor((s_xOffset + correction) * s_aaValue, (s_yOffset + correction) * s_aaValue, (s_xRes - (correction * 2)) * s_aaValue, (s_yRes - (correction * 2)) * s_aaValue);

  // Clear the whole frame (borders included) to black
  glDisable(GL_SCISSOR_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_SCISSOR_TEST);
}

static void DestroyRenderers()
{
  delete s_crosshair;
  s_crosshair = nullptr;
  delete s_render2D;
  s_render2D = nullptr;
  delete s_render3D;
  s_render3D = nullptr;
  delete s_superAA;
  s_superAA = nullptr;
  s_renderersReady = false;
}

static void SaveStateToBlockFile(CBlockFile *file);
static bool LoadStateFromBlockFile(CBlockFile *file);

static bool CreateRenderers()
{
  DestroyRenderers();

  UpscaleMode upscaleMode = (UpscaleMode) s_config["UpscaleMode"].ValueAs<int>();
  CRTcolor crtColors = (CRTcolor) s_config["CRTcolors"].ValueAs<int>();

  s_superAA = new SuperAA(s_aaValue, crtColors);
  s_superAA->Init(s_totalXRes, s_totalYRes);

  if (dynamic_cast<CPro1000 *>(s_emu))
    s_render2D = new CPro1000Render2D();
  else
    s_render2D = new CRender2D(s_config);

#ifndef SUPERMODEL_OSX
  if (s_config["New3DEngine"].ValueAs<bool>())
    s_render3D = new New3D::CNew3D(s_config, s_game.name);
  else
    s_render3D = new Legacy3D::CLegacy3D(s_config);
#else
  s_render3D = new New3D::CNew3D(s_config, s_game.name);
#endif

  unsigned aa = (unsigned) s_aaValue;
  if (Result::OKAY != s_render2D->Init(s_xOffset * aa, s_yOffset * aa, s_xRes * aa, s_yRes * aa, s_totalXRes * aa, s_totalYRes * aa, s_superAA->GetTargetID(), upscaleMode))
    return false;
  if (Result::OKAY != s_render3D->Init(s_xOffset * aa, s_yOffset * aa, s_xRes * aa, s_yRes * aa, s_totalXRes * aa, s_totalYRes * aa, s_superAA->GetTargetID()))
    return false;

  s_crosshairStyle = s_config["CrosshairStyle"].ValueAs<std::string>();
  s_crosshair = new CCrosshair(s_config);
  if (s_crosshair->Init() != Result::OKAY)
  {
    ErrorLog("Unable to initialize crosshairs.");
    delete s_crosshair;
    s_crosshair = nullptr;
  }

  s_emu->AttachRenderers(s_render2D, s_render3D, s_superAA);
  s_renderersReady = true;
  return true;
}

static void ContextReset()
{
  if (!s_emu)
    return;
  if (!InitGL() || !CreateRenderers())
  {
    ErrorLog("Unable to initialize the renderers.");
    DestroyRenderers();
    return;
  }

  if (!s_emulatorReset)
  {
    s_emu->Reset();
    s_emulatorReset = true;
    if (!s_pendingState.empty())
    {
      CBlockFile restore;
      restore.LoadFromMemory(s_pendingState.data(), s_pendingState.size());
      LoadStateFromBlockFile(&restore);
      restore.Close();
      s_pendingState.clear();
    }
  }
  else
  {
    // The context was lost: the new renderers do not hold the textures and
    // render state of the old ones. Reloading the current state uploads them.
    s_emu->PauseThreads();
    std::vector<uint8_t> data;
    if (!s_pendingState.empty())
      data.swap(s_pendingState);
    else
    {
      CBlockFile state;
      state.CreateInMemory("Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION);
      SaveStateToBlockFile(&state);
      state.Close();
      data = state.GetMemoryBuffer();
    }
    CBlockFile restore;
    restore.LoadFromMemory(data.data(), data.size());
    LoadStateFromBlockFile(&restore);
    restore.Close();
    s_emu->ResumeThreads();
  }
}

static void ContextDestroy()
{
  DestroyRenderers();
}

/******************************************************************************
 OSD Video Callbacks (called by the emulator while rendering a frame)
******************************************************************************/

bool BeginFrameVideo()
{
  return s_renderersReady;
}

void EndFrameVideo()
{
  // Show crosshairs for light gun games
  if (s_gameHasLightguns && s_crosshair)
    s_crosshair->Update(s_game.inputs, s_inputs, s_xOffset, s_yOffset, s_xRes, s_yRes);
}

/******************************************************************************
 Save States and NVRAM
******************************************************************************/

static void SaveStateToBlockFile(CBlockFile *file)
{
  int32_t fileVersion = STATE_FILE_VERSION;
  file->Write(&fileVersion, sizeof(fileVersion));
  file->Write(s_emu->GetGame().name);
  s_emu->SaveState(file);
}

static bool LoadStateFromBlockFile(CBlockFile *file)
{
  if (Result::OKAY != file->FindBlock("Supermodel Save State"))
  {
    ErrorLog("Not a valid Supermodel save state.");
    return false;
  }
  int32_t fileVersion = 0;
  file->Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != STATE_FILE_VERSION)
  {
    ErrorLog("Save state is incompatible with this version of Supermodel.");
    return false;
  }
  s_emu->LoadState(file);
  return true;
}

static std::string NVRAMPath()
{
  return FileSystemPath::GetPath(FileSystemPath::NVRAM) + s_emu->GetGame().name + ".nv";
}

static void SaveNVRAM()
{
  CBlockFile nvram;
  std::string path = NVRAMPath();
  if (Result::OKAY != nvram.Create(path, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    ErrorLog("Unable to save NVRAM to '%s'.", path.c_str());
    return;
  }
  int32_t fileVersion = NVRAM_FILE_VERSION;
  nvram.Write(&fileVersion, sizeof(fileVersion));
  nvram.Write(s_emu->GetGame().name);
  s_emu->SaveNVRAM(&nvram);
  nvram.Close();
  InfoLog("Saved NVRAM to '%s'.", path.c_str());
}

static void LoadNVRAM()
{
  CBlockFile nvram;
  std::string path = NVRAMPath();
  if (Result::OKAY != nvram.Load(path))
    return;
  if (Result::OKAY != nvram.FindBlock("Supermodel NVRAM State"))
  {
    ErrorLog("'%s' does not appear to be a valid NVRAM file.", path.c_str());
    return;
  }
  int32_t fileVersion;
  nvram.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != NVRAM_FILE_VERSION)
  {
    ErrorLog("'%s' is incompatible with this version of Supermodel.", path.c_str());
    return;
  }
  s_emu->LoadNVRAM(&nvram);
  nvram.Close();
  InfoLog("Loaded NVRAM from '%s'.", path.c_str());
}

/******************************************************************************
 Helpers
******************************************************************************/

static void UpdateAVInfo();

static void UpdateInputSettings()
{
  if (!s_inputs)
    return;
  bool autoReload = LibretroOptions::GetGunAutoReload(s_environ);
  LibretroInputs::StoreMappings(&s_config, autoReload);
  s_inputs->LoadFromConfig(s_config);
  s_inputSystem->SetRumbleStrength(LibretroOptions::GetRumbleStrength(s_environ));
}

static void CheckOptions()
{
  bool updated = false;
  if (!s_environ(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) || !updated)
    return;

  float oldRefreshRate = s_config["RefreshRate"].ValueAs<float>();
  LibretroOptions::Apply(s_environ, &s_config, false);

  LibretroAudio::SetBalance(s_config["BalanceLeftRight"].ValueAs<float>(), s_config["BalanceFrontRear"].ValueAs<float>());
  UpdateInputSettings();

  // The crosshair style is chosen when the crosshairs are created
  std::string style = s_config["CrosshairStyle"].ValueAs<std::string>();
  if (s_renderersReady && style != s_crosshairStyle)
  {
    s_crosshairStyle = style;
    delete s_crosshair;
    s_crosshair = new CCrosshair(s_config);
    if (s_crosshair->Init() != Result::OKAY)
    {
      delete s_crosshair;
      s_crosshair = nullptr;
    }
  }

  if (s_config["RefreshRate"].ValueAs<float>() != oldRefreshRate)
    UpdateAVInfo();
}

static void GetAVInfo(retro_system_av_info *info)
{
  memset(info, 0, sizeof(*info));
  info->geometry.base_width = s_totalXRes;
  info->geometry.base_height = s_totalYRes;
  info->geometry.max_width = s_totalXRes;
  info->geometry.max_height = s_totalYRes;
  info->geometry.aspect_ratio = float(s_totalXRes) / float(s_totalYRes);
  s_refreshRate = s_config["RefreshRate"].ValueAsDefault<float>(60.0f);
  if (s_refreshRate < 30.0f || s_refreshRate > 120.0f)
    s_refreshRate = 60.0f;
  info->timing.fps = s_refreshRate;
  // One sound board frame (44100/60 samples) is produced per emulated frame
  info->timing.sample_rate = double(44100 / 60) * s_refreshRate;
}

static void UpdateAVInfo()
{
  retro_system_av_info info;
  GetAVInfo(&info);
  s_environ(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
}

static void UpdateLightGunState()
{
  s_gameHasLightguns = !!(s_game.inputs & (Game::INPUT_GUN1 | Game::INPUT_GUN2)) || s_game.name == "lostwsga";
}

static bool LoadGameDefinitions(std::unique_ptr<GameLoader> *loader)
{
  std::string xmlFile = FileSystemPath::GetPath(FileSystemPath::Config) + "Games.xml";
  std::error_code ec;
  if (std::filesystem::exists(std::filesystem::u8path(xmlFile), ec))
  {
    InfoLog("Using game definitions from '%s'.", xmlFile.c_str());
    loader->reset(new GameLoader(xmlFile));
    return true;
  }
#ifdef SUPERMODEL_LIBRETRO_EMBEDDED_GAMES_XML
  InfoLog("Using built-in game definitions ('%s' not found).", xmlFile.c_str());
  Util::Config::Node xml("xml");
  std::string text((const char *) s_gamesXML, (size_t) s_gamesXML_size);
  if (Util::Config::FromXML(&xml, text))
  {
    ErrorLog("Built-in game definitions could not be parsed.");
    return false;
  }
  loader->reset(new GameLoader(xml, "Games.xml (built-in)"));
  return true;
#else
  ErrorLog("Game definitions not found: copy Config/Games.xml to '%s'.", xmlFile.c_str());
  return false;
#endif
}

/******************************************************************************
 libretro API
******************************************************************************/

RETRO_API void retro_set_environment(retro_environment_t cb)
{
  s_environ = cb;

  retro_log_callback logging;
  if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
    s_log = logging.log;

  LibretroOptions::Register(cb);

  static const retro_controller_description gunPortDevices[] =
  {
    { "RetroPad", RETRO_DEVICE_JOYPAD },
    { "Light Gun", RETRO_DEVICE_LIGHTGUN },
  };
  static const retro_controller_description padPortDevices[] =
  {
    { "RetroPad", RETRO_DEVICE_JOYPAD },
  };
  static const retro_controller_info ports[] =
  {
    { gunPortDevices, 2 },
    { gunPortDevices, 2 },
    { padPortDevices, 1 },
    { padPortDevices, 1 },
    { nullptr, 0 },
  };
  cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *) ports);

  bool noGame = false;
  cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &noGame);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { s_videoRefresh = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { s_audioSample = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { s_audioSampleBatch = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { s_inputPoll = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { s_inputState = cb; }

RETRO_API void retro_init(void)
{
  SetLogger(std::make_shared<CLibretroLogger>());
}

RETRO_API void retro_deinit(void)
{
}

RETRO_API unsigned retro_api_version(void)
{
  return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(retro_system_info *info)
{
  memset(info, 0, sizeof(*info));
  info->library_name = "Supermodel";
  info->library_version = SUPERMODEL_VERSION;
  info->valid_extensions = "zip";
  info->need_fullpath = true;    // ROM sets (and parent sets) are read from disk
  info->block_extract = true;    // GameLoader reads the zip files itself
}

RETRO_API void retro_get_system_av_info(retro_system_av_info *info)
{
  GetAVInfo(info);
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
  if (port >= CLibretroInputSystem::NUM_PORTS)
    return;
  if (device != RETRO_DEVICE_LIGHTGUN || port >= CLibretroInputSystem::NUM_GUN_PORTS)
    device = RETRO_DEVICE_JOYPAD;
  s_portDevice[port] = device;
  if (s_inputSystem)
    s_inputSystem->SetPortDevice(port, device);
}

RETRO_API void retro_reset(void)
{
  if (!s_emu)
    return;
  s_emu->PauseThreads();
  s_emu->Reset();
  s_emu->ResumeThreads();
  InfoLog("Model 3 reset.");
}

static void UnloadAll()
{
  delete s_emu;
  s_emu = nullptr;
  delete s_inputs;
  s_inputs = nullptr;
  s_inputSystem.reset();
  delete s_outputs;
  s_outputs = nullptr;
}

RETRO_API bool retro_load_game(const retro_game_info *info)
{
  if (!info || !info->path)
  {
    ErrorLog("No ROM set specified.");
    return false;
  }

  // Directories
  const char *dir = nullptr;
  if (s_environ(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
    LibretroPaths::SetSystemDirectory(dir);
  dir = nullptr;
  if (s_environ(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir)
    LibretroPaths::SetSaveDirectory(dir);

  retro_pixel_format pixelFormat = RETRO_PIXEL_FORMAT_XRGB8888;
  if (!s_environ(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixelFormat))
  {
    ErrorLog("XRGB8888 pixel format is not supported by the frontend.");
    return false;
  }

  // Configuration from core options
  s_config = LibretroOptions::DefaultConfig();
  LibretroOptions::Apply(s_environ, &s_config, true);

  // Load the game
  std::unique_ptr<GameLoader> loader;
  if (!LoadGameDefinitions(&loader))
    return false;
  ROMSet romSet;
  if (loader->Load(&s_game, &romSet, info->path))
  {
    ErrorLog("Unable to load ROM set '%s'.", info->path);
    return false;
  }
  loader.reset();
  InfoLog("Loading '%s' (%s).", s_game.title.c_str(), s_game.name.c_str());

  // Create the emulator
  if (s_game.platform == Game::PLATFORM_PRO1000)
    s_emu = new CPro1000(s_config);
  else
    s_emu = new CModel3(s_config);
  if (Result::OKAY != s_emu->Init(s_game, romSet))
  {
    UnloadAll();
    return false;
  }
  romSet = ROMSet();  // free memory

  // Custom music for games with MPEG boards (optional)
  MpegDec::LoadCustomTracks(FileSystemPath::GetPath(FileSystemPath::Config) + "Music.xml", s_game);

  LoadNVRAM();
  if (LibretroOptions::GetClearNVRAM(s_environ))
  {
    s_emu->ClearNVRAM();
    InfoLog("NVRAM cleared.");
  }

  // Display geometry
  s_aaValue = std::max(1, std::min(8, s_config["Supersampling"].ValueAs<int>()));
  ComputeGeometry();

  // Audio
  SetAudioType(s_game.audio);
  OpenAudio(s_config);

  // Inputs
  bool bitmasks = s_environ(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, nullptr);
  s_inputSystem = std::make_shared<CLibretroInputSystem>();
  s_inputSystem->SetInputStateCallback(s_inputState);
  s_inputSystem->SetBitmasksSupported(bitmasks);
  s_inputSystem->SetFrameSize(s_totalXRes, s_totalYRes);
  for (unsigned port = 0; port < CLibretroInputSystem::NUM_PORTS; port++)
    s_inputSystem->SetPortDevice(port, s_portDevice[port]);
  retro_rumble_interface rumble;
  if (s_environ(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble))
    s_inputSystem->SetRumbleInterface(rumble.set_rumble_state);

  s_inputs = new CInputs(s_inputSystem);
  if (!s_inputs->Initialize())
  {
    ErrorLog("Unable to initialize inputs.");
    UnloadAll();
    return false;
  }
  UpdateInputSettings();
  s_emu->AttachInputs(s_inputs);
  UpdateLightGunState();

  std::vector<retro_input_descriptor> descriptors = LibretroInputs::GetDescriptors(s_game.inputs);
  s_environ(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, descriptors.data());

  // Outputs
#ifdef SUPERMODEL_LIBRETRO_NET
  if (s_config["Outputs"].ValueAs<std::string>() == "net")
  {
    CNetOutputs *netOutputs = new CNetOutputs();
    if (s_config["OutputsWithLF"].ValueAs<bool>())
      netOutputs->SetFrameEnding(std::string("\r\n"));
    netOutputs->SetTcpPort(s_config["OutputsTCPPort"].ValueAs<unsigned int>());
    netOutputs->SetUdpBroadcastPort(s_config["OutputsUDPBroadcastPort"].ValueAs<unsigned int>());
    s_outputs = netOutputs;
    if (!s_outputs->Initialize())
    {
      ErrorLog("Unable to initialize outputs.");
      delete s_outputs;
      s_outputs = nullptr;
    }
    else
      s_emu->AttachOutputs(s_outputs);
  }
#endif

  // OpenGL context: core profile 4.1 for the new 3D engine (as the SDL
  // build), compatibility profile with depth/stencil for the legacy engine
  memset(&s_hwRender, 0, sizeof(s_hwRender));
  bool new3D = s_config["New3DEngine"].ValueAs<bool>();
#ifdef SUPERMODEL_OSX
  new3D = true;
#endif
  if (new3D)
  {
    s_hwRender.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
    s_hwRender.version_major = 4;
    s_hwRender.version_minor = 1;
    s_hwRender.depth = false;
    s_hwRender.stencil = false;
  }
  else
  {
    s_hwRender.context_type = RETRO_HW_CONTEXT_OPENGL;
    s_hwRender.depth = true;
    s_hwRender.stencil = true;
  }
  s_hwRender.bottom_left_origin = true;
  s_hwRender.context_reset = ContextReset;
  s_hwRender.context_destroy = ContextDestroy;
  if (!s_environ(RETRO_ENVIRONMENT_SET_HW_RENDER, &s_hwRender))
  {
    ErrorLog("The frontend does not provide the required OpenGL context (%s).", new3D ? "core profile 4.1" : "compatibility profile");
    UnloadAll();
    return false;
  }
  // Keep the frontend's own GL state separate from the renderers' state
  s_environ(RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT, nullptr);

  s_emulatorReset = false;
  s_stateSize = 0;
  return true;
}

RETRO_API bool retro_load_game_special(unsigned game_type, const retro_game_info *info, size_t num_info)
{
  (void) game_type;
  (void) info;
  (void) num_info;
  return false;
}

RETRO_API void retro_unload_game(void)
{
  if (s_emu)
  {
    s_emu->PauseThreads();
    SaveNVRAM();
  }
  CloseAudio();
  // The renderers belong to the GL context and are freed in ContextDestroy();
  // detach them here in case the context outlives the game
  UnloadAll();
  s_game = Game();
  s_emulatorReset = false;
  s_pendingState.clear();
}

RETRO_API void retro_run(void)
{
  CheckOptions();

  s_inputPoll();
  s_inputs->Poll(&s_game, s_xOffset, s_yOffset, s_xRes, s_yRes);

  if (s_renderersReady)
  {
    SetGLState();
    s_emu->RunFrame();
    s_videoRefresh(RETRO_HW_FRAME_BUFFER_VALID, s_totalXRes, s_totalYRes, 0);
  }
  else
    s_videoRefresh(nullptr, s_totalXRes, s_totalYRes, 0);

  // Audio produced during the frame
  size_t frames = LibretroAudio::Drain(&s_audioBuffer);
  const int16_t *samples = s_audioBuffer.data();
  while (frames > 0)
  {
    size_t written = s_audioSampleBatch(samples, frames);
    if (written == 0)
      break;
    written = std::min(written, frames);
    samples += written * 2;
    frames -= written;
  }

  s_inputSystem->ApplyRumble();
}

RETRO_API size_t retro_serialize_size(void)
{
  if (!s_emu)
    return 0;
  if (s_stateSize == 0)
  {
    // The size depends only on the game; measure it once (with some slack)
    s_emu->PauseThreads();
    CBlockFile state;
    state.CreateInMemory("Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION);
    SaveStateToBlockFile(&state);
    state.Close();
    s_emu->ResumeThreads();
    s_stateSize = state.GetMemoryBuffer().size() + 64 * 1024;
  }
  return s_stateSize;
}

RETRO_API bool retro_serialize(void *data, size_t size)
{
  if (!s_emu)
    return false;
  s_emu->PauseThreads();
  CBlockFile state;
  state.CreateInMemory("Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION);
  SaveStateToBlockFile(&state);
  state.Close();
  s_emu->ResumeThreads();

  const std::vector<uint8_t> &buffer = state.GetMemoryBuffer();
  if (buffer.size() > size)
  {
    ErrorLog("Save state buffer too small (%u bytes needed).", (unsigned) buffer.size());
    return false;
  }
  memcpy(data, buffer.data(), buffer.size());
  memset((uint8_t *) data + buffer.size(), 0, size - buffer.size());
  return true;
}

RETRO_API bool retro_unserialize(const void *data, size_t size)
{
  if (!s_emu)
    return false;
  if (!s_renderersReady)
  {
    // Loading a state updates the renderers, which do not exist yet: apply
    // the state once the GL context is ready
    s_pendingState.assign((const uint8_t *) data, (const uint8_t *) data + size);
    return true;
  }
  CBlockFile state;
  if (Result::OKAY != state.LoadFromMemory(data, size))
    return false;
  s_emu->PauseThreads();
  bool ok = LoadStateFromBlockFile(&state);
  s_emu->ResumeThreads();
  state.Close();
  return ok;
}

RETRO_API void retro_cheat_reset(void)
{
}

RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
  (void) index;
  (void) enabled;
  (void) code;
}

RETRO_API unsigned retro_get_region(void)
{
  return RETRO_REGION_NTSC;
}

RETRO_API void *retro_get_memory_data(unsigned id)
{
  (void) id;
  return nullptr;  // NVRAM is kept in Supermodel's own .nv files
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
  (void) id;
  return 0;
}
