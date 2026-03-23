#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <cstring>
#include <vector>
#include "../Pkgs/libretro.h"
#include "Version.h"
#include <GL/glew.h>
#include "Supermodel.h"
#include "Util/Format.h"
#include "Util/ConfigBuilders.h"
#include "OSD/FileSystemPath.h"
#include "GameLoader.h"
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
  static struct retro_core_option_v2_category option_cats_us[] = {
    {
      "video",
      "Video",
      "Configure video backend and internal scaling."
    },
    {
      "input",
      "Input",
      "Select the control profile used by the core."
    },
    { NULL, NULL, NULL }
  };
  static struct retro_core_option_v2_definition option_defs_us[] = {
    {
      "supermodel_supersampling",
      "Supersampling",
      NULL,
      "Render internally at 1x to 8x using SuperAA.",
      NULL,
      "video",
      {
        { "1x", NULL },
        { "2x", NULL },
        { "3x", NULL },
        { "4x", NULL },
        { "5x", NULL },
        { "6x", NULL },
        { "7x", NULL },
        { "8x", NULL },
        { NULL, NULL }
      },
      "1x"
    },
    {
      "supermodel_input_profile",
      "Input Profile",
      NULL,
      "Select how Supermodel maps RetroArch inputs.",
      NULL,
      "input",
      {
        { "auto", "Auto" },
        { "joypad", "Joypad" },
        { "lightgun", "Lightgun" },
        { "mouse", "Mouse" },
        { NULL, NULL }
      },
      "auto"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL }
  };
  static struct retro_core_options_v2 options = {
    option_cats_us,
    option_defs_us
  };
  cb_env(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options);

  static const struct retro_controller_description port0_types[] = {
    { "RetroPad", RETRO_DEVICE_JOYPAD },
    { "Mouse", RETRO_DEVICE_MOUSE },
    { "Light Gun", RETRO_DEVICE_LIGHTGUN },
    { NULL, 0 }
  };
  static const struct retro_controller_info port0_info[] = {
    { port0_types, 3 },
    { NULL, 0 }
  };
  cb_env(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)port0_info);
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

enum class InputProfileMode
{
  Auto,
  Joypad,
  Lightgun,
  Mouse
};

// Init
bool initialized = false;
IEmulator *Model3 = nullptr;
std::shared_ptr<CInputSystem> InputSystem;
CInputs *Inputs = nullptr;
static Util::Config::Node s_runtime_config("Global");
const unsigned SUPERMODEL_W = 496;
const unsigned SUPERMODEL_H = 384;
static bool hw_context_ready = false;
static bool renderers_ready = false;
static bool reset_pending = false;
static bool game_loaded = false;
static int supersampling = 1;
static unsigned output_width = SUPERMODEL_W;
static unsigned output_height = SUPERMODEL_H;
static unsigned x_offset = 0;
static unsigned y_offset = 0;
static unsigned x_res = SUPERMODEL_W;
static unsigned y_res = SUPERMODEL_H;
static unsigned total_x_res = SUPERMODEL_W;
static unsigned total_y_res = SUPERMODEL_H;
static unsigned reported_width = 0;
static unsigned reported_height = 0;
static SuperAA *superAA = nullptr;
static CRender2D *Render2D = nullptr;
static IRender3D *Render3D = nullptr;
static Game game;
static ROMSet rom_set;
static retro_hw_render_callback hw_render_cb = {};
static bool racer_profile_active = false;
static InputProfileMode input_profile_mode = InputProfileMode::Auto;
static unsigned frontend_controller_device = RETRO_DEVICE_JOYPAD;
static bool input_mode_dirty = true;
static std::string save_directory;
static int pointer_x = 248;
static int pointer_y = 192;

struct PointerOverlayVertex
{
  float x;
  float y;
};

struct PointerOverlayGL
{
  GLuint program = 0;
  GLuint vao = 0;
  GLuint vbo = 0;
  GLint resolution_loc = -1;
  GLint color_loc = -1;
  bool ready = false;
};

static PointerOverlayGL pointer_overlay;

static bool InitializeRenderers(bool reset_model);
static void UpdateInputDescriptors(void);
static void DrawPointerOverlay(void);
static void LoadNVRAMFromDisk(void);
static void SaveNVRAMToDisk(void);

static InputProfileMode GetAutoInputProfile(void)
{
  if (game.inputs & Game::INPUT_VEHICLE)
    return InputProfileMode::Joypad;

  if (game.inputs & (Game::INPUT_GUN1 | Game::INPUT_GUN2 | Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2))
  {
    if (frontend_controller_device == RETRO_DEVICE_MOUSE)
      return InputProfileMode::Mouse;
    if (frontend_controller_device == RETRO_DEVICE_LIGHTGUN)
      return InputProfileMode::Lightgun;
    return InputProfileMode::Lightgun;
  }

  if (frontend_controller_device == RETRO_DEVICE_MOUSE)
    return InputProfileMode::Mouse;
  if (frontend_controller_device == RETRO_DEVICE_LIGHTGUN)
    return InputProfileMode::Lightgun;

  return InputProfileMode::Joypad;
}

