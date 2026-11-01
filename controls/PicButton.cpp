/*
PicButton.h - animated button with picture
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
#include "Bitmap.h"
#include "PicButton.h"
#include "Utils.h"
#include "Scissor.h"
#include "Btns.h"
#include <stdlib.h>
#include "Framework.h"

static int g_hotkeys[PC_BUTTONCOUNT] =
{
	'n', // PC_NEW_GAME = 0,
	'r', // PC_RESUME_GAME,
	'h', // PC_HAZARD_COURSE,
	'c', // PC_CONFIG,
	'l', // PC_LOAD_GAME,
	's', // PC_SAVE_LOAD_GAME,
	'v', // PC_VIEW_README,
	'q', // PC_QUIT,
	'm', // PC_MULTIPLAYER,
	'e', // PC_EASY,
	'm', // PC_MEDIUM,
	'd', // PC_DIFFICULT,
	's', // PC_SAVE_GAME,
	'l', // PC_LOAD_GAME2,
	'c', // PC_CANCEL,
	'g', // PC_GAME_OPTIONS,
	'v', // PC_VIDEO,
	'a', // PC_AUDIO,
	'c', // PC_CONTROLS,
	'd', // PC_DONE,
	'q', // PC_QUICKSTART,
	'u', // PC_USE_DEFAULTS,
	'o', // PC_OK,
	'v', // PC_VID_OPT,
	'm', // PC_VID_MODES,
	'a', // PC_ADV_CONTROLS,
	'o', // PC_ORDER_HL,
	'd', // PC_DELETE,
	'i', // PC_INET_GAME,
	'h', // PC_CHAT_ROOMS,
	'l', // PC_LAN_GAME,
	'u', // PC_CUSTOMIZE,
	's', // PC_SKIP,
	'e', // PC_EXIT,
	'c', // PC_CONNECT,
	'r', // PC_REFRESH,
	'f', // PC_FILTER,
	'f', // PC_FILTER2,
	'c', // PC_CREATE,
	't', // PC_CREATE_GAME,
	'h', // PC_CHAT_ROOMS2,
	'l', // PC_LIST_ROOMS,
	's', // PC_SEARCH,
	's', // PC_SERVERS,
	'j', // PC_JOIN,
	'f', // PC_FIND,
	'r', // PC_CREATE_ROOM,
	'j', // PC_JOIN_GAME,
	's', // PC_SEARCH_GAMES,
	'f', // PC_FIND_GAME,
	't', // PC_START_GAME,
	'v', // PC_VIEW_GAME_INFO,
	'u', // PC_UPDATE,
	'a', // PC_ADD_SERVER,
	'd', // PC_DISCONNECT,
	'o', // PC_CONSOLE,	// a1ba: set to O
	'o', // PC_CONTENT_CONTROL,
	'u', // PC_UPDATE2,
	'w', // PC_VISIT_WON,
	'p', // PC_PREVIEWS,
	'a', // PC_ADV_OPT,
	0, // PC_3DINFO_SITE,
	'u', // PC_CUSTOM_GAME,
	'a', // PC_ACTIVATE,
	'i', // PC_INSTALL,
	'v', // PC_VISIT_WEB_SITE,
	'r', // PC_REFRESH_LIST,
	'e', // PC_DEACTIVATE,
	'a', // PC_ADV_OPT2,
	's', // PC_SPECTATE_GAME,
	'p', // PC_SPECTATE_GAMES,
};

CMenuPicButton::CMenuPicButton() : BaseClass()
{
	bEnableTransitions = true;
	eFocusAnimation = QM_HIGHLIGHTIFFOCUS;
	iFlags = QMF_DROPSHADOW;

	iFocusStartTime = 0;

	eTextAlignment = QM_LEFT;

	hPic = 0;
	hotkey = 0;
	button_id = -1;
	iOldState = BUTTON_NOFOCUS;
	m_iLastFocusTime = -512;
	bPulse = false;
	m_iTextHeightOverride = -1;
	m_bCustomTextStyle = false;

	size = uiStatic.buttons_draw_size;

	SetCharSize( QM_DEFAULTFONT );
}

/*
=================
CMenuPicButton::Key
=================
*/
bool CMenuPicButton::KeyUp( int key )
{
	bool handled = false;

	if( UI::Key::IsEnter( key ) && !(iFlags & QMF_MOUSEONLY) )
		handled = true;
	else if( UI::Key::IsLeftMouse( key ) && ( iFlags & QMF_HASMOUSEFOCUS ) )
		handled = true;

	if( handled )
		_Event( QM_RELEASED );

	return handled;
}

