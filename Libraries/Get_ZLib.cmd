@echo off
cls

setlocal

cd /d "%~dp0"

echo === zlib 1.3.1 Source Downloader ===

set URL=https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip
set ZIPFILE=v1.3.1.zip
set OUTDIR=zlib-1.3.1

echo Downloading zlib source ZIP...
curl -L "%URL%" -o "%ZIPFILE%"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: curl failed to download the file.
    pause
    exit /b 1
)

echo Extracting ZIP using Windows built-in unzip...
powershell -NoLogo -Command "Expand-Archive -LiteralPath '%ZIPFILE%' -DestinationPath '.' -Force"

if not exist "zlib-1.3.1" (
    echo ERROR: Extraction failed or folder not found.
    pause
    exit /b 1
)

echo Cleaning up ZIP...
del "%ZIPFILE%"

echo Done! zlib source is now in: %OUTDIR%

endlocal
