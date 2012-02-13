/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson
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
 * Video.h
 * 
 * Header file for OS-dependent video functionality.
 */

#ifndef INCLUDED_VIDEO_H
#define INCLUDED_VIDEO_H

/*
 * BeginFrameVideo()
 *
 * Called at beginning of rendering of video frame.  Can be used by OSD layer to prevent rendering to screen.
 *
 * Returns true if emulator should render frame to screen.
 */
extern bool BeginFrameVideo();

/*
 * EndFrameVideo()
 *
 * Called at end of rendering of video frame.  Can be used by OSD layer to overlay graphics on screen.
 */
extern void EndFrameVideo();

#endif	// INCLUDED_VIDEO_H
