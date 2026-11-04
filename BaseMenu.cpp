/*
ui_menu.c -- main menu interface
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2017 a1batross

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "extdll_menu.h"
#include "BaseMenu.h"
#include "PicButton.h"
#include "keydefs.h"
#include "Utils.h"
#include "Btns.h"
#include "YesNoMessageBox.h"
#include "BackgroundBitmap.h"
#include "FontManager.h"
#include "cursor_type.h"
#include "utflib.h"
// A7 build 87: UI_UpdateSoundSet needs the full CMenuBaseWindow (IsMainMenuRoot)
#include "BaseWindow.h"

cvar_t		*ui_showmodels;
cvar_t		*ui_showclassmodels;
cvar_t		*ui_show_window_stack;
cvar_t		*ui_borderclip;
cvar_t		*ui_prefer_won_background;
cvar_t		*ui_background_stretch;
cvar_t		*ui_logohorizontal;

uiStatic_t	uiStatic;
static CMenuEntry	*s_pEntries = NULL;

const char	*uiSoundOldPrefix	= "media/";
const char	*uiSoundNewPrefix	= "sound/common/";
// A7 build 64: the original CS 1.6 GameUI sound set, reverse-
// engineered like the video's GameUI: every VGUI button plays
// sound/UI/buttonclick.wav on press and buttonrollover.wav on hover;
// buttonclickrelease.wav marks grab/release/invalid state changes.
// The old media/launch_* set is the WON Half-Life menu, not the
// 2003 GameUI. Menu open/close (SND_IN/SND_OUT) is silent, exactly
// like the real GameUI - the triggering button already played its
// click. All three wavs ship in valve/sound/UI (device + valve zip).
const char	*uiSounds[] = {
	"",                                  // SND_IN      silent, real GameUI
	"",                                  // SND_OUT     silent, real GameUI
	"sound/UI/buttonclick.wav",          // SND_LAUNCH  press/activate
	"sound/UI/buttonrollover.wav",       // SND_ROLLOVER hover
	"sound/UI/buttonclick.wav",          // SND_GLOW    checkbox/toggle press
	"sound/UI/buttonclickrelease.wav",   // SND_BUZZ    invalid action
	"sound/UI/buttonclickrelease.wav",   // SND_KEY     key grabbed/changed
	"sound/UI/buttonclickrelease.wav",   // SND_REMOVEKEY unbind/remove
	"sound/UI/buttonrollover.wav",       // SND_MOVE    focus moved
	""                                   // SND_NULL
};

// A7 build 87: the original WON/GoldSource launch_* set for every window
// except the main menu (user request: "sub-menus use original GoldSource
// sounds, just the vgui main menu sound"). Same names as upstream mainui;
// when media/ is absent the loader rewrites them to sound/common/launch_*
const char	*uiSoundsGoldSource[] = {
	"media/launch_upmenu1.wav",  // SND_IN       menu opening
	"media/launch_dnmenu1.wav",  // SND_OUT      menu closing
	"media/launch_select2.wav",  // SND_LAUNCH   press/activate
	"",                          // SND_ROLLOVER (silent upstream too)
	"media/launch_glow1.wav",    // SND_GLOW     checkbox/toggle press
	"media/launch_deny2.wav",    // SND_BUZZ     invalid action
	"media/launch_select1.wav",  // SND_KEY      key grabbed/changed
	"media/launch_deny1.wav",    // SND_REMOVEKEY unbind/remove
	"",                          // SND_MOVE     focus moved
	""                           // SND_NULL
};

// they match default WON colors.lst now, except alpha
// a1ba: made uiColorHelp brighter so it's easier to read
// A7 build 87: defaults re-sampled pixel-by-pixel from the original
// CS 1.6 GameUI reference screenshots (panel #4c5844, secondary #3e4637,
// accent #c4b550, text #d8ded3, borders #889180/#282e22)
// A7 build 90: palette audit vs PC GoldSource GameUI - labels are the light
// warm white #d8ded3 (uiColorText/uiPromptTextColor), focus/selection keeps
// the GoldSource yellow-tinted accent #c4b550, ETF_SHADOW always paints pure
// black (uiColorBlack re-packed to the text alpha in UI_DrawString) and
// disabled/greyed items now fade to a plain mid grey instead of dark olive.
unsigned int uiColorHelp        = 0xFFA0AA95; // 160, 170, 149, 255 // hint letters color (text-3)
unsigned int uiPromptBgColor    = 0xFF4C5844; // 76,  88,  68,  255 // dialog background color (--bg)
unsigned int uiPromptTextColor  = 0xFFD8DED3; // 216, 222, 211, 255 // dialog or button letters color
unsigned int uiPromptFocusColor = 0xFFC4B550; // 196, 181, 80,  255 // dialog or button focus letters color (--accent)
unsigned int uiInputTextColor   = 0xFFC4B550; // 196, 181, 80,  255 // active/focused letters (--accent)
unsigned int uiInputBgColor     = 0xFF3E4637; // 62,  70,  55,  255 // field, scrollist, checkbox background (--secondary-bg)
unsigned int uiInputFgColor     = 0xFF282E22; // 40,  46,  34,  255 // field border/foreground (--border-dark)
unsigned int uiColorWhite       = 0xFFFFFFFF; // 255, 255, 255, 255 // useful for bitmaps
// build 90: PC GoldSource grays disabled items out to a neutral mid grey -
// the dark olive #282e22 read as dirt on the panel. Not used as a shadow
// anymore: ETF_SHADOW packs uiColorBlack (see UI_DrawString).
unsigned int uiColorDkGrey      = 0x80808080; // 128, 128, 128, 128 // grayed/disabled items
unsigned int uiColorBlack       = 0x80000000; //  0,   0,   0,  128 // some controls background
unsigned int uiColorConsole     = 0xFFC4B550; // just for reference

// A7 build 87 palette tokens (sampled from the GameUI reference)
unsigned int uiColorBg          = 0xFF4C5844; // #4c5844 panel background
unsigned int uiColorSecondaryBg = 0xFF3E4637; // #3e4637 input/list background
unsigned int uiColorAccent      = 0xFFC4B550; // #c4b550 gold accent (focus, active tab, checked)
unsigned int uiColorBorderLight = 0xFF889180; // #889180 bevel light
unsigned int uiColorBorderDark  = 0xFF282E22; // #282e22 bevel dark
unsigned int uiColorText        = 0xFFD8DED3; // #d8ded3 control labels (GameUI text-2)

// color presets (this is nasty hack to allow color presets to part of text)
const unsigned int g_iColorTable[8] =
{
0xFF000000, // black
0xFFFF0000, // red
0xFF00FF00, // green
0xFFFFFF00, // yellow
0xFF0000FF, // blue
0xFF00FFFF, // cyan
0xFFC4B550, // dialog or button letters color (A7: accent)
0xFFFFFFFF, // white
};

CMenuEntry::CMenuEntry(const char *cmd, void (*pfnPrecache)(), void (*pfnShow)(), void (*pfnShutdown)() ) :
	m_szCommand( cmd ),
	m_pfnPrecache( pfnPrecache ),
	m_pfnShow( pfnShow ),
	m_pfnShutdown( pfnShutdown ),
	m_pNext( s_pEntries )
{
	s_pEntries = this;
}

/*
=================
UI_ScaleCoords

Any parameter can be NULL if you don't want it
=================
*/
void UI_ScaleCoords( int *x, int *y, int *w, int *h )
{
	if( x ) *x *= uiStatic.scaleX;
	if( y ) *y *= uiStatic.scaleY;
	if( w ) *w *= uiStatic.scaleX;
	if( h ) *h *= uiStatic.scaleY;
}

