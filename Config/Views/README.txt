Per-Game View Definitions
=========================

Create one INI file per ROM set using the ROM set name, for example:

  Config/Views/dayto2pe.ini

Each section defines one selectable view. Section order is the runtime cycle
order. Coordinates are normalized to the complete output and use a top-left
origin.

  [ Cabinet ]
  Artwork = "Assets/Bezels/dayto2pe-cabinet.png"
  X = 0.1875
  Y = 0.092593
  Width = 0.628125
  Height = 0.818519
  WideScreen = 0
  CRTCurvature = 1

Artwork is optional. WideScreen controls the 3D projection independently of
whether artwork is present. CRTCurvature optionally overrides whether the CRT
post-processing pass curves the selected view; when omitted, CRTMode controls
it. X, Y, Width, and Height default to the full output.

The selected section name is stored as SelectedView in the matching per-game
section of Config/Supermodel.ini. A missing or renamed selection falls back to
the first view in this file.
