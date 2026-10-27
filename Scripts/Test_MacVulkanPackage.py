"""Verify the packaged macOS runtime using SDL, without a system Vulkan install."""
import ctypes
import os
from pathlib import Path
import sys
import tarfile
import tempfile

for name in ("SDL_VULKAN_LIBRARY", "DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH"):
    os.environ.pop(name, None)

with tempfile.TemporaryDirectory(prefix="supermodel-mac-package-") as temporary:
    with tarfile.open(sys.argv[1], "r:gz") as archive:
        archive.extractall(temporary, filter="data")
    packages = [p for p in Path(temporary).iterdir() if p.is_dir()]
    assert len(packages) == 1, "Expected one package directory"
    package = packages[0]
    frameworks = package / "Frameworks"
    runtime = frameworks / "libMoltenVK.dylib"
    for path in (package / "supermodel", runtime,
                 frameworks / "MoltenVK-LICENSE.txt", frameworks / "MoltenVK-NOTICE.txt"):
        assert path.is_file(), f"Missing package file: {path}"

    # Loading the library also resolves its dependencies.
    molten = ctypes.CDLL(str(runtime))
    assert molten.vkGetInstanceProcAddr, "Missing Vulkan entry point"
    sdl = ctypes.CDLL(str(frameworks / "SDL2.framework" / "SDL2"))
    sdl.SDL_Init.argtypes = [ctypes.c_uint32]
    sdl.SDL_Init.restype = ctypes.c_int
    sdl.SDL_GetError.restype = ctypes.c_char_p
    sdl.SDL_Vulkan_LoadLibrary.argtypes = [ctypes.c_char_p]
    sdl.SDL_Vulkan_LoadLibrary.restype = ctypes.c_int
    sdl.SDL_Vulkan_GetVkGetInstanceProcAddr.restype = ctypes.c_void_p
    assert sdl.SDL_Init(0x20) == 0, sdl.SDL_GetError()
    try:
        # Same explicit bundled path used by the application's Mac startup.
        result = sdl.SDL_Vulkan_LoadLibrary(os.fsencode(runtime))
        assert result == 0, sdl.SDL_GetError()
        assert sdl.SDL_Vulkan_GetVkGetInstanceProcAddr(), sdl.SDL_GetError()
        sdl.SDL_Vulkan_UnloadLibrary()
    finally:
        sdl.SDL_Quit()
    print("PASS: package contains MoltenVK and notices; SDL loads the bundled runtime without environment overrides.")
