/*
TabView.cpp -- tabbed view
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
#include "TabView.h"
#include "Scissor.h"

CMenuTabView::CMenuTabView() : BaseClass()
{
	m_bWrapCursor = true;
	SetCharSize( QM_BOLDFONT );
	eTextAlignment = QM_CENTER;
}

Point CMenuTabView::GetPositionOffset() const
{
	Point ret = m_scPos;
	ret.y += m_scChSize * 1.5f;

	return ret;
}

void CMenuTabView::VidInit()
{
	CalcPosition();
	CalcSizes();

	_VidInit();
	VidInitItems();
}

// A7 build 60: tab metrics must track the CURRENT strip size. VidInit-only
// computation left m_szTab stale after the float layout pass (negative-
// width strips resolve late) and after any size change - misaligned strips
// and wrong-tab hit tests ("disorganized"). RecalcRenderTree calls this too.
void CMenuTabView::CalcSizes( void )
{
	BaseClass::CalcSizes();

	m_szTab.w = m_pItems.Count() ? ( m_scSize.w / m_pItems.Count() ) : 0;
	m_szTab.h = m_scChSize * 1.5f;
}

void CMenuTabView::DrawTab(Point pt, const char *name, bool isEnd, bool isSelected, bool isHighlighted)
{
	// A7 build 87: GameUI tabs - both states sit on the panel colour; the
	// active tab is marked with accent text only, inactive tabs keep a
	// light bottom edge and divider like the reference strip
	uint textColor = uiColorText;
	uint fillColor = uiColorBg;
	uint textflags = ( iFlags & QMF_DROPSHADOW ) ? ETF_SHADOW : 0;

	if( isSelected )
	{
		textColor = uiColorAccent;
	}
	else if( isHighlighted )
	{
		textColor = uiPromptFocusColor;
	}

	UI_FillRect( pt, m_szTab, fillColor );
	UI_DrawString( font, pt, m_szTab, name, textColor, m_scChSize, eTextAlignment, textflags );

	// inactive tabs keep a light bottom edge (selected one merges with the panel)
	if( !isSelected )
	{
		UI_FillRect( pt.x, pt.y + m_szTab.h, m_szTab.w, UI_OUTLINE_WIDTH, uiColorBorderLight );
	}

	if( !isEnd )
	{
		int x = pt.x + m_szTab.w;
		int y = pt.y - UI_OUTLINE_WIDTH;
		int w = UI_OUTLINE_WIDTH;
		int h = m_szTab.h + UI_OUTLINE_WIDTH + UI_OUTLINE_WIDTH;

		// draw right divider
		UI_FillRect( x, y, w, h, uiColorBorderLight );
	}
}

void CMenuTabView::Draw()
{
	Point contentOffset = Point( m_scPos.x, m_scPos.y + m_scChSize * 1.5f );
	Size contentSize = Size( m_scSize.w, m_scSize.h - m_scChSize * 1.5f );

	// fill panel background first so tab edges draw over it
	UI_FillRect( contentOffset, contentSize, uiColorBg );

	// draw frame (raised bevel)
	UI_DrawBevel( m_scPos, m_scSize, false );

	// draw tabs
	Point tabOffset = m_scPos;
	FOR_EACH_VEC( m_pItems, i )
	{
		bool isEnd = i == ( m_pItems.Count() - 1 );
		bool isHighlighted = UI_CursorInRect( tabOffset, m_szTab );
		bool isSelected = i == m_iCursor;

		DrawTab( tabOffset, m_pItems[i]->szName, isEnd, isSelected, isHighlighted );
		tabOffset.x += m_szTab.w;
	}

	// draw contents
	if( m_iCursor >= 0 && m_iCursor < m_pItems.Count() )
	{
		UI::Scissor::PushScissor( contentOffset, contentSize );
			m_pItems[m_iCursor]->Draw();
		UI::Scissor::PopScissor();
	}
}

// A7 remake: tab strip input. Hovering (or tapping - the touch press is
// preceded by a move) the strip selects the tab; the content area is
// forwarded to the current page only, since every page shares one rect.
bool CMenuTabView::MouseMove( int x, int y )
{
	if( m_szTab.w > 0 && !m_pItems.IsEmpty() &&
		UI_CursorInRect( m_scPos, Size( m_scSize.w, m_szTab.h )))
	{
		int idx = ( x - m_scPos.x ) / m_szTab.w;
		if( m_pItems.IsValidIndex( idx ) && idx != m_iCursor )
		{
			SetCursor( idx );
			PlayLocalSound( uiStatic.sounds[SND_MOVE] );
		}
		return true;
	}

	CMenuBaseItem *cur = ItemAtCursor();
	if( cur )
		return cur->MouseMove( x, y );

	return false;
}

bool CMenuTabView::KeyDown( int key )
{
	if( UI::Key::IsLeftMouse( key ) && m_szTab.w > 0 && !m_pItems.IsEmpty() &&
		UI_CursorInRect( m_scPos, Size( m_scSize.w, m_szTab.h )))
	{
		int idx = ( uiStatic.cursorX - m_scPos.x ) / m_szTab.w;
		if( m_pItems.IsValidIndex( idx ))
			SetCursor( idx );
		return true;
	}

	// A7 remake: every page shares one content rect, so the base
	// ItemsHolder fallback (which sends mouse keys to ALL non-focused
	// items) leaked each click to every sibling page - a click anywhere
	// in Options could press the Touch page's button behind the visible
	// page and open the touch menu. Wheel and all other keys go to the
	// current page only.
	CMenuBaseItem *cur = ItemAtCursor();
	return cur ? cur->KeyDown( key ) : false;
}

bool CMenuTabView::KeyUp( int key )
{
	// see KeyDown - sibling pages must never see the release either
	CMenuBaseItem *cur = ItemAtCursor();
	return cur ? cur->KeyUp( key ) : false;
}

// A7 build 91: F2/F3 switch tab pages from anywhere in the dialog
// (arrow keys keep their normal item-navigation meaning)
void CMenuTabView::SwitchPage( int delta )
{
	int count = m_pItems.Count();

	if( count <= 0 )
		return;

	int idx = ( m_iCursor + delta ) % count;

	if( idx < 0 )
		idx += count;

	if( idx != m_iCursor )
	{
		SetCursor( idx );
		PlayLocalSound( uiStatic.sounds[SND_MOVE] );
	}
}