static InputProfileMode GetEffectiveInputProfile(void)
{
  switch (input_profile_mode)
  {
    case InputProfileMode::Auto:
      return GetAutoInputProfile();
    case InputProfileMode::Joypad:
    case InputProfileMode::Lightgun:
    case InputProfileMode::Mouse:
      return input_profile_mode;
  }
  return InputProfileMode::Joypad;
}

static bool EnsureSaveDirectory(void)
{
  if (!save_directory.empty())
    return true;

  const char *path = nullptr;
  if (cb_env != nullptr && cb_env(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &path) && path != nullptr && path[0] != '\0')
  {
    save_directory = path;
    return true;
  }

  if (cb_env != nullptr && cb_env(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &path) && path != nullptr && path[0] != '\0')
  {
    save_directory = Util::Format() << path << "/Supermodel/NVRAM";
    return true;
  }

  return false;
}

static std::string GetNVRAMPath(void)
{
  if (!EnsureSaveDirectory())
    return std::string();
  return Util::Format() << save_directory << "/" << game.name << ".nv";
}

static void MarkInputModeDirty(void)
{
  input_mode_dirty = true;
}

static void UpdateOutputGeometry(void)
{
  x_res = SUPERMODEL_W;
  y_res = SUPERMODEL_H;
}

static void PushRetroGeometry(void)
{
  if (cb_env == nullptr)
    return;

  retro_game_geometry geom = {};
  geom.base_width = SUPERMODEL_W;
  geom.base_height = SUPERMODEL_H;
  geom.max_width = std::max(total_x_res, SUPERMODEL_W * 8u);
  geom.max_height = std::max(total_y_res, SUPERMODEL_H * 8u);
  geom.aspect_ratio = (float)SUPERMODEL_W / (float)SUPERMODEL_H;
  cb_env(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
  reported_width = total_x_res;
  reported_height = total_y_res;
}

static bool RefreshOutputGeometry(void)
{
  UpdateOutputGeometry();

  GLint viewport[4] = { 0, 0, 0, 0 };
  glGetIntegerv(GL_VIEWPORT, viewport);
  if (viewport[2] > 0 && viewport[3] > 0)
  {
    total_x_res = (unsigned)viewport[2];
    total_y_res = (unsigned)viewport[3];
    x_offset = (unsigned)viewport[0];
    y_offset = (unsigned)viewport[1];
  }
  else
  {
    total_x_res = SUPERMODEL_W;
    total_y_res = SUPERMODEL_H;
    x_offset = 0;
    y_offset = 0;
  }

  if (superAA != nullptr)
    superAA->SetOutputSize((int)total_x_res, (int)total_y_res);
  if (reported_width != total_x_res || reported_height != total_y_res)
    PushRetroGeometry();
  return false;
}

static GLuint CompileOverlayShader(GLenum type, const char *source)
{
  GLuint shader = glCreateShader(type);
  if (shader == 0)
    return 0;

  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled != GL_TRUE)
  {
    GLint log_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
    std::vector<char> log((size_t)std::max(1, log_len), '\0');
    glGetShaderInfoLog(shader, log_len, NULL, log.data());
    ErrorLog("Pointer overlay shader compile failed: %s\n", log.data());
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static bool InitializePointerOverlay(void)
{
  if (pointer_overlay.ready)
    return true;

  static const char *vertex_source = R"glsl(
    #version 330 core
    layout(location = 0) in vec2 in_pos;
    uniform vec2 u_resolution;

    void main(void)
    {
      vec2 zero_to_one = in_pos / u_resolution;
      vec2 zero_to_two = zero_to_one * 2.0;
      vec2 clip = zero_to_two - 1.0;
      gl_Position = vec4(clip.x, 1.0 - clip.y, 0.0, 1.0);
    }
  )glsl";

  static const char *fragment_source = R"glsl(
    #version 330 core
    uniform vec4 u_color;
    out vec4 frag_color;

    void main(void)
    {
      frag_color = u_color;
    }
  )glsl";

  GLuint vertex_shader = CompileOverlayShader(GL_VERTEX_SHADER, vertex_source);
  if (vertex_shader == 0)
    return false;

  GLuint fragment_shader = CompileOverlayShader(GL_FRAGMENT_SHADER, fragment_source);
  if (fragment_shader == 0)
  {
    glDeleteShader(vertex_shader);
    return false;
  }

  GLuint program = glCreateProgram();
  if (program == 0)
  {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return false;
  }

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE)
  {
    GLint log_len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
    std::vector<char> log((size_t)std::max(1, log_len), '\0');
    glGetProgramInfoLog(program, log_len, NULL, log.data());
    ErrorLog("Pointer overlay shader link failed: %s\n", log.data());
    glDeleteProgram(program);
    return false;
  }

  glGenVertexArrays(1, &pointer_overlay.vao);
  glGenBuffers(1, &pointer_overlay.vbo);
  glBindVertexArray(pointer_overlay.vao);
  glBindBuffer(GL_ARRAY_BUFFER, pointer_overlay.vbo);
  glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PointerOverlayVertex), (void*)0);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);

  pointer_overlay.program = program;
  pointer_overlay.resolution_loc = glGetUniformLocation(pointer_overlay.program, "u_resolution");
  pointer_overlay.color_loc = glGetUniformLocation(pointer_overlay.program, "u_color");
  pointer_overlay.ready = true;
  return true;
}

