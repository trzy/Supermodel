##
## Supermodel
## A Sega Model 3 Arcade Emulator.
## Copyright 2003-2022 The Supermodel Team
##
## This file is part of Supermodel.
##
## Supermodel is free software: you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free
## Software Foundation, either version 3 of the License, or (at your option)
## any later version.
##
## Supermodel is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
## more details.
##
## You should have received a copy of the GNU General Public License along
## with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
##

#
# Rules.inc
#
# Build rules, included by all (platform-specific) top-level Makefiles.
#


###############################################################################
# Output Locations
###############################################################################

OBJ_DIR = obj$(strip $(BITS))
BIN_DIR = bin$(strip $(BITS))

OUTFILE = supermodel


###############################################################################
# Compiler Flags
###############################################################################

#
# Construct Supermodel build options
#
SUPERMODEL_BUILD_FLAGS =

# If render state analyser is enabled, need to define DEBUG
ifeq ($(strip $(DEBUG)),1)
	SUPERMODEL_BUILD_FLAGS += -DDEBUG
endif

# If Net Board support is enabled, need to define NET_BOARD
ifeq ($(strip $(NET_BOARD)),1)
	SUPERMODEL_BUILD_FLAGS += -DNET_BOARD
endif

# If Bart's new frame timing is enabled, need to define NEW_FRAME_TIMING
ifeq ($(strip $(NEW_FRAME_TIMING)),1)
	SUPERMODEL_BUILD_FLAGS += -DNEW_FRAME_TIMING
endif

# If built-in debugger enabled, need to define SUPERMODEL_DEBUGGER
ifeq ($(strip $(ENABLE_DEBUGGER)),1)
	SUPERMODEL_BUILD_FLAGS += -DSUPERMODEL_DEBUGGER
endif

#
# Compiler options
#
ARCH =
OPT = -O3
WARN = -Wall
CSTD = -std=iso9899:2011
CXXSTD = -std=c++17


#
# Construct the compiler (C and C++) and linker flags
#
COMMON_CFLAGS = -c $(ARCH) $(OPT) $(WARN) $(addprefix -I,$(sort $(INCLUDE_DIRS))) -DGLEW_STATIC $(SUPERMODEL_BUILD_FLAGS)
CFLAGS = $(COMMON_CFLAGS) $(CSTD)
CXXFLAGS = $(PLATFORM_CXXFLAGS) $(COMMON_CFLAGS) $(CXXSTD)
LDFLAGS = -o $(BIN_DIR)/$(OUTFILE) $(PLATFORM_LDFLAGS) -s


###############################################################################
# Source Files
###############################################################################

SRC_FILES = \
	Src/CPU/PowerPC/PPCDisasm.cpp \
	Src/BlockFile.cpp \
	Src/Pkgs/unzip.cpp \
	Src/Pkgs/ioapi.cpp \
	Src/Model3/93C46.cpp \
	Src/Util/BitRegister.cpp \
	Src/JTAG.cpp \
	Src/Graphics/Legacy3D/Error.cpp \
	Src/Pkgs/glew.cpp \
	Src/Graphics/Shader.cpp \
	Src/Model3/Real3D.cpp \
	Src/Graphics/Legacy3D/Legacy3D.cpp \
	Src/Graphics/Legacy3D/Models.cpp \
	Src/Graphics/Legacy3D/TextureRefs.cpp \
	Src/Graphics/New3D/GLSLShader.cpp \
	Src/Graphics/New3D/R3DFrameBuffers.cpp \
	Src/Graphics/New3D/New3D.cpp \
	Src/Graphics/New3D/Mat4.cpp \
	Src/Graphics/New3D/Model.cpp \
	Src/Graphics/New3D/PolyHeader.cpp \
	Src/Graphics/New3D/VBO.cpp \
	Src/Graphics/New3D/Vec.cpp \
	Src/Graphics/New3D/R3DShader.cpp \
	Src/Graphics/New3D/R3DFloat.cpp \
	Src/Graphics/New3D/R3DScrollFog.cpp \
	Src/Graphics/New3D/TextureBank.cpp \
	Src/Graphics/FBO.cpp \
	Src/Graphics/Render2D.cpp \
	Src/Graphics/SuperAA.cpp \
	Src/Model3/TileGen.cpp \
	Src/Model3/Model3.cpp \
	Src/CPU/PowerPC/ppc.cpp \
	Src/OSD/SDL/Main.cpp \
	Src/OSD/SDL/Audio.cpp \
	Src/OSD/SDL/Thread.cpp \
	Src/Model3/SoundBoard.cpp \
	Src/Sound/SCSP.cpp \
	Src/Sound/SCSPDSP.cpp \
	Src/CPU/68K/68K.cpp \
	$(OBJ_DIR)/m68kcpu.c \
	$(OBJ_DIR)/m68kopnz.c \
	$(OBJ_DIR)/m68kopdm.c \
	$(OBJ_DIR)/m68kopac.c \
	$(OBJ_DIR)/m68kops.c \
	Src/Model3/DSB.cpp \
	Src/CPU/Z80/Z80.cpp \
	Src/Model3/IRQ.cpp \
	Src/Model3/53C810.cpp \
	Src/Model3/PCI.cpp \
	Src/Model3/RTC72421.cpp \
	Src/Model3/DriveBoard/DriveBoard.cpp \
	Src/Model3/DriveBoard/WheelBoard.cpp \
	Src/Model3/DriveBoard/JoystickBoard.cpp \
	Src/Model3/DriveBoard/SkiBoard.cpp \
	Src/Model3/DriveBoard/BillBoard.cpp \
	Src/Model3/MPC10x.cpp \
	Src/Inputs/Input.cpp \
	Src/Inputs/Inputs.cpp \
	Src/Inputs/InputSource.cpp \
	Src/Inputs/InputSystem.cpp \
	Src/Inputs/InputTypes.cpp \
	Src/Inputs/MultiInputSource.cpp \
	Src/OSD/SDL/SDLInputSystem.cpp \
	Src/OSD/SDL/Crosshair.cpp \
	Src/OSD/Outputs.cpp \
	Src/Sound/MPEG/MpegAudio.cpp \
	Src/Model3/Crypto.cpp \
	Src/OSD/Logger.cpp \
	Src/Util/Format.cpp \
	Src/Util/NewConfig.cpp \
	Src/Util/ByteSwap.cpp \
	Src/Util/ConfigBuilders.cpp \
	Src/GameLoader.cpp \
	Src/Pkgs/tinyxml2.cpp \
	Src/ROMSet.cpp \
	$(PLATFORM_SRC_FILES)

