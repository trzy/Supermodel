#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdio.h>
#include <cstring>
#include <exception>
#include <chrono>
#include <cstdarg>
#include <mutex>
#include <string>
#include <unordered_map>
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
#include "OSD/SDL/Crosshair.h"
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
static void RefreshFrontendInterfaces(void);
RETRO_API void retro_set_environment(retro_environment_t cb) {
  cb_env = cb;
  RefreshFrontendInterfaces();

  static struct retro_core_option_v2_category option_cats_us[] = {
    {
      "video",
      "Video",
      "Configure video backend and internal scaling."
    },
    {
      "audio",
      "Audio",
      "Configure sound compatibility options."
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
      "supermodel_upscale_mode",
      "2D Upscale Mode",
      NULL,
      "Select the filter used when scaling 2D layers.",
      NULL,
      "video",
      {
        { "nearest", "Nearest" },
        { "biquintic", "Biquintic" },
        { "bilinear", "Bilinear" },
        { "bicubic", "Bicubic" },
        { NULL, NULL }
      },
      "bilinear"
    },
    {
      "supermodel_widescreen",
      "Wide Screen",
      NULL,
      "Enable widescreen rendering for supported games.",
      NULL,
      "video",
      {
        { "disabled", "Disabled" },
        { "enabled", "Enabled" },
        { NULL, NULL }
      },
      "disabled"
    },
    {
      "supermodel_quad_rendering",
      "Quad Rendering",
      NULL,
      "Render 3D quads as single primitives when supported.",
      NULL,
      "video",
      {
        { "disabled", "Disabled" },
        { "enabled", "Enabled" },
        { NULL, NULL }
      },
      "disabled"
    },
    {
      "supermodel_no_white_flash",
      "No White Flash",
      NULL,
      "Disable white flash effects used by some 3D block culling paths.",
      NULL,
      "video",
      {
        { "disabled", "Disabled" },
        { "enabled", "Enabled" },
        { NULL, NULL }
      },
      "disabled"
    },
    {
      "supermodel_libretro_crosshair",
      "Libretro Crosshair",
      NULL,
      "Show the vector crosshair overlay drawn by the libretro core.",
      NULL,
      "video",
      {
        { "enabled", "Enabled" },
        { "disabled", "Disabled" },
        { NULL, NULL }
      },
      "disabled"
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
  static const struct retro_controller_description port1_types[] = {
    { "RetroPad", RETRO_DEVICE_JOYPAD },
    { NULL, 0 }
  };
  static const struct retro_controller_info port0_info[] = {
    { port0_types, 3 },
    { port1_types, 1 },
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
static bool fighting_profile_active = false;
static InputProfileMode input_profile_mode = InputProfileMode::Auto;
static unsigned frontend_controller_device = RETRO_DEVICE_JOYPAD;
static bool input_mode_dirty = true;
static bool libretro_crosshair_enabled = false;
static std::string save_directory;
static std::vector<uint8_t> serialized_state_buffer;
static bool serialized_state_ready = false;
static bool frontend_fastforwarding = false;
static bool baseline_throttle_enabled = true;
static bool baseline_vsync_enabled = true;
static int pointer_x = 248;
static int pointer_y = 192;
static CCrosshair *s_crosshair = nullptr;
static retro_log_callback s_retro_log_cb = {};
static retro_log_printf_t s_retro_log_printf = nullptr;
static retro_perf_callback s_retro_perf_cb = {};
static bool s_retro_perf_available = false;
static bool s_retro_perf_registered = false;
static unsigned s_message_interface_version = 0;
static retro_perf_counter s_perf_frame_total = { "supermodel.frame_total" };
static retro_perf_counter s_perf_input_poll = { "supermodel.input_poll" };
static retro_perf_counter s_perf_emulate_frame = { "supermodel.emulate_frame" };
static retro_perf_counter s_perf_video_submit = { "supermodel.video_submit" };
static retro_perf_counter s_perf_serialize = { "supermodel.serialize" };
static retro_perf_counter s_perf_unserialize = { "supermodel.unserialize" };

struct ThrottleEntry
{
  std::chrono::steady_clock::time_point last_emit;
  unsigned suppressed = 0;
  bool seen = false;
};

struct ThrottleDecision
{
  bool emit = true;
  unsigned suppressed = 0;
};

static std::unordered_map<std::string, ThrottleEntry> s_log_throttle_entries;
static std::unordered_map<std::string, ThrottleEntry> s_runtime_message_throttle_entries;
static std::mutex s_throttle_mutex;
static const uint64_t LOG_THROTTLE_MS_DEFAULT = 1000;
static const uint64_t MESSAGE_THROTTLE_MS_DEFAULT = 800;
static const size_t THROTTLE_TABLE_MAX_ENTRIES = 512;

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

static std::string FormatStringV(const char *fmt, va_list args)
{
  char buffer[4096];
  int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
  if (written < 0)
    return std::string("Formatting error");
  if ((size_t)written >= sizeof(buffer))
    return std::string(buffer, buffer + sizeof(buffer) - 1);
  return std::string(buffer);
}

static const char *LogLevelName(enum retro_log_level level)
{
  switch (level)
  {
    case RETRO_LOG_DEBUG: return "DEBUG";
    case RETRO_LOG_INFO:  return "INFO";
    case RETRO_LOG_WARN:  return "WARN";
    case RETRO_LOG_ERROR: return "ERROR";
    default:              return "LOG";
  }
}

static ThrottleDecision GetThrottleDecision(std::unordered_map<std::string, ThrottleEntry> &table, const std::string &key, uint64_t min_interval_ms)
{
  std::lock_guard<std::mutex> lock(s_throttle_mutex);

  if (table.size() > THROTTLE_TABLE_MAX_ENTRIES)
    table.clear();

  ThrottleDecision decision;
  ThrottleEntry &entry = table[key];
  auto now = std::chrono::steady_clock::now();

  if (entry.seen)
  {
    uint64_t elapsed_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.last_emit).count();
    if (elapsed_ms < min_interval_ms)
    {
      entry.suppressed++;
      decision.emit = false;
      return decision;
    }
    decision.suppressed = entry.suppressed;
  }

  entry.last_emit = now;
  entry.suppressed = 0;
  entry.seen = true;
  return decision;
}

static void EmitFrontendLogDirect(enum retro_log_level level, const std::string &message)
{
  if (s_retro_log_printf != nullptr)
  {
    s_retro_log_printf(level, "%s\n", message.c_str());
    return;
  }

  fprintf(stderr, "SM[%s]: %s\n", LogLevelName(level), message.c_str());
}

static void EmitFrontendLogThrottledText(const std::string &key, enum retro_log_level level, uint64_t min_interval_ms, const std::string &message)
{
  ThrottleDecision decision = GetThrottleDecision(s_log_throttle_entries, key, min_interval_ms);
  if (!decision.emit)
    return;

  if (decision.suppressed > 0)
  {
    char summary[256];
    snprintf(summary, sizeof(summary), "[dedupe] Suppressed %u repeated entries for key '%s'.", decision.suppressed, key.c_str());
    EmitFrontendLogDirect(level, summary);
  }

  EmitFrontendLogDirect(level, message);
}

static void EmitFrontendLogThrottledV(const char *key, enum retro_log_level level, uint64_t min_interval_ms, const char *fmt, va_list args)
{
  std::string message = FormatStringV(fmt, args);
  std::string dedupe_key = key != nullptr ? std::string(key) : (std::to_string((int)level) + ":" + message);
  EmitFrontendLogThrottledText(dedupe_key, level, min_interval_ms, message);
}

static void EmitFrontendLogThrottled(const char *key, enum retro_log_level level, uint64_t min_interval_ms, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  EmitFrontendLogThrottledV(key, level, min_interval_ms, fmt, args);
  va_end(args);
}

static void PushRuntimeMessage(const std::string &message, enum retro_log_level level, unsigned duration_ms, unsigned priority)
{
  if (cb_env == nullptr || message.empty())
    return;

  if (s_message_interface_version > 0)
  {
    retro_message_ext ext = {};
    ext.msg = message.c_str();
    ext.duration = duration_ms;
    ext.priority = priority;
    ext.level = level;
    ext.target = RETRO_MESSAGE_TARGET_OSD;
    ext.type = RETRO_MESSAGE_TYPE_STATUS;
    ext.progress = -1;
    if (cb_env(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &ext))
      return;
  }

  retro_message legacy = {};
  legacy.msg = message.c_str();
  legacy.frames = std::max(1u, (duration_ms + 16u) / 17u);
  cb_env(RETRO_ENVIRONMENT_SET_MESSAGE, &legacy);
}

static void PushRuntimeMessageThrottledV(const char *key, enum retro_log_level level, unsigned duration_ms, unsigned priority, uint64_t min_interval_ms, const char *fmt, va_list args)
{
  std::string message = FormatStringV(fmt, args);
  std::string dedupe_key = key != nullptr ? std::string(key) : message;
  ThrottleDecision decision = GetThrottleDecision(s_runtime_message_throttle_entries, dedupe_key, min_interval_ms);
  if (!decision.emit)
    return;
  PushRuntimeMessage(message, level, duration_ms, priority);
}

static void PushRuntimeMessageThrottled(const char *key, enum retro_log_level level, unsigned duration_ms, unsigned priority, uint64_t min_interval_ms, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  PushRuntimeMessageThrottledV(key, level, duration_ms, priority, min_interval_ms, fmt, args);
  va_end(args);
}

static void RefreshFrontendInterfaces(void)
{
  s_retro_log_cb = {};
  s_retro_log_printf = nullptr;
  if (cb_env != nullptr && cb_env(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &s_retro_log_cb) && s_retro_log_cb.log != nullptr)
    s_retro_log_printf = s_retro_log_cb.log;

  s_retro_perf_cb = {};
  s_retro_perf_available = false;
  if (cb_env != nullptr && cb_env(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &s_retro_perf_cb))
  {
    s_retro_perf_available = s_retro_perf_cb.perf_register != nullptr &&
                             s_retro_perf_cb.perf_start != nullptr &&
                             s_retro_perf_cb.perf_stop != nullptr;
  }
  s_retro_perf_registered = false;

  s_message_interface_version = 0;
  if (cb_env != nullptr)
    cb_env(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &s_message_interface_version);
}

static void RegisterPerfCounter(retro_perf_counter *counter)
{
  if (!s_retro_perf_available || counter == nullptr || counter->registered || s_retro_perf_cb.perf_register == nullptr)
    return;
  s_retro_perf_cb.perf_register(counter);
}

static void RegisterPerfCounters(void)
{
  if (!s_retro_perf_available || s_retro_perf_registered)
    return;

  RegisterPerfCounter(&s_perf_frame_total);
  RegisterPerfCounter(&s_perf_input_poll);
  RegisterPerfCounter(&s_perf_emulate_frame);
  RegisterPerfCounter(&s_perf_video_submit);
  RegisterPerfCounter(&s_perf_serialize);
  RegisterPerfCounter(&s_perf_unserialize);
  s_retro_perf_registered = true;
}

static inline void PerfStart(retro_perf_counter *counter)
{
  if (s_retro_perf_available && counter != nullptr && counter->registered && s_retro_perf_cb.perf_start != nullptr)
    s_retro_perf_cb.perf_start(counter);
}

static inline void PerfStop(retro_perf_counter *counter)
{
  if (s_retro_perf_available && counter != nullptr && counter->registered && s_retro_perf_cb.perf_stop != nullptr)
    s_retro_perf_cb.perf_stop(counter);
}

class ScopedPerfCounter
{
public:
  explicit ScopedPerfCounter(retro_perf_counter *counter) : m_counter(counter)
  {
    PerfStart(m_counter);
  }
  ~ScopedPerfCounter()
  {
    PerfStop(m_counter);
  }

private:
  retro_perf_counter *m_counter;
};

class CLibretroFrontendLogger : public CLogger
{
public:
  void DebugLog(const char *fmt, va_list vl) override
  {
    EmitFrontendLogThrottledV(NULL, RETRO_LOG_DEBUG, LOG_THROTTLE_MS_DEFAULT, fmt, vl);
  }

  void InfoLog(const char *fmt, va_list vl) override
  {
    EmitFrontendLogThrottledV(NULL, RETRO_LOG_INFO, LOG_THROTTLE_MS_DEFAULT, fmt, vl);
  }

  void ErrorLog(const char *fmt, va_list vl) override
  {
    EmitFrontendLogThrottledV(NULL, RETRO_LOG_ERROR, LOG_THROTTLE_MS_DEFAULT, fmt, vl);
  }
};

static bool InitializeRenderers(bool reset_model);
static void DestroyCrosshair(void);
static bool InitializeCrosshair(void);
static void UpdateInputDescriptors(void);
static void DrawPointerOverlay(void);
static void LoadNVRAMFromDisk(void);
static void SaveNVRAMToDisk(void);
static InputProfileMode GetEffectiveInputProfile(void);
static void ApplyFastForwardAwareness(bool fastforwarding);

static bool IsGunGame(void)
{
  return (game.inputs & (Game::INPUT_GUN1 | Game::INPUT_GUN2 | Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) != 0;
}

static bool IsGunProfileActive(void)
{
  InputProfileMode profile = GetEffectiveInputProfile();
  return IsGunGame() && (profile == InputProfileMode::Lightgun || profile == InputProfileMode::Mouse || profile == InputProfileMode::Joypad);
}

static InputProfileMode GetAutoInputProfile(void)
{
  if (game.inputs & Game::INPUT_VEHICLE)
    return InputProfileMode::Joypad;

  if (IsGunGame())
  {
    if (frontend_controller_device == RETRO_DEVICE_MOUSE)
      return InputProfileMode::Mouse;
    if (frontend_controller_device == RETRO_DEVICE_LIGHTGUN)
      return InputProfileMode::Lightgun;
    return InputProfileMode::Joypad;
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

static bool IsWideScreenEnabled(void)
{
  return s_runtime_config["WideScreen"].ValueAsDefault<bool>(false);
}

static void UpdateOutputGeometry(void)
{
  x_res = SUPERMODEL_W;
  y_res = SUPERMODEL_H;
  if (IsWideScreenEnabled())
  {
    total_y_res = y_res;
    total_x_res = (unsigned)std::lround((double)total_y_res * 16.0 / 9.0);
    if (total_x_res < x_res)
      total_x_res = x_res;
    x_offset = (total_x_res - x_res) / 2;
    y_offset = 0;
  }
  else
  {
    total_x_res = x_res;
    total_y_res = y_res;
    x_offset = 0;
    y_offset = 0;
  }
}

static void PushRetroGeometry(void)
{
  if (cb_env == nullptr)
    return;

  retro_game_geometry geom = {};
  geom.base_width = IsWideScreenEnabled() ? total_x_res : SUPERMODEL_W;
  geom.base_height = IsWideScreenEnabled() ? total_y_res : SUPERMODEL_H;
  geom.max_width = std::max(total_x_res, SUPERMODEL_W * 8u);
  geom.max_height = std::max(total_y_res, SUPERMODEL_H * 8u);
  geom.aspect_ratio = IsWideScreenEnabled() ? (16.0f / 9.0f) : ((float)SUPERMODEL_W / (float)SUPERMODEL_H);
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
    #version 130
    in vec2 in_pos;
    uniform vec2 u_resolution;

    void main(void)
    {
      vec2 zero_to_one = in_pos / u_resolution;
      vec2 zero_to_two = zero_to_one * 2.0;
      vec2 clip = zero_to_two - 1.0;
      gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    }
  )glsl";

  static const char *fragment_source = R"glsl(
    #version 130
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

  glBindAttribLocation(program, 0, "in_pos");
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
  return libretro_crosshair_enabled;
}

static bool GetPointerOverlayCoords(float *x, float *y)
{
  if (Inputs == nullptr || !IsGunGame())
    return false;

  if ((game.inputs & Game::INPUT_ANALOG_GUN1) && Inputs->analogGunX[0] && Inputs->analogGunY[0])
  {
    *x = (float)Inputs->analogGunX[0]->value / 255.0f;
    *y = (255.0f - (float)Inputs->analogGunY[0]->value) / 255.0f;
    return true;
  }

  if ((game.inputs & Game::INPUT_GUN1) && Inputs->gunX[0] && Inputs->gunY[0])
  {
    *x = ((float)Inputs->gunX[0]->value - 150.0f) / (651.0f - 150.0f);
    *y = ((float)Inputs->gunY[0]->value - 80.0f) / (465.0f - 80.0f);
    return true;
  }

  return false;
}

static void DrawPointerOverlay(void)
{
  if (!pointer_overlay.ready || !ShouldDrawPointerOverlay())
    return;

  GLint saved_viewport[4] = { 0, 0, 0, 0 };
  GLboolean saved_scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
  GLint saved_scissor_box[4] = { 0, 0, 0, 0 };
  GLboolean saved_blend_enabled = glIsEnabled(GL_BLEND);
  GLboolean saved_depth_enabled = glIsEnabled(GL_DEPTH_TEST);
  GLint saved_program = 0;
  GLint saved_vao = 0;
  glGetIntegerv(GL_VIEWPORT, saved_viewport);
  glGetIntegerv(GL_SCISSOR_BOX, saved_scissor_box);
  glGetIntegerv(GL_CURRENT_PROGRAM, &saved_program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &saved_vao);

  glViewport(0, 0, (GLsizei)total_x_res, (GLsizei)total_y_res);
  float norm_x = 0.5f;
  float norm_y = 0.5f;
  if (!GetPointerOverlayCoords(&norm_x, &norm_y))
  {
    norm_x = (float)pointer_x / (float)SUPERMODEL_W;
    norm_y = (float)pointer_y / (float)SUPERMODEL_H;
  }

  if (norm_x < 0.0f) norm_x = 0.0f;
  if (norm_x > 1.0f) norm_x = 1.0f;
  if (norm_y < 0.0f) norm_y = 0.0f;
  if (norm_y > 1.0f) norm_y = 1.0f;

  const float scale_x = (x_res > 0) ? ((float)total_x_res / (float)x_res) : 1.0f;
  const float scale_y = (y_res > 0) ? ((float)total_y_res / (float)y_res) : 1.0f;
  const float overlay_scale = 0.4f;
  float draw_x = ((float)x_offset + norm_x * (float)x_res) * scale_x;
  float draw_y = ((float)y_offset + norm_y * (float)y_res) * scale_y;

  std::vector<PointerOverlayVertex> verts;
  verts.reserve(24);

  const float outer_length = 14.0f * overlay_scale * scale_x;
  const float outer_thickness = 4.0f * overlay_scale * scale_y;
  const float inner_length = 10.0f * overlay_scale * scale_x;
  const float inner_thickness = 2.0f * overlay_scale * scale_y;

  AppendRect(verts, draw_x - outer_length, draw_y - outer_thickness, draw_x + outer_length, draw_y + outer_thickness);
  AppendRect(verts, draw_x - outer_thickness, draw_y - outer_length, draw_x + outer_thickness, draw_y + outer_length);
  AppendRect(verts, draw_x - inner_length, draw_y - inner_thickness, draw_x + inner_length, draw_y + inner_thickness);
  AppendRect(verts, draw_x - inner_thickness, draw_y - inner_length, draw_x + inner_thickness, draw_y + inner_length);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(pointer_overlay.program);
  glUniform2f(pointer_overlay.resolution_loc, (float)total_x_res, (float)total_y_res);
  glBindVertexArray(pointer_overlay.vao);
  glBindBuffer(GL_ARRAY_BUFFER, pointer_overlay.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(PointerOverlayVertex)), verts.data(), GL_DYNAMIC_DRAW);

  glUniform4f(pointer_overlay.color_loc, 0.0f, 0.85f, 1.0f, 0.85f);
  glDrawArrays(GL_TRIANGLES, 0, 12);
  glUniform4f(pointer_overlay.color_loc, 1.0f, 0.15f, 0.85f, 0.95f);
  glDrawArrays(GL_TRIANGLES, 12, 12);

  glBindVertexArray(0);
  glUseProgram(0);
  glDisable(GL_BLEND);

  if (saved_depth_enabled == GL_TRUE)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);

  if (saved_blend_enabled == GL_TRUE)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);

  if (saved_scissor_enabled == GL_TRUE)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);

  glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
  glScissor(saved_scissor_box[0], saved_scissor_box[1], saved_scissor_box[2], saved_scissor_box[3]);
  glUseProgram((GLuint)saved_program);
  glBindVertexArray((GLuint)saved_vao);
}

static void DestroyCrosshair(void)
{
  delete s_crosshair;
  s_crosshair = nullptr;
}

static bool InitializeCrosshair(void)
{
  if (!IsGunGame())
  {
    DestroyCrosshair();
    return true;
  }

  DestroyCrosshair();
  s_runtime_config.Set("Crosshairs", int(3), "Video", 0, 0, { 0,1,2,3 });
  s_crosshair = new CCrosshair(s_runtime_config);
  if (s_crosshair->Init() != Result::OKAY)
  {
    ErrorLog("Unable to initialize native crosshair.\n");
    DestroyCrosshair();
    return false;
  }
  return true;
}

static void ApplyGLGeometry(void)
{
  const unsigned internal_x_offset = x_offset * (unsigned)supersampling;
  const unsigned internal_y_offset = y_offset * (unsigned)supersampling;
  const unsigned internal_x_res = x_res * (unsigned)supersampling;
  const unsigned internal_y_res = y_res * (unsigned)supersampling;
  const unsigned internal_total_x_res = total_x_res * (unsigned)supersampling;
  const unsigned internal_total_y_res = total_y_res * (unsigned)supersampling;
  const UINT32 correction = (UINT32)((((double)internal_y_res / 384.0) * 2.0) + 0.5);

  glViewport(0, 0, internal_x_res, internal_y_res);
  glEnable(GL_SCISSOR_TEST);
  if (IsWideScreenEnabled())
    glScissor(0, correction, internal_total_x_res, internal_total_y_res - (correction * 2));
  else
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
  if (supersampling != previous && game_loaded)
  {
    PushRuntimeMessageThrottled("video.supersampling", RETRO_LOG_INFO, 1400, 1, MESSAGE_THROTTLE_MS_DEFAULT,
      "Supersampling set to %dx", supersampling);
    EmitFrontendLogThrottled("video.supersampling", RETRO_LOG_INFO, LOG_THROTTLE_MS_DEFAULT,
      "Supersampling changed from %dx to %dx", previous, supersampling);
  }
  return supersampling != previous;
}

static bool ParseEnabledOption(const char *value)
{
  return value != NULL && strcmp(value, "enabled") == 0;
}

static void ApplyFastForwardAwareness(bool fastforwarding)
{
  if (frontend_fastforwarding == fastforwarding)
    return;

  frontend_fastforwarding = fastforwarding;
  s_runtime_config.Get("Throttle").SetValue(frontend_fastforwarding ? false : baseline_throttle_enabled);
  s_runtime_config.Get("VSync").SetValue(frontend_fastforwarding ? false : baseline_vsync_enabled);
  EmitFrontendLogThrottled("run.fastforward", RETRO_LOG_DEBUG, LOG_THROTTLE_MS_DEFAULT,
    "Fast-forward state changed: %s", frontend_fastforwarding ? "enabled" : "disabled");
}

static bool ApplyVideoCoreOptions(void)
{
  bool changed = false;
  struct retro_variable var = { NULL, NULL };

  var.key = "supermodel_upscale_mode";
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL)
  {
    int mode = 2;
    if (strcmp(var.value, "nearest") == 0)
      mode = 0;
    else if (strcmp(var.value, "biquintic") == 0)
      mode = 1;
    else if (strcmp(var.value, "bilinear") == 0)
      mode = 2;
    else if (strcmp(var.value, "bicubic") == 0)
      mode = 3;
    if (s_runtime_config["UpscaleMode"].ValueAs<int>() != mode)
    {
      s_runtime_config.Get("UpscaleMode").SetValue(mode);
      changed = true;
    }
  }

  var.key = "supermodel_widescreen";
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL)
  {
    bool wide = ParseEnabledOption(var.value);
    if (s_runtime_config["WideScreen"].ValueAs<bool>() != wide)
    {
      s_runtime_config.Get("WideScreen").SetValue(wide);
      changed = true;
    }
  }

  var.key = "supermodel_quad_rendering";
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL)
  {
    bool quad = ParseEnabledOption(var.value);
    if (s_runtime_config["QuadRendering"].ValueAs<bool>() != quad)
    {
      s_runtime_config.Get("QuadRendering").SetValue(quad);
      changed = true;
    }
  }

  var.key = "supermodel_no_white_flash";
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL)
  {
    bool no_white = ParseEnabledOption(var.value);
    if (s_runtime_config["NoWhiteFlash"].ValueAs<bool>() != no_white)
    {
      s_runtime_config.Get("NoWhiteFlash").SetValue(no_white);
      changed = true;
    }
  }

  return changed;
}

