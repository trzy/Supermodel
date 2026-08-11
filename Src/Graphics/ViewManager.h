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
 **/

#ifndef INCLUDED_VIEWMANAGER_H
#define INCLUDED_VIEWMANAGER_H

#include <cstddef>
#include <string>
#include <vector>

namespace Video
{

struct NormalizedViewport
{
  float x = 0.0f;
  float y = 0.0f;
  float width = 1.0f;
  float height = 1.0f;
};

struct View
{
  std::string name;
  std::string artworkPath;
  NormalizedViewport gameViewport;
  bool widescreen = false;
  // -1 inherits CRTMode, 0 disables curvature, and 1 enables it.
  int crtCurvature = -1;
};

// Pixel coordinates use OpenGL's bottom-left origin.
struct GameViewport
{
  unsigned x = 0;
  unsigned y = 0;
  unsigned width = 0;
  unsigned height = 0;
};

class CViewManager
{
public:
  void Init(const std::string& definitionPath, const std::string& selectedViewName,
            unsigned outputWidth, unsigned outputHeight, const GameViewport& fallbackViewport);
  void SetOutputGeometry(unsigned outputWidth, unsigned outputHeight, const GameViewport& fallbackViewport);

  const View* GetCurrentView() const;
  const GameViewport& GetGameViewport() const;
  bool SelectView(const std::string& name);
  bool NextView();
  bool PreviousView();

private:
  void LoadViewDefinitions(const std::string& definitionPath);
  void ApplyCurrentView();

  std::vector<View> m_views;
  std::size_t m_currentView = 0;
  unsigned m_outputWidth = 0;
  unsigned m_outputHeight = 0;
  GameViewport m_fallbackViewport;
  GameViewport m_gameViewport;
};

} // namespace Video

#endif // INCLUDED_VIEWMANAGER_H
