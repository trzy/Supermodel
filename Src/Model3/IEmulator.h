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
 * IEmulator.h
 * 
 * Header file defining the emulator interface, IEmulator.
 */

#ifndef INCLUDED_IEMULATOR_H
#define INCLUDED_IEMULATOR_H

class CBlockFile;
struct Game;
struct ROMSet;
class CRender2D;
class IRender3D;
class CInputs;
class COutputs;
class SuperAA;

/*
 * IEmulator:
 *
 * Interface (abstract base class) for emulation.
 */
class IEmulator
{
public:
  /*
   * SaveState(SaveState):
   *
   * Saves an image of the current state. Must never be called while emulator
   * is running (i.e., inside RunFrame()).
   *
   * Parameters:
   *    SaveState   Block file to save state information to.
   */
  virtual void SaveState(CBlockFile *SaveState) = 0;

  /*
   * LoadState(SaveState):
   *
   * Loads and resumes execution from a state image. Modifies data that may
   * be used by multiple threads -- use with caution and ensure threads are
   * not accessing data that will be touched, this can be done by calling
   * PauseThreads beforehand. Must never be called while emulator is running
   * (inside RunFrame()).
   *
   * Parameters:
   *    SaveState   Block file to load state information from.
   */
  virtual void LoadState(CBlockFile *SaveState) = 0;
  
  /*
   * SaveNVRAM(NVRAM):
   *
   * Saves an image of the current non-volatile memory state.
   *
   * Parameters:
   *    NVRAM   Block file to save NVRAM to.
   */
  virtual void SaveNVRAM(CBlockFile *NVRAM) = 0;

  /*
   * LoadNVRAM(NVRAM):
   *
   * Loads a non-volatile memory state.
   *
   * Parameters:
   *    NVRAM   Block file to load NVRAM state from.
   */
  virtual void LoadNVRAM(CBlockFile *NVRAM) = 0;
  
  /*
   * ClearNVRAM(void):
   *
   * Clears all non-volatile memory.
   */
  virtual void ClearNVRAM(void) = 0;
  
  /*
   * RunFrame(void):
   *
   * Runs one video frame.
   */
  virtual void RunFrame(void) = 0;

  /*
   * RenderFrame(void):
   *
   * Renders one video frame.
   */
  virtual void RenderFrame(void) = 0;

  /*
   * Reset(void):
   *
   * Resets the system. Does not modify non-volatile memory.
   */
  virtual void Reset(void) = 0;

  /* 
   * GetGame(void):
   *
   * Returns:
   *    A reference to the presently loaded game's information structure (which
   *    will be blank if no game has yet been loaded).
   */
  virtual const Game &GetGame(void) const = 0;

  /*
   * LoadGame(game, rom_set):
   *
   * Loads a game, copying in the provided ROMs and setting the hardware
   * stepping.
   *
    * Parameters:
   *    game      Game information.
   *    rom_set   ROMs.
   *
   * Returns:
   *    OKAY if successful, FAIL otherwise. Prints errors.
   */
  virtual bool LoadGame(const Game &game, const ROMSet &rom_set) = 0;
  
  /*
   * AttachRenderers(Render2DPtr, Render3DPtr):
   *
   * Attaches the renderers to the appropriate device objects.
   *
   * Parameters:
   *    Render2DPtr   Pointer to a tile renderer object.
   *    Render3DPtr   Same as above but for a 3D renderer.
   */
  virtual void AttachRenderers(CRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA) = 0;
  
  /*
   * AttachInputs(InputsPtr):
   *
   * Attaches OSD-managed inputs.
   *
   * Parameters:
   *    InputsPtr Pointer to the object containing input states.
   */
  virtual void AttachInputs(CInputs *InputsPtr) = 0;
  
  /*
   * AttachOutputs(InputsPtr):
   *
   * Attaches OSD-managed outputs (cabinet lamps, etc.)
   *
   * Parameters:
   *    OutputsPtr  Pointer to the object containing output states.
   */
  virtual void AttachOutputs(COutputs *OutputsPtr) = 0;

  /*
   * Init(void):
   *
   * One-time initialization of the context. Must be called prior to all
   * other members.
   *
   * NOTE: Command line settings will not have been applied here yet.
   *
   * Returns:
   *    OKAY is successful, otherwise FAILED if a non-recoverable error
   *    occurred. Prints own error messages.
   */
  virtual bool Init(void) = 0;

  /*
   * PauseThreads(void):
   *
   * Flags that any running threads should pause and waits for them to do so.
   * Should be used before invoking any method that accesses the internal 
   * state, eg LoadState or SaveState.
   */
  virtual bool PauseThreads(void) = 0;

  /*
   * ResumeThreads(void):
   *
   * Flags that any paused threads should resume running.
   */
  virtual bool ResumeThreads(void) = 0;

  /*
   * ~IEmulator(void):
   *
   * Destructor. Must free all resources.
   */
  
  virtual ~IEmulator(void)
  {
  }
};

#endif  // INCLUDED_IEMULATOR_H