void UI_ScaleCoords( int &x, int &y )
{
	x *= uiStatic.scaleX;
	y *= uiStatic.scaleY;
}

void UI_ScaleCoords( int &x, int &y, int &w, int &h )
{
	UI_ScaleCoords( x, y );
	UI_ScaleCoords( w, h );
}

Point Point::Scale()
{
	return Point( x * uiStatic.scaleX, y * uiStatic.scaleY );
}

Size Size::Scale()
{
	return Size( w * uiStatic.scaleX, h * uiStatic.scaleY );
}

/*
=================
UI_CursorInRect
=================
*/
bool UI_CursorInRect( int x, int y, int w, int h )
{
	if( uiStatic.cursorX < x || uiStatic.cursorX > x + w )
		return false;
	if( uiStatic.cursorY < y || uiStatic.cursorY > y + h )
		return false;
	return true;
}

/*
=================
UI_EnableAlphaFactor
=================
*/
void UI_EnableAlphaFactor( float a )
{
	uiStatic.enableAlphaFactor = true;
	uiStatic.alphaFactor = bound( 0.0f, a, 1.0f );
}

/*
=================
UI_DisableAlphaFactor
=================
*/
void UI_DisableAlphaFactor()
{
	uiStatic.enableAlphaFactor = false;
}

/*
=================
UI_DrawPic
=================
*/
void UI_DrawPic( int x, int y, int width, int height, const unsigned int color, CImage &pic, const ERenderMode eRenderMode )
{
	HIMAGE hPic = pic.Handle();

	if( !hPic )
		return;

	int r, g, b, a;
	UnpackRGBA( r, g, b, a, color );

	EngFuncs::PIC_Set( hPic, r, g, b, a );
	switch( eRenderMode )
	{
	case QM_DRAWNORMAL:
		if( !uiStatic.enableAlphaFactor )
		{
			EngFuncs::PIC_Draw( x, y, width, height );
			break;
		}
		// intentional fallthrough
	case QM_DRAWTRANS:
		EngFuncs::PIC_DrawTrans( x, y, width, height );
		break;
	case QM_DRAWADDITIVE:
		EngFuncs::PIC_DrawAdditive( x, y, width, height );
		break;
	case QM_DRAWHOLES:
		EngFuncs::PIC_DrawHoles( x, y, width, height );
		break;
	}
}

/*
=================
UI_FillRect
=================
*/
void UI_FillRect( int x, int y, int width, int height, const unsigned int color )
{
	int r, g, b, a;
	UnpackRGBA( r, g, b, a, color );

	EngFuncs::FillRGBA( x, y, width, height, r, g, b, a );
}

/*
=================
UI_DrawRectangleExt
=================
*/
void UI_DrawRectangleExt( int in_x, int in_y, int in_w, int in_h, const unsigned int color, int outlineWidth, int flag )
{
	int	x, y, w, h;

	if( outlineWidth == 0 ) outlineWidth = uiStatic.outlineWidth;

	if( flag & QM_LEFT )
	{
		x = in_x - outlineWidth;
		y = in_y - outlineWidth;
		w = outlineWidth;
		h = in_h + outlineWidth + outlineWidth;

		// draw left
		UI_FillRect( x, y, w, h, color );
	}

	if( flag & QM_RIGHT )
	{
		x = in_x + in_w;
		y = in_y - outlineWidth;
		w = outlineWidth;
		h = in_h + outlineWidth + outlineWidth;

		// draw right
		UI_FillRect( x, y, w, h, color );
	}

	if( flag & QM_TOP )
	{
		x = in_x;
		y = in_y - outlineWidth;
		w = in_w;
		h = outlineWidth;

		// draw top
		UI_FillRect( x, y, w, h, color );
	}

	if( flag & QM_BOTTOM )
	{
		// draw bottom
		x = in_x;
		y = in_y + in_h;
		w = in_w;
		h = outlineWidth;

		UI_FillRect( x, y, w, h, color );
	}
}

/*
=================
UI_DrawBevel

A7 retheme: classic two-tone bevel border, drawn outside the rect like
UI_DrawRectangleExt. raised = light top/left, dark bottom/right;
sunken = inverted (classic input field look).
=================
*/
void UI_DrawBevel( int x, int y, int w, int h, bool bSunken, int outlineWidth )
{
	unsigned int light = uiColorBorderLight;
	unsigned int dark  = uiColorBorderDark;

	if( outlineWidth <= 0 ) outlineWidth = uiStatic.outlineWidth;

	if( bSunken )
	{
		unsigned int tmp = light; light = dark; dark = tmp;
	}

	UI_DrawRectangleExt( x, y, w, h, light, outlineWidth, QM_TOP | QM_LEFT );
	UI_DrawRectangleExt( x, y, w, h, dark,  outlineWidth, QM_BOTTOM | QM_RIGHT );
}

