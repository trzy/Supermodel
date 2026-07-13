@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "VM_USER=juan"
set "VM_HOST=192.168.31.128"
set "REMOTE_WORKDIR=/home/juan/Supermodel"
set "REMOTE_CORE_DIR=/home/juan/.config/retroarch/cores"
set "REMOTE_INFO_DIR=/home/juan/.config/retroarch/info"
set "REMOTE_SYSTEM_DIR=/home/juan/.config/retroarch/system/Supermodel/Config"
set "REMOTE_SAVE_DIR=/home/juan/.config/retroarch/system/Supermodel/NVRAM"
set "REMOTE_TEST_DIR=/home/juan/Supermodel"
set "STAGE=%TEMP%\Supermodel-linux-stage"
set "ARCHIVE=%TEMP%\Supermodel-linux-stage.tar"
set "INFOFILE=%TEMP%\supermodel_libretro.info"
set "ROMFILE=D:\Emulation\ROMs\Sega Model 3\vf3.zip"
set "SYNC_SCOPE=Src Makefiles Config README.md build32.bat build64.bat buildlinux.bat"

pushd "%ROOT%"

echo Checking remote tree...
ssh %VM_USER%@%VM_HOST% "test -d '%REMOTE_WORKDIR%/Src'"
if errorlevel 1 goto bootstrap

echo Incrementally syncing changed files...
for /f "delims=" %%F in ('git diff --name-only --diff-filter=ACMRTUXB HEAD -- %SYNC_SCOPE%') do call :SyncOne "%%F"
for /f "delims=" %%F in ('git ls-files --others --exclude-standard -- %SYNC_SCOPE%') do call :SyncOne "%%F"
for /f "delims=" %%F in ('git diff --name-only --diff-filter=D HEAD -- %SYNC_SCOPE%') do call :DeleteOne "%%F"
goto build

:bootstrap
echo Staging source tree...
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%" >nul 2>&1
if errorlevel 1 exit /b 1

robocopy "%ROOT%Src" "%STAGE%\Src" /MIR /XD .git >nul
set "RC=%ERRORLEVEL%"
if %RC% GEQ 8 exit /b %RC%

robocopy "%ROOT%Makefiles" "%STAGE%\Makefiles" /MIR /XD .git >nul
set "RC=%ERRORLEVEL%"
if %RC% GEQ 8 exit /b %RC%

robocopy "%ROOT%Config" "%STAGE%\Config" /MIR /XD .git >nul
set "RC=%ERRORLEVEL%"
if %RC% GEQ 8 exit /b %RC%

copy /Y "%ROOT%README.md" "%STAGE%\README.md" >nul
if errorlevel 1 exit /b 1

echo Packing archive...
tar -cf "%ARCHIVE%" -C "%STAGE%" .
if errorlevel 1 exit /b 1

echo Uploading source archive...
scp "%ARCHIVE%" %VM_USER%@%VM_HOST%:/tmp/Supermodel-linux-stage.tar
if errorlevel 1 exit /b 1

echo Extracting source tree on the VM...
ssh %VM_USER%@%VM_HOST% "bash -lc 'set -e; mkdir -p \"%REMOTE_WORKDIR%\" \"%REMOTE_CORE_DIR%\" \"%REMOTE_INFO_DIR%\" \"%REMOTE_SYSTEM_DIR%\" \"%REMOTE_SAVE_DIR%\" \"%REMOTE_TEST_DIR%\"; tar -xf /tmp/Supermodel-linux-stage.tar -C \"%REMOTE_WORKDIR%\"; rm -f /tmp/Supermodel-linux-stage.tar'"
if errorlevel 1 exit /b 1

:build
echo Building on the VM...
ssh %VM_USER%@%VM_HOST% "bash -lc 'set -e; mkdir -p \"%REMOTE_CORE_DIR%\" \"%REMOTE_INFO_DIR%\" \"%REMOTE_SYSTEM_DIR%\" \"%REMOTE_SAVE_DIR%\" \"%REMOTE_TEST_DIR%\"; cd \"%REMOTE_WORKDIR%\"; make -f Makefiles/Makefile.UNIX LIBRETRO=1 BITS=64 -j2; cp \"lib64/supermodel_libretro.so\" \"%REMOTE_CORE_DIR%/supermodel_libretro.so\"'"
if errorlevel 1 exit /b 1

echo Creating RetroArch core info...
> "%INFOFILE%" (
  echo display_name = "Supermodel"
  echo corename = "Supermodel"
  echo systemname = "Sega Model 3"
  echo authors = "Supermodel Team"
  echo supported_extensions = "zip"
  echo licenses = "GPL-3.0-or-later"
  echo permissions = ""
)
if errorlevel 1 exit /b 1

scp "%INFOFILE%" %VM_USER%@%VM_HOST%:"%REMOTE_INFO_DIR%/supermodel_libretro.info"
if errorlevel 1 exit /b 1

echo Installing system files...
scp "%ROOT%Config\Games.xml" %VM_USER%@%VM_HOST%:"%REMOTE_SYSTEM_DIR%/Games.xml"
if errorlevel 1 exit /b 1
scp "%ROOT%Config\Music.xml" %VM_USER%@%VM_HOST%:"%REMOTE_SYSTEM_DIR%/Music.xml"
if errorlevel 1 exit /b 1

echo Copying test ROM...
scp "%ROMFILE%" %VM_USER%@%VM_HOST%:"%REMOTE_TEST_DIR%/vf3.zip"
if errorlevel 1 exit /b 1

echo Done.
popd
exit /b 0

:SyncOne
set "REL=%~1"
set "SRC=%ROOT%%REL%"
if not exist "%SRC%" exit /b 0
echo   syncing %REL%
ssh %VM_USER%@%VM_HOST% "mkdir -p \"$(dirname '%REMOTE_WORKDIR%/%REL%')\""
if errorlevel 1 exit /b 1
scp "%SRC%" %VM_USER%@%VM_HOST%:"%REMOTE_WORKDIR%/%REL%"
if errorlevel 1 exit /b 1
exit /b 0

:DeleteOne
set "REL=%~1"
echo   deleting %REL%
ssh %VM_USER%@%VM_HOST% "rm -f '%REMOTE_WORKDIR%/%REL%'"
if errorlevel 1 exit /b 1
exit /b 0
