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
 * LibretroOptions.cpp
 *
 * Core option definitions and their translation into Supermodel's
 * configuration keys.
 *
 * Command line option        Core option                       Config key
 * -------------------        -----------                       ----------
 * -ppc-frequency             supermodel_ppc_frequency          PowerPCFrequency
 * -threads/-no-threads       supermodel_multi_threaded         MultiThreaded
 * -gpu-multi-threaded/...    supermodel_gpu_multi_threaded     GPUMultiThreaded
 * -true-hz                   supermodel_refresh_rate           RefreshRate
 * -new3d/-legacy3d           supermodel_renderer               New3DEngine
 * -res                       supermodel_resolution             XResolution/YResolution
 * -ss                        supermodel_supersampling          Supersampling
 * -wide-screen               supermodel_widescreen             WideScreen
 * -wide-bg                   supermodel_wide_background        WideBackground
 * -stretch                   supermodel_stretch                Stretch
 * -upscalemode               supermodel_upscale_mode           UpscaleMode
 * -crtcolors                 supermodel_crt_colors             CRTcolors
 * -quad-rendering/...        supermodel_quad_rendering         QuadRendering
 * -multi-texture/...         supermodel_multi_texture          MultiTexture
 * -no-white-flash            supermodel_no_white_flash         NoWhiteFlash
 * -crosshairs                supermodel_crosshairs             Crosshairs
 * -crosshair-style           supermodel_crosshair_style        CrosshairStyle
 * -sound-volume              supermodel_sound_volume           SoundVolume
 * -music-volume              supermodel_music_volume           MusicVolume
 * -balance                   supermodel_balance                Balance
 * (Supermodel.ini)           supermodel_balance_left_right     BalanceLeftRight
 * (Supermodel.ini)           supermodel_balance_front_rear     BalanceFrontRear
 * -flip-stereo               supermodel_flip_stereo            FlipStereo
 * -sound/-no-sound           supermodel_emulate_sound          EmulateSound
 * -dsb/-no-dsb               supermodel_emulate_dsb            EmulateDSB
 * -force-feedback            supermodel_force_feedback         ForceFeedback
 * (Supermodel.ini)           supermodel_analog_deadzone        InputJoy*DeadZone
 * (Supermodel.ini)           supermodel_analog_saturation      InputJoy*Saturation
 * (Supermodel.ini)           supermodel_digital_sensitivity    InputDigitalSensitivity
 * (Supermodel.ini)           supermodel_digital_decay          InputDigitalDecaySpeed
 * -net/-no-net               supermodel_network                Network          (LIBRETRO_NET builds)
 * -port, -port-in            supermodel_net_port_in            PortIn           (LIBRETRO_NET builds)
 * -port-out                  supermodel_net_port_out           PortOut          (LIBRETRO_NET builds)
 * -simulate/-emulate-netboard supermodel_simulate_net          SimulateNet      (LIBRETRO_NET builds)
 * -outputs                   supermodel_outputs                Outputs          (LIBRETRO_NET builds)
 *
 * Window, full screen, vsync, throttle, frame rate display, save states,
 * logging, input configuration and the ROM/XML/shader file options are
 * handled by the frontend or do not apply to a libretro core.
 */

#include "LibretroOptions.h"

