OUTFILE = m3.exe

CC = cl
LD = link

CFLAGS = /c /nologo /I ".\\" /I "win32\\" /I "osd_common\\" /I "c:\dxsdk\include\\" /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /D "_LOG_" #/D "RENDERER_D3D"
LFLAGS = /libpath:"c:\dxsdk\lib" /nologo /subsystem:console /incremental:no /machine:I386 /out:"$(OUTFILE)" 
OBJ_LIBS = zlib.lib dxguid.lib dinput8.lib dxerr9.lib user32.lib gdi32.lib advapi32.lib
OBJ_D3D_LIBS = d3dx9.lib d3d9.lib
OBJ_GL_LIBS = OpenGL32.lib GLu32.lib

#
# Object Files
#
# OBJ are the object files as seen by Make, OBJ_MS are how they must be passed
# to MSVC (MS-style backslashes are used.)
#

OBJ_DIR = obj
OBJ = $(OBJ_DIR)/disasm.obj $(OBJ_DIR)/ppc.obj $(OBJ_DIR)/tilegen.obj $(OBJ_DIR)/win_input.obj $(OBJ_DIR)/win_main.obj $(OBJ_DIR)/bridge.obj $(OBJ_DIR)/controls.obj $(OBJ_DIR)/dma.obj $(OBJ_DIR)/dsb1.obj $(OBJ_DIR)/eeprom.obj $(OBJ_DIR)/r3d.obj $(OBJ_DIR)/rtc.obj $(OBJ_DIR)/scsi.obj $(OBJ_DIR)/model3.obj
OBJ_D3D = $(OBJ_DIR)/dx_render.obj
OBJ_GL = $(OBJ_DIR)/glrender.obj $(OBJ_DIR)/win_gl.obj
OBJ_MS = $(OBJ_DIR)\disasm.obj $(OBJ_DIR)\ppc.obj $(OBJ_DIR)\tilegen.obj $(OBJ_DIR)\win_input.obj $(OBJ_DIR)\win_main.obj $(OBJ_DIR)\bridge.obj $(OBJ_DIR)\controls.obj $(OBJ_DIR)\dma.obj $(OBJ_DIR)\dsb1.obj $(OBJ_DIR)\eeprom.obj $(OBJ_DIR)\r3d.obj $(OBJ_DIR)\rtc.obj $(OBJ_DIR)\scsi.obj $(OBJ_DIR)\model3.obj
OBJ_D3D_MS = $(OBJ_DIR)\dx_render.obj
OBJ_GL_MS = $(OBJ_DIR)\glrender.obj $(OBJ_DIR)\win_gl.obj

#
# Rules
#
# "d3d" makes the Direct3D version, "gl" makes the OpenGL version.
#

gl:	$(OBJ) $(OBJ_GL)
	$(LD) $(OBJ_MS) $(OBJ_GL_MS) $(OBJ_LIBS) $(OBJ_GL_LIBS) $(LFLAGS) 

d3d:	$(OBJ) $(OBJ_D3D)
	$(LD) $(OBJ_MS) $(OBJ_D3D_MS) $(OBJ_LIBS) $(OBJ_D3D_LIBS) $(LFLAGS) 

clean:	
	del obj\*.obj

$(OBJ_DIR)/%.obj:	%.c %.h
	$(CC) $< $(CFLAGS) /Fo$(OBJ_DIR)\$(*F).obj

$(OBJ_DIR)/%.obj:	%.c
	$(CC) $< $(CFLAGS) /Fo$(OBJ_DIR)\$(*F).obj
	
$(OBJ_DIR)/%.obj:	ppc/%.c ppc/%.h
	$(CC) ppc\$(*F).c $(CFLAGS) /Fo$(OBJ_DIR)\$(*F).obj
	
$(OBJ_DIR)/%.obj:	osd_common/%.c osd_common/%.h
	$(CC) osd_common\$(*F).c $(CFLAGS) /Fo$(OBJ_DIR)\$(*F).obj
	
$(OBJ_DIR)/%.obj:	osd_common/%.c
	$(CC) osd_common\$(*F).c $(CFLAGS) /Fo$(OBJ_DIR)\$(*F).obj	
	
$(OBJ_DIR)/%.obj:	win32/%.c
	$(CC) win32\$(*F).c $(CFLAGS) /Fo$(OBJ_DIR)\$(*F).obj
	