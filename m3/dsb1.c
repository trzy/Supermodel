/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * dsb1.c
 *
 * DSB1 Digital Sound Board emulation.
 */

#include "model3.h"

void dsb1_init(void)
{

}

void dsb1_reset(void)
{

}

void dsb1_shutdown(void)
{

}

/*
 * void dsb1_save_state(FILE *fp);
 *
 * Saves the state of the DSB1 (for games which have it) to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void dsb1_save_state(FILE *fp)
{
    if (!(m3_config.flags & GAME_OWN_DSB1))
        return;
}

/*
 * void dsb1_load_state(FILE *fp);
 *
 * Loads the state of the DSB1 (for games which have it) from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void dsb1_load_state(FILE *fp)
{
    if (!(m3_config.flags & GAME_OWN_DSB1))
        return;
}