/*
=================
UI_DrawTriangle

A7 retheme: pixel-triangle up/down arrow, drawn from axis-aligned
rectangles (used where gfx/shell arrow art is missing).
=================
*/
void UI_DrawTriangle( Point pos, Size size, bool bUp, unsigned int color )
{
	int u = (int)( uiStatic.scaleX + 0.5f );
	if( u < 1 ) u = 1;

	int h = size.h;
	int w = size.w;
	if( h < u * 2 ) h = u * 2;
	if( w > h ) w = h; // keep the triangle reasonably narrow

	int rows = h / u;
	int cols = w / u;
	if( rows < 2 ) rows = 2;
	if( cols < 2 ) cols = 2;

	int x0 = pos.x + ( size.w - cols * u ) / 2;
	int y0 = pos.y + ( size.h - rows * u ) / 2;

	for( int i = 0; i < rows; i++ )
	{
		int span = cols * ( rows - i ) / rows; // widest row first (points down)
		if( span < 1 ) span = 1;

		int row = bUp ? ( rows - 1 - i ) : i;
		int x = x0 + ( cols - span ) * u / 2;
		int y = y0 + row * u;

		UI_FillRect( x, y, span * u, u, color );
	}
}

/*
=================
UI_DrawString
=================
*/
int UI_DrawString( HFont font, int x, int y, int w, int h,
		const char *string, const unsigned int color,
		int charH, uint justify, uint flags )
{
	uint	modulate, shadowModulate = 0;
	int	xx = 0, yy, shadowOffset = 0, ch;
	int maxX = x;

	// A7 build 112: also reject LOW/foreign pointers. The View-game-info
	// crash read string == 0x1 (an out-of-range table cell handed back a
	// garbage CUtlString buffer) - a null test alone misses those, so any
	// pointer below 0x1000 is treated as absent. Belt: the ServerInfo
	// models bounds-check their rows themselves (user: "it crash after").
	//
	// A7 build 127 (round 128): the round-128 log crashed with
	// string == 0x64696f00000000 - a HIGH garbage pointer (bytes "oid "
	// spliced out of reused text memory), which the 0x1000 floor above
	// cannot catch, and string[0] then faulted. AArch46/x86-64 user
	// addresses are canonical: bits 48..63 must be all 0 (or all 1 for
	// kernel). Every pointer that is about to be dereferenced as a C
	// string must live below bit 48 - reject anything else and draw
	// nothing rather than crash the browser (user: "crash when view
	// game servers").
	if( !string || string < (const char *)0x1000 || ( (uintptr_t)string >> 48 ) != 0 || !string[0] )
	{
		// build 127: one-shot breadcrumb so the scan can prove the guard is
		// in the shipped .so the first time it actually fires in the field.
		static bool bBadStringWarned;
		if( !bBadStringWarned && string )
		{
			bBadStringWarned = true;
			Con_Printf( "[A7] build127: UI_DrawString refused a non-canonical string ptr (crash guard)\n" );
		}
		return x;
	}

	if( flags & ETF_SHADOW )
	{
		shadowModulate = PackAlpha( uiColorBlack, UnpackAlpha( color ));
		// build 90: PC GoldSource drops the shadow exactly one device pixel
		// down/right - the old 3x-scale offset doubled the letters over
		// themselves and read as blur/glow instead of a hard black shadow
		shadowOffset = Q_max( 1, (int)( uiStatic.scaleX + 0.5f ));
	}

	// build 90: some labels pass a fixed DESIGN height (the main menu
	// buttons hardcode 26 in menu units) while the font is rasterized at
	// the device scale; remap so wrapping, ellipsis and vertical centering
	// measure the very line the glyphs were rasterized for. Callers that
	// already pass m_scChSize (or anything at scale 1) normalize onto an
	// equal value and are left untouched.
	if( charH > 0 )
	{
		// must use the SAME truncation as m_scChSize (charSize * scaleY) and
		// CFontBuilder::Create, otherwise a 1px rounding gap stops matching
		int deviceCharH = (int)((float)charH * uiStatic.scaleY );

		if( deviceCharH != charH && deviceCharH == g_FontMgr->GetFontRequestTall( font ))
			charH = deviceCharH;
	}

	modulate = color;

	if( justify & QM_TOP )
	{
		yy = y;
		h -= h % charH;
	}
	else if( justify & QM_BOTTOM )
	{
		yy = y + h - charH;
		h -= h % charH;
	}
	else
	{
		yy = y + (h - charH)/2;
		h -= charH;
	}

	if( flags & ETF_NO_WRAP )
		h = charH;

	int i = 0;
	// build 90: measures "..." in the same units the loop below accumulates
	// (the possibly re-mapped charH), so wrapping can't truncate ASCII early
	int ellipsisWide = g_FontMgr->GetEllipsisWideScaled( font, charH );
	bool giveup = false;

	while( string[i] && !giveup )
	{
		char line[1024], *l;
		int j = i, len = 0;
		int pixelWide = 0;
		int save_pixelWide = 0;
		int save_j = 0;
		utfstate_t state;

		while( string[j] )
		{
			if( string[j] == '\n' )
			{
				j++;
				break;
			}

			if( len == sizeof( line ) - 1 )
				break;

			line[len] = string[j];

			uint32_t uch = state.Decode((uint8_t)string[j] );

			if( IsColorString( string + j )) // don't calc wides for colorstrings
			{
				line[len+1] = string[j+1];
				len += 2;
				j += 2;
			}
			else if( !uch ) // don't calc wides for invalid codepoints
			{
				len++;
				j++;
			}
			else
			{
				int charWide;

				// does we have free space for new line?
				if( yy < (y + h) - charH )
				{
					if( uch == ' ' && pixelWide < w ) // remember last whitespace
					{
						save_pixelWide = pixelWide;
						save_j = j;
					}
				}
				else
				{
					// remember last position, when we still fit
					if( pixelWide + ellipsisWide < w && j > 0 )
					{
						save_pixelWide = pixelWide;
						save_j = j;
					}
				}

				charWide = g_FontMgr->GetCharacterWidthScaled( font, uch, charH );

				if( !(flags & ETF_NOSIZELIMIT) && pixelWide + charWide > w )
				{
					// do we have free space for new line?
					if( yy < (y + h) - charH )
					{
						// try to word wrap
						if( save_j != 0 && save_pixelWide != 0 )
						{
							pixelWide = save_pixelWide;
							len -= j - save_j; // skip whitespace
							j = save_j + 1; // skip whitespace
						}

						break;
					}
					else
					{

						if( save_j != 0 && save_pixelWide != 0 )
						{
							pixelWide = save_pixelWide;
							len -= j - save_j;
							j = save_j;


							if( len > 0 )
							{
								line[len] = '.';
								line[len+1] = '.';
								line[len+2] = '.';
								len += 3;
							}
						}

						// we don't have free space anymore, so just stop drawing
						giveup = true;

						break;
					}
				}
				else
				{
					pixelWide += charWide;
					j++;
					len++;
				}
			}
		}
		line[len] = 0;

		// align the text as appropriate
		if( justify & QM_LEFT  )
		{
			xx = x;
		}
		else if( justify & QM_RIGHT )
		{
			xx = x + (w - pixelWide);
		}
		else // QM_LEFT
		{
			xx = x + (w - pixelWide) / 2.0f;
		}

		// draw it
		l = line;
		state.Reset();
		while( *l )
		{
			if( IsColorString( l ))
			{
				int colorNum = ColorIndex( *(l+1) );

				if( colorNum == 7 && color != 0 )
				{
					modulate = color;
				}
				else if( !(flags & ETF_FORCECOL) )
				{
					modulate = PackAlpha( g_iColorTable[colorNum], UnpackAlpha( color ));
				}

				l += 2;
				continue;
			}

			ch = *l++;
			ch &= 255;

// when using custom font render, we use utf-8
			ch = state.Decode((uint8_t)ch );
			if( !ch )
				continue;

			if( flags & ETF_SHADOW )
				g_FontMgr->DrawCharacter( font, ch, Point( xx + shadowOffset, yy + shadowOffset ), charH, shadowModulate, flags & ETF_ADDITIVE );

#ifdef DEBUG_WHITESPACE
			if( ch == ' ' )
			{
				g_FontMgr->DrawCharacter( font, '_', Point( xx, yy ), charH, modulate, flags & ETF_ADDITIVE );
				xx += g_FontMgr->GetCharacterWidthScaled( font, ch, charH );
				continue;
			}
#endif

			xx += g_FontMgr->DrawCharacter( font, ch, Point( xx, yy ), charH, modulate, flags & ETF_ADDITIVE );

			maxX = Q_max( xx, maxX );
		}
		yy += charH;

		i = j;
	}

	return maxX;
}