static void DestroyPointerOverlay(void)
{
  if (pointer_overlay.vbo != 0)
  {
    glDeleteBuffers(1, &pointer_overlay.vbo);
    pointer_overlay.vbo = 0;
  }
  if (pointer_overlay.vao != 0)
  {
    glDeleteVertexArrays(1, &pointer_overlay.vao);
    pointer_overlay.vao = 0;
  }
  if (pointer_overlay.program != 0)
  {
    glDeleteProgram(pointer_overlay.program);
    pointer_overlay.program = 0;
  }

  pointer_overlay.resolution_loc = -1;
  pointer_overlay.color_loc = -1;
  pointer_overlay.ready = false;
}

static void AppendRect(std::vector<PointerOverlayVertex> &verts, float left, float top, float right, float bottom)
{
  verts.push_back({ left, top });
  verts.push_back({ right, top });
  verts.push_back({ right, bottom });
  verts.push_back({ left, top });
  verts.push_back({ right, bottom });
  verts.push_back({ left, bottom });
}

static bool ShouldDrawPointerOverlay(void)
{
  InputProfileMode profile = GetEffectiveInputProfile();
  return profile == InputProfileMode::Lightgun || profile == InputProfileMode::Mouse;
}

static void DrawPointerOverlay(void)
{
  if (!pointer_overlay.ready || !ShouldDrawPointerOverlay())
    return;

  glViewport(0, 0, (GLsizei)total_x_res, (GLsizei)total_y_res);
  float draw_x = (float)x_offset + ((float)pointer_x * (float)total_x_res / (float)SUPERMODEL_W);
  float draw_y = (float)y_offset + ((float)pointer_y * (float)total_y_res / (float)SUPERMODEL_H);

  std::vector<PointerOverlayVertex> verts;
  verts.reserve(24);

  const float outer_length = 14.0f;
  const float outer_thickness = 4.0f;
  const float inner_length = 10.0f;
  const float inner_thickness = 2.0f;

  AppendRect(verts, draw_x - outer_length, draw_y - outer_thickness, draw_x + outer_length, draw_y + outer_thickness);
  AppendRect(verts, draw_x - outer_thickness, draw_y - outer_length, draw_x + outer_thickness, draw_y + outer_length);
  AppendRect(verts, draw_x - inner_length, draw_y - inner_thickness, draw_x + inner_length, draw_y + inner_thickness);
  AppendRect(verts, draw_x - inner_thickness, draw_y - inner_length, draw_x + inner_thickness, draw_y + inner_length);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glUseProgram(pointer_overlay.program);
  glUniform2f(pointer_overlay.resolution_loc, (float)total_x_res, (float)total_y_res);
  glBindVertexArray(pointer_overlay.vao);
  glBindBuffer(GL_ARRAY_BUFFER, pointer_overlay.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(PointerOverlayVertex)), verts.data(), GL_DYNAMIC_DRAW);

  glUniform4f(pointer_overlay.color_loc, 0.0f, 0.0f, 0.0f, 1.0f);
  glDrawArrays(GL_TRIANGLES, 0, 12);
  glUniform4f(pointer_overlay.color_loc, 1.0f, 1.0f, 1.0f, 1.0f);
  glDrawArrays(GL_TRIANGLES, 12, 12);

  glBindVertexArray(0);
  glUseProgram(0);
}

static void ApplyGLGeometry(void)
{
  const unsigned internal_x_offset = 0;
  const unsigned internal_y_offset = 0;
  const unsigned internal_x_res = SUPERMODEL_W * (unsigned)supersampling;
  const unsigned internal_y_res = SUPERMODEL_H * (unsigned)supersampling;
  const unsigned internal_total_x_res = internal_x_res;
  const unsigned internal_total_y_res = internal_y_res;
  const UINT32 correction = (UINT32)((((double)internal_y_res / 384.0) * 2.0) + 0.5);

  glViewport(0, 0, internal_total_x_res, internal_total_y_res);
  glEnable(GL_SCISSOR_TEST);
  glScissor(internal_x_offset + correction, internal_y_offset + correction, internal_x_res - (correction * 2), internal_y_res - (correction * 2));
}

