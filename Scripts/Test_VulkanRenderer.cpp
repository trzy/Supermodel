// ROM-free Vulkan integration smoke test. Run after the x64 Release build.
// Exercises pipeline creation, 2D uploads, 3D targets, resize, presentation and teardown.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "Graphics/Vulkan3D/VulkanContext.h"
#include "Graphics/Vulkan3D/Vulkan2D.h"
#include "Graphics/Vulkan3D/Vulkan3D.h"
#include "Util/NewConfig.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

static unsigned errors = 0;
static unsigned periodicReports = 0;
void DebugLog(const char*, ...) {}
void InfoLog(const char* fmt, ...)
{
  if (strstr(fmt, "Vulkan: textures -") || strstr(fmt, "Vulkan: ROM geometry -"))
    ++periodicReports;
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  puts("");
}
Result ErrorLog(const char* fmt, ...)
{
  ++errors;
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputs("\n", stderr);
  return Result::FAIL;
}

static void Check(bool ok, const char* operation)
{
  if (!ok) throw std::runtime_error(operation);
}

static void Run(SDL_Window* window)
{
  Vulkan3D::CVulkanContext context;
  Check(context.InitDevice(window, true, false) == Result::OKAY, "InitDevice");
  Check(context.Validation(), "Vulkan validation layer required for this test");
  Util::Config::Node config("Global");
  config.Set("WideScreen", false);
  config.Set("WideBackground", false);
  config.Set("NoWhiteFlash", false);
  auto bottom = std::make_shared<TileGenBuffer>();
  auto top = std::make_shared<TileGenBuffer>();
  for (unsigned y = 0; y < 384; ++y)
    for (unsigned x = 0; x < 496; ++x)
      bottom->data[y * 512 + x] = ((x / 8 + y / 8) & 1) ? 0xffffffff : 0xff403020;
  std::vector<uint32_t> culling(0x100000);
  std::vector<uint16_t> textures(2048 * 2048);
  Vulkan3D::CVulkan2D output(config, context);
  Vulkan3D::CVulkan3D scene(config, "smoke-test", context, output);
  scene.SetStepping(Game::Stepping::MODEL3_2_1);
  scene.AttachMemory(culling.data(), culling.data(), culling.data(), culling.data(), textures.data());
  // A culled frame still allocates and clears the 3D render target, without needing a ROM scene.
  scene.SetBlockCulling(true);
  for (int pass = 0; pass < 8; ++pass)
  {
    if (pass == 4)
    {
      SDL_SetWindowSize(window, 1920, 1200);
      SDL_PumpEvents();
      Check(context.RecreateSwapchain(), "RecreateSwapchain");
    }
    const auto extent = context.Extent();
    Check(output.Init(0, 0, extent.width, extent.height, extent.width, extent.height, 0,
      static_cast<UpscaleMode>(pass % 4)) == Result::OKAY, "2D Init");
    Check(scene.Init(0, 0, extent.width, extent.height, extent.width, extent.height, 0)
      == Result::OKAY, "3D Init");
    output.AttachDrawBuffers(bottom, top);
    unsigned rendered = 0;
    const unsigned frameCount = pass < 4 ? 6 : 160;
    for (unsigned attempt = 0; attempt < frameCount + 100 && rendered < frameCount; ++attempt)
    {
      SDL_PumpEvents();
      if (!context.BeginFrame()) continue;
      output.BeginFrame();
      scene.BeginFrame();
      output.PreRenderFrame();
      output.RenderFrameBottom();
      scene.RenderFrame();
      output.RenderFrameTop();
      scene.EndFrame();
      output.EndFrame();
      context.EndFrame();
      ++rendered;
    }
    Check(rendered == frameCount, "Present requested frames");
  }
}

int main()
{
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
  SDL_Window* window = SDL_CreateWindow("Vulkan smoke test", SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED, 496, 384, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
  int result = 0;
  try
  {
    Check(window != nullptr, SDL_GetError());
    Run(window);
    // A second context checks teardown and reuse of the dynamic loader.
    Run(window);
    Check(errors == 0, "Vulkan errors or validation warnings were logged");
    Check(periodicReports == 0, "Periodic diagnostics must be disabled by default");
    puts("PASS: all four upscale modes, 1328 frames, 1920x1200, no periodic reports, resize and teardown.");
  }
  catch (const std::exception& e)
  {
    fprintf(stderr, "FAIL: %s\n", e.what());
    result = 1;
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
  return result;
}
