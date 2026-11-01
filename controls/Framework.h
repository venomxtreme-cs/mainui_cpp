/*
Framework.h -- base menu fullscreen root window
Copyright (C) 2017 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include "BaseWindow.h"
#include "PicButton.h"
#include "Primitive.h"

#define MAX_FRAMEWORK_PICBUTTONS 16

/*
 * WON-style menu framework
 */
class CMenuFramework : public CMenuBaseWindow
{
public:
	typedef CMenuBaseWindow BaseClass;

	CMenuFramework( const char *name = "Unnamed Framework" );
	virtual ~CMenuFramework() override;

	void Show() override;
	void Draw() override;
	void Init() final override;
	void VidInit() final override;
	void Hide() override;
	bool IsRoot() const override { return true; }
	bool IsFloatDialog() const override { return m_bFloat; }

	bool KeyDown( int key ) override;
	bool KeyUp( int key ) override;
	bool MouseMove( int x, int y ) override;

	CMenuPicButton *AddButton( const char *szName, const char *szStatus,
		EDefaultBtns iButton, CEventCallback onReleased = CEventCallback(), int iFlags = 0 );

	CMenuPicButton *AddButton( const char *szName, const char *szStatus,
		const char *szButtonPath, CEventCallback onReleased = CEventCallback(), int iFlags = 0, int hotkey = 0 );

	void RealignButtons();

	bool DrawAnimation() override;

	void PrepareBannerAnimation( EAnimation direction, CMenuPicButton *initiator );

	class CMenuBannerBitmap : public CMenuBaseItem
	{
	public:
		CMenuBannerBitmap();
		void Draw() override;
		void SetPicture( const char *pic );

		void Draw( Point pt, Size sz );

	private:
		CImage image;
	} banner;

protected:
	EAnimation bannerAnimDirection;
	Rect bannerRects[2];

	CMenuPicButton *m_apBtns[MAX_FRAMEWORK_PICBUTTONS];
	int m_iBtnsNum;

	// A7: GoldSource-style floating dialog windows. Every root window except
	// the main menu gets a framed panel with a title bar and close box; the
	// panel is centered and draggable. REMAKE: the float offset is never
	// baked into item coordinates - it flows into every descendant through
	// GetPositionOffset() at render time, so dragging can't change sizes and
	// labels/hidden pages follow on RecalcRenderTree.
	bool m_bFloat;
	bool m_bFrameValid;
	bool m_bClosePressed;
	// A7 build 58 fit-to-screen scale, REMOVED as per-dialog scaling in
	// build 60 (one global scale from BaseMenu now): kept at 1.0 so the
	// title bar / padding formulas stay valid and scale-free.
	float m_flLayoutScale;
	int  m_iFloatDX, m_iFloatDY;    // float offset (virtual coords)
	// build 90: hug the right edge instead of centering - leaves the
	// main-menu column visible behind the dialog (reference screenshot 2)
	bool m_bFloatRightAlign;
	Rect m_frameRect;               // panel rect, virtual coords
	Rect m_scFrameRect;             // panel rect, screen coords
	Rect m_scCloseRect;             // close box rect, screen coords
	char m_szFloatTitle[64];

	// A7: title-bar drag (GoldSource dialogs move with the cursor)
	bool m_bDragging;
	bool m_bDragPast;               // cursor passed the tap-jitter threshold
	Point m_scDragOrigin;           // cursor screen pos when drag started
	int  m_iDragBaseDX, m_iDragBaseDY;   // float offset at drag start
	int  m_iDragAppliedX, m_iDragAppliedY; // offset the cursor delta is measured from

	Point GetPositionOffset() const override;
	void ApplyFloatLayout();
	void DrawFloatFrame();
	void UpdateFloatDrag();
	void SyncFloatScreenRects();
};

#endif // FRAMEWORK_H
