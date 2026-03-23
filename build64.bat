@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
pushd "%ROOT%"

set "PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%"
set "SDL2_INCLUDE_DIR=C:/msys64/mingw64/include/SDL2"
set "SDL2_LIB_DIR=C:/msys64/mingw64/lib"

echo Building Supermodel libretro Win64 DLL...
mingw32-make.exe -f Makefiles/Makefile.Win32 LIBRETRO=1 BITS=64 SDL2_INCLUDE_DIR=%SDL2_INCLUDE_DIR% SDL2_LIB_DIR=%SDL2_LIB_DIR% -j2
set "RC=%ERRORLEVEL%"

popd
exit /b %RC%
