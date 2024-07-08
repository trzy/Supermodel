/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2016 Bart Trzynadlowski, Nik Henson
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
 * Model3GraphicsState.h
 *
 * Minimalistic implementation of IEmulator designed to load and view graphics
 * state.
 */

#ifndef INCLUDED_MODEL3GRAPHICSSTATE_H
#define INCLUDED_MODEL3GRAPHICSSTATE_H

#include "BlockFile.h"
#include "Game.h"
#include "ROMSet.h"
#include "CPU/Bus.h"
#include "Model3/IEmulator.h"
#include "Model3/IRQ.h"
#include "Model3/Real3D.h"
#include "Model3/TileGen.h"
#include "OSD/Logger.h"
#include "OSD/Video.h"
#include "Util/NewConfig.h"

/*
 * CModel3GraphicsState:
 *
 * Stores only graphics (tilegen and Real3D) state.
 */
class CModel3GraphicsState: public IEmulator, public IBus
{
public:
  void SaveState(CBlockFile *SaveState) override
  {
  }

  void LoadState(CBlockFile *SaveState) override
  {
    m_real3D.LoadState(SaveState);
    m_tileGen.LoadState(SaveState);
  }

  void SaveNVRAM(CBlockFile *NVRAM) override
  {
  }

  void LoadNVRAM(CBlockFile *NVRAM) override
  {
  }

  void ClearNVRAM(void) override
  {
  }

  void RunFrame(void) override
  {
    RenderFrame();
  }

  void RenderFrame(void) override
  {
    BeginFrameVideo();
    m_tileGen.BeginFrame();
    m_real3D.BeginFrame();
    m_real3D.RenderFrame();
    m_real3D.EndFrame();
    m_tileGen.EndFrame();
    EndFrameVideo();
  }

  void Reset(void) override
  {
    // Load state
    CBlockFile SaveState;
    if (OKAY != SaveState.Load(m_stateFilePath.c_str()))
      ErrorLog("Unable to load state from '%s'.", m_stateFilePath.c_str());
    else
    {
      LoadState(&SaveState);
      SaveState.Close();
    }
  }

  const Game &GetGame(void) const override
  {
    return m_game;
  }

  bool LoadGame(const Game &game, const ROMSet &rom_set) override
  {
    m_game = game;
    if (rom_set.get_rom("vrom").size <= 32*0x100000)
    {
      rom_set.get_rom("vrom").CopyTo(&m_vrom.get()[0], 32*100000);
      rom_set.get_rom("vrom").CopyTo(&m_vrom.get()[32*0x100000], 32*0x100000);
    }
    else
      rom_set.get_rom("vrom").CopyTo(m_vrom.get(), 64*0x100000);
    int stepping = ((m_game.stepping[0] - '0') << 4) | (m_game.stepping[2] - '0');
    m_real3D.SetStepping(stepping);
    return OKAY;
  }

  void AttachRenderers(CRender2D *render2D, IRender3D *render3D, SuperAA* superAA) override
  {
    m_tileGen.AttachRenderer(render2D);
    m_real3D.AttachRenderer(render3D);
  }

  void AttachInputs(CInputs *InputsPtr) override
  {
  }

  void AttachOutputs(COutputs *OutputsPtr) override
  {
  }

  bool Init(void) override
  {
    m_vrom.reset(new uint8_t[64*1024*1024], std::default_delete<uint8_t[]>());
    m_irq.Init();
    if (OKAY != m_tileGen.Init(&m_irq))
      return FAIL;
    if (OKAY != m_real3D.Init(m_vrom.get(), this, &m_irq, 0x100))
      return FAIL;
    return OKAY;
  }

  bool PauseThreads(void) override
  {
    return true;
  }

  bool ResumeThreads(void) override
  {
    return true;
  }

  CModel3GraphicsState(const Util::Config::Node &config, const std::string &filePath)
    : m_stateFilePath(filePath),
      m_tileGen(config),
      m_real3D(config)
  {
  }

  virtual ~CModel3GraphicsState(void)
  {
  }

private:
  const std::string         m_stateFilePath;
  std::shared_ptr<uint8_t>  m_vrom;
  Game                      m_game;
  CIRQ                      m_irq;
  CTileGen                  m_tileGen;
  CReal3D                   m_real3D;
};

#endif  // INCLUDED_CMODEL3GRAPHICSSTATE_H
