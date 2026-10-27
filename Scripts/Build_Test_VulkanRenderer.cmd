@echo off
setlocal
cd /d "%~dp0.."
call "%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
set "OBJ=MSVC\x64\Release"
cl /nologo /std:c++17 /EHsc /MT /O2 /DSUPERMODEL_WIN32 /I Src /I Src\OSD /I Src\OSD\SDL /I Src\Pkgs /I Libraries\SDL2-2.30.11\include /I "%VULKAN_SDK%\Include" /I Libraries\Vulkan-Headers\include /Fo"%OBJ%\Test_VulkanRenderer.obj" /Fe"%OBJ%\Test_VulkanRenderer.exe" Scripts\Test_VulkanRenderer.cpp ^
 "%OBJ%\R3DSceneBuilder.obj" "%OBJ%\Vulkan2D.obj" "%OBJ%\Vulkan3D.obj" "%OBJ%\VulkanContext.obj" "%OBJ%\VulkanMemory.obj" "%OBJ%\VulkanRingBuffer.obj" "%OBJ%\VulkanTextureCache.obj" "%OBJ%\volk_impl.obj" ^
 "%OBJ%\Mat4.obj" "%OBJ%\Model.obj" "%OBJ%\PolyHeader.obj" "%OBJ%\R3DFloat.obj" "%OBJ%\Vec.obj" "%OBJ%\NewConfig.obj" "%OBJ%\Format.obj" ^
 /link /LTCG /LIBPATH:"%OBJ%" SDL2.lib shell32.lib setupapi.lib advapi32.lib uuid.lib version.lib imm32.lib winmm.lib gdi32.lib user32.lib ole32.lib oleaut32.lib
exit /b %errorlevel%
