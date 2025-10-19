#include <cassert>
#include <stdio.h>
#include "../Pkgs/libretro.h"
#include "Version.h"
#include <GL/glew.h>
#include "Supermodel.h"
#include "Util/Format.h"
#include "Util/ConfigBuilders.h"
#include "OSD/FileSystemPath.h"
#include "GameLoader.h"
#include "SDLIncludes.h"
#include "Graphics/New3D/New3D.h"
#include "Model3/IEmulator.h"
#include "Model3/Model3.h"
#include "Graphics/SuperAA.h"
#include "OSD/DefaultConfigFile.h"

// Before everything
RETRO_API unsigned retro_api_version(void) {
  return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info *info) {
  info->library_name = "Supermodel";
  info->library_version = SUPERMODEL_VERSION;
  info->valid_extensions = "zip";
  info->need_fullpath = true;
  info->block_extract = true;
}

// Before init
retro_environment_t cb_env;
RETRO_API void retro_set_environment(retro_environment_t cb) {
  cb_env = cb;
}

std::string config_path(const char *file) {
  const char* system_path;
  cb_env(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_path);
  return Util::Format() << system_path << "/Supermodel/Config/" << file;
}

// Before run
retro_video_refresh_t cb_video_refresh;
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) {
  cb_video_refresh = cb;
}
retro_audio_sample_t cb_audio_sample;
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) {
  cb_audio_sample = cb;
}
retro_audio_sample_batch_t cb_audio_sample_batch;
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
  cb_audio_sample_batch = cb;
}
retro_input_poll_t cb_input_poll;
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) {
  cb_input_poll = cb;
}
retro_input_state_t cb_input_state;
RETRO_API void retro_set_input_state(retro_input_state_t cb) {
  cb_input_state = cb;
}

// Init
bool initialized = false;
IEmulator *Model3 = nullptr;
std::shared_ptr<CInputSystem> InputSystem;
CInputs *Inputs = nullptr;
static Util::Config::Node s_runtime_config("Global");
SDL_Window *r_window = nullptr;

static void WriteDefaultConfigurationFileIfNotPresent(std::string s_configFilePath)
{
    // Test whether file exists by opening it
    FILE* fp = fopen(s_configFilePath.c_str(), "r");
    if (fp)
    {
        fclose(fp);
        return;
    }

    // Write config
    fp = fopen(s_configFilePath.c_str(), "w");
    if (!fp)
    {
        ErrorLog("Unable to write default configuration file to %s", s_configFilePath.c_str());
        return;
    }
    fputs(s_defaultConfigFileContents, fp);
    fclose(fp);
    InfoLog("Wrote default configuration file to %s", s_configFilePath.c_str());
}


const unsigned SUPERMODEL_W = 496;
const unsigned SUPERMODEL_H = 384;

static Result CreateGLScreen()
{
  GLenum err;

  // Call only once per program session (this is because of issues with
  // DirectInput when the window is destroyed and a new one created). Use
  // ResizeGLScreen() to change resolutions instead.
  if (r_window != nullptr)
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
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  r_window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SUPERMODEL_W, SUPERMODEL_H, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN );
  if (nullptr == r_window)
  {
    ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    return Result::FAIL;
  }

  SDL_GLContext context = SDL_GL_CreateContext(r_window);
  if (nullptr == context)
  {
    ErrorLog("Unable to create OpenGL context: %s\n", SDL_GetError());
    return Result::FAIL;
  }

  SDL_GL_SetSwapInterval(1);
  SDL_GL_MakeCurrent(r_window, context);

  // Initialize GLEW, allowing us to use features beyond OpenGL 1.2
  err = glewInit();
  if (GLEW_OK != err)
  {
    ErrorLog("OpenGL initialization failed: %s\n", glewGetErrorString(err));
    return Result::FAIL;
  }

  // print some basic GPU info
  GLint profile = 0;
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
  printf("GPU info: %s ", glGetString(GL_VERSION));
  if (profile & GL_CONTEXT_CORE_PROFILE_BIT) {
      printf("(core profile)");
  }
  if (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
      printf("(compatibility profile)");
  }
  printf("\n\n");

  // OpenGL initialization
  glViewport(0,0,SUPERMODEL_W,SUPERMODEL_H);
  glClearColor(0.0,0.0,0.0,0.0);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  // Clear both buffers to ensure a black border
  for (int i = 0; i < 3; i++)
  {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    SDL_GL_SwapWindow(r_window);
  }

  UINT32 correction = (UINT32)(((SUPERMODEL_H / 384.) * 2.) + 0.5); // due to the 2D layer compensation (2 pixels off)

  glEnable(GL_SCISSOR_TEST);

  // Scissor box (to clip visible area)
  glScissor(correction, correction, (SUPERMODEL_W - (correction * 2)), (SUPERMODEL_H - (correction * 2)));

  return Result::OKAY;
}