/*
=================
UI_DrawMouseCursor
=================
*/
void UI_DrawMouseCursor( void )
{
	CMenuBaseItem	*item;
	void *hCursor = (void *)dc_arrow;
	int cursor = uiStatic.menu.Current()->GetCursor();
	item = uiStatic.menu.Current()->GetItemByIndex( cursor );

	if( item && FBitSet( item->iFlags, QMF_HASMOUSEFOCUS )) 	// fast approach
	{
		if( FBitSet( item->iFlags, QMF_GRAYED ))
			hCursor = (void *)dc_no;
		else hCursor = (void *)item->CursorAction();
	}
	else
	{
		FOR_EACH_VEC( uiStatic.menu.Current()->m_pItems, i )
		{
			item = (CMenuBaseItem *)uiStatic.menu.Current()->GetItemByIndex(cursor);

			if( !item->IsVisible( ))
				continue;

			if( !FBitSet( item->iFlags, QMF_HASMOUSEFOCUS ))
				continue;

			if( FBitSet( item->iFlags, QMF_GRAYED ))
				hCursor = (void *)dc_no;
			else hCursor = (void *)item->CursorAction();

			break;
		}
	}

	EngFuncs::SetCursor( hCursor );
}

// =====================================================================

/*
=================
UI_StartBackGroundMap
=================
*/
bool UI_StartBackGroundMap( void )
{
	static int check_dll;

	// the list is empty
	if( uiStatic.bgmaps.IsEmpty())
		return false;

	// check if server dll available only once, as it might be implemented
	// very inefficiently
	if( !check_dll )
	{
		if( EngFuncs::CheckGameDll( ))
			check_dll = 1;
		else
			check_dll = -1;
	}

	if( check_dll < 0 )
		return false;

	// some map is already running
	if( CL_IsActive() || EngFuncs::GetCvarFloat( "cl_background" ) != 0.0f || gpGlobals->demoplayback )
		return false;

	int bgmapid = EngFuncs::RandomLong( 0, uiStatic.bgmaps.Count() - 1 );

	char cmd[128];
	snprintf( cmd, sizeof( cmd ), "maps/%s.bsp", uiStatic.bgmaps[bgmapid].Get( ));
	if( !EngFuncs::FileExists( cmd, true ))
		return false;

	snprintf( cmd, sizeof( cmd ), "map_background %s\n", uiStatic.bgmaps[bgmapid].Get( ));
	EngFuncs::ClientCmd( false, cmd );

	return true;
}

// =====================================================================

/*
=================
UI_CloseMenu
=================
*/
void UI_CloseMenu( void )
{
	uiStatic.menu.Clean();

//	EngFuncs::KEY_ClearStates ();
	if( !uiStatic.client.IsActive() )
		EngFuncs::KEY_SetDest( KEY_GAME );
}

void UI_CloseClientMenu( void )
{
	if( !uiStatic.client.IsActive() )
		return;

	// A7 build 73: runtime trace of the menu chain (test build)
	Con_Printf( "MenuTrace: UI_CloseClientMenu\n" );

	uiStatic.client.Clean();
	EngFuncs::KEY_ClearStates();
	EngFuncs::KEY_SetDest( KEY_GAME );
	EngFuncs::ClientCmd( false, "touch_setclientonly 0" );
}

// =====================================================================