ifeq ($(strip $(NET_BOARD)),1)
	SRC_FILES += \
		Src/Network/TCPReceive.cpp \
		Src/Network/TCPSend.cpp \
		Src/Network/NetBoard.cpp \
		Src/Network/SimNetBoard.cpp
endif

ifeq ($(strip $(ENABLE_DEBUGGER)),1)
	SRC_FILES += \
		Src/Debugger/Debugger.cpp \
		Src/Debugger/ConsoleDebugger.cpp \
		Src/Debugger/SupermodelDebugger.cpp \
		Src/Debugger/CPUDebug.cpp \
		Src/Debugger/AddressTable.cpp \
		Src/Debugger/Breakpoint.cpp \
		Src/Debugger/CodeAnalyser.cpp \
		Src/Debugger/Exception.cpp \
		Src/Debugger/Interrupt.cpp \
		Src/Debugger/DebuggerIO.cpp \
		Src/Debugger/Label.cpp \
		Src/Debugger/Register.cpp \
		Src/Debugger/Watch.cpp \
		Src/Debugger/CPU/PPCDebug.cpp \
		Src/Debugger/CPU/68KDebug.cpp \
		Src/Debugger/CPU/Musashi68KDebug.cpp \
		Src/Debugger/CPU/Z80Debug.cpp
endif

#
# Sorted-path compile order
#
OBJ_FILES = $(foreach file,$(SRC_FILES),$(OBJ_DIR)/$(basename $(notdir $(file))).o)

#
# Deduce include directories from the source file list. The sort function
# removes duplicates and is used to construct a set.
#
INCLUDE_DIRS = $(sort $(foreach file,$(SRC_FILES),$(dir	$(file))))


###############################################################################
# Targets
###############################################################################

#
# Default target: build Supermodel binary. Must be specified *before* the auto-
# generated dependencies because otherwise, make gets confused for some reason
# and thinks the default target is just one of the object files.
#
all:	$(BIN_DIR)/$(OUTFILE)

