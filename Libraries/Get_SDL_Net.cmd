@echo off
cls

setlocal

cd /d "%~dp0"

echo === SDL_net 2.4.0 Source Downloader ===

set URL=https://github.com/libsdl-org/SDL_net/archive/refs/tags/release-2.4.0.zip
set ZIPFILE=release-2.4.0.zip
set OUTDIR=SDL2_net-2.4.0

echo Downloading SDL_net source ZIP...
curl -L "%URL%" -o "%ZIPFILE%"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: curl failed to download the file.
    pause
    exit /b 1
)

echo Extracting ZIP using Windows built-in unzip...
powershell -NoLogo -Command "Expand-Archive -LiteralPath '%ZIPFILE%' -DestinationPath '.' -Force"

if not exist "SDL_net-release-2.4.0" (
    echo ERROR: Extraction failed.
    pause
    exit /b 1
)

echo Renaming folder...
ren SDL_net-release-2.4.0 %OUTDIR%

echo Cleaning up ZIP...
del "%ZIPFILE%"

echo Done! SDL_net source is now in: %OUTDIR%

endlocal