// Input system
#include "Inputs/InputSystem.h"
const JoyDetails theJoystick = {
  "RetroPad", 4, 0, 16, false, {false}, {""}, {false}
};
class CRetroInputSystem : public CInputSystem
{
  private:
  protected:
  public:
	CRetroInputSystem() : CInputSystem("LibRetro") {}
	void SetMouseVisibility(bool visible) {}
	int GetNumMice() const { return 0; }
  int GetMouseAxisValue(int mseNum, int axisNum) const { return 0; }
  int GetMouseWheelDir(int mseNum) const { return 0; }
  bool IsMouseButPressed(int mseNum, int butNum) const { return false; }
  const MouseDetails *GetMouseDetails(int mseNum) { return NULL; }

  int GetNumKeyboards() const { return 0; }
  int GetKeyIndex(const char *keyName) { return 0; }
  const char *GetKeyName(int keyIndex) { return NULL; }
  bool IsKeyPressed(int kbdNum, int keyIndex) const {
    return false;
  }
  const KeyDetails *GetKeyDetails(int kbdNum) { return NULL; }

  int GetNumJoysticks() const { return 1; }
  int GetJoyAxisValue(int joyNum, int axisNum) const {
    return cb_input_state(joyNum, RETRO_DEVICE_ANALOG, axisNum / 2, axisNum % 2);
  }
  bool IsJoyPOVInDir(int joyNum, int povNum, int povDir) const { return false; }
  const JoyDetails *GetJoyDetails(int joyNum) { return &theJoystick; }
  bool IsJoyButPressed(int joyNum, int butNum) const {
    auto r = cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, butNum);
    return r != 0;
  }

  bool ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd) { return false; }

  bool InitializeSystem() {
    return true;
  }

	bool Poll() {
    cb_input_poll();
    return true;
	}

};

