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

#include "ViewManager.h"

#include "Supermodel.h"
#include "Util/ConfigBuilders.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <iterator>
#include <utility>

namespace Video
{

namespace
{

unsigned NormalizedToPixels(float value, unsigned extent)
{
  value = std::clamp(value, 0.0f, 1.0f);
  return static_cast<unsigned>(std::lround(value * static_cast<float>(extent)));
}

float ReadFloat(const Util::Config::Node& section, const char* key, float defaultValue)
{
  const Util::Config::Node* setting = section.TryGet(key);
  return setting == nullptr ? defaultValue : setting->ValueAs<float>();
}

bool IsValidViewport(const NormalizedViewport& viewport)
{
  constexpr float epsilon = 0.0001f;
  return std::isfinite(viewport.x) && std::isfinite(viewport.y) &&
         std::isfinite(viewport.width) && std::isfinite(viewport.height) &&
         viewport.x >= 0.0f && viewport.y >= 0.0f &&
         viewport.width > 0.0f && viewport.height > 0.0f &&
         viewport.x + viewport.width <= 1.0f + epsilon &&
         viewport.y + viewport.height <= 1.0f + epsilon;
}

} // anonymous namespace

void CViewManager::Init(const std::string& definitionPath, const std::string& selectedViewName,
                        unsigned outputWidth, unsigned outputHeight, const GameViewport& fallbackViewport)
{
  m_views.clear();
  m_currentView = 0;
  LoadViewDefinitions(definitionPath);

  if (!selectedViewName.empty())
  {
    auto selected = std::find_if(m_views.begin(), m_views.end(), [&selectedViewName](const View& view) {
      return view.name == selectedViewName;
    });
    if (selected != m_views.end())
      m_currentView = static_cast<std::size_t>(std::distance(m_views.begin(), selected));
    else if (!m_views.empty())
      ErrorLog("View '%s' is not defined in '%s'; using '%s'.", selectedViewName.c_str(), definitionPath.c_str(), m_views.front().name.c_str());
  }

  SetOutputGeometry(outputWidth, outputHeight, fallbackViewport);
}

void CViewManager::SetOutputGeometry(unsigned outputWidth, unsigned outputHeight, const GameViewport& fallbackViewport)
{
  m_outputWidth = outputWidth;
  m_outputHeight = outputHeight;
  m_fallbackViewport = fallbackViewport;
  ApplyCurrentView();
}

const View* CViewManager::GetCurrentView() const
{
  if (m_views.empty() || m_currentView >= m_views.size())
    return nullptr;
  return &m_views[m_currentView];
}

const GameViewport& CViewManager::GetGameViewport() const
{
  return m_gameViewport;
}

bool CViewManager::SelectView(const std::string& name)
{
  auto selected = std::find_if(m_views.begin(), m_views.end(), [&name](const View& view) {
    return view.name == name;
  });
  if (selected == m_views.end())
    return false;

  m_currentView = static_cast<std::size_t>(std::distance(m_views.begin(), selected));
  ApplyCurrentView();
  return true;
}

bool CViewManager::NextView()
{
  if (m_views.size() < 2)
    return false;
  m_currentView = (m_currentView + 1) % m_views.size();
  ApplyCurrentView();
  return true;
}

bool CViewManager::PreviousView()
{
  if (m_views.size() < 2)
    return false;
  m_currentView = (m_currentView + m_views.size() - 1) % m_views.size();
  ApplyCurrentView();
  return true;
}

void CViewManager::LoadViewDefinitions(const std::string& definitionPath)
{
  std::ifstream definitionFile(definitionPath);
  if (!definitionFile.good())
    return;
  definitionFile.close();

  Util::Config::Node definitions("Global");
  if (Util::Config::FromINIFile(&definitions, definitionPath))
    return;

  for (const Util::Config::Node& section : definitions)
  {
    if (!section.HasChildren())
      continue;

    try
    {
      View view;
      view.name = section.Key();
      view.gameViewport.x = ReadFloat(section, "X", 0.0f);
      view.gameViewport.y = ReadFloat(section, "Y", 0.0f);
      view.gameViewport.width = ReadFloat(section, "Width", 1.0f);
      view.gameViewport.height = ReadFloat(section, "Height", 1.0f);

      const Util::Config::Node* artwork = section.TryGet("Artwork");
      if (artwork != nullptr)
        view.artworkPath = artwork->ValueAs<std::string>();

      const Util::Config::Node* widescreen = section.TryGet("WideScreen");
      if (widescreen != nullptr)
        view.widescreen = widescreen->ValueAs<bool>();

      const Util::Config::Node* crtCurvature = section.TryGet("CRTCurvature");
      if (crtCurvature != nullptr)
        view.crtCurvature = crtCurvature->ValueAs<bool>() ? 1 : 0;

      if (view.name.empty() || !IsValidViewport(view.gameViewport))
      {
        ErrorLog("Ignoring invalid view '%s' in '%s'.", view.name.c_str(), definitionPath.c_str());
        continue;
      }

      m_views.emplace_back(std::move(view));
    }
    catch (const std::exception& error)
    {
      ErrorLog("Ignoring invalid view '%s' in '%s': %s", section.Key().c_str(), definitionPath.c_str(), error.what());
    }
  }

  if (!m_views.empty())
    InfoLog("Loaded %u view(s) from '%s'.", static_cast<unsigned>(m_views.size()), definitionPath.c_str());
}

void CViewManager::ApplyCurrentView()
{
  const View* view = GetCurrentView();
  if (view == nullptr)
  {
    m_gameViewport = m_fallbackViewport;
    return;
  }

  const NormalizedViewport& normalized = view->gameViewport;
  const unsigned left = NormalizedToPixels(normalized.x, m_outputWidth);
  const unsigned top = NormalizedToPixels(normalized.y, m_outputHeight);
  unsigned width = NormalizedToPixels(normalized.width, m_outputWidth);
  unsigned height = NormalizedToPixels(normalized.height, m_outputHeight);

  width = (std::min)(width, m_outputWidth - (std::min)(left, m_outputWidth));
  height = (std::min)(height, m_outputHeight - (std::min)(top, m_outputHeight));

  m_gameViewport.x = (std::min)(left, m_outputWidth);
  m_gameViewport.y = m_outputHeight - (std::min)(top + height, m_outputHeight);
  m_gameViewport.width = width;
  m_gameViewport.height = height;
}

} // namespace Video