static void DestroyRenderers(void)
{
  DestroyPointerOverlay();
  if (Render3D != nullptr)
  {
    delete Render3D;
    Render3D = nullptr;
  }
  if (Render2D != nullptr)
  {
    delete Render2D;
    Render2D = nullptr;
  }
  if (superAA != nullptr)
  {
    delete superAA;
    superAA = nullptr;
  }
  renderers_ready = false;
}

static bool ApplySupersamplingOption(void)
{
  int previous = supersampling;
  struct retro_variable var = { "supermodel_supersampling", NULL };
  supersampling = 1;
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL)
  {
    int aa = std::atoi(var.value);
    if (aa >= 1 && aa <= 8)
      supersampling = aa;
  }
  s_runtime_config.Set("Supersampling", supersampling, "Video", 1, 8);
  return supersampling != previous;
}

static bool InitGLState(void)
{
  GLenum err;

  glewExperimental = GL_TRUE;
  err = glewInit();
  if (GLEW_OK != err)
  {
    ErrorLog("OpenGL initialization failed: %s\n", glewGetErrorString(err));
    return false;
  }

  GLint viewport[4] = { 0, 0, 0, 0 };
  glGetIntegerv(GL_VIEWPORT, viewport);
  if (viewport[2] > 0 && viewport[3] > 0)
  {
    output_width = (unsigned)viewport[2];
    output_height = (unsigned)viewport[3];
  }
  UpdateOutputGeometry();
  PushRetroGeometry();

  GLint profile = 0;
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
  printf("GPU info: %s ", glGetString(GL_VERSION));
  if (profile & GL_CONTEXT_CORE_PROFILE_BIT)
    printf("(core profile)");
  if (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
    printf("(compatibility profile)");
  printf("\n\n");

  glClearColor(0.0,0.0,0.0,0.0);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  ApplyGLGeometry();

  for (int i = 0; i < 3; i++)
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  hw_context_ready = true;
  return true;
}

static void retro_context_reset(void)
{
  if (!InitGLState())
    return;
  if (!InitializeRenderers(false))
    return;
  if (reset_pending && Model3 != nullptr)
  {
    Model3->Reset();
    reset_pending = false;
  }
}

static void retro_context_destroy(void)
{
  hw_context_ready = false;
  DestroyRenderers();
}

static bool RequestHWContext(void)
{
  unsigned preferred = RETRO_HW_CONTEXT_OPENGL_CORE;
  cb_env(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred);
  if (preferred != RETRO_HW_CONTEXT_OPENGL && preferred != RETRO_HW_CONTEXT_OPENGL_CORE)
    preferred = RETRO_HW_CONTEXT_OPENGL_CORE;

  retro_hw_render_callback hw_render = {};
  hw_render.context_type = (enum retro_hw_context_type)preferred;
  hw_render.context_reset = retro_context_reset;
  hw_render.context_destroy = retro_context_destroy;
  hw_render.depth = true;
  hw_render.stencil = true;
  hw_render.bottom_left_origin = true;
  hw_render.version_major = 3;
  hw_render.version_minor = 0;
  hw_render.cache_context = true;
  hw_render.debug_context = false;

  if (!cb_env(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
  {
    ErrorLog("RetroArch rejected hardware rendering context");
    return false;
  }
  hw_render_cb = hw_render;
  return true;
}

static void BindCurrentFramebuffer(void)
{
  if (hw_render_cb.get_current_framebuffer != nullptr)
  {
    GLuint framebuffer = (GLuint)hw_render_cb.get_current_framebuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
  }
}

void BindSupermodelDefaultFramebuffer(void)
{
  if (hw_render_cb.get_current_framebuffer != nullptr)
  {
    GLuint framebuffer = (GLuint)hw_render_cb.get_current_framebuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
  }
  else
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
  }
}

static bool InitializeRenderers(bool reset_model)
{
  if (!hw_context_ready || Model3 == nullptr || renderers_ready)
    return renderers_ready;

  const unsigned internal_x_offset = 0;
  const unsigned internal_y_offset = 0;
  const unsigned internal_x_res = SUPERMODEL_W * (unsigned)supersampling;
  const unsigned internal_y_res = SUPERMODEL_H * (unsigned)supersampling;
  const unsigned internal_total_x_res = internal_x_res;
  const unsigned internal_total_y_res = internal_y_res;

  ApplyGLGeometry();

  superAA = new SuperAA(supersampling, CRTcolor::None);
  superAA->Init((int)SUPERMODEL_W, (int)SUPERMODEL_H);
  superAA->SetOutputSize((int)total_x_res, (int)total_y_res);

  Render2D = new CRender2D(s_runtime_config);
  Render3D = new New3D::CNew3D(s_runtime_config, Model3->GetGame().name);

  UpscaleMode upscaleMode = (UpscaleMode)s_runtime_config["UpscaleMode"].ValueAs<int>();
  if (Result::OKAY != Render2D->Init(internal_x_offset, internal_y_offset, internal_x_res, internal_y_res, internal_total_x_res, internal_total_y_res, superAA->GetTargetID(), upscaleMode))
  {
    ErrorLog("Failed to initialize 2D renderer");
    DestroyRenderers();
    return false;
  }
  if (Result::OKAY != Render3D->Init(internal_x_offset, internal_y_offset, internal_x_res, internal_y_res, internal_total_x_res, internal_total_y_res, superAA->GetTargetID()))
  {
    ErrorLog("Failed to initialize 3D renderer");
    DestroyRenderers();
    return false;
  }

  Model3->AttachRenderers(Render2D, Render3D, superAA);
  InitializePointerOverlay();
  renderers_ready = true;

  if (reset_model)
  {
    Model3->Reset();
    reset_pending = false;
  }
  return true;
}

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

// Input system
#include "Inputs/InputSystem.h"
const JoyDetails theJoystick = {
  "RetroPad",
  8,
  0,
  16,
  false,
  { true, true, true, true, false, false, true, true },
  {
    "Left Stick X",
    "Left Stick Y",
    "Right Stick X",
    "Right Stick Y",
    "Unused Axis 5",
    "Unused Axis 6",
    "Right Trigger",
    "Left Trigger"
  },
  { false, false, false, false, false, false, false, false }
};

// Expose L2/R2 as virtual slider axes so racers can bind them with analog sensitivity.
static uint16_t get_analog_trigger(int joyNum, unsigned id)
{
  uint16_t trigger = (uint16_t)cb_input_state(joyNum,
                                              RETRO_DEVICE_ANALOG,
                                              RETRO_DEVICE_INDEX_ANALOG_BUTTON,
                                              id);

  if (trigger == 0)
    trigger = cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, id) ? 0x7FFF : 0;

  return trigger;
}

class CRetroInputSystem : public CInputSystem
{
  private:
  protected:
  public:
	CRetroInputSystem() : CInputSystem("LibRetro") {}
  void SetMouseVisibility(bool visible) {}
  int GetNumMice() const {
    InputProfileMode profile = GetEffectiveInputProfile();
    return (profile == InputProfileMode::Lightgun || profile == InputProfileMode::Mouse) ? 1 : 0;
  }
  int GetMouseAxisValue(int mseNum, int axisNum) const {
    InputProfileMode profile = GetEffectiveInputProfile();
    if (profile != InputProfileMode::Lightgun && profile != InputProfileMode::Mouse)
      return 0;

    if (axisNum == AXIS_X)
      return pointer_x;
    if (axisNum == AXIS_Y)
      return pointer_y;
    return 0;
  }
  int GetMouseWheelDir(int mseNum) const { return 0; }
  bool IsMouseButPressed(int mseNum, int butNum) const {
    InputProfileMode profile = GetEffectiveInputProfile();
  if (profile != InputProfileMode::Lightgun && profile != InputProfileMode::Mouse)
      return false;

    switch (butNum)
    {
      case 0:
        return profile == InputProfileMode::Lightgun
          ? cb_input_state(mseNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) != 0
          : cb_input_state(mseNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) != 0;
      case 1:
        return profile == InputProfileMode::Lightgun
          ? cb_input_state(mseNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD) != 0
          : cb_input_state(mseNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE) != 0;
      case 2:
        return profile == InputProfileMode::Lightgun
          ? cb_input_state(mseNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A) != 0
          : cb_input_state(mseNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) != 0;
      case 3:
        return profile == InputProfileMode::Lightgun
          ? cb_input_state(mseNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_B) != 0
          : cb_input_state(mseNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_4) != 0;
      default:
        return false;
    }
  }
  const MouseDetails *GetMouseDetails(int mseNum) {
    static MouseDetails details = {};
    InputProfileMode profile = GetEffectiveInputProfile();
    snprintf(details.name, sizeof(details.name), "%s", profile == InputProfileMode::Lightgun ? "RetroArch Lightgun" : "RetroArch Mouse");
    details.isAbsolute = (profile == InputProfileMode::Lightgun || profile == InputProfileMode::Mouse);
    return &details;
  }

  int GetNumKeyboards() const { return 0; }
  int GetKeyIndex(const char *keyName) { return 0; }
  const char *GetKeyName(int keyIndex) { return NULL; }
  bool IsKeyPressed(int kbdNum, int keyIndex) const {
    return false;
  }
  const KeyDetails *GetKeyDetails(int kbdNum) { return NULL; }

  int GetNumJoysticks() const { return 1; }
  int GetJoyAxisValue(int joyNum, int axisNum) const {
    switch (axisNum)
    {
    case AXIS_X:
      return cb_input_state(joyNum, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
    case AXIS_Y:
      return cb_input_state(joyNum, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
    case AXIS_Z:
      return cb_input_state(joyNum, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
    case AXIS_RX:
      return cb_input_state(joyNum, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
    case AXIS_S1:
      return get_analog_trigger(joyNum, RETRO_DEVICE_ID_JOYPAD_R2);
    case AXIS_S2:
      return get_analog_trigger(joyNum, RETRO_DEVICE_ID_JOYPAD_L2);
    default:
      return 0;
    }
  }
  bool IsJoyPOVInDir(int joyNum, int povNum, int povDir) const { return false; }
  const JoyDetails *GetJoyDetails(int joyNum) { return &theJoystick; }
  bool IsJoyButPressed(int joyNum, int butNum) const {
    InputProfileMode profile = GetEffectiveInputProfile();
    if (profile == InputProfileMode::Lightgun)
    {
      switch (butNum)
      {
      case 0:
        return cb_input_state(joyNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) != 0;
      case 1:
        return cb_input_state(joyNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD) != 0;
      case 2:
        return cb_input_state(joyNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SELECT) != 0;
      case 3:
        return cb_input_state(joyNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START) != 0;
      case 10:
        return cb_input_state(joyNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A) != 0;
      case 11:
        return cb_input_state(joyNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_B) != 0;
      default:
        break;
      }
    }
    else if (profile == InputProfileMode::Mouse)
    {
      if (butNum == 10)
        return cb_input_state(joyNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE) != 0;
      if (butNum == 11)
        return cb_input_state(joyNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) != 0;
    }
    return cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, butNum) != 0;
  }

  bool ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd) { return false; }

  bool InitializeSystem() {
    return true;
  }

	bool Poll() {
    cb_input_poll();
    InputProfileMode profile = GetEffectiveInputProfile();
    if (profile == InputProfileMode::Lightgun)
    {
      int raw_x = (int16_t)cb_input_state(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
      int raw_y = (int16_t)cb_input_state(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
      if (m_dispW > 0 && m_dispH > 0)
      {
        pointer_x = (int)(((int64_t)raw_x + 0x8000) * (int64_t)m_dispW / 0x10000);
        pointer_y = (int)(((int64_t)raw_y + 0x8000) * (int64_t)m_dispH / 0x10000);
        if (pointer_x < 0) pointer_x = 0;
        if (pointer_y < 0) pointer_y = 0;
        if (pointer_x >= (int)m_dispW) pointer_x = (int)m_dispW - 1;
        if (pointer_y >= (int)m_dispH) pointer_y = (int)m_dispH - 1;
      }
    }
    else if (profile == InputProfileMode::Mouse)
    {
      int dx = cb_input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
      int dy = cb_input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
      pointer_x += dx;
      pointer_y += dy;
      if (m_dispW > 0 && pointer_x >= (int)m_dispW) pointer_x = (int)m_dispW - 1;
      if (m_dispH > 0 && pointer_y >= (int)m_dispH) pointer_y = (int)m_dispH - 1;
      if (pointer_x < 0) pointer_x = 0;
      if (pointer_y < 0) pointer_y = 0;
    }
    return true;
	}

};

static void ApplyVehicleInputDefaults(void)
{
  if (!racer_profile_active)
    return;

  // Racer-style mapping: left stick for steering, triggers for throttle/brake,
  // and face buttons for shift.
  s_runtime_config.Set<std::string>("InputSteering", "JOY1_XAXIS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputAccelerator", "KEY_UP,JOY1_SLIDER1_POS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputBrake", "KEY_DOWN,JOY1_SLIDER2_POS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputGearShiftUp", "KEY_Y,JOY1_BUTTON1", "Input", "", "");
  s_runtime_config.Set<std::string>("InputGearShiftDown", "KEY_H,JOY1_BUTTON2", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON10", "Input", "", "");
  if (game.inputs & Game::INPUT_HANDBRAKE)
  {
    s_runtime_config.Set<std::string>("InputHandBrake", "KEY_S,JOY1_BUTTON11", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceA", "NONE", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceB", "NONE", "Input", "", "");
  }
  else
  {
    s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON11", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON11", "Input", "", "");
    s_runtime_config.Set<std::string>("InputHandBrake", "NONE", "Input", "", "");
  }
}

static void ApplyGunInputDefaults(void)
{
  if (!(game.inputs & (Game::INPUT_GUN1 | Game::INPUT_GUN2 | Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)))
    return;

  InputProfileMode profile = GetEffectiveInputProfile();
  if (profile != InputProfileMode::Lightgun && profile != InputProfileMode::Mouse)
    return;

  if (profile == InputProfileMode::Mouse)
  {
    s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON10", "Input", "", "");
    s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON10", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON11", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON11", "Input", "", "");
    s_runtime_config.Set<std::string>("InputOffscreen", "KEY_S,JOY1_BUTTON2,MOUSE_MIDDLE_BUTTON", "Input", "", "");
    s_runtime_config.Set<std::string>("InputAutoTrigger", "1", "Input", "", "");
    s_runtime_config.Set<std::string>("InputAutoTrigger2", "1", "Input", "", "");
  }
  else
  {
    s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON10", "Input", "", "");
    s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON10", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON11", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON11", "Input", "", "");
    s_runtime_config.Set<std::string>("InputOffscreen", "KEY_S,JOY1_BUTTON2,MOUSE_MIDDLE_BUTTON", "Input", "", "");
    s_runtime_config.Set<std::string>("InputAutoTrigger", "1", "Input", "", "");
    s_runtime_config.Set<std::string>("InputAutoTrigger2", "1", "Input", "", "");
  }
}

static InputProfileMode ParseInputProfileValue(const char *value)
{
  if (value == nullptr)
    return InputProfileMode::Auto;
  if (strcmp(value, "joypad") == 0)
    return InputProfileMode::Joypad;
  if (strcmp(value, "lightgun") == 0)
    return InputProfileMode::Lightgun;
  if (strcmp(value, "mouse") == 0)
    return InputProfileMode::Mouse;
  return InputProfileMode::Auto;
}

static bool ApplyInputProfileOption(void)
{
  InputProfileMode previous = input_profile_mode;
  struct retro_variable var = { "supermodel_input_profile", NULL };
  input_profile_mode = InputProfileMode::Auto;
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL)
    input_profile_mode = ParseInputProfileValue(var.value);

  if (previous != input_profile_mode)
    MarkInputModeDirty();
  return previous != input_profile_mode;
}

static void UpdateInputDescriptors(void)
{
  static struct retro_input_descriptor desc[32];
  int descriptor_index = 0;

  InputProfileMode profile = GetEffectiveInputProfile();
  memset(desc, 0, sizeof(desc));

  if (profile == InputProfileMode::Lightgun)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT,  "D-Pad Left" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP,    "D-Pad Up" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN,  "D-Pad Down" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT, "D-Pad Right" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER,    "Trigger" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD,     "Reload" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SELECT,     "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START,      "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A,      "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_B,      "Service" };
  }
  else if (profile == InputProfileMode::Mouse)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT,   "Fire" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE, "Reload" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT,  "Aux" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X,      "Aim X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y,      "Aim Y" };
  }
  else if (racer_profile_active)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_X, "Steering" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Brake" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Accelerator" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Service/Handbrake" };
  }
  else
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Y" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" };
  }

  cb_env(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
  input_mode_dirty = false;
}

