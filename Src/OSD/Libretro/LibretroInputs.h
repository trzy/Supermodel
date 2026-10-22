/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * LibretroInputs.h
 *
 * Fixed mapping of Supermodel's game controls onto the libretro RetroPad and
 * light gun. This replaces the Input* key settings of Supermodel.ini: users
 * remap controls with the frontend's controller configuration, which shows
 * the names given here through input descriptors.
 */

#ifndef INCLUDED_LIBRETROINPUTS_H
#define INCLUDED_LIBRETROINPUTS_H

#include "Util/NewConfig.h"
#include "libretro.h"

#include <cstdint>
#include <string>
#include <vector>

namespace LibretroInputs
{
  /*
   * Stores the mapping of every Supermodel input ("Input<id>" keys) in the
   * given config node. Inputs that have no RetroPad assignment, including
   * all of Supermodel's own UI keys, are set to NONE. autoTrigger enables the
   * light gun auto-trigger on reload (InputAutoTrigger/InputAutoTrigger2).
   */
  void StoreMappings(Util::Config::Node *config, bool autoTrigger);

  /*
   * Builds the libretro input descriptors for a game's inputs (Game::inputs
   * flags). The strings point to static data and remain valid.
   */
  std::vector<retro_input_descriptor> GetDescriptors(uint32_t gameInputs);

  /*
   * Returns true if the game uses light guns (the core offers the libretro
   * light gun device on ports 1 and 2 for those games).
   */
  bool UsesLightGuns(uint32_t gameInputs);
}

#endif  // INCLUDED_LIBRETROINPUTS_H