#
# Release targets: used only to create official builds stamped with the Git
# version number. Double-colon rules should force ordered execution.
# The automated build script relies on these -- don't modify their output!
#
.PHONY: set_version
.PHONY: print_message
.PHONY: version
release:: set_version
release:: print_message
release:: clean
release:: all
set_version:
	$(eval VERSION = $(strip $(subst ',,0.3a-git-$(shell git rev-parse --short HEAD --sq))))
	$(eval SUPERMODEL_BUILD_FLAGS += -DSUPERMODEL_VERSION=\"$(VERSION)\")
print_message:
	$(info Building Supermodel Version $(VERSION))
version: set_version
	@echo $(VERSION)

$(BIN_DIR)/$(OUTFILE):	$(BIN_DIR) $(OBJ_DIR) $(OBJ_FILES)
	$(info --------------------------------------------------------------------------------)
	$(info Linking Supermodel     : $(BIN_DIR)/$(OUTFILE))
	$(SILENT)$(LD) $(OBJ_FILES) $(LDFLAGS)
	$(info --------------------------------------------------------------------------------)

$(BIN_DIR):
	$(info Creating directory     : $(BIN_DIR))
	$(SILENT)mkdir $(BIN_DIR)

$(OBJ_DIR):
	$(info Creating directory     : $(OBJ_DIR))
	$(SILENT)mkdir $(OBJ_DIR)


###############################################################################
# Rules
###############################################################################

#
# Create list of auto-generated dependency files (which contain rules that make
# understands) and include them all.
#
AUTODEPS := $(patsubst %.o,%.d,$(OBJ_FILES))
-include $(AUTODEPS)

#
# VPATH is the search path for files. This trick allows a single %.cpp rule to
# be matched against files in sub-directories of Src/.
#
VPATH = $(INCLUDE_DIRS)

#
# Compilation rules that both auto-generate the dependency files and compile
# the source code. This technique is described in the reply by user "rr-" at:
# https://stackoverflow.com/questions/8025766/makefile-auto-dependency-generation
#
$(OBJ_DIR)/%.o:	%.cpp
	$(info Generating dependencies: $< -> $(OBJ_DIR)/$(*F).d)
	$(SILENT)$(CXX) -MM -MP -MT $(OBJ_DIR)/$(*F).o -MT $(OBJ_DIR)/$(*F).d $(CXXFLAGS) $< > $(OBJ_DIR)/$(*F).d
	$(info Compiling              : $< -> $@)
	$(SILENT)$(CXX) $(CXXFLAGS) $< -o $@

$(OBJ_DIR)/%.o:	%.c
	$(info Generating dependencies: $< -> $(OBJ_DIR)/$(*F).d)
	$(SILENT)$(CC) -MM -MP -MT $(OBJ_DIR)/$(*F).o -MT $(OBJ_DIR)/$(*F).d $(CFLAGS) $< > $(OBJ_DIR)/$(*F).d
	$(info Compiling              : $< -> $@)
	$(SILENT)$(CC) $(CFLAGS) $< -o $@


#
# Musashi 68K emulator
#
# All generated source files are emitted to the object directory. For MSVC,
# INLINE must be redefined as "static __inline", which is the syntax in C mode.
#
# The .exe suffix is absolutely required for native Windows non-MSYS builds!
#

MUSASHI_OUTFILE = $(OBJ_DIR)/m68kmake.exe # do not remove the .exe suffix!
MUSASHI_CFLAGS = -ISrc/CPU/68K/Musashi -I$(OBJ_DIR) -DINLINE="static inline" -Wno-unused-variable
MUSASHI_LDFLAGS = -o $(MUSASHI_OUTFILE) $(OBJ_DIR)/m68kmake.o -s

$(MUSASHI_OUTFILE): Src/CPU/68K/Musashi/m68kmake.c Src/CPU/68K/Musashi/m68k_in.c
	$(info --------------------------------------------------------------------------------)
	$(info Compiling              : $< -> $(OBJ_DIR)/m68kmake.o)
	$(SILENT)$(CC) $< $(CFLAGS) -o $(OBJ_DIR)/m68kmake.o
	$(info Linking                : $(MUSASHI_OUTFILE))
	$(SILENT)$(LD) $(MUSASHI_LDFLAGS)

$(OBJ_DIR)/m68kops.h $(OBJ_DIR)/m68kops.c $(OBJ_DIR)/m68kopac.c $(OBJ_DIR)/m68kopdm.c $(OBJ_DIR)/m68kopnz.c:	$(MUSASHI_OUTFILE) Src/CPU/68K/Musashi/m68k_in.c Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h
	$(info Generating 68K emulator: $@)
	@$(MUSASHI_OUTFILE) $(OBJ_DIR) Src/CPU/68K/Musashi/m68k_in.c

$(OBJ_DIR)/m68kcpu.o:	Src/CPU/68K/Musashi/m68kcpu.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $(OBJ_DIR)/m68kcpu.o

$(OBJ_DIR)/m68kops.o:	$(OBJ_DIR)/m68kops.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopac.o: $(OBJ_DIR)/m68kopac.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopdm.o: $(OBJ_DIR)/m68kopdm.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopnz.o: $(OBJ_DIR)/m68kopnz.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE)
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@