RETRO_API void retro_init(void) {
  assert(!initialized);
  // Set logger
  // XXX in the future use the retro logging callback
  SetLogger(std::make_shared<CConsoleErrorLogger>());

  // Initialize SDL (individual subsystems get initialized later)
  if (SDL_Init(0) != 0)
  {
    ErrorLog("Unable to initialize SDL: %s\n", SDL_GetError());
    return;
  }

  // Flag as DPI-aware, otherwise the window content might be scaled by some graphics drivers
  SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "system");

  std::string config_file = config_path("Supermodel.ini");
  WriteDefaultConfigurationFileIfNotPresent(config_file);
  Util::Config::FromINIFile(&s_runtime_config, config_file);
  s_runtime_config.Set("New3DEngine", true, "Global");
  s_runtime_config.Set("QuadRendering", false, "Global");
  s_runtime_config.Set("MultiThreaded", false, "Core");
  s_runtime_config.Set("GPUMultiThreaded", false, "Core");
  s_runtime_config.Set("NoWhiteFlash", false, "Video");
  s_runtime_config.Set("EmulateSound", true, "Sound");
  s_runtime_config.Set("Balance", 0.0f, "Sound", -100.f, 100.f);
  s_runtime_config.Set("BalanceLeftRight", 0.0f, "Sound", -100.f, 100.f);
  s_runtime_config.Set("BalanceFrontRear", 0.0f, "Sound", -100.f, 100.f);
  s_runtime_config.Set("NbSoundChannels", 2, "Sound", 0, 0, { 1,2,4 });
  s_runtime_config.Set("SoundFreq", 57.6f, "Sound", 0.0f, 0.0f, { 57.524160f, 60.f }); // 60.0f? 57.524160f?
  s_runtime_config.Set("FlipStereo", false, "Sound");
  s_runtime_config.Set("EmulateDSB", true, "Sound");
  s_runtime_config.Set("SoundVolume", 100, "Sound", 0, 200);
  s_runtime_config.Set("MusicVolume", 100, "Sound", 0, 200);
  s_runtime_config.Set("LegacySoundDSP", false, "Sound"); // New config option for games that do not play correctly with MAME's SCSP sound core.

  s_runtime_config.Set<std::string>("InputStart1", "JOY1_BUTTON4", "Input", "", "");
  s_runtime_config.Set<std::string>("InputCoin1", "JOY1_BUTTON3", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceA", "JOY1_BUTTON15", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "JOY1_BUTTON13", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "JOY1_BUTTON16", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "JOY1_BUTTON14", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyUp", "JOY1_BUTTON5", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyDown", "JOY1_BUTTON6", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyLeft", "JOY1_BUTTON7", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyRight", "JOY1_BUTTON8", "Input", "", "");
  s_runtime_config.Set<std::string>("InputShift", "JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputBeat", "JOY1_BUTTON1", "Input", "", "");
  s_runtime_config.Set<std::string>("InputCharge", "JOY1_BUTTON9", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJump", "JOY1_BUTTON2", "Input", "", "");

  if (Result::OKAY != CreateGLScreen())
  {
    return;
  }

  int fmt = 1337;
  cb_env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

  initialized = true;
}

RETRO_API void retro_deinit(void) {
  assert(initialized);
  if (r_window != nullptr)
  {
    SDL_GL_DeleteContext(SDL_GL_GetCurrentContext());
    SDL_DestroyWindow(r_window);
  }
  SDL_Quit();
  initialized = false;
}

// Before load_game
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
  info->geometry.base_width = SUPERMODEL_W;
  info->geometry.base_height = SUPERMODEL_H;
  info->geometry.max_width = SUPERMODEL_W;
  info->geometry.max_height = SUPERMODEL_H;
  info->geometry.aspect_ratio = (float) SUPERMODEL_W / (float) SUPERMODEL_H;
  info->timing.fps = 60.0f;
  info->timing.sample_rate = 44100.0f;
}
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) {
}
RETRO_API void retro_reset(void) {
  assert(initialized);
  Model3->Reset();
}

// Load game
bool game_loaded = false;
Game game;
RETRO_API bool retro_load_game(const struct retro_game_info *rgame) {
  assert(initialized);
  assert(!game_loaded);

  Model3 = new CModel3(s_runtime_config);
  if (! Model3) {
    return false;
  }

  if (Result::OKAY != Model3->Init()) {
    return false;
  }

  ROMSet rom_set;

  GameLoader loader(config_path("Games.xml"));
  if (loader.Load(&game, &rom_set, rgame->path))
    return false;
  if (Model3->LoadGame(game, rom_set) != Result::OKAY)
    return false;
  rom_set = ROMSet();  // free up this memory we won't need anymore

  // Initialize input
  InputSystem = std::shared_ptr<CInputSystem>(new CRetroInputSystem());
  Inputs = new CInputs(InputSystem);
  if (!Inputs->Initialize())
  {
    return false;
  }
  Inputs->LoadFromConfig(s_runtime_config);
  Model3->AttachInputs(Inputs);

  // Initialize the renderers
  SuperAA* superAA = new SuperAA(1, CRTcolor::None);
  superAA->Init(SUPERMODEL_W, SUPERMODEL_H);  // pass actual frame sizes here
  CRender2D *Render2D = new CRender2D(s_runtime_config);
  IRender3D *Render3D = new New3D::CNew3D(s_runtime_config, Model3->GetGame().name);
  UpscaleMode upscaleMode = (UpscaleMode)s_runtime_config["UpscaleMode"].ValueAs<int>();

  if (Result::OKAY != Render2D->Init(0, 0, SUPERMODEL_W, SUPERMODEL_H, SUPERMODEL_W, SUPERMODEL_H, superAA->GetTargetID(), upscaleMode))
    return false;
  if (Result::OKAY != Render3D->Init(0, 0, SUPERMODEL_W, SUPERMODEL_H, SUPERMODEL_W, SUPERMODEL_H, superAA->GetTargetID()))
    return false;

  Model3->AttachRenderers(Render2D, Render3D, superAA);

  // Reset emulator
  Model3->Reset();

  game_loaded = true;
  return game_loaded;
}
RETRO_API void retro_unload_game(void) {
  assert(game_loaded);
  delete Model3;
  delete Inputs;
  game_loaded = false;
}
RETRO_API bool retro_load_game_special(
  unsigned game_type,
  const struct retro_game_info *info, size_t num_info
) {
  return false;
}
RETRO_API unsigned retro_get_region(void) {
  return RETRO_REGION_NTSC;
}