#include "Inputs/InputSystem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace
{
  enum class Type
  {
    Bool,       // "enabled" / "disabled"
    Int,
    Unsigned,
    Float,
    String,
    Special     // handled in ApplyOption()
  };

  struct Value
  {
    std::string value;
    std::string label;
  };

  struct Option
  {
    const char *key;
    const char *category;
    const char *desc;
    const char *info;
    std::vector<Value> values;
    std::string defaultValue;
    const char *configKey;  // Supermodel configuration key
    Type type;
    bool live;              // takes effect while a game is running
  };

  std::vector<Value> Range(int first, int last, int step, const char *suffix = "", const char *zeroLabel = nullptr)
  {
    std::vector<Value> values;
    for (int v = first; v <= last; v += step)
    {
      std::string value = std::to_string(v);
      std::string label = (v == 0 && zeroLabel) ? std::string(zeroLabel) : value + suffix;
      values.push_back({ value, label });
    }
    return values;
  }

  const std::vector<Value> s_onOff = { { "enabled", "Enabled" }, { "disabled", "Disabled" } };

  std::vector<Option> BuildOptions()
  {
    std::vector<Option> o;

    // System
    o.push_back({ "supermodel_ppc_frequency", "system", "PowerPC Frequency",
      "Clock of the emulated PowerPC. 'Auto' uses the frequency of the game's hardware stepping (66, 100 or 166 MHz). Underclocking reduces the CPU load but may slow down some games.",
      { { "0", "Auto" }, { "50", "50 MHz" }, { "66", "66 MHz" }, { "75", "75 MHz" }, { "83", "83 MHz" }, { "100", "100 MHz" }, { "116", "116 MHz" },
        { "133", "133 MHz" }, { "150", "150 MHz" }, { "166", "166 MHz" }, { "183", "183 MHz" }, { "200", "200 MHz" } },
      "0", "PowerPCFrequency", Type::Unsigned, true });
    o.push_back({ "supermodel_multi_threaded", "system", "Multi-Threading",
      "Run the sound board and drive board in separate threads. Takes effect when the game is restarted.",
      s_onOff, "enabled", "MultiThreaded", Type::Bool, false });
    o.push_back({ "supermodel_gpu_multi_threaded", "system", "GPU Multi-Threading",
      "Run the main board in a separate thread while the frame is being rendered. Takes effect when the game is restarted.",
      s_onOff, "enabled", "GPUMultiThreaded", Type::Bool, false });
    o.push_back({ "supermodel_refresh_rate", "system", "Refresh Rate",
      "57.524 Hz is the refresh rate of the original hardware. 60 Hz matches common displays.",
      { { "60", "60 Hz" }, { "57.524", "57.524 Hz (original hardware)" } },
      "60", "RefreshRate", Type::Float, true });
    o.push_back({ "supermodel_clear_nvram", "system", "Clear NVRAM at Startup",
      "Erase the game's backup RAM and EEPROM (settings, high scores) when the game starts. Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } },
      "disabled", nullptr, Type::Special, false });

    // Video
#ifndef SUPERMODEL_OSX
    o.push_back({ "supermodel_renderer", "video", "3D Engine",
      "The new engine is the most accurate. The legacy engine is faster but less accurate. Takes effect when the game is restarted.",
      { { "new3d", "New 3D Engine" }, { "legacy3d", "Legacy 3D Engine" } },
      "new3d", "New3DEngine", Type::Special, false });
#endif
    o.push_back({ "supermodel_resolution", "video", "Internal Resolution",
      "Rendering resolution. 496x384 is the original resolution; for 16:9 or 16:10 resolutions also enable 'Wide Screen'. Takes effect when the game is restarted.",
      { { "496x384", "496x384 (original)" }, { "992x768", "992x768 (2x)" }, { "1488x1152", "1488x1152 (3x)" }, { "1984x1536", "1984x1536 (4x)" },
        { "2480x1920", "2480x1920 (5x)" }, { "2976x2304", "2976x2304 (6x)" },
        { "640x480", "640x480 (4:3)" }, { "800x600", "800x600 (4:3)" }, { "1024x768", "1024x768 (4:3)" }, { "1280x960", "1280x960 (4:3)" },
        { "1600x1200", "1600x1200 (4:3)" }, { "1920x1440", "1920x1440 (4:3)" }, { "2880x2160", "2880x2160 (4:3)" },
        { "1280x720", "1280x720 (16:9)" }, { "1600x900", "1600x900 (16:9)" }, { "1920x1080", "1920x1080 (16:9)" }, { "2560x1440", "2560x1440 (16:9)" },
        { "3840x2160", "3840x2160 (16:9)" }, { "1680x1050", "1680x1050 (16:10)" }, { "1920x1200", "1920x1200 (16:10)" } },
      "496x384", nullptr, Type::Special, false });
    o.push_back({ "supermodel_supersampling", "video", "Supersampling",
      "Render at a multiple of the internal resolution and scale down (anti-aliasing). Takes effect when the game is restarted.",
      Range(1, 8, 1, "x"), "1", "Supersampling", Type::Int, false });
    o.push_back({ "supermodel_widescreen", "video", "Wide Screen",
      "Expand the 3D field of view to the width of the screen. Use with a wide internal resolution. Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "WideScreen", Type::Bool, false });
    o.push_back({ "supermodel_wide_background", "video", "Wide Background",
      "When 'Wide Screen' is enabled, also expand the 2D background layer to the width of the screen. Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "WideBackground", Type::Bool, false });
    o.push_back({ "supermodel_stretch", "video", "Stretch",
      "Fit the image to the internal resolution, ignoring the aspect ratio. Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "Stretch", Type::Bool, false });
    o.push_back({ "supermodel_upscale_mode", "video", "2D Upscaling Filter",
      "Filter used to scale the 2D layers to the internal resolution. Takes effect when the game is restarted.",
      { { "0", "None (sharp pixels)" }, { "1", "Biquintic" }, { "2", "Bilinear" }, { "3", "Bicubic" } },
      "2", "UpscaleMode", Type::Int, false });
    o.push_back({ "supermodel_crt_colors", "video", "CRT Colors",
      "Emulate the colors of the arcade monitor. Takes effect when the game is restarted.",
      { { "0", "None" }, { "1", "ARI/D93 (JP games)" }, { "2", "PVM_20M2U/D93" }, { "3", "BT601_525/D93" },
        { "4", "BT601_525/D65 (US games)" }, { "5", "BT601_625/D65 (EUR/AUS games)" } },
      "0", "CRTcolors", Type::Int, false });
    o.push_back({ "supermodel_quad_rendering", "video", "Quad Rendering",
      "Render quads as quads (accurate) instead of pairs of triangles (new 3D engine). Takes effect when the game is restarted.",
      s_onOff, "enabled", "QuadRendering", Type::Bool, false });
#ifndef SUPERMODEL_OSX
    o.push_back({ "supermodel_multi_texture", "video", "Multi-Texture (Legacy 3D Engine)",
      "Use 8 texture maps for decoding with the legacy 3D engine. Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "MultiTexture", Type::Bool, false });
#endif
    o.push_back({ "supermodel_no_white_flash", "video", "Disable White Flash",
      "Do not show the white flash when games disable 3D rendering. Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "NoWhiteFlash", Type::Bool, false });
    o.push_back({ "supermodel_crosshairs", "video", "Crosshairs",
      "Crosshairs for light gun games.",
      { { "0", "None" }, { "1", "Player 1 only" }, { "2", "Player 2 only" }, { "3", "Players 1 & 2" } },
      "0", "Crosshairs", Type::Int, true });
    o.push_back({ "supermodel_crosshair_style", "video", "Crosshair Style",
      "Bitmap crosshairs need p1crosshair.bmp and p2crosshair.bmp in system/supermodel/Assets.",
      { { "vector", "Vector" }, { "bmp", "Bitmap" } },
      "vector", "CrosshairStyle", Type::String, true });

    // Audio
    o.push_back({ "supermodel_sound_volume", "audio", "Sound Volume",
      "Volume of the sound effects (SCSP) in percent.",
      Range(0, 200, 10, "%"), "100", "SoundVolume", Type::Int, true });
    o.push_back({ "supermodel_music_volume", "audio", "Music Volume",
      "Volume of the Digital Sound Board (MPEG music) in percent.",
      Range(0, 200, 10, "%"), "100", "MusicVolume", Type::Int, true });
    o.push_back({ "supermodel_balance", "audio", "SCSP Balance",
      "Relative balance between the two sound chips (SCSP 1 / SCSP 2) in percent.",
      Range(-100, 100, 10, "%"), "0", "Balance", Type::Float, true });
    o.push_back({ "supermodel_balance_left_right", "audio", "Left/Right Balance",
      "Positive values favor the left channel, negative values the right channel.",
      Range(-100, 100, 10, "%"), "0", "BalanceLeftRight", Type::Float, true });
    o.push_back({ "supermodel_balance_front_rear", "audio", "Front/Rear Balance",
      "Balance between the front and rear speakers, which are mixed into the stereo output. Positive values favor the front speakers.",
      Range(-100, 100, 10, "%"), "0", "BalanceFrontRear", Type::Float, true });
    o.push_back({ "supermodel_flip_stereo", "audio", "Swap Stereo Channels",
      "Swap the left and right audio channels.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "FlipStereo", Type::Bool, true });
    o.push_back({ "supermodel_emulate_sound", "audio", "Sound Board",
      "Emulate the sound board (sound effects).",
      s_onOff, "enabled", "EmulateSound", Type::Bool, true });
    o.push_back({ "supermodel_emulate_dsb", "audio", "Digital Sound Board",
      "Emulate the Digital Sound Board (MPEG music).",
      s_onOff, "enabled", "EmulateDSB", Type::Bool, true });

    // Input
    o.push_back({ "supermodel_force_feedback", "input", "Force Feedback",
      "Translate the force feedback of driving games and other cabinets into controller rumble. Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "ForceFeedback", Type::Bool, false });
    o.push_back({ "supermodel_rumble_strength", "input", "Rumble Strength",
      "Strength of the rumble produced by force feedback.",
      Range(10, 100, 10, "%"), "100", nullptr, Type::Special, true });
    o.push_back({ "supermodel_analog_deadzone", "input", "Analog Dead Zone",
      "Dead zone of the analog sticks and triggers in percent.",
      Range(0, 30, 1, "%"), "3", nullptr, Type::Special, true });
    o.push_back({ "supermodel_analog_saturation", "input", "Analog Saturation",
      "Percentage of the analog range needed to reach the maximum value. Lower values make the controls more sensitive.",
      Range(50, 200, 10, "%"), "100", nullptr, Type::Special, true });
    o.push_back({ "supermodel_digital_sensitivity", "input", "Digital Sensitivity",
      "How quickly analog controls (steering, pedals, guns) move when driven by digital buttons or the D-pad.",
      Range(5, 100, 5, "%"), "25", "InputDigitalSensitivity", Type::Unsigned, true });
    o.push_back({ "supermodel_digital_decay", "input", "Digital Decay Speed",
      "How quickly analog controls driven by digital buttons return to center, relative to the sensitivity.",
      Range(10, 200, 10, "%"), "50", "InputDigitalDecaySpeed", Type::Unsigned, true });
    o.push_back({ "supermodel_gun_auto_reload", "input", "Light Gun Auto-Reload",
      "Pressing Reload (or pulling the trigger off screen) points the gun off screen and fires automatically, as needed to reload.",
      s_onOff, "enabled", nullptr, Type::Special, true });

#ifdef SUPERMODEL_LIBRETRO_NET
    // Network
    o.push_back({ "supermodel_network", "network", "Net Board",
      "Enable the net board to link cabinets (each one a separate instance). Takes effect when the game is restarted.",
      { { "disabled", "Disabled" }, { "enabled", "Enabled" } }, "disabled", "Network", Type::Bool, false });
    o.push_back({ "supermodel_simulate_net", "network", "Net Board Mode",
      "Simulate the net board (recommended) or emulate it (requires multi-threading to be disabled). Takes effect when the game is restarted.",
      { { "simulate", "Simulate" }, { "emulate", "Emulate" } }, "simulate", "SimulateNet", Type::Special, false });
    o.push_back({ "supermodel_net_port_in", "network", "Network Port",
      "TCP port of this instance's network server: the previous cabinet of the ring connects to it, and so do the Pro-1000 host programs (wnaspi32.dll). Takes effect when the game is restarted.",
      Range(1970, 1979, 1), "1970", "PortIn", Type::Unsigned, false });
    o.push_back({ "supermodel_net_port_out", "network", "Net Board Output Port",
      "TCP port of the next cabinet of the ring (on 127.0.0.1); the last cabinet connects back to the first. Takes effect when the game is restarted.",
      Range(1970, 1979, 1), "1971", "PortOut", Type::Unsigned, false });
    o.push_back({ "supermodel_outputs", "network", "Outputs",
      "Send the cabinet outputs (lamps, etc.) over TCP/UDP for external programs. Takes effect when the game is restarted.",
      { { "none", "None" }, { "net", "Network (TCP/UDP)" } }, "none", "Outputs", Type::String, false });
#endif

    return o;
  }

  const std::vector<Option> &Options()
  {
    static const std::vector<Option> options = BuildOptions();
    return options;
  }

  const char *GetVariable(retro_environment_t environ, const char *key)
  {
    retro_variable var = { key, nullptr };
    if (environ(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      return var.value;
    return nullptr;
  }

  std::string GetValue(retro_environment_t environ, const Option &option)
  {
    const char *value = GetVariable(environ, option.key);
    return value ? std::string(value) : option.defaultValue;
  }

  const Option *FindOption(const char *key)
  {
    for (const Option &option : Options())
    {
      if (!strcmp(option.key, key))
        return &option;
    }
    return nullptr;
  }

  std::string GetValue(retro_environment_t environ, const char *key)
  {
    const Option *option = FindOption(key);
    if (!option)
      return std::string();
    return GetValue(environ, *option);
  }

  // Joystick axes, see CInputSystem
  const char *s_joyAxisIds[] = { "X", "Y", "Z", "RX", "RY", "RZ", "S1", "S2" };

  void ApplyOption(const Option &option, const std::string &value, Util::Config::Node *config)
  {
    switch (option.type)
    {
    case Type::Bool:
      config->Set(option.configKey, value == "enabled");
      return;
    case Type::Int:
      config->Set(option.configKey, (int) strtol(value.c_str(), nullptr, 10));
      return;
    case Type::Unsigned:
      config->Set(option.configKey, (unsigned) strtoul(value.c_str(), nullptr, 10));
      return;
    case Type::Float:
      config->Set(option.configKey, (float) strtod(value.c_str(), nullptr));
      return;
    case Type::String:
      config->Set(option.configKey, value);
      return;
    case Type::Special:
      break;
    }

    std::string key = option.key;
    if (key == "supermodel_renderer")
      config->Set("New3DEngine", value != "legacy3d");
    else if (key == "supermodel_resolution")
    {
      unsigned x = 496, y = 384;
      if (sscanf(value.c_str(), "%ux%u", &x, &y) != 2 || x == 0 || y == 0)
      {
        x = 496;
        y = 384;
      }
      config->Set("XResolution", x);
      config->Set("YResolution", y);
    }
    else if (key == "supermodel_simulate_net")
      config->Set("SimulateNet", value != "emulate");
    else if (key == "supermodel_analog_deadzone")
    {
      unsigned deadZone = (unsigned) strtoul(value.c_str(), nullptr, 10);
      for (const char *axis : s_joyAxisIds)
        config->Set(std::string("InputJoy") + axis + "DeadZone", deadZone);
    }
    else if (key == "supermodel_analog_saturation")
    {
      unsigned saturation = (unsigned) strtoul(value.c_str(), nullptr, 10);
      for (const char *axis : s_joyAxisIds)
        config->Set(std::string("InputJoy") + axis + "Saturation", saturation);
    }
    // supermodel_clear_nvram, supermodel_rumble_strength and
    // supermodel_gun_auto_reload are read directly by the frontend code
  }
}

namespace LibretroOptions
{
  void Register(retro_environment_t environ)
  {
    static std::vector<retro_core_option_v2_category> categories;
    static std::vector<retro_core_option_v2_definition> definitions;
    static retro_core_options_v2 optionsV2;
    static std::vector<retro_variable> variables;
    static std::deque<std::string> legacyStrings;

    if (categories.empty())
    {
      categories.push_back({ "system", "System", "Emulated hardware and timing." });
      categories.push_back({ "video", "Video", "Rendering settings." });
      categories.push_back({ "audio", "Audio", "Volume and mixing." });
      categories.push_back({ "input", "Input", "Analog controls, light guns and force feedback." });
#ifdef SUPERMODEL_LIBRETRO_NET
      categories.push_back({ "network", "Network", "Net board link and outputs." });
#endif
      categories.push_back({ nullptr, nullptr, nullptr });

      for (const Option &option : Options())
      {
        retro_core_option_v2_definition def;
        memset(&def, 0, sizeof(def));
        def.key = option.key;
        def.desc = option.desc;
        def.desc_categorized = option.desc;
        def.info = option.info;
        def.info_categorized = option.info;
        def.category_key = option.category;
        size_t n = 0;
        for (const Value &value : option.values)
        {
          if (n + 1 >= RETRO_NUM_CORE_OPTION_VALUES_MAX)
            break;
          def.values[n].value = value.value.c_str();
          def.values[n].label = value.label.c_str();
          n++;
        }
        def.default_value = option.defaultValue.c_str();
        definitions.push_back(def);
      }
      retro_core_option_v2_definition terminator;
      memset(&terminator, 0, sizeof(terminator));
      definitions.push_back(terminator);
      optionsV2.categories = categories.data();
      optionsV2.definitions = definitions.data();
    }

    unsigned version = 0;
    if (!environ(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;
    if (version >= 2 && environ(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &optionsV2))
      return;

    // Legacy interface: "Description; default|value|value..."
    if (variables.empty())
    {
      for (const Option &option : Options())
      {
        std::string text = std::string(option.desc) + "; " + option.defaultValue;
        for (const Value &value : option.values)
        {
          if (value.value != option.defaultValue)
            text += "|" + value.value;
        }
        legacyStrings.push_back(text);
        variables.push_back({ option.key, legacyStrings.back().c_str() });
      }
      variables.push_back({ nullptr, nullptr });
    }
    environ(RETRO_ENVIRONMENT_SET_VARIABLES, variables.data());
  }

  Util::Config::Node DefaultConfig()
  {
    Util::Config::Node config("Global");

    // Core
    config.Set("PowerPCFrequency", 0u);
    config.Set("MultiThreaded", true);
    config.Set("GPUMultiThreaded", true);
    config.Set("RefreshRate", 60.0f);

    // Video
    config.Set("New3DEngine", true);
    config.Set("QuadRendering", true);
    config.Set("XResolution", 496u);
    config.Set("YResolution", 384u);
    config.Set("Supersampling", 1);
    config.Set("CRTcolors", 0);
    config.Set("UpscaleMode", 2);
    config.Set("WideScreen", false);
    config.Set("WideBackground", false);
    config.Set("Stretch", false);
    config.Set("NoWhiteFlash", false);
    config.Set("MultiTexture", false);
    config.Set<std::string>("VertexShader", "");
    config.Set<std::string>("FragmentShader", "");
    config.Set("Crosshairs", 0);
    config.Set<std::string>("CrosshairStyle", "vector");

    // Sound
    config.Set("EmulateSound", true);
    config.Set("EmulateDSB", true);
    config.Set("SoundVolume", 100);
    config.Set("MusicVolume", 100);
    config.Set("Balance", 0.0f);
    config.Set("BalanceLeftRight", 0.0f);
    config.Set("BalanceFrontRear", 0.0f);
    config.Set("FlipStereo", false);

    // Drive board
    config.Set("ForceFeedback", false);

    // Net board (always present so that the emulator can read it; the net
    // board is only built with LIBRETRO_NET=1)
    config.Set("Network", false);
    config.Set("SimulateNet", true);
    config.Set("PortIn", 1970u);
    config.Set("PortOut", 1971u);
    config.Set<std::string>("AddressOut", "127.0.0.1");

    // Outputs
    config.Set<std::string>("Outputs", "none");
    config.Set("OutputsWithLF", false);
    config.Set("OutputsTCPPort", 8000u);
    config.Set("OutputsUDPBroadcastPort", 8001u);

    // Debug
    config.Set("DumpMemory", false);
    config.Set("DumpTextures", false);

    // Input sensitivity (the mappings are added by LibretroInputs)
    config.Set("InputDigitalSensitivity", (unsigned) DEFAULT_DIGITAL_SENSITIVITY);
    config.Set("InputDigitalDecaySpeed", (unsigned) DEFAULT_DIGITAL_DECAYSPEED);
    for (const char *axis : s_joyAxisIds)
      config.Set(std::string("InputJoy") + axis + "DeadZone", (unsigned) DEFAULT_JOY_DEADZONE);
    for (const char *axis : s_joyAxisIds)
      config.Set(std::string("InputJoy") + axis + "Saturation", (unsigned) DEFAULT_JOY_SATURATION);

    return config;
  }

  void Apply(retro_environment_t environ, Util::Config::Node *config, bool initial)
  {
    for (const Option &option : Options())
    {
      if (!initial && !option.live)
        continue;
      ApplyOption(option, GetValue(environ, option), config);
    }
  }

  unsigned GetRumbleStrength(retro_environment_t environ)
  {
    return (unsigned) strtoul(GetValue(environ, "supermodel_rumble_strength").c_str(), nullptr, 10);
  }

  bool GetClearNVRAM(retro_environment_t environ)
  {
    return GetValue(environ, "supermodel_clear_nvram") == "enabled";
  }

  bool GetGunAutoReload(retro_environment_t environ)
  {
    return GetValue(environ, "supermodel_gun_auto_reload") == "enabled";
  }
}