void CMenuPicButton::_Event( int ev )
{
	BaseClass::_Event( ev );
	if( ev == QM_RELEASED )
	{
		PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
		CheckWindowChanged( );
	}
}

void CMenuPicButton::CheckWindowChanged()
{
	// parent is not a window, ignore
	if( !m_pParent->IsWindow())
		return;

	CMenuBaseWindow *parentWindow = (CMenuBaseWindow*)m_pParent;
	CMenuBaseWindow *newWindow = parentWindow->WindowStack()->Current();

	// menu is closed, ignore
	if( !newWindow )
		return;

	// no change, ignore
	if( parentWindow == newWindow )
		return;

	// parent and new are not a root windows, ignore
	if( !parentWindow->IsRoot() || !newWindow->IsRoot() )
		return;

	// decide transition direction
	if( FBitSet( parentWindow->iFlags, QMF_CLOSING ))
	{
		// our parent window is closing right now
		// play backward animation
		// Con_NPrintf( 10, "%s banner down", parentWindow->szName );

		CMenuFramework *f = (CMenuFramework*)parentWindow;
		f->PrepareBannerAnimation( CMenuFramework::ANIM_CLOSING, nullptr );
	}
	else
	{
		// new window overlaps parent window
		// play forward animation
		// Con_NPrintf( 10, "%s banner up", newWindow->szName );

		CMenuFramework *f = (CMenuFramework*)newWindow;
		f->PrepareBannerAnimation( CMenuFramework::ANIM_OPENING, this );
	}
}

bool CMenuPicButton::KeyDown( int key )
{
	bool handled = false;

	if( UI::Key::IsEnter( key ) && !(iFlags & QMF_MOUSEONLY) )
		handled = true;
	else if( UI::Key::IsLeftMouse( key ) && ( iFlags & QMF_HASMOUSEFOCUS ) )
		handled = true;

	if( handled )
		_Event( QM_PRESSED );

	return handled;
}

/*
=================
A7: is this item inside a floating dialog window? Dialog action buttons
(OK/Cancel/Apply, Join game/Refresh/Done...) draw with button chrome; the
non-float main menu keeps its flat text list.
=================
*/
static bool UI_ItemInFloatDialog( const CMenuBaseItem *item )
{
	CMenuItemsHolder *p = item->Parent();

	while( p && !p->IsWindow() )
		p = p->Parent();

	return p && p->IsWindow() && ((CMenuBaseWindow*)p)->IsFloatDialog();
}

/*
=================
CMenuPicButton::DrawButton
=================
*/
void CMenuPicButton::DrawButton( int r, int g, int b, int a, wrect_t *rects, int state )
{
	EngFuncs::PIC_Set( hPic, r, g, b, a );
	EngFuncs::PIC_DrawAdditive( m_scPos, uiStatic.buttons_draw_size.Scale(), &rects[state] );
}

/*
=================
CMenuPicButton::Draw
=================
*/
// A7 build 62: with QMF_NOTIFY the status text is drawn to the right of
// the button at a fixed anchor - a frame wrapped around the declared rect
// cut it off (the touch hub's description column hung outside the panel).
// Console-font width is estimated from the character height.
int CMenuPicButton::GetVisualWidth( void ) const
{
	if( !szStatusText || !FBitSet( iFlags, QMF_NOTIFY ) ||
		FBitSet( gMenu.m_gameinfo.flags, GFL_NOSKILLS ))
		return size.w;

	int w = (int)( uiStatic.buttons_draw_size.w + 40 );
	w += (int)( strlen( szStatusText ) * EngFuncs::ConsoleCharacterHeight() * 0.6f /
		( uiStatic.scaleX > 0.01f ? uiStatic.scaleX : 1.0f ));

	return Q_max( w, size.w );
}

