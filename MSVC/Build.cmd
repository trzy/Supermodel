@echo off

call "%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"

msbuild "Supermodel.sln" /m /p:Configuration=Release /p:Platform=x64

