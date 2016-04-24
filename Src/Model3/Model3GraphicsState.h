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

#include "Model3/IEmulator.h"

/*
 * CModel3GraphicsState:
 *
 * Stores only graphics (tilegen and Real3D) state.
 */
class CModel3GraphicsState: public IEmulator, public IBus
{
public:
  void SaveState(CBlockFile *SaveState)
  {
  }
  
  void LoadState(CBlockFile *SaveState)
  {
    m_real3D.LoadState(SaveState);
    m_tileGen.LoadState(SaveState);
  }

  void SaveNVRAM(CBlockFile *NVRAM)
  {
  }

  void LoadNVRAM(CBlockFile *NVRAM)
  {
  }

  void ClearNVRAM(void)
  {
  }

  void RunFrame(void)
  {
    RenderFrame();
  }

  void RenderFrame(void)
  {
    BeginFrameVideo();
    m_tileGen.BeginFrame();
    m_real3D.BeginFrame();
    m_real3D.RenderFrame();
    m_real3D.EndFrame();
    m_tileGen.EndFrame();
    EndFrameVideo();
  }

  void Reset(void)
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

  const struct GameInfo * GetGameInfo(void)
  {
    return m_game;
  }

  bool LoadROMSet(const struct GameInfo *gameList, const char *zipFile)
  {
    // Load ROM
    struct ROMMap map[] =
    {
      { "VROM", m_vrom.get() },
      { NULL, NULL }
    };
    m_game = LoadROMSetFromZIPFile(map, gameList, zipFile, false);
    if (NULL == m_game)
      return ErrorLog("Failed to load ROM set."); 
    if (m_game->vromSize < 0x4000000)   // VROM is actually 64 MB
      CopyRegion(m_vrom.get(), m_game->vromSize, 0x4000000, m_vrom.get(), m_game->vromSize);
    m_real3D.SetStep(m_game->step);
    return OKAY;
  }

  void AttachRenderers(CRender2D *render2D, IRender3D *render3D)
  {
    m_tileGen.AttachRenderer(render2D);
    m_real3D.AttachRenderer(render3D);
  }

  void AttachInputs(CInputs *InputsPtr)
  {
  }

  void AttachOutputs(COutputs *OutputsPtr)
  {
  }

  bool Init(void)
  {
    m_vrom.reset(new uint8_t[64*1024*1024], std::default_delete<uint8_t[]>());
    m_irq.Init();
    if (OKAY != m_tileGen.Init(&m_irq))
      return FAIL;
    if (OKAY != m_real3D.Init(m_vrom.get(), this, &m_irq, 0x100))
      return FAIL;
    return OKAY;
  }

  bool PauseThreads(void)
  {
    return true;
  }

  bool ResumeThreads(void)
  {
    return true;
  }

  CModel3GraphicsState(const std::string &filePath)
    : m_stateFilePath(filePath)
  {
  }

  virtual ~CModel3GraphicsState(void)
  {
  }

private:
  const std::string         m_stateFilePath;
  std::shared_ptr<uint8_t>  m_vrom;
  const struct GameInfo     *m_game;
  CIRQ                      m_irq;
  CTileGen                  m_tileGen;
  CReal3D                   m_real3D;
};

#endif  // INCLUDED_CMODEL3GRAPHICSSTATE_H
