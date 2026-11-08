/*
FontManager.cpp - font manager
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
#include <locale.h>
#include <math.h>
#include "FontManager.h"
#include "BaseMenu.h"
#include "Utils.h"

#include "BaseFontBackend.h"

#if defined(MAINUI_USE_FREETYPE)
#include "FreeTypeFont.h"
#elif defined(MAINUI_USE_STB)
#include "StbFont.h"
#elif defined(_WIN32)
#include "WinAPIFont.h"
#endif

#include "BitmapFont.h"
#include "utflib.h"

// A7: Tahoma matches the main menu / console fonts and the original CS 1.6
// GameUI look; Trebuchet was the stock xash3d menu font
#define DEFAULT_MENUFONT "Tahoma"
#define DEFAULT_CONFONT  "Tahoma"
#define DEFAULT_WEIGHT   500

CFontManager *g_FontMgr;

CFontManager::CFontManager()
{
#ifdef MAINUI_USE_FREETYPE
	FT_Init_FreeType( &CFreeTypeFont::m_Library );
#endif
	m_Fonts.EnsureCapacity( 4 );
	m_FontFiles.EnsureCapacity( 2 );
}

CFontManager::~CFontManager()
{
	DeleteAllFonts();
#ifdef MAINUI_USE_FREETYPE
	FT_Done_FreeType( CFreeTypeFont::m_Library );
	CFreeTypeFont::m_Library = NULL;
#endif

	FOR_EACH_HASHMAP( m_FontFiles, i )
	{
		byte *p = m_FontFiles.Element( i ).data;
		EngFuncs::COM_FreeFile( p );
	}

	m_FontFiles.Purge();
}

// build 90: design height -> DEVICE (raster) height. Bitmap/GDI/FreeType
// glyphs are 1:1 pixels, so the raster must equal the line the control lays
// its label out with (m_scChSize = charSize * uiStatic.scaleY, truncating):
// any other ratio makes every glyph resample at draw time, which is what
// turns text soft once uiScale > 1. Same rounding on both sides keeps the
// raster height, the line height and the ellipsis/width metrics identical.
static int UI_DeviceFontTall( int tall )
{
	if( tall <= 0 || !( uiStatic.scaleY > 0.0f ))
		return tall;

	return (int)((float)tall * uiStatic.scaleY);
}

void CFontManager::VidInit( void )
{
	static float prevScale = 0;

	float scale = uiStatic.scaleY;

	// build 90: rebuild on ANY scale change. The old 0.1 delta (and, on
	// SCALE_FONTS builds, "rebuild once ever") kept rasters from a previous
	// resolution: drawing them against the new m_scChSize upscales stale
	// glyphs - the blurry-font complaint after a video mode change. The
	// heights below stay DESIGN units; CFontBuilder::Create bakes the scale.
	if( !prevScale || fabs( scale - prevScale ) > 0.01f )
	{
		// A7 build 145 (round 145 item C, user: "after changing screen
		// resolution on PC the GameUI becomes invisible"): DeleteAllFonts()
		// dropped EVERY font, but only the standard set below was built
		// again. The ad-hoc fonts menus and the client dll make through
		// CFontBuilder (the main menu rows' SetCustomTextStyle, the
		// BuildFont API) kept integer handles into a list that no longer
		// had those slots - GetIFontFromHandle returned NULL, so every
		// control holding one drew NOTHING after a video mode change while
		// a fresh boot (fonts created after VidInit's standard set) drew
		// fine. Snapshot everything after the standard set and rebuild it
		// in the same order on top: the standard fonts land on their
		// forced handles again and the ad-hoc ones get the exact handles
		// they had before, so every cached HFont in the menus and in the
		// client dll stays valid across the scale change.
		struct adhoc_font_t
		{
			char   name[32];
			int    designTall, weight, flags;
			int    blur, outline, scanOffset;
			float  brighten, scanScale;
		};
		CUtlVector<adhoc_font_t> adhoc;
		// the standard set this block is about to build, in order: the six
		// forced handles + the two blur faces (unless lowmemory) + console
		const int standardCount = 6 + ( uiStatic.lowmemory ? 0 : 2 ) + 1;

		for( int i = standardCount; i < m_Fonts.Count(); i++ )
		{
			CBaseFont *font = m_Fonts[i];

			if( !font ) continue; // no DeleteFont callers today; belt only

			adhoc_font_t f;
			Q_strncpy( f.name, font->GetName(), sizeof( f.name ));
			// a font built without a recorded design height falls back to
			// its device height: keeps the handle alive (best effort)
			f.designTall = font->m_iDesignTall > 0 ? font->m_iDesignTall : font->GetRequestTall();
			f.weight     = font->GetWeight();
			f.flags      = font->GetFlags();
			f.blur       = font->m_iBlur;
			f.brighten   = font->m_fBrighten;
			f.outline    = font->m_iOutlineSize;
			f.scanOffset = font->m_iScanlineOffset;
			f.scanScale  = font->m_fScanlineScale;
			adhoc.AddToTail( f );
		}

		DeleteAllFonts();

		uiStatic.hDefaultFont = CFontBuilder( DEFAULT_MENUFONT, UI_MED_CHAR_HEIGHT, DEFAULT_WEIGHT )
			.SetHandleNum( QM_DEFAULTFONT )
			.Create();
		uiStatic.hSmallFont   = CFontBuilder( DEFAULT_MENUFONT, UI_SMALL_CHAR_HEIGHT, DEFAULT_WEIGHT )
			.SetHandleNum( QM_SMALLFONT )
			.Create();
		uiStatic.hSmallerFont = CFontBuilder( DEFAULT_MENUFONT, UI_SMALLER_CHAR_HEIGHT, DEFAULT_WEIGHT )
			.SetHandleNum( QM_SMALLERFONT )
			.Create();
		uiStatic.hTinyFont    = CFontBuilder( DEFAULT_MENUFONT, UI_TINY_CHAR_HEIGHT, DEFAULT_WEIGHT )
			.SetHandleNum( QM_TINYFONT )
			.Create();
		uiStatic.hBigFont     = CFontBuilder( DEFAULT_MENUFONT, UI_BIG_CHAR_HEIGHT, DEFAULT_WEIGHT )
			.SetHandleNum( QM_BIGFONT )
			.Create();
		uiStatic.hBoldFont = CFontBuilder( DEFAULT_MENUFONT, UI_MED_CHAR_HEIGHT, 1000 )
			.SetHandleNum( QM_BOLDFONT )
			.Create();

		if( !uiStatic.lowmemory )
		{
			uiStatic.hLightBlur = CFontBuilder( DEFAULT_MENUFONT, UI_MED_CHAR_HEIGHT, DEFAULT_WEIGHT )
				.SetBlurParams( 2 * scale, 1.25f )
				.Create();

			uiStatic.hHeavyBlur = CFontBuilder( DEFAULT_MENUFONT, UI_MED_CHAR_HEIGHT, DEFAULT_WEIGHT )
				.SetBlurParams( 8 * scale, 2.0f )
				.Create();
		}

		uiStatic.hConsoleFont = CFontBuilder( DEFAULT_CONFONT, UI_CONSOLE_CHAR_HEIGHT, 500 )
			.SetOutlineSize()
			.Create();

		// build 145: re-make the ad-hoc fonts on top, in their original
		// order - each CFontBuilder here appends, so they land on the very
		// handles they occupied before DeleteAllFonts (see snapshot above)
		for( int i = 0; i < adhoc.Count(); i++ )
		{
			const adhoc_font_t &f = adhoc[i];

			CFontBuilder( f.name, f.designTall, f.weight )
				.SetBlurParams( f.blur, f.brighten )
				.SetOutlineSize( f.outline )
				.SetScanlineParams( f.scanOffset, f.scanScale )
				.SetFlags( f.flags )
				.Create();
		}

		prevScale = scale;
	}
}

void CFontManager::DeleteAllFonts()
{
	for( int i = 0; i < m_Fonts.Count(); i++ )
	{
		delete m_Fonts[i];
	}
	m_Fonts.RemoveAll();
}

void CFontManager::DeleteFont(HFont hFont)
{
	CBaseFont *font = GetIFontFromHandle(hFont);
	if( font )
	{
		m_Fonts[hFont-1] = NULL;

		delete font;
	}
}

CBaseFont *CFontManager::GetIFontFromHandle(HFont font)
{
	if( m_Fonts.IsValidIndex( font - 1 ) )
		return m_Fonts[font-1];

	return NULL;
}

int CFontManager::GetEllipsisWide(HFont font)
{
	if( m_Fonts.IsValidIndex( font - 1 ) )
		return m_Fonts[font-1]->GetEllipsisWide();
	return 0;
}

int CFontManager::GetEllipsisWideScaled(HFont font, int charH)
{
	int wide = GetEllipsisWide( font );

#ifdef SCALE_FONTS
	// build 90: the cached "..." width is native raster pixels; convert it the
	// same way GetCharacterWidthScaled converts a glyph row, so ellipsis
	// accounting and the drawn dots land on the same columns
	int tall = GetFontRequestTall( font );
	if( charH > 0 && tall > 0 )
		wide = (int)((float)wide * ((float)charH / (float)tall) + 0.5f );
#endif

	return wide;
}

void CFontManager::GetCharABCWide(HFont font, int ch, int &a, int &b, int &c)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		pFont->GetCharABCWidths( ch, a, b, c );
	else
		a = b = c = 0;
}

int CFontManager::GetCharacterWidth(HFont font, int ch)
{
	int a, b, c;
	GetCharABCWide( font, ch, a, b, c );
	return a + b + c;
}

int CFontManager::GetFontRequestTall(HFont font)
{
	CBaseFont *pFont = GetIFontFromHandle( font );

	if( pFont && pFont->GetRequestTall() > 0 )
		return pFont->GetRequestTall();

	// build 90: font was built outside CFontBuilder - its own line box is
	// the best we have
	return GetFontTall( font );
}

int CFontManager::GetCharacterWidthScaled(HFont font, int ch, int height)
{
	int wide = GetCharacterWidth( font, ch );

#ifdef SCALE_FONTS
	// build 90: match the draw-scale factor against the requested (device)
	// height, so width metrics and CBaseFont::DrawCharacter agree pixel for
	// pixel with the line the caller is laying out
	int tall = GetFontRequestTall( font );
	if( tall > 0 && height > 0 )
		wide = wide * ((float)height / (float)tall);
#endif

	return wide;
}

HFont CFontManager::GetFontByName(const char *name)
{
	for( int i = 0; i < m_Fonts.Count(); i++ )
	{
		if( !stricmp( name, m_Fonts[i]->GetName() ) )
			return i;
	}
	return -1;
}

int CFontManager::GetFontTall(HFont font)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		return pFont->GetTall();
	return 0;
}

int CFontManager::GetFontAscent(HFont font)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		return pFont->GetAscent();
	return 0;
}

bool CFontManager::GetFontUnderlined(HFont font)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		return pFont->GetFlags() & FONT_UNDERLINE;
	return false;
}

void CFontManager::GetTextSize(HFont fontHandle, const char *text, int *wide, int *tall, int size )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );

	if( !font || !text || !text[0] )
	{
		if( wide ) *wide = 0;
		if( tall ) *tall = 0;
		return;
	}

	int fontTall = font->GetHeight(), x = 0;
	int _wide = 0, _tall;
	const char *ch = text;
	_tall = fontTall;
	int i = 0;
	utfstate_t state;

	while( *ch && ( size < 0 || i < size ) )
	{
		// Skip colorcodes
		if( IsColorString( ch ) )
		{
			ch += 2;
			continue;
		}

		int uch = state.Decode((uint8_t)*ch );

		if( uch )
		{
			if( uch == '\n' && *( ch + 1 ) != '\0' )
			{
				_tall += fontTall;
				x = 0;
			}
			else
			{
				int a, b, c;
				font->GetCharABCWidths( uch, a, b, c );
				x += a + b + c;
				if( x > _wide )
					_wide = x;
			}
		}
		i++;
		ch++;
	}

	if( tall ) *tall = _tall;
	if( wide ) *wide = _wide;
}

int CFontManager::CutText(HFont fontHandle, const char *text, int height, int visibleSize, bool reverse, bool stopAtWhitespace, int *wide, bool *remaining )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );

	if( remaining )
		*remaining = false;

	if( !font || !text || !text[0] || visibleSize <= 0 )
		return 0;

	int _wide = 0;
	const char *ch = text;
	utfstate_t state;

#ifdef SCALE_FONTS
	// build 90: same device-height basis as GetCharacterWidthScaled, so wrapping
	// cuts where the scaled glyph row actually ends up on screen
	visibleSize  = (float)visibleSize / (float)height * (float)GetFontRequestTall( fontHandle );
#endif

	int whiteSpacePos = 0;

	// calculate full text wide
	while( *ch )
	{
		// skip colorcodes
		if( IsColorString( ch ) )
		{
			ch += 2;
			continue;
		}

		int uch = state.Decode((uint8_t)*ch );
		int x = 0;
		if( uch )
		{
			if( uch == '\n' )
			{
				ch++;
				break; //
			}

			int a, b, c;
			font->GetCharABCWidths( uch, a, b, c );
			x = a + b + c;

			if( uch == ' ' )
			{
				whiteSpacePos = ch - text;
			}
		}

		if( !reverse && _wide + x >= visibleSize )
			break;

		ch++;
		_wide += x;
	}

	if( !reverse )
	{
		if( *ch && remaining ) *remaining = true;
		if( wide ) *wide = _wide;
		if( stopAtWhitespace && whiteSpacePos )
			return whiteSpacePos;
		return ch - text;
	}

	if( _wide < visibleSize )
	{
		if( remaining ) *remaining = false;
		if( wide ) *wide = _wide;
		return 0;
	}

	ch = text;

	whiteSpacePos = 0;

	// now remove character one by one to fit
	state.Reset();
	while( *ch && _wide > visibleSize )
	{
		// skip colorcodes
		if( IsColorString( ch ) )
		{
			ch += 2;
			continue;
		}

		int uch = state.Decode((uint8_t)*ch );
		if( uch )
		{
			// we don't need check for newlines here, it's only done for oneline Field widget
			int a, b, c;
			font->GetCharABCWidths( uch, a, b, c );
			_wide -= a + b + c;

			if( uch == ' ' )
			{
				whiteSpacePos = ch - text;
			}
		}
		ch++;
	}

	if( remaining ) *remaining = true;
	if( wide ) *wide = _wide;
	if( stopAtWhitespace && whiteSpacePos ) return whiteSpacePos;
	return ch - text;

}

int CFontManager::GetTextWide(HFont font, const char *text, int size)
{
	int wide;

	GetTextSize( font, text, &wide, NULL, size );

	return wide;
}

int CFontManager::GetTextHeight(HFont fontHandle, const char *text, int size )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );
	if( !font || !text || !text[0] )
	{
		return 0;
	}

	int height = font->GetHeight();

	// lightweight variant only for getting text height
	int i = 0;
	while( *text&&( size < 0 || i < size ) )
	{
		if( *text == '\n' )
			height += height;

		text++;
		i++;
	}
	return height;
}

int CFontManager::GetTextHeightExt( HFont fontHandle, const char *text, int height, int w, int size )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );
	if( !font || !text || !text[0] || !w )
	{
		return 0;
	}

	const char *text2 = text;
	int y = 0;

	while( *text2 && ( size < 0 || text2 - text < size ) )
	{
		int pos = CutText( fontHandle, text2, height, w, false, true );
		if( pos == 0 )
			break;

		y += height;
		text2 += pos;
	}

	return y;

}

int CFontManager::GetTextWideScaled(HFont font, const char *text, const int height, int size)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
	{
		return GetTextWide( font, text, size )
#ifdef SCALE_FONTS
			// build 90: keep width metrics on the requested device height
			* ((float)height / (float)GetFontRequestTall( font ))
#endif
		;
	}

	return 0;
}

void CFontManager::UploadTextureForFont(CBaseFont *font)
{
	// upload only latin needed for english and cyrillic needed for russian
	// maybe it would be extended someday...

	charRange_t range[] =
	{
	{ 0x0021, 0x007E, NULL, 0 }, // ascii printable range
	{ 0x00C0, 0x00FF, NULL, 0 }, // latin-1 supplement (letters only)
	{ 0x0100, 0x017F, NULL, 0 }, // latin extended a
	{ 0x0400, 0x045F, NULL, 0 }, // cyrillic range
	{ 0, 0, table_cp1251, V_ARRAYSIZE( table_cp1251 ) }, // cp1251
	};

	font->UploadGlyphsForRanges( range, V_ARRAYSIZE( range ) );
}

int CFontManager::DrawCharacter(HFont fontHandle, int ch, Point pt, int charH, const unsigned int color, bool forceAdditive )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );

	if( !font )
		return 0;

	return font->DrawCharacter( ch, pt, charH, color, forceAdditive );
}

void CFontManager::DebugDraw(HFont fontHandle)
{
	CBaseFont *font = GetIFontFromHandle(fontHandle);

	font->DebugDraw();
}


HFont CFontBuilder::Create()
{
	CBaseFont *font;

	// A7 build 145: keep the DESIGN height before the device scale is baked
	// in below - VidInit's scale-change rebuild needs it to re-raster this
	// font at the new ui scale instead of orphaning its handle
	int designTall = m_iTall;

	// build 90: bake the current UI scale into the raster height ONCE, for
	// every builder (menus/Main.cpp data-driven labels create ad-hoc
	// CFontBuilder("Tahoma", 26, 600) fonts that never saw uiStatic.scaleY).
	// A font is then always rasterized and drawn at its DEVICE height -
	// never a fixed-height bitmap stretched over a scaled label line.
	m_iTall = UI_DeviceFontTall( m_iTall );

	// check existing font at first
	if( !m_hForceHandle )
	{
		for( int i = 0; i < g_FontMgr->m_Fonts.Count(); i++ )
		{
			font = g_FontMgr->m_Fonts[i];

			// build 90: IsEqualTo compares m_iRequestTall, i.e. this scaled
			// height - two ui scales can never share an atlas entry
			if( font->IsEqualTo( m_szName, m_iTall, m_iWeight, m_iBlur, m_iFlags ) )
				return i + 1;
		}
	}

#if defined(MAINUI_USE_FREETYPE)
	font = new CFreeTypeFont();
#elif defined(MAINUI_USE_STB)
	font = new CStbFont();
#elif defined(_WIN32) && defined(MAINUI_USE_CUSTOM_FONT_RENDER)
	font = new CWinAPIFont();
#else
	font = new CBitmapFont();
#endif

	double starttime = EngFuncs::DoubleTime();

	if( !font->Create( m_szName, m_iTall, m_iWeight, m_iBlur, m_fBrighten, m_iOutlineSize, m_iScanlineOffset, m_fScanlineScale, m_iFlags ) )
	{
		delete font;

		// fallback to bitmap font
		font = new CBitmapFont();

		// should never fail
		if( !font->Create( "Bitmap Font", m_iTall, m_iWeight, m_iBlur, m_fBrighten, m_iOutlineSize, m_iScanlineOffset, m_fScanlineScale, m_iFlags ) )
		{
			delete font;
			return -1;
		}
	}

	// build 90: record the requested device height as the font's identity;
	// backends may pad m_iTall for their own line box (WinAPI adds leading)
	font->m_iRequestTall = m_iTall;
	// build 145: and the design height for the VidInit rebuild (see above)
	font->m_iDesignTall = designTall;

	g_FontMgr->UploadTextureForFont( font );

	double endtime = EngFuncs::DoubleTime();

	// A7 build 104: one visible line per created font saying which backend
	// really renders the menu - "Bitmap Font" means the old stretched
	// #XASH_SYSTEMFONT sheet (the gappy letters from the user's screenshot),
	// anything else is the crisp Tahoma raster. Printed on success only,
	// so a missing line = Create() failed before this point.
	Con_Printf( "[A7] menu font: %s @ %d -> %s backend\n", m_szName, m_iTall, font->GetName());

	Con_DPrintf( "Rendering %s(%i, %i) took %f seconds\n", font->GetName(), m_iTall, m_iWeight, endtime - starttime );

	if( m_hForceHandle != -1 && g_FontMgr->m_Fonts.Count() != m_hForceHandle )
	{
		if( g_FontMgr->m_Fonts.IsValidIndex( m_hForceHandle ) )
		{
			g_FontMgr->m_Fonts.FastRemove( m_hForceHandle );
			return g_FontMgr->m_Fonts.InsertBefore( m_hForceHandle, font );
		}
	}

	return g_FontMgr->m_Fonts.AddToTail(font) + 1;
}

bool CFontManager::FindFontDataFile( const char *name, int tall, int weight, int flags, char *dataFile, size_t dataFileChars )
{
	// build 90: case-insensitive match - any caller spelling ("tahoma",
	// "Tahoma", ...) must still find the shipped ttf, otherwise that font
	// silently drops to the Bitmap fallback and the whole menu goes chunky
	if( !stricmp( name, "Trebuchet MS" ))
	{
		Q_strncpy( dataFile, "gfx/fonts/FiraSans-Regular.ttf", dataFileChars );
		return true;
	}
	else if( !stricmp( name, "Tahoma" ))
	{
		Q_strncpy( dataFile, "gfx/fonts/tahoma.ttf", dataFileChars );
		return true;
	}

	return false;
}

byte *CFontManager::LoadFontDataFile( const char *vfspath, int *plen )
{
	int i = m_FontFiles.Find( vfspath );
	if( i != m_FontFiles.InvalidIndex( ))
	{
		if( plen )
			*plen = m_FontFiles[i].length;

		return m_FontFiles[i].data;
	}
	int len = 0;
	byte *p = EngFuncs::COM_LoadFile( vfspath, &len );
	if( p != nullptr )
	{
		if( plen )
			*plen = len;

		font_file file = { len, p };
		m_FontFiles.Insert( vfspath, file );
	}

	return p;
}
