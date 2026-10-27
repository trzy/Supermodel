// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 The Supermodel-VK authors
// Copyright 2003-2026 The Supermodel Team.
// Derived from Supermodel-VK and Supermodel; see Docs/LICENSE.txt.
#pragma once
namespace Vulkan3D {
inline void ComputePictureRect(int extentWidth, int extentHeight,
                                 int xOffset, int yOffset, int xRes, int yRes,
                                 int correction, bool wideBackground, bool wideScreen,
                                 int& outX, int& outY, int& outW, int& outH)
{
	if (wideBackground || wideScreen)
	{
		/*
		 * The picture spans the full width, so there is nothing to hide at the sides - but the same
		 * 2D-versus-3D misalignment applies vertically, and the OpenGL backend crops only top and
		 * bottom here (its scissor is full width). Matching that keeps the two backends showing the
		 * same picture.
		 *
		 * The two cases are one branch because they want the same rectangle. They differ in where the
		 * extra width comes from - the tile layer being stretched, or the 3D frustum being widened -
		 * not in what the picture occupies.
		 *
		 * The vertical bounds still come from the emulated picture rather than from the output. Using
		 * the full output height was only correct when the picture filled it; under integer scaling
		 * the picture is letterboxed, and the 2D layer sits `correction` pixels above the 3D viewport,
		 * so an output-height rectangle leaves that overhang visible as a bright line along the top
		 * edge. The non-wide branch below computes exactly the bounds wanted here.
		 */
		int y = extentHeight - (yOffset + yRes) + correction;
		int h = yRes - 2 * correction;
		if (y < 0) { h += y; y = 0; }
		if (y + h > extentHeight) h = extentHeight - y;
		if (h < 0) h = 0;

		outX = 0;
		outY = y;
		outW = extentWidth;
		outH = h;
		return;
	}

	/*
	 * Inset by correction, not expanded by it.
	 *
	 * The 2D tile layer is drawn correction pixels away from the 3D scene because the two are known to
	 * disagree by that much, and the OpenGL backend hides the overhang with a scissor box inset by the
	 * same amount. This renderer does the same thing, so the picture the user sees is the inset
	 * rectangle - which is therefore the rectangle the backdrop has to keep away from.
	 */
	int x = xOffset + correction;
	// The inset is about the 3D viewport, which is what the OpenGL scissor insets - the 2D layer's own
	// offset is the thing being hidden, not the thing being measured.
	int y = extentHeight - (yOffset + yRes) + correction;
	int w = xRes - 2 * correction;
	int h = yRes - 2 * correction;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > extentWidth) w = extentWidth - x;
	if (y + h > extentHeight) h = extentHeight - y;
	if (w < 0) w = 0;
	if (h < 0) h = 0;

	outX = x;
	outY = y;
	outW = w;
	outH = h;
}

}