static bool UI_UpdateWindowInsets( void )
{
	const float *insets = NULL;
	int left = 0, top = 0, right = 0, bottom = 0;

	if( EngFuncs::textfuncs.pfnGetNativeObject )
		insets = (const float *)EngFuncs::textfuncs.pfnGetNativeObject( "WindowInsets" );
	if( insets )
	{
		left = ceilf( ScreenWidth * insets[0] );
		top = ceilf( ScreenHeight * insets[1] );
		right = ceilf( ScreenWidth * insets[2] );
		bottom = ceilf( ScreenHeight * insets[3] );
	}
	bool changed = left != uiStatic.safeLeft || top != uiStatic.safeTop ||
		right != uiStatic.safeRight || bottom != uiStatic.safeBottom;
	uiStatic.safeLeft = left;
	uiStatic.safeTop = top;
	uiStatic.safeRight = right;
	uiStatic.safeBottom = bottom;
	return changed;
}

/*
=================
UI_UpdateMenu
=================
*/
void UI_UpdateMenu( float flTime )
{
	if( !uiStatic.initialized )
		return;

	if( UI_UpdateWindowInsets() )
		UI_VidInit();

	static bool loadStuff = true;

	// can't do this in Init, since these are dependent on cvar values
	// set from user configs
	if( loadStuff )
	{
		// load background bitmaps
		CMenuBackgroundBitmap::LoadBackground( );

		// load localized strings
		UI_LoadCustomStrings();

		// load scr
		UI_LoadScriptConfig();

		loadStuff = false;
	}

	UI_DrawFinalCredits ();

	// also moved opening main menu here from SetActiveMenu, so
	// translation strings could will be loaded at this moment
	if( uiStatic.nextFrameActive )
	{
		if( !uiStatic.menu.IsActive( ))
			UI_Main_Menu();

		uiStatic.nextFrameActive = false;
	}

	// advance global time
	uiStatic.realTime = flTime * 1000;

	// let's use engine credits "feature" for drawing client windows
	if( uiStatic.client.IsActive( ))
		uiStatic.client.Update();

	if( !uiStatic.menu.IsActive( ))
		return;

	if( !EngFuncs::ClientInGame() && EngFuncs::GetCvarFloat( "cl_background" ) != 0.0f )
		return;	// don't draw menu while level is loading

	if( uiStatic.firstDraw )
	{
		static bool first = true;

		if( first )
		{
			first = false;

			// we loading background so skip SCR_Update
			if( UI_StartBackGroundMap( ))
				return;

			// load background track
			if( !CL_IsActive( ))
				EngFuncs::PlayBackgroundTrack( "media/gamestartup", "media/gamestartup" );
		}

		uiStatic.firstDraw = false;
	}

	// draw cursor
	UI_DrawMouseCursor();

	// background doesn't need to be drawn with transparency
	bool enableAlphaFactor = uiStatic.enableAlphaFactor;
	uiStatic.enableAlphaFactor = false;
	uiStatic.background->Draw();
	uiStatic.enableAlphaFactor = enableAlphaFactor;

	uiStatic.menu.Update();
}

/*
=================
UI_KeyEvent
=================
*/
void UI_KeyEvent( int key, int down )
{
	bool clientActive, menuActive;

	if( !uiStatic.initialized )
		return;

	if( key == K_MOUSE1 )
	{
		g_bCursorDown = !!down;
	}

	clientActive = uiStatic.client.IsActive();
	menuActive = uiStatic.menu.IsActive();

	if( clientActive && !menuActive )
		down ? uiStatic.client.KeyDownEvent( key ) :
			uiStatic.client.KeyUpEvent( key );

	if( menuActive )
		down ? uiStatic.menu.KeyDownEvent( key ) :
			uiStatic.menu.KeyUpEvent( key );
}

/*
=================
UI_CharEvent
=================
*/
void UI_CharEvent( int key )
{
	if( !uiStatic.initialized )
		return;

	bool clientActive = uiStatic.client.IsActive();
	bool menuActive = uiStatic.menu.IsActive();

	if( clientActive && !menuActive )
		uiStatic.client.CharEvent( key );

	if( menuActive )
		uiStatic.menu.CharEvent( key );
}

bool g_bCursorDown;
float cursorDY;

/*
=================
UI_MouseMove
=================
*/
void UI_MouseMove( int x, int y )
{
	bool clientActive, menuActive;

	if( !uiStatic.initialized )
		return;

	clientActive = uiStatic.client.IsActive();
	menuActive = uiStatic.menu.IsActive();

	if( !clientActive && !menuActive )
		return;

	if( uiStatic.cursorX == x && uiStatic.cursorY == y )
		return;

	if( g_bCursorDown )
	{
		static bool prevDown = false;

		if( !prevDown )
		{
			prevDown = true;
			cursorDY = 0;
		}
		else if( y - uiStatic.cursorY )
		{
			cursorDY += y - uiStatic.cursorY;
		}
	}
	else
		cursorDY = 0;
	//Con_Printf("%d %d %f\n",x, y, cursorDY);

	// now menu uses absolute coordinates
	uiStatic.cursorX = x;
	uiStatic.cursorY = y;

	if( UI_CursorInRect( 1, 1, ScreenWidth - 1, ScreenHeight - 1 ))
		uiStatic.mouseInRect = true;
	else uiStatic.mouseInRect = false;

	uiStatic.cursorX = bound( 0, uiStatic.cursorX, ScreenWidth );
	uiStatic.cursorY = bound( 0, uiStatic.cursorY, ScreenHeight );

	if( clientActive && !menuActive )
		uiStatic.client.MouseEvent( x, y );

	if( menuActive )
		uiStatic.menu.MouseEvent( x, y );
}


/*
=================
UI_SetActiveMenu
=================
*/
void UI_SetActiveMenu( int fActive )
{
	if( !uiStatic.initialized )
		return;

	// don't continue firing if we leave game
	EngFuncs::KEY_ClearStates();

	if( fActive )
	{
		EngFuncs::KEY_SetDest( KEY_MENU );
		uiStatic.nextFrameActive = true; // main menu open moved to UI_UpdateMenu
	}
	else
	{
		UI_CloseMenu();
		uiStatic.nextFrameActive = false; // don't call main menu next frame
	}
}