static bool ApplyCrosshairOverlayOption(void)
{
  bool changed = false;
  struct retro_variable var = { "supermodel_libretro_crosshair", NULL };
  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != NULL)
  {
    bool show_overlay = ParseEnabledOption(var.value);
    if (libretro_crosshair_enabled != show_overlay)
    {
      libretro_crosshair_enabled = show_overlay;
      changed = true;
    }
  }
  return changed;
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

  UpdateOutputGeometry();
  const unsigned internal_x_offset = x_offset * (unsigned)supersampling;
  const unsigned internal_y_offset = y_offset * (unsigned)supersampling;
  const unsigned internal_x_res = x_res * (unsigned)supersampling;
  const unsigned internal_y_res = y_res * (unsigned)supersampling;
  const unsigned internal_total_x_res = total_x_res * (unsigned)supersampling;
  const unsigned internal_total_y_res = total_y_res * (unsigned)supersampling;

  PushRetroGeometry();
  ApplyGLGeometry();

  superAA = new SuperAA(supersampling, CRTcolor::None);
  superAA->Init((int)total_x_res, (int)total_y_res);
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
    return IsGunProfileActive() ? 1 : 0;
  }
  int GetMouseAxisValue(int mseNum, int axisNum) const {
    if (!IsGunProfileActive())
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
    if (!IsGunProfileActive())
      return false;

    switch (butNum)
    {
      case 0:
        if (profile == InputProfileMode::Lightgun)
          return cb_input_state(mseNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) != 0;
        if (profile == InputProfileMode::Mouse)
          return cb_input_state(mseNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) != 0;
        return false;
      case 1:
        if (profile == InputProfileMode::Lightgun)
          return cb_input_state(mseNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD) != 0;
        if (profile == InputProfileMode::Mouse)
          return cb_input_state(mseNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE) != 0;
        return false;
      case 2:
        if (profile == InputProfileMode::Lightgun)
          return cb_input_state(mseNum, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD) != 0;
        if (profile == InputProfileMode::Mouse)
          return cb_input_state(mseNum, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) != 0;
        return false;
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
    details.isAbsolute = IsGunProfileActive();
    return &details;
  }

  int GetNumKeyboards() const { return 0; }
  int GetKeyIndex(const char *keyName) { return 0; }
  const char *GetKeyName(int keyIndex) { return NULL; }
  bool IsKeyPressed(int kbdNum, int keyIndex) const {
    return false;
  }
  const KeyDetails *GetKeyDetails(int kbdNum) { return NULL; }

  int GetNumJoysticks() const { return fighting_profile_active ? 2 : 1; }
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
    if (IsGunProfileActive() && profile == InputProfileMode::Lightgun)
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
        return cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) != 0;
      case 11:
        return cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) != 0;
      default:
        return cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, butNum) != 0;
      }
    }
    else if (IsGunProfileActive() && profile == InputProfileMode::Mouse)
    {
      switch (butNum)
      {
      case 10:
        return cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) != 0;
      case 11:
        return cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) != 0;
      default:
        return cb_input_state(joyNum, RETRO_DEVICE_JOYPAD, 0, butNum) != 0;
      }
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
    if (IsGunProfileActive() && profile == InputProfileMode::Lightgun)
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
    else if (IsGunProfileActive() && profile == InputProfileMode::Mouse)
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
    else if (IsGunProfileActive() && profile == InputProfileMode::Joypad)
    {
      int raw_x = (int16_t)cb_input_state(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
      int raw_y = (int16_t)cb_input_state(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
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
    return true;
	}

};

static void ApplyVehicleInputDefaults(void)
{
  if (!racer_profile_active)
    return;

  // Racer-style mapping: left stick for steering, triggers for throttle/brake,
  // face buttons for shift/view change, shoulders for operator buttons.
  s_runtime_config.Set<std::string>("InputSteering", "JOY1_XAXIS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputAccelerator", "KEY_UP,JOY1_SLIDER1_POS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputBrake", "KEY_DOWN,JOY1_SLIDER2_POS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputGearShiftUp", "KEY_Y,JOY1_BUTTON9", "Input", "", "");
  s_runtime_config.Set<std::string>("InputGearShiftDown", "KEY_H,JOY1_BUTTON1", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON11", "Input", "", "");
  if (game.inputs & Game::INPUT_VIEWCHANGE)
    s_runtime_config.Set<std::string>("InputViewChange", "KEY_A,JOY1_BUTTON2", "Input", "", "");
  else
    s_runtime_config.Set<std::string>("InputViewChange", "NONE", "Input", "", "");
  if (game.inputs & Game::INPUT_HANDBRAKE)
  {
    s_runtime_config.Set<std::string>("InputHandBrake", "KEY_S,JOY1_BUTTON12", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceA", "NONE", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceB", "NONE", "Input", "", "");
  }
  else
  {
    s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON12", "Input", "", "");
    s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON12", "Input", "", "");
    s_runtime_config.Set<std::string>("InputHandBrake", "NONE", "Input", "", "");
  }
}

static void ApplyAnalogJoystickInputDefaults(void)
{
  if (!(game.inputs & Game::INPUT_ANALOG_JOYSTICK))
    return;

  // Keep the analog joystick profile self-contained in libretro.
  // Match the standalone defaults, but expose the common RetroPad face buttons.
  s_runtime_config.Set<std::string>("InputAnalogJoyLeft",  "KEY_LEFT,JOY1_LEFT",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyRight", "KEY_RIGHT,JOY1_RIGHT", "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyUp",    "KEY_UP,JOY1_UP",    "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyDown",  "KEY_DOWN,JOY1_DOWN",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyX",     "JOY1_XAXIS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyY",     "JOY1_YAXIS", "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyTrigger",  "KEY_A,JOY1_BUTTON9",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyEvent",    "KEY_S,JOY1_BUTTON1",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyTrigger2", "KEY_D,JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputAnalogJoyEvent2",    "KEY_F,JOY1_BUTTON2",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON12", "Input", "", "");
}

static void ApplySoccerInputDefaults(void)
{
  if (!(game.inputs & Game::INPUT_SOCCER))
    return;

  // Virtua Striker style controls: short pass, long pass and shoot.
  // Expose them on the familiar RetroPad face buttons.
  s_runtime_config.Set<std::string>("InputShortPass",  "KEY_A,JOY1_BUTTON9",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputLongPass",   "KEY_S,JOY1_BUTTON1",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputShoot",      "KEY_D,JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputShortPass2",  "JOY2_BUTTON9", "Input", "", "");
  s_runtime_config.Set<std::string>("InputLongPass2",   "JOY2_BUTTON1", "Input", "", "");
  s_runtime_config.Set<std::string>("InputShoot2",      "JOY2_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON12", "Input", "", "");
}

static void ApplyTwinJoystickInputDefaults(void)
{
  if (!(game.inputs & Game::INPUT_TWIN_JOYSTICKS))
    return;

  // Map the two joysticks to the two analog sticks on a modern controller.
  // Keep the macro movement buttons available on the D-pad and shoulder/press buttons.
  s_runtime_config.Set<std::string>("InputTwinJoyTurnLeft",     "KEY_Q,JOY1_LEFT",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyTurnRight",    "KEY_W,JOY1_RIGHT", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyForward",      "KEY_UP,JOY1_UP",   "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyReverse",      "KEY_DOWN,JOY1_DOWN","Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyStrafeLeft",    "KEY_A,JOY1_BUTTON13", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyStrafeRight",   "KEY_D,JOY1_BUTTON14", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyJump",         "KEY_E,JOY1_BUTTON15", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyCrouch",       "KEY_R,JOY1_BUTTON16", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyLeft1",        "JOY1_XAXIS_NEG",   "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyRight1",       "JOY1_XAXIS_POS",   "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyUp1",          "JOY1_YAXIS_NEG",   "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyDown1",        "JOY1_YAXIS_POS",   "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyLeft2",        "JOY1_RXAXIS_NEG",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyRight2",       "JOY1_RXAXIS_POS",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyUp2",          "JOY1_RYAXIS_NEG",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyDown2",        "JOY1_RYAXIS_POS",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyShot1",        "KEY_A,JOY1_BUTTON9", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyShot2",        "KEY_S,JOY1_BUTTON1", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyTurbo1",       "KEY_D,JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTwinJoyTurbo2",       "KEY_F,JOY1_BUTTON2", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON12", "Input", "", "");
}

static void ApplyFightingInputDefaults(void)
{
  if (!(game.inputs & Game::INPUT_FIGHTING))
    return;

  // Map the four face buttons to the four fighting actions.
  // RetroPad/LibRetro joypad semantics:
  //   A = BUTTON9, B = BUTTON1, X = BUTTON10, Y = BUTTON2.
  s_runtime_config.Set<std::string>("InputPunch",  "KEY_A,JOY1_BUTTON9",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputKick",   "KEY_S,JOY1_BUTTON1",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputGuard",  "KEY_D,JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputEscape", "KEY_F,JOY1_BUTTON2",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputStart2", "JOY2_BUTTON4",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputCoin2",  "JOY2_BUTTON3",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyUp2",    "JOY2_BUTTON5", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyDown2",  "JOY2_BUTTON6", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyLeft2",  "JOY2_BUTTON7", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyRight2", "JOY2_BUTTON8", "Input", "", "");
  s_runtime_config.Set<std::string>("InputPunch2",  "JOY2_BUTTON9",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputKick2",   "JOY2_BUTTON1",  "Input", "", "");
  s_runtime_config.Set<std::string>("InputGuard2",  "JOY2_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputEscape2", "JOY2_BUTTON2",  "Input", "", "");

  // Keep operator inputs off the face buttons so they do not interfere with fights.
  s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON12", "Input", "", "");
}

static void ApplyGunInputDefaults(void)
{
  if (!IsGunGame())
    return;

  InputProfileMode profile = GetEffectiveInputProfile();
  if (profile != InputProfileMode::Lightgun && profile != InputProfileMode::Mouse && profile != InputProfileMode::Joypad)
    return;

  s_runtime_config.Set<std::string>("InputTestA", "KEY_6,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "KEY_8,JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceA", "KEY_5,JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "KEY_7,JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set("Crosshairs", int(3), "Video", 0, 0, { 0,1,2,3 });

  if (game.inputs & (Game::INPUT_GUN1 | Game::INPUT_GUN2))
  {
    s_runtime_config.Set<std::string>("InputGunX", "MOUSE_XAXIS", "Input", "", "");
    s_runtime_config.Set<std::string>("InputGunY", "MOUSE_YAXIS", "Input", "", "");
    if (profile == InputProfileMode::Joypad)
    {
      s_runtime_config.Set<std::string>("InputTrigger", "JOY1_SLIDER1_POS,JOY1_BUTTON1", "Input", "", "");
      s_runtime_config.Set<std::string>("InputOffscreen", "JOY1_SLIDER2_POS,JOY1_BUTTON2", "Input", "", "");
    }
    else
    {
      s_runtime_config.Set<std::string>("InputTrigger", "MOUSE_LEFT_BUTTON", "Input", "", "");
      s_runtime_config.Set<std::string>("InputOffscreen", "MOUSE_RIGHT_BUTTON,MOUSE_MIDDLE_BUTTON", "Input", "", "");
    }
    s_runtime_config.Set<std::string>("InputAutoTrigger", "0", "Input", "", "");
    s_runtime_config.Set<std::string>("InputAutoTrigger2", "0", "Input", "", "");
  }

  if (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2))
  {
    s_runtime_config.Set<std::string>("InputAnalogGunX", "MOUSE_XAXIS", "Input", "", "");
    s_runtime_config.Set<std::string>("InputAnalogGunY", "MOUSE_YAXIS", "Input", "", "");
    if (profile == InputProfileMode::Joypad)
    {
      s_runtime_config.Set<std::string>("InputAnalogTriggerLeft", "JOY1_SLIDER1_POS,JOY1_BUTTON1", "Input", "", "");
      s_runtime_config.Set<std::string>("InputAnalogTriggerRight", "JOY1_SLIDER2_POS,JOY1_BUTTON2", "Input", "", "");
    }
    else
    {
      s_runtime_config.Set<std::string>("InputAnalogTriggerLeft", "MOUSE_LEFT_BUTTON", "Input", "", "");
      s_runtime_config.Set<std::string>("InputAnalogTriggerRight", "MOUSE_RIGHT_BUTTON,MOUSE_MIDDLE_BUTTON", "Input", "", "");
    }
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
  {
    MarkInputModeDirty();
    if (game_loaded)
    {
      const char *profile_name = "auto";
      if (input_profile_mode == InputProfileMode::Joypad)
        profile_name = "joypad";
      else if (input_profile_mode == InputProfileMode::Lightgun)
        profile_name = "lightgun";
      else if (input_profile_mode == InputProfileMode::Mouse)
        profile_name = "mouse";
      PushRuntimeMessageThrottled("input.profile", RETRO_LOG_INFO, 1400, 1, MESSAGE_THROTTLE_MS_DEFAULT,
        "Input profile: %s", profile_name);
    }
  }
  return previous != input_profile_mode;
}

static void UpdateInputDescriptors(void)
{
  static struct retro_input_descriptor desc[32];
  int descriptor_index = 0;

  InputProfileMode profile = GetEffectiveInputProfile();
  memset(desc, 0, sizeof(desc));

  if (IsGunGame() && profile == InputProfileMode::Lightgun)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER,    (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) ? "Left Trigger" : "Trigger" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD,     (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) ? "Right Trigger" : "Reload" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SELECT,     "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START,      "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A,      "Button 1" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_B,      "Button 2" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD,   0, RETRO_DEVICE_ID_JOYPAD_L,            "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD,   0, RETRO_DEVICE_ID_JOYPAD_R,            "Service" };
  }
  else if (IsGunGame() && profile == InputProfileMode::Mouse)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT,   (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) ? "Left Trigger" : "Fire" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT,  (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) ? "Right Trigger" : "Reload" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE, (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) ? "Right Trigger Alt" : "Reload Alt" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X,      "Aim X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y,      "Aim Y" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Service" };
  }
  else if (IsGunGame() && profile == InputProfileMode::Joypad)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Aim X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Aim Y" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) ? "Left Trigger" : "Fire" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, (game.inputs & (Game::INPUT_ANALOG_GUN1 | Game::INPUT_ANALOG_GUN2)) ? "Right Trigger" : "Reload" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Service" };
  }
  else if (game.inputs & Game::INPUT_FIGHTING)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Punch" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "Kick" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Guard" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Escape" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Service" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "P2 Punch" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "P2 Kick" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "P2 Guard" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "P2 Escape" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "P2 Up" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "P2 Down" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "P2 Left" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "P2 Right" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "P2 Start" };
    desc[descriptor_index++] = { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "P2 Coin" };
  }
  else if (game.inputs & Game::INPUT_ANALOG_JOYSTICK)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Trigger 1" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "Event 1" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Trigger 2" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Event 2" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Service" };
  }
  else if (game.inputs & Game::INPUT_SOCCER)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Short Pass" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "Long Pass" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Shoot" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Service" };
  }
  else if (game.inputs & Game::INPUT_TWIN_JOYSTICKS)
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_X, "Left Stick X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_Y, "Left Stick Y" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right Stick X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Stick Y" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,       "Macro Turn Left" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,      "Macro Turn Right" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,         "Macro Forward" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,       "Macro Reverse" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,         "Macro Strafe Left" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,         "Macro Strafe Right" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,         "Macro Jump" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,         "Macro Crouch" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,          "Left Shot" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,          "Right Shot" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,          "Left Turbo" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,          "Right Turbo" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,          "Test" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,          "Service" };
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
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "Shift Up" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "Shift Down" };
    if (game.inputs & Game::INPUT_VIEWCHANGE)
      desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,    "View Change" };
  }
  else
  {
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Y" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" };
    desc[descriptor_index++] = { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" };
  }

  cb_env(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
  input_mode_dirty = false;
}