// Run
uint8_t pixels[SUPERMODEL_W * SUPERMODEL_H * 4];
RETRO_API void retro_run(void) {
  assert(game_loaded);
  Inputs->Poll(&game, 0, 0, SUPERMODEL_W, SUPERMODEL_H);
  Model3->RunFrame();
  SDL_GL_SwapWindow(r_window);
  glReadPixels(0, 0, SUPERMODEL_W, SUPERMODEL_H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  cb_video_refresh(pixels, SUPERMODEL_W, SUPERMODEL_H, SUPERMODEL_W * 4);
}

// Audio
const unsigned audio_max_samples = 735;
int16_t audio_buffer[audio_max_samples * 2] = {0};
int16_t fTo16(float f1, float f2) {
  int32_t d = (int32_t)((f1 + f2)*0.5f);
  if (d > INT16_MAX) {
    d = INT16_MAX;
  } else if (d < INT16_MIN) {
    d = INT16_MIN;
  }
  return (int16_t)d;
}
bool OutputAudio(unsigned numSamples, const float* leftFrontBuffer, const float* rightFrontBuffer, const float* leftRearBuffer, const float* rightRearBuffer, bool flipStereo) {
  numSamples = std::min(numSamples, audio_max_samples);
  for (unsigned i = 0; i < numSamples; i++) {
    audio_buffer[i * 2] = fTo16(leftFrontBuffer[i], leftRearBuffer[i]);
    audio_buffer[i * 2 + 1] = fTo16(rightFrontBuffer[i], rightRearBuffer[i]);
  }
  cb_audio_sample_batch(audio_buffer, numSamples);
  return true;
}

// State
static const int STATE_FILE_VERSION = 5;  // save state file version
const char *savestate_path = ".supermodel.st";
bool serialize_size_called = false;
RETRO_API size_t retro_serialize_size(void) {
  assert(!serialize_size_called);
  CBlockFile  SaveState;

  if (Result::OKAY != SaveState.Create(savestate_path, "Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    return 0;
  }
  int32_t fileVersion = STATE_FILE_VERSION;
  SaveState.Write(&fileVersion, sizeof(fileVersion));
  SaveState.Write(Model3->GetGame().name);
  Model3->SaveState(&SaveState);
  SaveState.Close();

  FILE* file = fopen(savestate_path, "rb");
  if (file == nullptr) {
      return 0;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
      fclose(file);
      return 0;
  }
  long size = ftell(file);
  fclose(file);
  serialize_size_called = true;
  return size;
}
RETRO_API bool retro_serialize(void *data, size_t len) {
  assert(serialize_size_called);
  serialize_size_called = false;
  FILE* file = fopen(savestate_path, "rb");
  if (file == nullptr) {
      return false;
  }
  fread(data, 1, len, file);
  fclose(file);
  remove(savestate_path);
  return true;
}
RETRO_API bool retro_unserialize(const void *data, size_t len) {
  FILE* file = fopen(savestate_path, "wb");
  if (file == nullptr) {
      return false;
  }
  fwrite(data, 1, len, file);
  fclose(file);
  CBlockFile  SaveState;
  if (Result::OKAY != SaveState.Load(savestate_path))
  {
    return false;
  }
  if (Result::OKAY != SaveState.FindBlock("Supermodel Save State"))
  {
    return false;
  }
  int32_t fileVersion;
  SaveState.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != STATE_FILE_VERSION)
  {
    return false;
  }
  Model3->LoadState(&SaveState);
  SaveState.Close();

  remove(savestate_path);
  return true;
}

// Cheats
RETRO_API void retro_cheat_reset(void) {
}
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code) {
}

// Memory
RETRO_API void *retro_get_memory_data(unsigned id) {
  return NULL;
}
RETRO_API size_t retro_get_memory_size(unsigned id) {
  return 0;
}