/*
=================
UI_IsVisible

Some systems may need to know if it is visible or not
=================
*/
int UI_IsVisible( void )
{
	if( !uiStatic.initialized )
		return false;
	return uiStatic.menu.IsActive();
}

void UI_GetCursorPos( int *pos_x, int *pos_y )
{
	if( pos_x ) *pos_x = uiStatic.cursorX;
	if( pos_y ) *pos_y = uiStatic.cursorY;
}

// dead callback
void UI_SetCursorPos( int pos_x, int pos_y )
{
	(void)(pos_x);
	(void)(pos_y);
	uiStatic.mouseInRect = true;
}

void UI_ShowCursor( int show )
{
	uiStatic.hideCursor = (show) ? false : true;
}

int UI_MouseInRect( void )
{
	return uiStatic.mouseInRect;
}

/*
=================
UI_Precache
=================
*/
void UI_Precache( void )
{
	if( !uiStatic.initialized )
		return;

	EngFuncs::PIC_Load( UI_LEFTARROW );
	EngFuncs::PIC_Load( UI_LEFTARROWFOCUS );
	EngFuncs::PIC_Load( UI_RIGHTARROW );
	EngFuncs::PIC_Load( UI_RIGHTARROWFOCUS );
	EngFuncs::PIC_Load( UI_UPARROW );
	EngFuncs::PIC_Load( UI_UPARROWFOCUS );
	EngFuncs::PIC_Load( UI_DOWNARROW );
	EngFuncs::PIC_Load( UI_DOWNARROWFOCUS );
	EngFuncs::PIC_Load( "gfx/shell/splash" );

	// load all menu buttons
	uiStatic.btns.LoadBmpButtons();

	// all menu buttons have the same view sizes
	if( uiStatic.btns.GetWidth() == 0 || uiStatic.btns.GetHeight() == 0 )
		uiStatic.buttons_draw_size = Size( UI_BUTTONS_WIDTH, UI_BUTTONS_HEIGHT );
	else
		uiStatic.buttons_draw_size = Size( uiStatic.btns.GetWidth() * 1024 / 640, uiStatic.btns.GetHeight() * 768 / 480 );

	for( CMenuEntry *entry = s_pEntries; entry; entry = entry->m_pNext )
	{
		if( entry->m_pfnPrecache )
			entry->m_pfnPrecache();
	}
}

void UI_ParseColor( char *&pfile, unsigned int *outColor )
{
	int color[3] = { 0xFF, 0xFF, 0xFF };
	char token[1024];

	for( int i = 0; i < 3; i++ )
	{
		pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ));
		if( !pfile ) break;
		color[i] = atoi( token );
	}

	*outColor = PackRGB( color[0], color[1], color[2] );
}

void UI_ApplyCustomColors( void )
{
	char *afile = (char *)EngFuncs::COM_LoadFile( "gfx/shell/colors.lst" );
	char *pfile = afile;
	char token[1024];

	if( !afile )
	{
		// not error, not warning, just notify
		Con_Printf( "UI_ApplyCustomColors: colors.lst not found\n" );
		// A7 build 130 (round 131): no file on disk -> the palette is the
		// built-in one; say so once so an orange menu on the device can
		// never be blamed on an invisible colors.lst
		Con_Printf( "[A7] build130: colors not applied - colors.lst absent, "
			"built-in palette active\n" );
		return;
	}

	while(( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))) != NULL )
	{
		if( !stricmp( token, "HELP_COLOR" ))
		{
			UI_ParseColor( pfile, &uiColorHelp );
		}
		else if( !stricmp( token, "PROMPT_BG_COLOR" ))
		{
			UI_ParseColor( pfile, &uiPromptBgColor );
		}
		else if( !stricmp( token, "PROMPT_TEXT_COLOR" ))
		{
			UI_ParseColor( pfile, &uiPromptTextColor );
		}
		else if( !stricmp( token, "PROMPT_FOCUS_COLOR" ))
		{
			UI_ParseColor( pfile, &uiPromptFocusColor );
		}
		else if( !stricmp( token, "INPUT_TEXT_COLOR" ))
		{
			UI_ParseColor( pfile, &uiInputTextColor );
		}
		else if( !stricmp( token, "INPUT_BG_COLOR" ))
		{
			UI_ParseColor( pfile, &uiInputBgColor );
		}
		else if( !stricmp( token, "INPUT_FG_COLOR" ))
		{
			UI_ParseColor( pfile, &uiInputFgColor );
		}
		else if( !stricmp( token, "CON_TEXT_COLOR" ))
		{
			UI_ParseColor( pfile, &uiColorConsole );
		}
	}

	// A7 build 130 (round 131, user screenshots: the main-menu rows draw
	// ORANGE): this parse is the ONLY writer of the palette besides the
	// defaults at the top of this file - so a stale on-device
	// gfx/shell/colors.lst repaints the whole UI. Report the palette that
	// was actually applied; one line in engine.log answers "is the device
	// serving a stale colours file?". The bundle ships this file now
	// (stage/*/gfx/shell/colors.lst, always refreshed by the installers),
	// carrying exactly these default values.
	{
		int pr, pg, pb, fr, fg, fb, br, bg, bb;
		UnpackRGB( pr, pg, pb, uiPromptTextColor );
		UnpackRGB( fr, fg, fb, uiPromptFocusColor );
		UnpackRGB( br, bg, bb, uiPromptBgColor );
		Con_Printf( "[A7] build130: colors applied from gfx/shell/colors.lst: "
			"prompt %d %d %d, focus %d %d %d, bg %d %d %d\n",
			pr, pg, pb, fr, fg, fb, br, bg, bb );
	}

	int	r, g, b;

	UnpackRGB( r, g, b, uiColorConsole );
	EngFuncs::SetConsoleDefaultColor( r, g, b );

	EngFuncs::COM_FreeFile( afile );
}

