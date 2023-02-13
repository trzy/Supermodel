/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2019 Bart Trzynadlowski, Nik Henson, Ian Curtis,
 **                     Harry Tuttle, and Spindizzi
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * SDLIncludes.h
 *
 * Header file for SDL library.
 */

#ifndef INCLUDED_SDLINCLUDES_H
#define INCLUDED_SDLINCLUDES_H

#ifdef SUPERMODEL_OSX
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#else
#include <SDL.h>
#include <SDL_audio.h>
#endif

#ifdef NET_BOARD
#ifdef SUPERMODEL_OSX
#include <SDL2_net/SDL_net.h>
#else
#include <SDL_net.h>
#endif
#endif


SDL_Window *get_window();
unsigned int get_total_width();
unsigned int get_total_height();

#endif  // INCLUDED_SDLINCLUDES_H
