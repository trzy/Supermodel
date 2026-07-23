@echo off
cls

setlocal

cd /d "%~dp0"

echo === SDL 2.30.11 Source Downloader ===

set URL=https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.11.zip
set ZIPFILE=release-2.30.11.zip
set OUTDIR=SDL2-2.30.11

echo Downloading SDL source ZIP...
curl -L "%URL%" -o "%ZIPFILE%"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: curl failed to download the file.
    pause
    exit /b 1
)

echo Extracting ZIP using Windows built-in unzip...(sorry it's slow af)
powershell -NoLogo -Command "Expand-Archive -LiteralPath '%ZIPFILE%' -DestinationPath '.' -Force"

if not exist "SDL-release-2.30.11" (
    echo ERROR: Extraction failed.
    pause
    exit /b 1
)

echo Renaming folder...
ren SDL-release-2.30.11 %OUTDIR%

echo Cleaning up ZIP...
del "%ZIPFILE%"

echo Done! SDL source is now in: %OUTDIR%

endlocal