static void UI_LoadBackgroundMapList( void )
{
	if( !EngFuncs::FileExists( "scripts/chapterbackgrounds.txt", true ))
		return;

	char *afile = (char *)EngFuncs::COM_LoadFile( "scripts/chapterbackgrounds.txt", NULL );
	char *pfile = afile;
	char token[1024];

	if( !afile )
	{
		Con_Printf( "UI_LoadBackgroundMapList: chapterbackgrounds.txt not found\n" );
		return;
	}

	while(( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))) != NULL )
	{
		// skip the numbers (old format list)
		if( isdigit( token[0] )) continue;

		uiStatic.bgmaps.AddToTail( token );
	}

	EngFuncs::COM_FreeFile( afile );
}

static void UI_LoadSoundsFrom( const char * const *sounds, char ( &dest )[SND_COUNT][40] )
{
	memset( dest, 0, sizeof( dest ) );

	if ( uiStatic.lowmemory )
		return;

	for ( int i = 0; i < SND_COUNT; i++ )
	{
		if ( !sounds[i] || *sounds[i] == '\0' )
			continue;

		if ( !EngFuncs::FileExists( sounds[i] ) )
		{
			size_t len = strlen( uiSoundOldPrefix );

			if ( !strncmp( sounds[i], uiSoundOldPrefix, len ) )
				snprintf( dest[i], sizeof( dest[i] ), "%s%s", uiSoundNewPrefix, sounds[i] + len );
		}
		else
			Q_strncpy( dest[i], sounds[i], sizeof( dest[i] ) );
	}
}

// A7 build 87: both sets are resolved once; window Show/Hide swaps the
// active one at runtime (see UI_UpdateSoundSet)
static char uiSndCacheGameUI[SND_COUNT][40];
static char uiSndCacheGold[SND_COUNT][40];

static void UI_LoadSounds( void )
{
	UI_LoadSoundsFrom( uiSounds, uiSndCacheGameUI );
	UI_LoadSoundsFrom( uiSoundsGoldSource, uiSndCacheGold );

	// the menu opens on the main menu, so start with the GameUI set
	memcpy( uiStatic.sounds, uiSndCacheGameUI, sizeof( uiStatic.sounds ));
}

// A7 build 87: runtime sound-set switch - the main menu keeps the 2003
// GameUI sounds, every other window uses the original GoldSource set
void UI_SetSoundSet( bool goldSource )
{
	memcpy( uiStatic.sounds,
		goldSource ? uiSndCacheGold : uiSndCacheGameUI,
		sizeof( uiStatic.sounds ));
}

// pick the set for whatever window now sits on top of the stacks: the
// main menu root is the only GameUI-sound window; dialogs on the menu
// stack and the in-game client windows (buy menu) get GoldSource
void UI_UpdateSoundSet( void )
{
	CMenuBaseWindow *top = uiStatic.client.Current();

	if( !top )
		top = uiStatic.menu.Current();

	UI_SetSoundSet( !top || !top->IsMainMenuRoot( ));
}

/*
=================
UI_VidInit
=================
*/


int UI_VidInit( void )
{
	static bool calledOnce = false;
	if( uiStatic.textInput )
	{
		uiStatic.menu.InputMethodResized();

		return 0;
	}

	if( !calledOnce )
	{
		UI_Precache();
	}

	UI_UpdateWindowInsets();
	float width = ScreenWidth - uiStatic.safeLeft - uiStatic.safeRight;
	float height = ScreenHeight - uiStatic.safeTop - uiStatic.safeBottom;

	if( width * 3 < height * 4 )
	{
		uiStatic.scaleX = uiStatic.scaleY = width / 1024.0f;
		uiStatic.yOffset = height / ( 2.0f * uiStatic.scaleY ) - 768.0f / 2.0f;
	}
	else
	{
		uiStatic.scaleX = uiStatic.scaleY = height / 768.0f;
		uiStatic.yOffset = 0;
	}

	// A7 build 60: scale floor vs the RAW screen. A floor computed from the
	// same inset-shrunk width/height as the branches above could never
	// trigger (both branches already yield exactly that fit) - dead code.
	// Broken Android inset reporting halves the usable height on some
	// devices, shrinking the WHOLE UI to about half size while framed
	// dialogs each compensated by their own factor (user report: "different
	// sizes", "disorganized"). Never let the scale fall below 85% of the
	// RAW-screen fit - one global correction keeps every menu, dialog and
	// font consistent. Healthy devices already sit at >= 0.85 * fit and are
	// untouched.
	{
		float fitRaw = Q_min( ScreenWidth / 1024.0f, ScreenHeight / 768.0f );

		if( fitRaw > 0.0f && uiStatic.scaleX < fitRaw * 0.85f )
		{
			uiStatic.scaleX = uiStatic.scaleY = fitRaw * 0.85f;

			// center the 1024x768 menu in the usable area again
			uiStatic.yOffset = height / ( 2.0f * uiStatic.scaleY ) - 768.0f / 2.0f;
		}
	}

	uiStatic.xOffset = uiStatic.safeLeft / uiStatic.scaleX;
	uiStatic.yOffset += uiStatic.safeTop / uiStatic.scaleY;
	uiStatic.width = width / uiStatic.scaleX;

	// A7 build 130 (round 131, user: menu rows look "cut at the left edge"):
	// one-shot geometry dump. The code guarantees xOffset >= 0 (it is the
	// safe-area inset divided by the scale), so either the device reports
	// insets that make it draw way off, or the screenshot crop lies - this
	// line settles it from a single engine.log.
	{
		static bool loggedLayout = false;
		if( !loggedLayout )
		{
			loggedLayout = true;
			Con_Printf( "[A7] build130: layout screen %dx%d safe L%d T%d R%d B%d "
				"scale %.4f x %.4f off %d,%d uiw %d\n",
				(int)ScreenWidth, (int)ScreenHeight,
				uiStatic.safeLeft, uiStatic.safeTop,
				uiStatic.safeRight, uiStatic.safeBottom,
				uiStatic.scaleX, uiStatic.scaleY,
				uiStatic.xOffset, uiStatic.yOffset,
				uiStatic.width );
		}
	}

	// move cursor to screen center
	uiStatic.cursorX = ScreenWidth / 2;
	uiStatic.cursorY = ScreenHeight / 2;
	uiStatic.outlineWidth = 4;

	UI_ScaleCoords( NULL, NULL, &uiStatic.outlineWidth, NULL );

	// trying to load chapterbackgrounds.txt
	UI_LoadBackgroundMapList ();

	// VidInit FontManager
	g_FontMgr->VidInit();

	// load button sounds
	UI_LoadSounds();

	// init global background, root windows don't have background anymore
	if( !uiStatic.background )
		uiStatic.background = new CMenuBackgroundBitmap();
	uiStatic.background->VidInit();

	uiStatic.menu.VidInit( calledOnce );

	if( !calledOnce ) calledOnce = true;

	return 1;
}

