# Optional Vulkan renderer

The default renderer is still OpenGL. To use Vulkan for Model 3 games, add this
to the existing `Supermodel.ini` and restart the emulator:

```ini
[Global]
Vulkan3DEngine = true
```

`false`, or omitting the setting, uses the original OpenGL renderer and honors
`New3DEngine` as before. Game-specific INI sections can override the setting.
The original launcher and input UI are retained. Pro-1000 uses OpenGL.

The launcher's Video tab offers mutually exclusive `OpenGL (4.1)` and `Vulkan`
checkboxes. Selecting OpenGL saves `New3DEngine = true` and
`Vulkan3DEngine = false`; selecting Vulkan saves the opposite values. One choice
stays selected. Legacy OpenGL remains available through manual INI configuration.

This port includes the Vulkan 2D/3D rendering and presentation path only.
Vulkan does not provide post-processing filters, OpenGL supersampling/CRT colors,
screenshots, or the OpenGL bitmap gun crosshair. Lightgun input is unchanged.
OpenGL retains its existing features. Vulkan initialization failures are reported
in the log; set `Vulkan3DEngine = false` to use OpenGL again.

The 2D tile layers keep their native 496x384 resolution, as in OpenGL. The existing
`UpscaleMode` setting applies in Vulkan too: 0 nearest, 1 biquintic, 2 bilinear
(default), 3 bicubic. The 3D scene renders at the output resolution and does not
receive the tile upscale filter. This is separate from post-processing effects.

Detailed texture checks and 600-frame texture/geometry reports are disabled by
default. They add CPU work and synchronous file logging on the rendering thread.
For investigations only, set `VulkanDiagnostics = true` in the INI to enable them.

The Vulkan path requires a compatible GPU and driver (Vulkan 1.3, or the backend's
supported Vulkan 1.2 extension path). The Vulkan loader is loaded dynamically;
OpenGL sessions do not initialize it.

## Building

Windows/MSVC: install the Vulkan SDK and set `VULKAN_SDK` (the SDK installer
normally sets it). Its `Include` directory supplies the Vulkan headers. The
runtime is loaded through the bundled MIT-licensed volk loader, so no Vulkan
import library is linked. Alternatively, place Vulkan-Headers in
`Libraries/Vulkan-Headers`.

Makefile builds also accept `Libraries/Vulkan-Headers/include` and
`VULKAN_SDK/Include` (or `include`), or system-installed Vulkan headers. Linux additionally
links `libdl`. The normal SDL2 and other build dependencies still apply.
Embedded SPIR-V headers are included, so shader compilation is unnecessary for
normal builds. After changing GLSL, regenerate its corresponding `*.spv.h` with
glslangValidator and retain the array's `<name>_spv_size = sizeof(<name>_spv)`.

For a ROM-free Windows GPU smoke test, first build x64 Release, then run
`Scripts/Build_Test_VulkanRenderer.cmd` and
`MSVC/x64/Release/Test_VulkanRenderer.exe`. It requires the Vulkan validation
layer, exercises all four tile upscale modes at native and 1920x1200 resolution,
crosses the 600-frame reporting interval with diagnostics disabled, resizes the
swapchain and recreates the context. It does not verify game geometry, visual
accuracy, or reproduce gameplay frame-time spikes.

## Source and attribution

The Vulkan renderer, shaders and volk loader were imported from
[Igloo-Garage/Supermodel-VK](https://github.com/Igloo-Garage/Supermodel-VK),
revision `a44717117305d3e821e8672266426c0376a880b6`.
The imported renderer is Copyright 2026 The Supermodel-VK authors, derived from
Supermodel, Copyright 2003–2026 The Supermodel Team, under GPL-3.0-or-later.
Original notices are retained in the source files. volk is Copyright 2018–2024
Arseny Kapoulkine, under the MIT license reproduced in `Src/Pkgs/volk/volk.h`.

Local adaptations retain the original frontend and emulation interfaces, map the
current hardware stepping enum, generalize the tile-generator renderer hooks, and
remove post-processing, bezel, overlay and screenshot support from the imported
compositor/context. The composite shader has three image slots for the bottom
tiles, 3D scene and top tiles.