RETRO_API void retro_init(void) {
  assert(!initialized);
  // Set logger
  // XXX in the future use the retro logging callback
  SetLogger(std::make_shared<CConsoleErrorLogger>());

  std::string config_file = config_path("Supermodel.ini");
  WriteDefaultConfigurationFileIfNotPresent(config_file);
  Util::Config::FromINIFile(&s_runtime_config, config_file);
  s_runtime_config.Set("New3DEngine", true, "Global");
  s_runtime_config.Set("QuadRendering", false, "Global");
  s_runtime_config.Set("MultiThreaded", false, "Core");
  s_runtime_config.Set("GPUMultiThreaded", false, "Core");
  s_runtime_config.Set("NoWhiteFlash", false, "Video");
  s_runtime_config.Set("WideScreen", false, "Video");
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
  s_runtime_config.Set<std::string>("InputServiceA", "JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyUp", "JOY1_BUTTON5", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyDown", "JOY1_BUTTON6", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyLeft", "JOY1_BUTTON7", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyRight", "JOY1_BUTTON8", "Input", "", "");
  s_runtime_config.Set<std::string>("InputShift", "JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputBeat", "JOY1_BUTTON1", "Input", "", "");
  s_runtime_config.Set<std::string>("InputCharge", "JOY1_BUTTON9", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJump", "JOY1_BUTTON2", "Input", "", "");

  ApplySupersamplingOption();

  if (!RequestHWContext())
  {
    return;
  }

  initialized = true;
}

RETRO_API void retro_deinit(void) {
  assert(initialized);
  DestroyRenderers();
  delete Model3;
  Model3 = nullptr;
  delete Inputs;
  Inputs = nullptr;
  InputSystem.reset();
  initialized = false;
  hw_context_ready = false;
  reset_pending = false;
  game_loaded = false;
  racer_profile_active = false;
  input_mode_dirty = true;
}

// Before load_game
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
  info->geometry.base_width = SUPERMODEL_W;
  info->geometry.base_height = SUPERMODEL_H;
  info->geometry.max_width = std::max(total_x_res, SUPERMODEL_W * 8u);
  info->geometry.max_height = std::max(total_y_res, SUPERMODEL_H * 8u);
  info->geometry.aspect_ratio = (float)SUPERMODEL_W / (float)SUPERMODEL_H;
  info->timing.fps = 60.0f;
  info->timing.sample_rate = 44100.0f;
}
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) {
  if (port != 0)
    return;
  if (frontend_controller_device != device)
  {
    frontend_controller_device = device;
    MarkInputModeDirty();
  }
}
RETRO_API void retro_reset(void) {
  assert(initialized);
  if (renderers_ready && Model3 != nullptr)
    Model3->Reset();
  else
    reset_pending = true;
}