void CMenuPicButton::Draw( )
{
	int state = BUTTON_NOFOCUS;

	if( iFlags & (QMF_HASMOUSEFOCUS|QMF_HASKEYBOARDFOCUS))
	{
		state = BUTTON_FOCUS;
	}

	// make sure what cursor in rect
	if( m_bPressed )
		state = BUTTON_PRESSED;

	if( iOldState == BUTTON_NOFOCUS && state != BUTTON_NOFOCUS )
		iFocusStartTime = uiStatic.realTime;

	if( szStatusText && FBitSet( iFlags, QMF_NOTIFY ) && !FBitSet( gMenu.m_gameinfo.flags, GFL_NOSKILLS ))
	{
		Point coord;

		coord.x = m_scPos.x + ( uiStatic.buttons_draw_size.w + 40 ) * uiStatic.scaleX;
		coord.y = m_scPos.y + m_scSize.h / 2 - EngFuncs::ConsoleCharacterHeight() / 2;

		int	r, g, b;

		UnpackRGB( r, g, b, uiColorHelp );
		EngFuncs::DrawSetTextColor( r, g, b );
		EngFuncs::DrawConsoleString( coord, szStatusText );
	}

	int a = (512 - (uiStatic.realTime - m_iLastFocusTime)) >> 1;

	// A7 button style: dialogs' action buttons must look like buttons -
	// cs16.css/GameUI sunken fill with bevel edges (hover = accent edge,
	// press = sunken) instead of the bare flat text the main menu uses.
	bool bChrome = !hPic && UI_ItemInFloatDialog( this );

	if( bChrome )
	{
		eTextAlignment = QM_CENTER; // dialog buttons center their labels

		bool bGrayed = ( iFlags & QMF_GRAYED ) != 0;
		unsigned int light = uiColorBorderLight;
		unsigned int dark  = uiColorBorderDark;

		if( state == BUTTON_FOCUS && !bGrayed )
			light = uiColorAccent; // hover edge, same as the checkbox

		UI_FillRect( m_scPos, m_scSize, bGrayed ? uiColorBlack : uiInputBgColor );

		if( m_bPressed )
		{
			// sunken while pressed
			UI_DrawRectangleExt( m_scPos, m_scSize, dark,  0, QM_TOP | QM_LEFT );
			UI_DrawRectangleExt( m_scPos, m_scSize, light, 0, QM_BOTTOM | QM_RIGHT );
		}
		else
		{
			UI_DrawRectangleExt( m_scPos, m_scSize, light, 0, QM_TOP | QM_LEFT );
			UI_DrawRectangleExt( m_scPos, m_scSize, dark,  0, QM_BOTTOM | QM_RIGHT );
		}
	}

	if( hPic && !uiStatic.renderPicbuttonText )
	{
		int r, g, b;

		UnpackRGB( r, g, b, iFlags & QMF_GRAYED ? uiColorDkGrey : uiColorWhite );

		wrect_t rects[3];
		for( int i = 0; i < 3; i++ )
		{
			if( button_id >= 0 )
			{
				rects[i].left = uiStatic.btns.GetX( button_id );
				rects[i].right = uiStatic.btns.GetX( button_id ) + uiStatic.btns.GetWidth();
				rects[i].top = uiStatic.btns.GetY( button_id ) + i * uiStatic.btns.GetTexStride();
				rects[i].bottom = rects[i].top + uiStatic.btns.GetTexH();
			}
			else
			{
				rects[i].left = 0;
				rects[i].right = EngFuncs::PIC_Width( hPic );
				rects[i].top = round( EngFuncs::PIC_Height( hPic ) * i / 3.0f );
				rects[i].bottom = round( EngFuncs::PIC_Height( hPic ) * ( i + 1 ) / 3.0f );
			}
		}

		// decay
		if( state == BUTTON_NOFOCUS && a > 0 )
		{
			DrawButton( r, g, b, a, rects, BUTTON_FOCUS );
		}

		// pulse code.
		if( ( state == BUTTON_NOFOCUS && bPulse ) ||
			( state == BUTTON_FOCUS   && eFocusAnimation == QM_PULSEIFFOCUS ) )
		{
			a = 255 * (0.5f + 0.5f * sin( (float)uiStatic.realTime / ( UI_PULSE_DIVISOR * 2 )));

			DrawButton( r, g, b, a, rects, BUTTON_FOCUS );
			DrawButton( r, g, b, 255, rects, BUTTON_NOFOCUS );
		}
		// special handling for focused
		else if( state == BUTTON_FOCUS )
		{
			DrawButton( r, g, b, 255, rects, BUTTON_FOCUS );
			DrawButton( r, g, b, 255, rects, BUTTON_NOFOCUS );
		}
		else
		{
			// just draw
			DrawButton( r, g, b, 255, rects, state );
		}
	}
	else if( m_bCustomTextStyle )
	{
		// build 90: ETF_NO_WRAP - a menu item must never break onto a
		// second line ("stucked"/half-word rows in the user's screenshot)
		uint textflags = ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP;
		const int buttonTextHeight = ( m_iTextHeightOverride > 0 ) ? m_iTextHeightOverride : m_scChSize;

		SetBits( textflags, ( iFlags & QMF_DROPSHADOW ) ? ETF_SHADOW : 0 );

		if( iFlags & QMF_GRAYED )
		{
			UI_DrawString( font, m_scPos, m_scSize, szName, uiColorDkGrey, buttonTextHeight, eTextAlignment, textflags );
		}
		else if( this != m_pParent->ItemAtCursor() )
		{
			UI_DrawString( font, m_scPos, m_scSize, szName, colorBase, buttonTextHeight, eTextAlignment, textflags );
		}
		else if( eFocusAnimation == QM_HIGHLIGHTIFFOCUS )
		{
			UI_DrawString( font, m_scPos, m_scSize, szName, colorFocus, buttonTextHeight, eTextAlignment, textflags );
		}
		else if( eFocusAnimation == QM_PULSEIFFOCUS )
		{
			float pulsar = 0.5f + 0.5f * sin( (float)uiStatic.realTime / UI_PULSE_DIVISOR );

			UI_DrawString( font, m_scPos, m_scSize, szName,
				InterpColor( colorBase, colorFocus, pulsar ), buttonTextHeight, eTextAlignment, textflags );
		}
	}
	else
	{
		// A7: flat GoldSrc-style button text with hard drop shadow.
		// The xash3d additive blur-glow text (hHeavyBlur/hLightBlur) that used
		// to draw here is what made every subpage read as a stock xash port;
		// original CS 1.6 menu text is flat white with a black shadow. Works
		// for pressed/grayed/pulse states too (focus uses colorFocus above).
		(void)a; // only the picture path uses the focus-decay alpha
		uint textflags = ETF_NOSIZELIMIT | ETF_FORCECOL;
		const int buttonTextHeight = ( m_iTextHeightOverride > 0 ) ? m_iTextHeightOverride : m_scChSize;

		SetBits( textflags, (iFlags & QMF_DROPSHADOW) ? ETF_SHADOW : 0 );

		if( iFlags & QMF_GRAYED )
		{
			UI_DrawString( font, m_scPos, m_scSize, szName, uiColorDkGrey, buttonTextHeight, eTextAlignment, textflags );
		}
		else if( this != m_pParent->ItemAtCursor() )
		{
			UI_DrawString( font, m_scPos, m_scSize, szName, colorBase, buttonTextHeight, eTextAlignment, textflags );
		}
		else if( eFocusAnimation == QM_HIGHLIGHTIFFOCUS )
		{
			UI_DrawString( font, m_scPos, m_scSize, szName, colorFocus, buttonTextHeight, eTextAlignment, textflags );
		}
		else if( eFocusAnimation == QM_PULSEIFFOCUS )
		{
			float pulsar = 0.5f + 0.5f * sin( (float)uiStatic.realTime / UI_PULSE_DIVISOR );

			UI_DrawString( font, m_scPos, m_scSize, szName,
				InterpColor( colorBase, colorFocus, pulsar ), buttonTextHeight, eTextAlignment, textflags );
		}
	}

	iOldState = state;
}

void CMenuPicButton::SetPicture( EDefaultBtns ID )
{
	if( ID < 0 || ID > PC_BUTTONCOUNT )
		return; // bad id

	hPic = uiStatic.btns.GetPic( ID );
	button_id = ID;
	hotkey = g_hotkeys[ID];
}

void CMenuPicButton::SetPicture( const char *filename, int hk )
{
	hPic = EngFuncs::PIC_Load( filename );
	hotkey = hk;
}

bool CMenuPicButton::HotKey( int key )
{
	return hotkey == key;
}
