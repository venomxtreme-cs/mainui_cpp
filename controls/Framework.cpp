/*
Framework.cpp -- base menu fullscreen root window
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
#include "Framework.h"
#include "PicButton.h"
// A7 build 91: F2/F3 switch tab pages in tabbed dialogs
#include "TabView.h"
#include "keydefs.h"

// menu banners used fiexed rectangle (virtual screenspace at 640x480)
#define UI_BANNER_POSX		72
#define UI_BANNER_POSY		72
#define UI_BANNER_WIDTH		736
#define UI_BANNER_HEIGHT		128

CMenuFramework::CMenuFramework( const char *name ) : BaseClass( name )
{
	memset( m_apBtns, 0, sizeof( m_apBtns ) );
	m_iBtnsNum = 0;
	bannerAnimDirection = ANIM_NO;

	// A7 floating dialog state
	m_bFloat = false;
	m_bFrameValid = false;
	m_bClosePressed = false;
	m_iFloatDX = m_iFloatDY = 0;
	m_bFloatRightAlign = false;
	m_frameRect = Rect( Point( 0, 0 ), Size( 0, 0 ) );
	m_scFrameRect = Rect( Point( 0, 0 ), Size( 0, 0 ) );
	m_scCloseRect = Rect( Point( 0, 0 ), Size( 0, 0 ) );
	m_szFloatTitle[0] = 0;
	m_bDragging = false;
	m_bDragPast = false;
	m_scDragOrigin = Point( 0, 0 );
	m_iDragBaseDX = m_iDragBaseDY = 0;
	m_iDragAppliedX = m_iDragAppliedY = 0;
	m_flLayoutScale = 1.0f;  // A7 build 60: fixed - per-dialog fit scaling removed
}

CMenuFramework::~CMenuFramework()
{
	for( int i = 0; i < m_iBtnsNum; i++ )
	{
		RemoveItem( *( m_apBtns[i] ) );
		delete m_apBtns[i];
		m_apBtns[i] = NULL;
	}
}

void CMenuFramework::Show()
{
	BaseClass::Show();
}

void CMenuFramework::Draw()
{
	static int statusFadeTime;
	static CMenuBaseItem *lastItem;
	CMenuBaseItem *item;

	// A7 floating dialog: apply any in-progress title-bar drag before chrome
	if( m_bFloat && m_bDragging )
		UpdateFloatDrag();

	// A7 floating dialog chrome goes behind the items
	if( m_bFloat && m_bFrameValid )
		DrawFloatFrame();

	BaseClass::Draw();

	item = ItemAtCursor();
	// a1ba: maybe this should be somewhere else?
	if( item != lastItem )
	{
		if( item ) item->m_iLastFocusTime = uiStatic.realTime;
		statusFadeTime = uiStatic.realTime;

		lastItem = item;
	}

	// draw status text if status text isn't rendered at right side of an item
	// ideally, framework shouldn't even know about this but since everything
	// WON-styled goes here, let's leave it here...
	if( item && item == lastItem && !FBitSet( item->iFlags, QMF_NOTIFY ) && item->szStatusText != NULL )
	{
		float alpha = bound( 0, ((( uiStatic.realTime - statusFadeTime ) - 100 ) * 0.01f ), 1 );
		int r, g, b, x, len;

		EngFuncs::ConsoleStringLen( item->szStatusText, &len, NULL );

		UnpackRGB( r, g, b, uiColorHelp );
		EngFuncs::DrawSetTextColor( r, g, b, alpha * 255 );
		x = ( ScreenWidth - len ) * 0.5f; // centering

		EngFuncs::DrawConsoleString( x, uiStatic.yOffset + 720 * uiStatic.scaleY, item->szStatusText );
	}
	else statusFadeTime = uiStatic.realTime;
}

void CMenuFramework::Hide()
{
	BaseClass::Hide();
}

void CMenuFramework::Init()
{
	BaseClass::Init();
	pos.x = uiStatic.xOffset;
	pos.y = uiStatic.yOffset;
	size.w = uiStatic.width;
	size.h = 768;

	// A7: every root window except the main menu opens as a GoldSource-style
	// floating dialog (the main menu stays visible underneath). The touch
	// EDITOR (raw on-screen button dragging) keeps its fullscreen layout -
	// it edits raw screen coordinates and would break when translated.
	// A7 build 62: the touch BUTTONS list editor gets the frame too
	// ("touch edit doesn't have" the dialog).
	m_bFloat = strcmp( szName, "CMenuMain" ) != 0
		&& strcmp( szName, "CMenuTouchEdit" ) != 0;
}

void CMenuFramework::VidInit()
{
	pos.x = uiStatic.xOffset;
	pos.y = uiStatic.yOffset;
	size.w = uiStatic.width;
	size.h = 768;

	BaseClass::VidInit();

	// A7 floating dialogs (remake): the float offset is not baked into item
	// coordinates, so there is nothing to undo - VidInit rebuilds the raw
	// layout and ApplyFloatLayout re-derives the offset from scratch.
	if( m_bFloat )
		ApplyFloatLayout();
}

CMenuPicButton * CMenuFramework::AddButton(const char *szName, const char *szStatus, EDefaultBtns buttonPicId, CEventCallback onReleased, int iFlags)
{
	if( m_iBtnsNum >= MAX_FRAMEWORK_PICBUTTONS )
	{
		Host_Error( "Too many pic buttons in framework!" );
		return NULL;
	}

	CMenuPicButton *btn = new CMenuPicButton();

	btn->SetNameAndStatus( szName, szStatus );
	btn->SetPicture( buttonPicId );
	btn->iFlags |= iFlags;
	btn->onReleased = onReleased;
	btn->SetCoord( 72, 230 + m_iBtnsNum * 50 );
	AddItem( btn );

	m_apBtns[m_iBtnsNum++] = btn;

	return btn;
}

CMenuPicButton * CMenuFramework::AddButton( const char *szName, const char *szStatus, const char *szButtonPath, CEventCallback onReleased, int iFlags, int hotkey )
{
	if( m_iBtnsNum >= MAX_FRAMEWORK_PICBUTTONS )
	{
		Host_Error( "Too many pic buttons in framework!" );
		return NULL;
	}

	CMenuPicButton *btn = new CMenuPicButton();

	btn->SetNameAndStatus( szName, szStatus );
	btn->SetPicture( szButtonPath, hotkey );
	btn->iFlags |= iFlags;
	btn->onReleased = onReleased;
	btn->SetCoord( 72, 230 + m_iBtnsNum * 50 );
	AddItem( btn );

	m_apBtns[m_iBtnsNum++] = btn;

	return btn;
}

void CMenuFramework::RealignButtons( void )
{
	for( int i = 0, j = 0; i < m_iBtnsNum; i++ )
	{
		if( !m_apBtns[i]->IsVisible())
			continue;

		// raw coords are correct even for floating dialogs: the float
		// offset flows through GetPositionOffset at render time, so
		// adding it here would apply it twice
		m_apBtns[i]->SetCoord( 72, 230 + j * 50 );
		m_apBtns[i]->CalcPosition();
		j++;
	}
}

bool CMenuFramework::KeyDown( int key )
{
	// A7 build 91: F2/F3 switch tab pages (previous/next) in tabbed
	// dialogs - Options, New Game, the server browser. arrow keys keep
	// their normal item-navigation meaning
	if( key == K_F2 || key == K_F3 )
	{
		for( int i = 0; i < m_pItems.Count(); i++ )
		{
			CMenuTabView *tabs = m_pItems[i]->GetTabView();

			if( tabs )
			{
				tabs->SwitchPage( key == K_F3 ? 1 : -1 );
				return true;
			}
		}
	}

	// A7: a second press while a title-bar drag is active is swallowed -
	// otherwise it reaches items that moved under the cursor mid-drag
	if( m_bFloat && m_bDragging )
		return true;

	// A7 floating dialogs: press on the title-bar close box
	if( m_bFloat && m_bFrameValid && UI::Key::IsLeftMouse( key )
		&& UI_CursorInRect( m_scCloseRect.pt, m_scCloseRect.sz ))
	{
		m_bClosePressed = true;
		return true;
	}

	// A7 floating dialogs: press on the title bar (not the close box) to drag
	if( m_bFloat && m_bFrameValid && UI::Key::IsLeftMouse( key ))
	{
		int barH = (int)( 44 * m_flLayoutScale * uiStatic.scaleY + 0.5f );
		Point tp = m_scFrameRect.pt;
		Size  ts( m_scFrameRect.sz.w, barH );

		if( UI_CursorInRect( tp, ts ) && !UI_CursorInRect( m_scCloseRect.pt, m_scCloseRect.sz ))
		{
			m_bDragging = true;
			m_bDragPast = false; // wait for real movement (tap-jitter threshold)
			m_scDragOrigin = Point( uiStatic.cursorX, uiStatic.cursorY );
			m_iDragBaseDX = m_iFloatDX;
			m_iDragBaseDY = m_iFloatDY;
			// A7 fix: the delta must be measured from the CURRENT offset.
			// Starting at 0 made the first drag frame apply base+delta as
			// a delta - the dialog jumped sideways by the whole layout
			// offset on any tap near the title bar ("if I click, it moves
			// all of them").
			m_iDragAppliedX = m_iFloatDX;
			m_iDragAppliedY = m_iFloatDY;
			return true;
		}
	}

	bool b = BaseClass::KeyDown( key );

	if( UI::Key::IsEscape( key ))
	{
		const CMenuBaseWindow *newWindow = WindowStack()->Current();

		if( newWindow != nullptr && newWindow != this && newWindow->IsRoot() )
		{
			PrepareBannerAnimation( ANIM_CLOSING, nullptr );
		}
	}

	return b;
}

bool CMenuFramework::KeyUp( int key )
{
	// A7 floating dialogs: release ends an in-progress drag
	if( m_bFloat && m_bDragging && UI::Key::IsLeftMouse( key ))
	{
		m_bDragging = false;
		return true;
	}

	// A7 floating dialogs: release inside the close box closes the window
	if( m_bFloat && m_bClosePressed )
	{
		m_bClosePressed = false;

		if( UI::Key::IsLeftMouse( key ) && UI_CursorInRect( m_scCloseRect.pt, m_scCloseRect.sz ))
			Hide();

		return true;
	}

	return BaseClass::KeyUp( key );
}

// A7: while a title-bar drag is active, hover must not reach the items -
// the dialog moves under the cursor, and reacting to it (TabView's hover
// tab-switch, item focus) made content jump around mid-drag
bool CMenuFramework::MouseMove( int x, int y )
{
	if( m_bFloat && m_bDragging )
		return true;

	return BaseClass::MouseMove( x, y );
}

void CMenuFramework::PrepareBannerAnimation( EAnimation direction, CMenuPicButton *initiator )
{
	// banner is not present, ignore
	if( banner.Parent() != this )
		return;

	// A7 floating dialogs: the banner is replaced by the title bar,
	// there is no banner to animate
	if( m_bFloat )
		return;

	bannerAnimDirection = direction;
	if( initiator )
	{
		bannerRects[0].pt = initiator->GetRenderPosition();
		bannerRects[0].sz = initiator->GetRenderSize();

		bannerRects[1].pt = banner.GetRenderPosition();
		bannerRects[1].sz = banner.GetRenderSize();

		// keep an explicit page title (derived in SetPicture from the missing
		// banner art); only adopt the button label when there is no title yet
		if( !banner.szName || !banner.szName[0] )
			banner.szName = initiator->szName;
	}
}

bool CMenuFramework::DrawAnimation()
{
	bool b = CMenuBaseWindow::DrawAnimation( );

	if( bannerAnimDirection != ANIM_NO )
	{
		float frac;

		if( bannerAnimDirection == ANIM_OPENING )
			frac = ( uiStatic.realTime - m_iTransitionStartTime ) / TTT_PERIOD;
		else
			frac = 1.0f - ( uiStatic.realTime - m_iTransitionStartTime ) / TTT_PERIOD;
		Rect r = Rect::Lerp( bannerRects[0], bannerRects[1], frac );

		banner.Draw( r.pt, r.sz );
	}

	if( b )
		bannerAnimDirection = ANIM_NO;

	return b;
}

CMenuFramework::CMenuBannerBitmap::CMenuBannerBitmap()
{
	SetRect( UI_BANNER_POSX, UI_BANNER_POSY, UI_BANNER_WIDTH, UI_BANNER_HEIGHT );
}

// A7: derive a human page title from the banner art path, used as the flat
// text fallback when gfx/shell/head_*.tga isn't present on the device:
// "gfx/shell/head_vidmodes" -> "Video Modes"
static const char *UI_BannerTitleFromArt( const char *pic )
{
	static char title[64];
	const char *base;
	const char *ext;
	int len;

	if( !pic || !pic[0] )
		return "";

	base = strrchr( pic, '/' );
	base = base ? base + 1 : pic;
	ext = strrchr( base, '.' );
	len = ext ? (int)( ext - base ) : (int)strlen( base );

	if( len > 5 && !strnicmp( base, "head_", 5 ) )
	{
		base += 5;
		len -= 5;
	}

	if( len >= (int)sizeof( title ) )
		len = sizeof( title ) - 1;
	memcpy( title, base, len );
	title[len] = 0;

	static const struct
	{
		const char *key;
		const char *name;
	} fixes[] =
	{
		{ "config",       "Options" },
		{ "multi",        "Multiplayer" },
		{ "inetgames",    "Internet Games" },
		{ "lan",          "LAN Games" },
		{ "vidmodes",     "Video Modes" },
		{ "vidoptions",   "Video Options" },
		{ "xhair",        "Crosshair" },
		{ "advoptions",   "Advanced Options" },
		{ "gameopts",     "Game Options" },
		{ "custom",       "Custom Game" },
		{ "saveload",     "Save / Load" },
		{ "load",         "Load Game" },
		{ "save",         "Save Game" },
		{ "newgame",      "New Game" },
		{ "creategame",   "Create Game" },
		{ "customize",    "Player Setup" },
		{ "inputdevices", "Input Devices" },
		{ "touch_buttons","Touch Buttons" },
		{ "touch_options","Touch Options" },
		{ "touchoptions", "Touch Options" },
	};

	for( int i = 0; i < (int)( sizeof( fixes ) / sizeof( fixes[0] ) ); i++ )
	{
		if( !stricmp( title, fixes[i].key ) )
			return fixes[i].name;
	}

	// generic: underscores become spaces, capitalize each word
	for( char *p = title; *p; p++ )
	{
		if( *p == '_' )
			*p = ' ';

		if( ( *p >= 'a' && *p <= 'z' ) && ( p == title || p[-1] == ' ' ) )
			*p = (char)( *p - 'a' + 'A' );
	}

	return title;
}

// A7 floating dialogs ----------------------------------------------------

// "CMenuVideoOptions" -> "Video Options": fallback caption for frameworks
// that have no banner text
static void UI_FloatTitleFromName( const char *name, char *out, size_t outsz )
{
	const char *p = name ? name : "";
	size_t o = 0;
	bool cap = true;

	if( !strnicmp( p, "CMenu", 5 ))
		p += 5;

	for( ; *p && o + 1 < outsz; p++ )
	{
		char c = *p;

		if( c == '_' )
		{
			if( o && out[o - 1] != ' ' )
				out[o++] = ' ';
			cap = true;
			continue;
		}

		if( cap )
		{
			if( c >= 'a' && c <= 'z' )
				c -= 32;
			cap = false;
		}
		else if( o && c >= 'A' && c <= 'Z' && out[o - 1] != ' ' )
		{
			out[o++] = ' ';
			if( o + 1 >= outsz )
				break;
		}

		out[o++] = c;
	}

	out[o] = 0;
}

// A7 floating dialogs (REMAKE): the float offset is NOT baked into item
// coordinates - it is composed at render time through this hook, which
// every descendant chains via CalcPosition (holders return their own
// m_scPos, so the offset cascades into nested tab pages automatically).
// Consequences over the old pos-mutating ShiftContent:
//  - dragging can never change any size;
//  - labels and hidden tab pages follow the dialog on RecalcRenderTree;
//  - VidInit/RealignButtons stay valid without offset bookkeeping.
Point CMenuFramework::GetPositionOffset() const
{
	Point ret = BaseClass::GetPositionOffset();

	if( m_bFloat )
	{
		ret.x += (int)( m_iFloatDX * uiStatic.scaleX );
		ret.y += (int)( m_iFloatDY * uiStatic.scaleY );
	}

	return ret;
}

void CMenuFramework::ApplyFloatLayout()
{
	m_bFrameValid = false;

	// capture the page title, hide the banner
	if( banner.Parent() == this )
	{
		if( banner.szName && banner.szName[0] )
			Q_strncpy( m_szFloatTitle, banner.szName, sizeof( m_szFloatTitle ));
		else
			UI_FloatTitleFromName( szName, m_szFloatTitle, sizeof( m_szFloatTitle ));

		banner.SetVisibility( false );
	}
	else if( !m_szFloatTitle[0] )
		UI_FloatTitleFromName( szName, m_szFloatTitle, sizeof( m_szFloatTitle ));

	// bounds over all visible content items (virtual coords)
	bool any = false;
	int bx = 0, by = 0, br = 0, bb = 0;

	FOR_EACH_VEC( m_pItems, i )
	{
		CMenuBaseItem *it = m_pItems[i];

		if( it == &banner )
			continue;
		if( FBitSet( it->iFlags, QMF_HIDDEN | QMF_HIDDENBYPARENT ))
			continue;

		// freeze right/bottom-anchored (negative) sizes once, against the
		// raw pre-offset layout: CalcSizes re-resolves negatives from the
		// current render position, so a shifted item would otherwise
		// stretch/shrink on every drag ("move menu size also"). Resolving
		// to positive here matches CalcSizes' virtual-space math.
		if( it->size.w < 0 || it->size.h < 0 )
		{
			CMenuItemsHolder *par = it->Parent();
			int pw = par ? par->size.w : (int)uiStatic.width;
			int ph = par ? par->size.h : 768;

			if( it->size.w < 0 )
				it->size.w = Q_max( 0, pw + it->size.w - it->pos.x );
			if( it->size.h < 0 )
				it->size.h = Q_max( 0, ph + it->size.h - it->pos.y );
		}

		// A7 build 62: use the visual extent - labels drawn outside the
		// declared rect (checkbox/switch text, QMF_NOTIFY status column)
		// must stay inside the frame
		int w = it->GetVisualWidth();
		int h = it->size.h;

		if( !any )
		{
			bx = it->pos.x;
			by = it->pos.y;
			br = bx + w;
			bb = by + h;
			any = true;
		}
		else
		{
			bx = Q_min( bx, it->pos.x );
			by = Q_min( by, it->pos.y );
			br = Q_max( br, it->pos.x + w );
			bb = Q_max( bb, it->pos.y + h );
		}
	}

	if( !any )
		return;

	int bw = br - bx;
	int bh = bb - by;

	// A7 build 60: the per-dialog fit pass of builds 58/59 is REMOVED.
	// Each dialog derived its OWN scale factor, so dialogs rendered at
	// different sizes ("different sizes") - and the windows that never got
	// one (YesNo, color picker) never matched them. The factor was also
	// applied after items had cached tab widths/slider steps in VidInit,
	// leaving strips and knobs misaligned ("disorganized"). One scale for
	// the entire UI now comes from the engine (BaseMenu UI_VidInit floor);
	// here the frame simply wraps the authored content, centered.
	int uiW = (int)uiStatic.width;
	int uiH = (int)( ScreenHeight / ( uiStatic.scaleY > 0.01f ? uiStatic.scaleY : 1.0f ));

	// frame padding scales with the layout so proportions stay constant
	const int PAD = (int)( 14 * m_flLayoutScale + 0.5f );
	const int BAR = (int)( 44 * m_flLayoutScale + 0.5f );

	// modest minimum: small dialogs still read as windows, the fit pass
	// already made real dialogs large
	int minW = uiW * 34 / 100;
	int minH = 260 * m_flLayoutScale;
	int fw = Q_max( bw + PAD * 2, minW );
	int fh = Q_max( bh + PAD * 2 + BAR, minH );
	if( fw > uiW ) fw = uiW;
	if( fh > uiH ) fh = uiH;

	int fx = ( uiW - fw ) / 2;
	if( fx < 0 ) fx = 0;
	int fy = ( uiH - fh ) / 2;
	if( fy < 0 ) fy = 0;

	// shift the content to the panel's inner top-left, then clamp so
	// everything stays on the virtual screen
	int dx = ( fx + PAD ) - bx;
	int dy = ( fy + PAD + BAR ) - by;

	int dxMin = PAD - bx;
	int dxMax = ( uiW - PAD ) - br;
	if( dxMax < dxMin )
		dx = dxMin;
	else
		dx = bound( dxMin, dx, dxMax );

	int dyMin = ( BAR + PAD ) - by;
	int dyMax = ( uiH - PAD ) - bb;
	if( dyMax < dyMin )
		dy = dyMin;
	else
		dy = bound( dyMin, dy, dyMax );

	// A7 build 60: a VidInit with unchanged geometry must not recenter a
	// dialog the user has dragged - keep the placed offset (clamped to the
	// screen) so the panel stays exactly where it was left
	if( m_bFrameValid && fw == m_frameRect.sz.w && fh == m_frameRect.sz.h )
	{
		dx = bound( dxMin, m_iFloatDX, dxMax );
		dy = bound( dyMin, m_iFloatDY, dyMax );
	}

	// build 90: right-aligned dialogs (Options, New Game, Player list)
	// are not centered - pull them as far right as padding allows so the
	// main-menu column (x36..~200) stays fully visible behind them like
	// the reference screenshots. NOTE: a later VidInit re-places them at
	// the right edge (drag persistence is not kept for these).
	if( m_bFloatRightAlign && dxMax >= dxMin )
		dx = dxMax;

	// store the offset, then recompose the whole render tree with it -
	// no logical coordinates are touched
	m_iFloatDX = dx;
	m_iFloatDY = dy;
	RecalcRenderTree();

	// frame wraps the shifted content: title bar above, padding around
	int px = ( bx + dx ) - PAD;
	int py = ( by + dy ) - BAR - PAD;
	int pw = bw + PAD * 2;
	int ph = bh + PAD * 2 + BAR;

	if( pw < minW ) pw = minW;
	if( ph < minH ) ph = minH;
	if( px < 0 ) px = 0;
	if( py < 0 ) py = 0;
	if( px + pw > uiW ) px = uiW - pw;
	if( py + ph > uiH ) py = uiH - ph;
	if( px < 0 ) { px = 0; pw = uiW; }
	if( py < 0 ) { py = 0; ph = uiH; }

	// A7 build 61: store the frame rect WITHOUT the layout offset. The
	// stored rect used to contain (bx+dx) while SyncFloatScreenRects added
	// the applied drag offset on top - the offset was applied TWICE to the
	// panel and never to the content, so the frame was drawn displaced
	// right/down by dx while its content sat at bx+dx (lists and menu
	// columns hung outside the panel: "view still bugged"). Storing the
	// raw rect makes sync apply the shift exactly once; drag clamps in
	// UpdateFloatDrag see the same raw base and stay consistent.
	m_frameRect = Rect( Point( px - dx, py - dy ), Size( pw, ph ));

	// reset any previous drag
	m_bDragging = false;
	m_bDragPast = false;
	m_iDragAppliedX = m_iFloatDX;
	m_iDragAppliedY = m_iFloatDY;

	SyncFloatScreenRects();
	m_bFrameValid = true;
}

// Recompute the screen-space frame + close-box rects from m_frameRect plus
// the applied drag offset. Uses the same transform CalcPosition applies.
void CMenuFramework::SyncFloatScreenRects()
{
	const int BAR = (int)( 44 * m_flLayoutScale + 0.5f );
	Point off = GetRenderPosition();

	int px = m_frameRect.pt.x + m_iDragAppliedX;
	int py = m_frameRect.pt.y + m_iDragAppliedY;
	int pw = m_frameRect.sz.w;
	int ph = m_frameRect.sz.h;

	Point fp = Point( px, py ).Scale();
	fp += off;
	m_scFrameRect = Rect( fp, Size( pw, ph ).Scale() );

	const int cs = (int)( 30 * m_flLayoutScale + 0.5f );
	Point cp( px + pw - 6 - cs, py + ( BAR - cs ) / 2 );
	Point csp = cp.Scale();
	csp += off;
	m_scCloseRect = Rect( csp, Size( cs, cs ).Scale() );
}

// A7: live title-bar drag. The cursor moves the whole dialog (frame + its
// content) together; the shift is clamped so the panel never leaves screen.
void CMenuFramework::UpdateFloatDrag()
{
	if( !m_bFrameValid )
		return;

	float sx = uiStatic.scaleX ? uiStatic.scaleX : 1.0f;
	float sy = uiStatic.scaleY ? uiStatic.scaleY : 1.0f;

	int curX = uiStatic.cursorX;
	int curY = uiStatic.cursorY;

	// A7 tap-jitter threshold: a plain tap (or a couple of px of finger
	// wobble) on the title bar must not move anything. Once the cursor
	// travels far enough, rebase so applying the offset doesn't jump.
	// build 58: the threshold is relative to the screen - a fixed 10 px
	// was far below finger jitter on high-DPI phones ("still moving").
	if( !m_bDragPast )
	{
		int tdx = curX - m_scDragOrigin.x;
		int tdy = curY - m_scDragOrigin.y;
		int thresh = Q_max( 12, ScreenWidth / 60 ); // build 60: smaller, so deliberate small moves engage ("it stays in place")

		if( tdx * tdx + tdy * tdy < thresh * thresh )
			return;

		m_bDragPast = true;
		m_scDragOrigin = Point( curX, curY );
		m_iDragBaseDX = m_iFloatDX;
		m_iDragBaseDY = m_iFloatDY;
		m_iDragAppliedX = m_iFloatDX;
		m_iDragAppliedY = m_iFloatDY;
	}

	// desired extra shift (virtual coords) from the drag origin
	int wantX = m_iDragBaseDX + (int)(( curX - m_scDragOrigin.x ) / sx );
	int wantY = m_iDragBaseDY + (int)(( curY - m_scDragOrigin.y ) / sy );

	// clamp so the frame stays fully on the virtual screen
	int uiW = (int)uiStatic.width;
	int uiH = (int)( ScreenHeight / ( uiStatic.scaleY > 0.01f ? uiStatic.scaleY : 1.0f ));
	int px = m_frameRect.pt.x + wantX;
	int py = m_frameRect.pt.y + wantY;
	if( px < 0 ) wantX -= px;
	if( py < 0 ) wantY -= py;
	if( px + m_frameRect.sz.w > uiW ) wantX -= ( px + m_frameRect.sz.w - uiW );
	if( py + m_frameRect.sz.h > uiH ) wantY -= ( py + m_frameRect.sz.h - uiH );

	int shiftX = wantX - m_iDragAppliedX;
	int shiftY = wantY - m_iDragAppliedY;

	if( !shiftX && !shiftY )
		return;

	m_iFloatDX += shiftX;
	m_iFloatDY += shiftY;
	m_iDragAppliedX = wantX;
	m_iDragAppliedY = wantY;

	// recompose instead of mutating pos: sizes can't change, every
	// descendant (labels, hidden tab pages) follows the new offset
	RecalcRenderTree();
	SyncFloatScreenRects();
}

void CMenuFramework::DrawFloatFrame()
{
	if( !m_bFrameValid )
		return;

	Point p = m_scFrameRect.pt;
	Size  s = m_scFrameRect.sz;
	// A7 build 58: title bar scales with the fit pass so it keeps its
	// proportions inside bigger dialogs
	int   barH = (int)( 44 * m_flLayoutScale * uiStatic.scaleY + 0.5f );

	// A7 build 87: opaque GameUI panel - the reference dialogs are solid
	// olive, not a translucent overlay over the collage
	UI_FillRect( p, s, uiPromptBgColor );
	UI_DrawBevel( p, s, false ); // raised edge

	// title bar: same panel colour with a light underline (GameUI has no
	// dark bar or gold line under the caption)
	int accH = Q_max( 1, (int)( 2 * uiStatic.scaleY ));
	UI_FillRect( p.x, p.y + barH - accH, s.w, accH, uiColorBorderLight );

	// caption (banner text style: big Tahoma with a hard drop shadow)
	float fls = m_flLayoutScale;
	Point tp( p.x + (int)( 16 * fls * uiStatic.scaleX ), p.y );
	Size  ts( s.w - (int)( 80 * fls * uiStatic.scaleX ), barH );
	UI_DrawString( uiStatic.hBigFont, tp, ts, m_szFloatTitle, uiColorWhite,
		(int)( UI_BIG_CHAR_HEIGHT * fls * uiStatic.scaleY + 0.5f ), QM_LEFT,
		ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_SHADOW );

	// close box
	bool hover = UI_CursorInRect( m_scCloseRect.pt, m_scCloseRect.sz );
	Point cp = m_scCloseRect.pt;
	Size  cs = m_scCloseRect.sz;

	UI_FillRect( cp, cs, hover ? uiColorAccent : uiColorBg );
	UI_DrawRectangleExt( cp, cs, hover ? uiColorAccent : uiColorBorderLight, 0 );
	UI_DrawString( uiStatic.hBigFont, cp, cs, "X",
		hover ? uiColorBlack : uiColorWhite,
		(int)( 22 * m_flLayoutScale * uiStatic.scaleY + 0.5f ), QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );
}

void CMenuFramework::CMenuBannerBitmap::SetPicture(const char *pic)
{
	// A7-full-port: the stock xash/WON banner art (orange bubble letters in
	// gfx/shell/head_*.tga) is not original CS 1.6. Skip loading it and render
	// a flat text page title instead (the CS 1.6 GameUI is text-driven); the
	// title is derived from the art name. image is left invalid so Draw()
	// takes the text path.
	const char *title = UI_BannerTitleFromArt( pic );

	if( title[0] )
		SetText( title );
}

void CMenuFramework::CMenuBannerBitmap::Draw( Point pt, Size sz )
{
	if( image.IsValid() )
	{
		UI_DrawPic( pt, sz, uiColorWhite, image, QM_DRAWADDITIVE );
	}
	else
	{
		// A7: banner art (gfx/shell/head_*.tga) isn't shipped with the Android
		// data set, so draw a plain flat page title here instead of the stock
		// xash3d blur-glow text - big Tahoma with a hard drop shadow, like the
		// original CS 1.6 menu banners
		UI_DrawString( uiStatic.hBigFont, pt,
			sz,
			szName,
			uiPromptTextColor, UI_BIG_CHAR_HEIGHT * uiStatic.scaleY,
			QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_SHADOW );
	}
}

void CMenuFramework::CMenuBannerBitmap::Draw()
{
	CMenuFramework *pParent = (CMenuFramework *)Parent();

	if (pParent && (pParent->bannerAnimDirection == ANIM_OPENING || pParent->eTransitionType == ANIM_OPENING))
		return;

	Draw( m_scPos, m_scSize );
}