RETRO_API bool retro_load_game(const struct retro_game_info *rgame) {
  assert(initialized);
  assert(!game_loaded);

  ApplySupersamplingOption();

  Model3 = new CModel3(s_runtime_config);
  if (! Model3) {
    return false;
  }

  if (Result::OKAY != Model3->Init()) {
    delete Model3;
    Model3 = nullptr;
    return false;
  }

  GameLoader loader(config_path("Games.xml"));
  if (loader.Load(&game, &rom_set, rgame->path))
  {
    delete Model3;
    Model3 = nullptr;
    return false;
  }
  if (Model3->LoadGame(game, rom_set) != Result::OKAY)
  {
    delete Model3;
    Model3 = nullptr;
    return false;
  }
  rom_set = ROMSet();  // free up this memory we won't need anymore

  racer_profile_active = (game.inputs & Game::INPUT_VEHICLE) != 0;
  ApplyInputProfileOption();
  ApplyVehicleInputDefaults();
  ApplyGunInputDefaults();

  // Initialize input
  InputSystem = std::shared_ptr<CInputSystem>(new CRetroInputSystem());
  Inputs = new CInputs(InputSystem);
  if (!Inputs->Initialize())
  {
    delete Model3;
    Model3 = nullptr;
    delete Inputs;
    Inputs = nullptr;
    return false;
  }
  Inputs->LoadFromConfig(s_runtime_config);
  Model3->AttachInputs(Inputs);
  UpdateInputDescriptors();
  pointer_x = (int)(x_res / 2);
  pointer_y = (int)(y_res / 2);
  input_mode_dirty = false;

  reset_pending = true;
  if (hw_context_ready && !InitializeRenderers(true))
  {
    delete Model3;
    Model3 = nullptr;
    delete Inputs;
    Inputs = nullptr;
    return false;
  }

  game_loaded = true;
  LoadNVRAMFromDisk();
  return game_loaded;
}
RETRO_API void retro_unload_game(void) {
  assert(game_loaded);
  SaveNVRAMToDisk();
  DestroyRenderers();
  delete Model3;
  Model3 = nullptr;
  delete Inputs;
  Inputs = nullptr;
  InputSystem.reset();
  rom_set = ROMSet();
  game = Game();
  game_loaded = false;
  reset_pending = false;
  racer_profile_active = false;
  input_mode_dirty = true;
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
RETRO_API void retro_run(void) {
  assert(game_loaded);
  bool variables_updated = false;
  bool refresh_input_mode = input_mode_dirty;
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &variables_updated) && variables_updated)
  {
    bool supersampling_changed = ApplySupersamplingOption();
    if (ApplyInputProfileOption())
      refresh_input_mode = true;
    if (supersampling_changed && renderers_ready)
    {
      DestroyRenderers();
      if (!InitializeRenderers(false))
        return;
    }
  }
  if (refresh_input_mode)
  {
    if (racer_profile_active)
      ApplyVehicleInputDefaults();
    ApplyGunInputDefaults();
    if (Inputs != nullptr)
      Inputs->LoadFromConfig(s_runtime_config);
    UpdateInputDescriptors();
    pointer_x = (int)(x_res / 2);
    pointer_y = (int)(y_res / 2);
  }
  if (!renderers_ready)
  {
    if (!InitializeRenderers(false))
      return;
  }
  BindCurrentFramebuffer();
  RefreshOutputGeometry();
  Inputs->Poll(&game, 0, 0, SUPERMODEL_W, SUPERMODEL_H);
  Model3->RunFrame();
  DrawPointerOverlay();
  glFlush();
  cb_video_refresh(RETRO_HW_FRAME_BUFFER_VALID, total_x_res, total_y_res, 0);
}

