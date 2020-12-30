##
## Supermodel
## A Sega Model 3 Arcade Emulator.
## Copyright 2011-2020 Bart Trzynadlowski, Nik Henson, Ian Curtis,
##                     Harry Tuttle, and Spindizzi
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
# Options.inc
#
# Build options.
#


###############################################################################
# Build Options
###############################################################################

#
# Verbose progression
#
VERBOSE =
ifneq ($(filter $(strip $(VERBOSE)),0 1),$(strip $(VERBOSE)))
	override VERBOSE =
endif
SILENT = @
ifeq ($(strip $(VERBOSE)),1)
	SILENT =
endif

#
# Enable render state analyser for the legacy 3D engine (will slow down emulation!)
#
DEBUG =
ifneq ($(filter $(strip $(DEBUG)),0 1),$(strip $(DEBUG)))
	override DEBUG =
endif

#
# Enable support for Model3 Net Board emulation
#
NET_BOARD =
ifneq ($(filter $(strip $(NET_BOARD)),0 1),$(strip $(NET_BOARD)))
	override NET_BOARD =
endif

#
# Include console-based debugger in emulator ('yes' or 'no')
#
ENABLE_DEBUGGER =
ifneq ($(filter $(strip $(ENABLE_DEBUGGER)),0 1),$(strip $(ENABLE_DEBUGGER)))
	override ENABLE_DEBUGGER =
endif