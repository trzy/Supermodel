@if "%~1" equ "" (
echo This file cannot be run without arguments. Please run 'Build.Release.bat' instead.
pause
exit
)
set CONFIG=%~1
set PLATFORM=%2
set PROJECT=%3
@if "%4" neq "" (set ACTION=%4) else (set ACTION=Build)
if %PLATFORM% == x64 (set MACHINE=x64) else (set MACHINE=x86)
call "%VS90COMNTOOLS%..\..\VC\vcvarsall.bat" %MACHINE%
devenv /nologo Supermodel.sln /Project %PROJECT% /%ACTION% "%CONFIG%|%PLATFORM%"
@if %ERRORLEVEL% neq 0 (
echo %~1 %3 %4 failed
pause
exit
)