// NVRAM
static const int NVRAM_FILE_VERSION = 0;

static void SaveNVRAMToDisk(void)
{
  if (Model3 == nullptr)
    return;

  std::string nvram_path = GetNVRAMPath();
  if (nvram_path.empty())
    return;

  CBlockFile NVRAM;
  if (Result::OKAY != NVRAM.Create(nvram_path, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    ErrorLog("Unable to save NVRAM to '%s'. Make sure directory exists!", nvram_path.c_str());
    return;
  }

  int32_t fileVersion = NVRAM_FILE_VERSION;
  NVRAM.Write(&fileVersion, sizeof(fileVersion));
  NVRAM.Write(Model3->GetGame().name);
  Model3->SaveNVRAM(&NVRAM);
  NVRAM.Close();
}

static void LoadNVRAMFromDisk(void)
{
  if (Model3 == nullptr)
    return;

  std::string nvram_path = GetNVRAMPath();
  if (nvram_path.empty())
    return;

  CBlockFile NVRAM;
  if (Result::OKAY != NVRAM.Load(nvram_path))
    return;

  if (Result::OKAY != NVRAM.FindBlock("Supermodel NVRAM State"))
    return;

  int32_t fileVersion;
  NVRAM.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != NVRAM_FILE_VERSION)
    return;

  Model3->LoadNVRAM(&NVRAM);
  NVRAM.Close();
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
