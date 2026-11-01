/*
CheckBox.h - checkbox
Copyright (C) 2010 Uncle Mike
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

#include "extdll_menu.h"
#include "BaseMenu.h"
#include "CheckBox.h"
#include "Utils.h"

CMenuCheckBox::CMenuCheckBox() : BaseClass()
{
	SetCharSize( QM_DEFAULTFONT );
	SetSize( 32, 32 );
	SetPicture( UI_CHECKBOX_EMPTY,
		UI_CHECKBOX_FOCUS,
		UI_CHECKBOX_PRESSED,
		UI_CHECKBOX_ENABLED,
		UI_CHECKBOX_GRAYED );
	bChecked = false;
	eFocusAnimation = QM_HIGHLIGHTIFFOCUS;
	iFlags |= QMF_DROPSHADOW;
	bChangeOnPressed = false;
	colorBase = uiColorWhite;
	colorFocus = uiColorWhite;
	iMask = 0;
	bInvertMask = false;
}

/*
=================
CMenuCheckBox::Init
=================
*/
void CMenuCheckBox::VidInit( void )
{
	colorText.SetDefault( uiColorText ); // A7 build 87: GameUI label colour
	BaseClass::VidInit();
}

// A7 float-dialog fix: the label position is derived from the render
// position, so it must be refreshed inside CalcSizes - which both VidInit
// and RecalcRenderTree call. Computed at VidInit only, the label stayed at
// its pre-shift place when the dialog was moved ("text still in other
// place").
void CMenuCheckBox::CalcSizes( void )
{
	BaseClass::CalcSizes();

	m_scTextPos.x = m_scPos.x + ( m_scSize.w * 1.25f );
	m_scTextPos.y = m_scPos.y;

	m_scTextSize.w = g_FontMgr->GetTextWideScaled( font, szName, m_scChSize );
	m_scTextSize.h = m_scChSize;
}

// A7 build 62: the label is drawn at 1.25x the box width, OUTSIDE the
// declared rect - the float frame must wrap it (Gamepad "Invert" and any
// other checkbox near the right edge). m_scTextSize is the exact label
// width in screen px; the frame works in layout coords.
int CMenuCheckBox::GetVisualWidth( void ) const
{
	int w = (int)( size.w * 1.25f );

	if( uiStatic.scaleX > 0.01f )
		w += (int)( m_scTextSize.w / uiStatic.scaleX );

	return w;
}

bool CMenuCheckBox::KeyUp( int key )
{
	const char	*sound = 0;

	if( UI::Key::IsLeftMouse( key ) && FBitSet( iFlags, QMF_HASMOUSEFOCUS ))
		sound = uiStatic.sounds[SND_GLOW];
	else if( UI::Key::IsEnter( key ) && !FBitSet( iFlags, QMF_MOUSEONLY ))
		sound = uiStatic.sounds[SND_GLOW];

	if( sound )
	{
		_Event( QM_RELEASED );
		if( !bChangeOnPressed )
		{
			bChecked = !bChecked;	// apply on release
			SetCvarValue( bChecked );
			_Event( QM_CHANGED );
		}
		PlayLocalSound( sound );
	}

	return sound != NULL;
}

bool CMenuCheckBox::KeyDown( int key )
{
	const char	*sound = 0;

	if( UI::Key::IsLeftMouse( key ) && FBitSet( iFlags, QMF_HASMOUSEFOCUS ))
		sound = uiStatic.sounds[SND_GLOW];
	else if( UI::Key::IsEnter( key ) && !FBitSet( iFlags, QMF_MOUSEONLY ))
		sound = uiStatic.sounds[SND_GLOW];

	if( sound )
	{
		_Event( QM_PRESSED );
		if( bChangeOnPressed )
		{
			bChecked = !bChecked;	// apply on release
			SetCvarValue( bChecked );
			_Event( QM_CHANGED );
		}
		PlayLocalSound( sound );
	}

	return sound != NULL;
}


/*
=================
CMenuCheckBox::Draw
=================
*/
void CMenuCheckBox::Draw( void )
{
	uint textflags = ( iFlags & QMF_DROPSHADOW ? ETF_SHADOW : 0 ) | ETF_NOSIZELIMIT | ETF_FORCECOL;

	UI_DrawString( font, m_scTextPos, m_scTextSize, szName, colorText, m_scChSize, eTextAlignment, textflags );

	if( szStatusText && iFlags & QMF_NOTIFY )
	{
		Point coord;

		if( szName[0] )
			coord.x = ( uiStatic.buttons_draw_size.w + 40 ) * uiStatic.scaleX;
		else
			coord.x = m_scSize.w + 16 * uiStatic.scaleX;
		coord.x += m_scPos.x;
		coord.y = m_scPos.y + m_scSize.h / 2 - EngFuncs::ConsoleCharacterHeight() / 2;

		int	r, g, b;

		UnpackRGB( r, g, b, uiColorHelp );
		EngFuncs::DrawSetTextColor( r, g, b );
		EngFuncs::DrawConsoleString( coord, szStatusText );
	}

	// A7 retheme: draw the checkbox box directly (referenced gfx/shell/cb_* art
	// is not in the game data). cs16.css style: sunken box, accent check mark.
	bool bGrayed = ( iFlags & QMF_GRAYED ) != 0;
	bool bFocus = true;
	bool bPressed = m_bPressed;

	if( iFlags & QMF_GRAYED )
		bFocus = false;
	else if ((( iFlags & QMF_MOUSEONLY ) && !( iFlags & QMF_HASMOUSEFOCUS ))
		|| ( this != m_pParent->ItemAtCursor() ))
		bFocus = false;

	unsigned int light = uiColorBorderLight;
	unsigned int dark  = uiColorBorderDark;

	// fill
	UI_FillRect( m_scPos, m_scSize, bGrayed ? uiColorBlack : uiInputBgColor );

	if( bFocus )
		light = bPressed ? dark : uiColorAccent; // highlight edge when focused

	if( bPressed )
	{
		dark = uiColorBorderDark;
	}

	UI_DrawRectangleExt( m_scPos, m_scSize, light, 0, QM_TOP | QM_LEFT );
	UI_DrawRectangleExt( m_scPos, m_scSize, dark,  0, QM_BOTTOM | QM_RIGHT );

	if( bChecked )
	{
		// staircase check mark on an 8x8 unit grid
		int u = m_scSize.w / 8;
		if( u < 1 ) u = 1;

		static const int cells[][2] = {
			{1,4},{2,5},{3,6},{4,5},{5,4},{6,3},{7,2}
		};

		unsigned int checkColor = bGrayed ? uiColorDkGrey : uiColorAccent;

		for( int i = 0; i < 7; i++ )
			UI_FillRect( m_scPos.x + cells[i][0] * u, m_scPos.y + cells[i][1] * u, u, u, checkColor );
	}
}

void CMenuCheckBox::UpdateEditable()
{
	bChecked = !!CvarValue();
}
