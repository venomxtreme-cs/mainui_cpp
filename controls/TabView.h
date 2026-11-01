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
#ifndef TABWIDGET_H
#define TABWIDGET_H

#include "ItemsHolder.h"

class CMenuTabView : public CMenuItemsHolder
{
public:
	typedef CMenuItemsHolder BaseClass;
	CMenuTabView();

	void VidInit() override;
	// A7 build 60: tab metrics track the CURRENT strip size. Computing them
	// in VidInit only left m_szTab stale after the float layout pass
	// (negative-width strips like the server browser's resolve late), so
	// strips drew/hit-tested misaligned ("disorganized").
	void CalcSizes() override;
	void Draw() override;
	Point GetPositionOffset() const override;

	// A7 remake: pages all live in the same content rect, so the base
	// region test would jump to the last tab whenever the cursor enters the
	// content. Handle the tab strip ourselves and forward mouse input to the
	// current page only.
	bool KeyDown( int key ) override;
	bool KeyUp( int key ) override;
	bool MouseMove( int x, int y ) override;

	// A7 build 91: F2/F3 page switching (previous/next tab)
	void SwitchPage( int delta );
	CMenuTabView *GetTabView( void ) override { return this; }

	inline void SetTabName( int idx, const char *name )
	{
		if( m_pItems.IsValidIndex( idx ))
			m_pItems[idx]->szName = name;
	}

	inline void AddTabItem( CMenuBaseItem &item, const char *name )
	{
		AddItem( item );
		SetTabName( m_pItems.Count() - 1, name );
	}

private:
	void DrawTab(Point pt, const char *name, bool isEnd , bool isSelected, bool isHighlighted);

	Size m_szTab;
};

#endif // TABWIDGET_H