RETRO_API void retro_init(void) {
  assert(!initialized);
  // Set frontend-integrated logger (with throttling fallback to stderr).
  SetLogger(std::make_shared<CLibretroFrontendLogger>());
  RefreshFrontendInterfaces();
  RegisterPerfCounters();
  EmitFrontendLogThrottled("core.frontend_interfaces", RETRO_LOG_INFO, 0,
    "Frontend interfaces ready (log=%s, perf=%s, message_ext=%s).",
    s_retro_log_printf != nullptr ? "yes" : "no",
    s_retro_perf_available ? "yes" : "no",
    s_message_interface_version > 0 ? "yes" : "no");

  s_runtime_config.Set("New3DEngine", true, "Global");
  s_runtime_config.Set("QuadRendering", false, "Global");
  s_runtime_config.Set("WideBackground", false, "Video");
  s_runtime_config.Set("MultiThreaded", false, "Core");
  s_runtime_config.Set("GPUMultiThreaded", false, "Core");
  s_runtime_config.Set("MultiTexture", false, "Legacy3D");
  s_runtime_config.Set<std::string>("VertexShader", "", "Legacy3D", "", "");
  s_runtime_config.Set<std::string>("FragmentShader", "", "Legacy3D", "", "");
  s_runtime_config.Set("NoWhiteFlash", false, "Video");
  s_runtime_config.Set("UpscaleMode", 2, "Video", 0, 0, { 0,1,2,3 });
  s_runtime_config.Set("WideScreen", false, "Video");
  s_runtime_config.Set("SimulateNet", true, "Network");
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
  s_runtime_config.Set("Stretch", false, "Video");
  s_runtime_config.Set("VSync", true, "Video");
  s_runtime_config.Set("Throttle", true, "Video");
  s_runtime_config.Set("RefreshRate", 60.0f, "Video", 0.0f, 0.0f, { 57.5f,60.f });
  s_runtime_config.Set("ShowFrameRate", false, "Video");
  s_runtime_config.Set("Crosshairs", int(0), "Video", 0, 0, { 0,1,2,3 });
  s_runtime_config.Set<std::string>("CrosshairStyle", "vector", "Video", "", "", { "bmp","vector" });

  s_runtime_config.Set<std::string>("InputStart1", "JOY1_BUTTON4", "Input", "", "");
  s_runtime_config.Set<std::string>("InputCoin1", "JOY1_BUTTON3", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceA", "JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputServiceB", "JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestA", "JOY1_BUTTON11", "Input", "", "");
  s_runtime_config.Set<std::string>("InputTestB", "JOY1_BUTTON12", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyUp", "JOY1_BUTTON5", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyDown", "JOY1_BUTTON6", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyLeft", "JOY1_BUTTON7", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJoyRight", "JOY1_BUTTON8", "Input", "", "");
  s_runtime_config.Set<std::string>("InputShift", "JOY1_BUTTON10", "Input", "", "");
  s_runtime_config.Set<std::string>("InputBeat", "JOY1_BUTTON1", "Input", "", "");
  s_runtime_config.Set<std::string>("InputCharge", "JOY1_BUTTON9", "Input", "", "");
  s_runtime_config.Set<std::string>("InputJump", "JOY1_BUTTON2", "Input", "", "");

  baseline_vsync_enabled = s_runtime_config["VSync"].ValueAsDefault<bool>(true);
  baseline_throttle_enabled = s_runtime_config["Throttle"].ValueAsDefault<bool>(true);
  frontend_fastforwarding = false;

  ApplySupersamplingOption();

  if (!RequestHWContext())
  {
    return;
  }

  initialized = true;
}

RETRO_API void retro_deinit(void) {
  assert(initialized);
  if (s_retro_perf_available && s_retro_perf_cb.perf_log != nullptr)
    s_retro_perf_cb.perf_log();
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
  fighting_profile_active = false;
  input_mode_dirty = true;
  serialized_state_buffer.clear();
  serialized_state_ready = false;
  frontend_fastforwarding = false;
  baseline_vsync_enabled = true;
  baseline_throttle_enabled = true;
  s_retro_perf_registered = false;
  s_log_throttle_entries.clear();
  s_runtime_message_throttle_entries.clear();
}

// Before load_game
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
  UpdateOutputGeometry();
  info->geometry.base_width = IsWideScreenEnabled() ? total_x_res : SUPERMODEL_W;
  info->geometry.base_height = IsWideScreenEnabled() ? total_y_res : SUPERMODEL_H;
  info->geometry.max_width = std::max(total_x_res, SUPERMODEL_W * 8u);
  info->geometry.max_height = std::max(total_y_res, SUPERMODEL_H * 8u);
  info->geometry.aspect_ratio = IsWideScreenEnabled() ? (16.0f / 9.0f) : ((float)SUPERMODEL_W / (float)SUPERMODEL_H);
  info->timing.fps = 60.0f;
  info->timing.sample_rate = 44100.0f;
}
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) {
  if (port > 1)
    return;
  if (port == 0 && frontend_controller_device != device)
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
  try
  {
    ApplySupersamplingOption();
    ApplyVideoCoreOptions();

    Model3 = new CModel3(s_runtime_config);
    if (!Model3)
      return false;

    if (Result::OKAY != Model3->Init())
    {
      ErrorLog("Model3 initialization failed.");
      delete Model3;
      Model3 = nullptr;
      return false;
    }

    GameLoader loader(config_path("Games.xml"));
    if (loader.Load(&game, &rom_set, rgame->path))
    {
      ErrorLog("Unable to identify ROM set for '%s'.", rgame->path);
      delete Model3;
      Model3 = nullptr;
      return false;
    }
    if (Model3->LoadGame(game, rom_set) != Result::OKAY)
    {
      ErrorLog("Model3::LoadGame() failed for '%s'.", game.name.c_str());
      delete Model3;
      Model3 = nullptr;
      return false;
    }
    rom_set = ROMSet();  // free up this memory we won't need anymore

    racer_profile_active = (game.inputs & Game::INPUT_VEHICLE) != 0;
    fighting_profile_active = (game.inputs & Game::INPUT_FIGHTING) != 0;
    if (fighting_profile_active)
    {
      PushRuntimeMessageThrottled("profile.fighters", RETRO_LOG_INFO, 1800, 1, MESSAGE_THROTTLE_MS_DEFAULT,
        "Fighters profile active: P2 port enabled.");
      EmitFrontendLogThrottled("profile.fighters", RETRO_LOG_INFO, LOG_THROTTLE_MS_DEFAULT,
        "Fighters profile detected for '%s'. P2 joystick path enabled.", game.name.c_str());
    }
    ApplyInputProfileOption();
    ApplyCrosshairOverlayOption();
    ApplyFightingInputDefaults();
    ApplyVehicleInputDefaults();
    ApplyAnalogJoystickInputDefaults();
    ApplySoccerInputDefaults();
    ApplyTwinJoystickInputDefaults();
    ApplyGunInputDefaults();

    // Initialize input
    InputSystem = std::shared_ptr<CInputSystem>(new CRetroInputSystem());
    Inputs = new CInputs(InputSystem);
    if (!Inputs->Initialize())
    {
      ErrorLog("Input initialization failed.");
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
      ErrorLog("Renderer initialization failed.");
      delete Model3;
      Model3 = nullptr;
      delete Inputs;
      Inputs = nullptr;
      return false;
    }
    if (renderers_ready && !InitializeCrosshair())
    {
      ErrorLog("Crosshair initialization failed; falling back to software overlay.");
    }

    game_loaded = true;
    LoadNVRAMFromDisk();
    return game_loaded;
  }
  catch (const std::exception &e)
  {
    ErrorLog("retro_load_game exception: %s", e.what());
  }
  catch (...)
  {
    ErrorLog("retro_load_game threw an unknown exception.");
  }

  delete Model3;
  Model3 = nullptr;
  delete Inputs;
  Inputs = nullptr;
  return false;
}
RETRO_API void retro_unload_game(void) {
  assert(game_loaded);
  SaveNVRAMToDisk();
  DestroyRenderers();
  DestroyCrosshair();
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
  fighting_profile_active = false;
  input_mode_dirty = true;
  serialized_state_buffer.clear();
  serialized_state_ready = false;
  frontend_fastforwarding = false;
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
  ScopedPerfCounter perf_frame_scope(&s_perf_frame_total);
  bool variables_updated = false;
  bool refresh_input_mode = input_mode_dirty;
  bool fastforwarding = false;
  if (cb_env != nullptr)
    cb_env(RETRO_ENVIRONMENT_GET_FASTFORWARDING, &fastforwarding);
  ApplyFastForwardAwareness(fastforwarding);

  if (cb_env(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &variables_updated) && variables_updated)
  {
    bool supersampling_changed = ApplySupersamplingOption();
    bool video_options_changed = ApplyVideoCoreOptions();
    ApplyCrosshairOverlayOption();
    if (ApplyInputProfileOption())
      refresh_input_mode = true;
    if ((supersampling_changed || video_options_changed) && renderers_ready)
    {
      DestroyRenderers();
      DestroyCrosshair();
      if (!InitializeRenderers(false))
        return;
      if (!InitializeCrosshair())
        ErrorLog("Crosshair initialization failed; falling back to software overlay.");
    }
  }
  if (refresh_input_mode)
  {
    if (fighting_profile_active)
      ApplyFightingInputDefaults();
    if (racer_profile_active)
      ApplyVehicleInputDefaults();
    ApplyAnalogJoystickInputDefaults();
    ApplySoccerInputDefaults();
    ApplyTwinJoystickInputDefaults();
    ApplyGunInputDefaults();
    if (Inputs != nullptr)
    {
      Inputs->LoadFromConfig(s_runtime_config);
    }
    UpdateInputDescriptors();
    pointer_x = (int)(x_res / 2);
    pointer_y = (int)(y_res / 2);
  }
  if (!renderers_ready)
  {
    if (!InitializeRenderers(false))
      return;
    if (!InitializeCrosshair())
      ErrorLog("Crosshair initialization failed; falling back to software overlay.");
  }
  BindCurrentFramebuffer();
  RefreshOutputGeometry();
  {
    ScopedPerfCounter perf_input_scope(&s_perf_input_poll);
    Inputs->Poll(&game, x_offset, y_offset, x_res, y_res);
  }
  {
    ScopedPerfCounter perf_emulate_scope(&s_perf_emulate_frame);
    Model3->RunFrame();
  }
  {
    ScopedPerfCounter perf_video_scope(&s_perf_video_submit);
    if (!frontend_fastforwarding && s_crosshair != nullptr)
      s_crosshair->Update(game.inputs, Inputs, x_offset, y_offset, x_res, y_res);
    if (!frontend_fastforwarding && ShouldDrawPointerOverlay())
      DrawPointerOverlay();
    glFlush();
    cb_video_refresh(RETRO_HW_FRAME_BUFFER_VALID, total_x_res, total_y_res, 0);
  }
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

static bool BuildSerializedStateBuffer(void)
{
  ScopedPerfCounter perf_serialize_scope(&s_perf_serialize);
  if (Model3 == nullptr || !game_loaded)
    return false;

  CBlockFile SaveState;
  if (Result::OKAY != SaveState.CreateMemory("Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION))
    return false;

  int32_t fileVersion = STATE_FILE_VERSION;
  SaveState.Write(&fileVersion, sizeof(fileVersion));
  SaveState.Write(Model3->GetGame().name);
  Model3->SaveState(&SaveState);

  const uint8_t *stateData = SaveState.GetMemoryData();
  const size_t stateSize = SaveState.GetMemorySize();
  if (stateData == nullptr || stateSize == 0)
  {
    SaveState.Close();
    return false;
  }

  serialized_state_buffer.assign(stateData, stateData + stateSize);
  serialized_state_ready = true;
  SaveState.Close();
  return true;
}

RETRO_API size_t retro_serialize_size(void) {
  if (!BuildSerializedStateBuffer())
    return 0;
  return serialized_state_buffer.size();
}
RETRO_API bool retro_serialize(void *data, size_t len) {
  ScopedPerfCounter perf_serialize_scope(&s_perf_serialize);
  if (data == nullptr)
    return false;
  if (!serialized_state_ready && !BuildSerializedStateBuffer())
    return false;
  if (len < serialized_state_buffer.size())
    return false;

  std::memcpy(data, serialized_state_buffer.data(), serialized_state_buffer.size());
  serialized_state_ready = false;
  return true;
}
RETRO_API bool retro_unserialize(const void *data, size_t len) {
  ScopedPerfCounter perf_unserialize_scope(&s_perf_unserialize);
  if (data == nullptr || len == 0)
    return false;

  CBlockFile  SaveState;
  if (Result::OKAY != SaveState.LoadMemory(data, len))
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
  serialized_state_buffer.clear();
  serialized_state_ready = false;
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