void UI_OpenUpdatePage( bool engine, bool preferstore )
{
	const char *updateUrl = NULL;

	if( engine || !gMenu.m_gameinfo.update_url[0] )
	{
		if( preferstore )
			updateUrl = PLATFORM_UPDATE_PAGE;
		else
			updateUrl = GENERIC_UPDATE_PAGE;
	}
	else
	{
		updateUrl = gMenu.m_gameinfo.update_url;
	}

	if( updateUrl )
		EngFuncs::ShellExecute( updateUrl, NULL, true );
}

void UI_UpdateDialog( int preferStore )
{
	static CMenuYesNoMessageBox msgBox;
	static bool ignore = false;
	static bool staticPreferStore;

	if( ignore )
		return;

	staticPreferStore = preferStore != 0;

	msgBox.SetMessage( "A new update is available.\nPress Update to open download page." );
	msgBox.SetPositiveButton( "Update", PC_UPDATE );
	msgBox.SetNegativeButton( "Later", PC_CANCEL );

	SET_EVENT( msgBox.onPositive, UI_OpenUpdatePage( true, *(bool*)pExtra ) );
	msgBox.onPositive.pExtra = &staticPreferStore;

	SET_EVENT( msgBox.onNegative, *(bool*)pExtra = true ); // set ignore
	msgBox.onNegative.pExtra = &ignore;

	msgBox.Show();

}

static void UI_UpdateDialog_f( void )
{
	if( !strcmp( EngFuncs::CmdArgv( 1 ), "nostore" ))
		UI_UpdateDialog( false );
	else
		UI_UpdateDialog( true );
}

ADD_COMMAND( menu_updatedialog, UI_UpdateDialog_f );

/*
=================
UI_Init
=================
*/
void UI_Init( void )
{
	// A7 build 112: menu anti-theft handshake (engine half in
	// cl_gameui.c UI_LoadProgs, which stamps ui_build_token right before
	// loading this library). A foreign client that ripped libmenu.so out
	// of our APK and loads it in its own engine never stamps the token,
	// so bail out BEFORE uiStatic.initialized is set - every exported
	// entry (UI_UpdateMenu, UI_KeyEvent, UI_SetActiveMenu, ...)
	// early-returns on that flag below, and a shake-attempt on a wrong
	// value falls through to the same refusal (user: "even if he used to
	// his own client won't show gameui goldsource").
	const char *token = EngFuncs::GetCvarString( "ui_build_token" );

	if( !token || !token[0] || stricmp( token, "M3nuS3al7xK9pQ2vT6wJ5rB8dH4fN1cZ" ))
	{
		Con_Printf( "menu: integrity token missing or wrong - GameUI refused to start\n" );
		return;
	}

	// register our cvars and commands
	ui_showmodels = EngFuncs::CvarRegister( "ui_showmodels", "0", FCVAR_ARCHIVE );
	ui_showclassmodels = EngFuncs::CvarRegister( "ui_showclassmodels", "1", FCVAR_ARCHIVE );
	ui_show_window_stack = EngFuncs::CvarRegister( "ui_show_window_stack", "0", FCVAR_ARCHIVE );
	ui_borderclip = EngFuncs::CvarRegister( "ui_borderclip", "0", FCVAR_ARCHIVE );
	ui_prefer_won_background = EngFuncs::CvarRegister( "ui_prefer_won_background", "0", FCVAR_ARCHIVE );
	ui_background_stretch = EngFuncs::CvarRegister( "ui_background_stretch", "0", FCVAR_ARCHIVE );
	ui_logohorizontal = EngFuncs::CvarRegister( "ui_logohorizontal", "0", FCVAR_ARCHIVE );

	// show cl_predict dialog
	EngFuncs::CvarRegister( "menu_mp_firsttime2", "1", FCVAR_ARCHIVE );

	// autofill ammo after bought weapon
	EngFuncs::CvarRegister( "ui_cs_autofill", "0", FCVAR_ARCHIVE );

	for( CMenuEntry *entry = s_pEntries; entry; entry = entry->m_pNext )
	{
		if( entry->m_szCommand && entry->m_pfnShow )
		{
			EngFuncs::Cmd_AddCommand( entry->m_szCommand, entry->m_pfnShow );
		}
	}

	g_FontMgr = new CFontManager();

	uiStatic.initialized = true;
	uiStatic.lowmemory = (int)EngFuncs::GetCvarFloat( "host_lowmemorymode" );

	// setup game info
	gameinfo2_t *gi = EngFuncs::GetGameInfo();
	if( !gi )
		Host_Error( "pfnGetGameInfo returned NULL!\n" );

	gMenu.m_gameinfo = *gi;

	// A7-full-port: original CS 1.6 GameUI renders menu buttons as text, not
	// the stock xash/WON picture atlas (gfx/shell/btns_main.bmp - the orange
	// bubble letters). Force text rendering regardless of the gameinfo flag.
	uiStatic.renderPicbuttonText = true;

	// trying to load colors.lst
	UI_ApplyCustomColors ();
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown( void )
{
	if( !uiStatic.initialized )
		return;

	for( CMenuEntry *entry = s_pEntries; entry; entry = entry->m_pNext )
	{
		if( entry->m_szCommand && entry->m_pfnShow )
		{
			EngFuncs::Cmd_RemoveCommand( entry->m_szCommand );
		}

		if( entry->m_pfnShutdown )
		{
			entry->m_pfnShutdown();
		}
	}

	UI_FreeCustomStrings();

	delete uiStatic.background;
	delete g_FontMgr;

	memset( &uiStatic, 0, sizeof( uiStatic_t ));
}
