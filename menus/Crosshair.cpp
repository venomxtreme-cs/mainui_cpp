/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2025 APAmk2, Vladislav4KZ, Velaron

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

#include "Framework.h"
#include "PicButton.h"
#include "Slider.h"
#include "CheckBox.h"
#include "SpinControl.h"
#include "StringArrayModel.h"

#define ART_BANNER "gfx/shell/head_xhair"

static const char *g_szCrosshairSizes[] = { "auto", "small", "medium", "large" };

static struct
{
	const char *name;
	int r, g, b;
} g_CrosshairColors[] = {
	{ "#Valve_Green", 50, 250, 50 },
	{ "#Valve_Red", 250, 50, 50 },
	{ "#Valve_Blue", 50, 50, 250 },
	{ "#Valve_Yellow", 250, 250, 50 },
	{ "#Valve_Ltblue", 50, 250, 250 }
};

class CMenuClassicCrosshair : public CMenuItemsHolder
{
public:
	void Save();

private:
	void _Init() override;
	void Reload() override;

	// A7 build 143 (round 143 item 3): live write when the size spin moves
	void SizeChanged();

	CMenuSpinControl size;
	CMenuSpinControl color;
	CMenuCheckBox translucent;

	class CMenuCrosshairPreview : public CMenuBaseItem
	{
	public:
		void Draw() override;

		HIMAGE hImage;
		HIMAGE hWhite;
	} preview;
};

class CMenuXhair : public CMenuItemsHolder
{
public:
	void Save();

private:
	void _Init() override;
	void Reload() override;

	CMenuCheckBox additive;

	CMenuSlider r;
	CMenuSlider g;
	CMenuSlider b;
	CMenuSlider a;

	CMenuCheckBox dot;
	CMenuCheckBox dynamicMove;
	CMenuSlider dynamicScale;
	CMenuCheckBox gapUseWeaponValue;
	CMenuSlider gap;
	CMenuSlider pad;
	CMenuSlider size;
	CMenuCheckBox xhairT;
	CMenuSlider thick;
	// A7 round 147 item 7: xash's cl_crosshair_type (cross / T / circle /
	// dot) was console-only - expose it next to the other shape controls
	CMenuSpinControl shape;

	class CMenuCrosshairPreview : public CMenuBaseItem
	{
	public:
		void Draw() override;
		void DrawCrosshairPadding( int _pad, int _x0, int _y0, int _x1, int _y1 );
		void DrawCrosshairSection( int _x0, int _y0, int _x1, int _y1 );
		int ScaleForRes( float value, int height );

		HIMAGE hImage;
		HIMAGE hWhite;
	} preview;
};

class CMenuCrosshair : public CMenuFramework
{
public:
	CMenuCrosshair() :
	    CMenuFramework( "CMenuCrosshair" ) { }

	void SaveAndPopMenu() override;
	void ToggleMenu();

	CMenuCheckBox useXhair;
	CMenuClassicCrosshair crosshair;
	CMenuXhair xhair;
	CMenuPicButton cancel;
	CMenuPicButton done;

private:
	void _Init() override;
};

void CMenuCrosshair::SaveAndPopMenu()
{
	useXhair.WriteCvar();

	xhair.Save();
	crosshair.Save();

	CMenuFramework::SaveAndPopMenu();
}

void CMenuCrosshair::ToggleMenu()
{
	if ( useXhair.bChecked )
	{
		xhair.Show();
		crosshair.Hide();
	}
	else
	{
		xhair.Hide();
		crosshair.Show();
	}
}

void CMenuCrosshair::_Init()
{
	banner.SetPicture( ART_BANNER );

	useXhair.SetNameAndStatus( L( "Enhanced crosshair" ), L( "Enables enhanced crosshair" ) );
	useXhair.LinkCvar( "xhair_enable" );
	useXhair.SetCoord( 72, 230 );
	useXhair.onChanged = VoidCb( &CMenuCrosshair::ToggleMenu );

	crosshair.SetRect( 72, 350, 880, 466 );
	xhair.SetRect( 72, 350, 880, 466 );

	cancel.szName = L( "GameUI_Cancel" );
	cancel.SetCoord( 72, 280 );
	cancel.SetPicture( PC_CANCEL );
	cancel.onReleased = VoidCb( &CMenuCrosshair::Hide );

	done.szName = L( "GameUI_OK" );
	done.SetCoord( 222, 280 );
	done.SetPicture( PC_DONE );
	done.onReleased = VoidCb( &CMenuCrosshair::SaveAndPopMenu );

	AddItem( banner );
	AddItem( useXhair );
	AddItem( crosshair );
	AddItem( xhair );
	AddItem( cancel );
	AddItem( done );

	ToggleMenu();
}

void CMenuClassicCrosshair::_Init()
{
	int x = 0, y = 0;

	static const char *itemlist[V_ARRAYSIZE( g_CrosshairColors )];
	static CStringArrayModel colors( itemlist, V_ARRAYSIZE( g_CrosshairColors ) );
	for ( size_t i = 0; i < V_ARRAYSIZE( g_CrosshairColors ); i++ )
		itemlist[i] = L( g_CrosshairColors[i].name );

	static const char *sizelist[] = { L( "GameUI_Auto" ), L( "GameUI_Small" ), L( "GameUI_Medium" ), L( "GameUI_Large" ) };
	static CStringArrayModel sizes( sizelist, V_ARRAYSIZE( sizelist ) );

	size.Setup( &sizes );
	// A7 build 143 (round 143 item 3, user: "crosshair size ... sometime
	// it didn't move"): CVAR_VALUE read the cvar as a FLOAT, so "medium"
	// parsed to 0 and the spin snapped back to Auto on every open; string
	// link + write at spin time (CHudAmmo::CalculateCrosshairSize re-reads
	// cl_crosshair_size every frame, so the crosshair moves live)
	size.LinkCvar( "cl_crosshair_size", CMenuEditable::CVAR_STRING );
	size.onChanged = VoidCb( &CMenuClassicCrosshair::SizeChanged );
	size.SetRect( x, y, 220, 32 );
	y += 60;

	color.Setup( &colors );
	color.LinkCvar( "cl_crosshair_color", CMenuEditable::CVAR_STRING );
	color.SetRect( x, y, 220, 32 );
	y += 60;

	translucent.SetNameAndStatus( L( "GameUI_Translucent" ), NULL );
	translucent.LinkCvar( "cl_crosshair_translucent" );
	translucent.SetCoord( x, y );
	y = 0;
	x = 584;

	preview.szName = L( "GameUI_CrosshairDescription" );
	preview.SetRect( x, y, 200, 200 );
	preview.hImage = EngFuncs::PIC_Load( "gfx/vgui/crosshair.tga", 0 );
	preview.hWhite = EngFuncs::PIC_Load( "*white" );

	AddItem( preview );
	AddItem( size );
	AddItem( color );
	AddItem( translucent );
}

void CMenuXhair::_Init()
{
	int x = 0, y = 0;

	r.SetNameAndStatus( L( "Red" ), NULL );
	r.Setup( 0, 255, 1 );
	r.SetCoord( x, y );
	y += 60;

	g.SetNameAndStatus( L( "Green" ), NULL );
	g.Setup( 0, 255, 1 );
	g.SetCoord( x, y );
	y += 60;

	b.SetNameAndStatus( L( "Blue" ), NULL );
	b.Setup( 0, 255, 1 );
	b.SetCoord( x, y );
	y += 60;

	a.SetNameAndStatus( L( "Alpha" ), NULL );
	a.Setup( 0, 255, 1 );
	a.SetCoord( x, y );
	y += 60;

	dynamicMove.SetNameAndStatus( L( "Dynamic move" ), NULL );
	dynamicMove.LinkCvar( "xhair_dynamic_move" );
	dynamicMove.SetCoord( x, y );
	y += 60;

	gapUseWeaponValue.SetNameAndStatus( L( "Gap, use weapon value" ), NULL );
	gapUseWeaponValue.LinkCvar( "xhair_gap_useweaponvalue" );
	gapUseWeaponValue.SetCoord( x, y );
	y += 60;

	additive.SetNameAndStatus( L( "Additive" ), NULL );
	additive.LinkCvar( "xhair_additive" );
	additive.SetCoord( x, y );
	y = 0;
	x += 292;

	gap.SetNameAndStatus( L( "Gap" ), NULL );
	gap.Setup( -6, 15, 1 );
	gap.LinkCvar( "xhair_gap" );
	gap.SetCoord( x, y );
	y += 60;

	pad.SetNameAndStatus( L( "Padding" ), NULL );
	pad.Setup( 0, 16, 1 );
	pad.LinkCvar( "xhair_pad" );
	pad.SetCoord( x, y );
	y += 60;

	size.SetNameAndStatus( L( "Size" ), NULL );
	size.Setup( 0, 32, 1 );
	size.LinkCvar( "xhair_size" );
	size.SetCoord( x, y );
	y += 60;

	thick.SetNameAndStatus( L( "Thickness" ), NULL );
	thick.Setup( 0, 32, 1 );
	thick.LinkCvar( "xhair_thick" );
	thick.SetCoord( x, y );
	y += 60;

	dynamicScale.SetNameAndStatus( L( "Dynamic scale" ), NULL );
	dynamicScale.Setup( 0, 3.0f, 0.1f );
	dynamicScale.LinkCvar( "xhair_dynamic_scale" );
	dynamicScale.SetCoord( x, y );
	y += 60;

	dot.SetNameAndStatus( L( "Dot" ), NULL );
	dot.LinkCvar( "xhair_dot" );
	dot.SetCoord( x, y );
	y += 60;

	xhairT.SetNameAndStatus( L( "T-Shape" ), NULL );
	xhairT.LinkCvar( "xhair_t" );
	xhairT.SetCoord( x, y );
	y += 60;

	// A7 round 147 item 7 (user: "advanced custom crosshair from xash"):
	// the engine-side shape cvar the vector HUD reads every frame -
	// 0 = classic cross, 1 = T, 2 = circle, 3 = dot
	{
		static const char *shapelist[] = { L( "Cross" ), L( "T shape" ), L( "Circle" ), L( "Dot" ) };
		static CStringArrayModel shapes( shapelist, V_ARRAYSIZE( shapelist ) );

		shape.SetNameAndStatus( L( "Shape" ), L( "Crosshair shape: cross, T, circle or dot" ) );
		shape.Setup( &shapes );
		shape.LinkCvar( "cl_crosshair_type" );
		shape.SetRect( x, y, 220, 32 );
	}
	y = 0;
	x += 292;

	preview.SetNameAndStatus( L( "GameUI_CrosshairDescription" ), NULL );
	preview.SetRect( x, y, 200, 200 );
	preview.hImage = EngFuncs::PIC_Load( "gfx/vgui/crosshair.tga", 0 );
	preview.hWhite = EngFuncs::PIC_Load( "*white" );

	AddItem( r );
	AddItem( g );
	AddItem( b );
	AddItem( a );

	AddItem( dynamicMove );
	AddItem( gap );
	AddItem( pad );
	AddItem( size );
	AddItem( thick );
	AddItem( dynamicScale );
	AddItem( dot );
	AddItem( xhairT );
	AddItem( shape );
	AddItem( gapUseWeaponValue );
	AddItem( additive );

	AddItem( preview );
}

void CMenuXhair::Reload()
{
	int cvarColor[4] = { 0, 255, 0, 255 };

	CMenuItemsHolder::Reload();

	if ( sscanf( EngFuncs::GetCvarString( "xhair_color" ), "%d %d %d %d", &cvarColor[0], &cvarColor[1], &cvarColor[2], &cvarColor[3] ) == 4 )
	{
		r.SetCurrentValue( bound( 0, cvarColor[0], 255 ) );
		g.SetCurrentValue( bound( 0, cvarColor[1], 255 ) );
		b.SetCurrentValue( bound( 0, cvarColor[2], 255 ) );
		a.SetCurrentValue( bound( 0, cvarColor[3], 255 ) );
	}
}

void CMenuClassicCrosshair::Save()
{
	int i;
	char colorstr[32];

	i = size.GetCurrentValue();
	// A7 build 143 (item 3): -1 when the cvar string matched no entry -
	// skip the write instead of indexing the array out of bounds
	if ( i >= 0 && i < (int)V_ARRAYSIZE( g_szCrosshairSizes ) )
		EngFuncs::CvarSetString( "cl_crosshair_size", g_szCrosshairSizes[i] );

	i = color.GetCurrentValue();
	if ( i >= 0 && i < V_ARRAYSIZE( g_CrosshairColors ) )
	{
		snprintf( colorstr, sizeof( colorstr ), "%i %i %i", g_CrosshairColors[i].r, g_CrosshairColors[i].g, g_CrosshairColors[i].b );
		EngFuncs::CvarSetString( "cl_crosshair_color", colorstr );
	}

	translucent.WriteCvar();
}

// A7 build 143 (round 143 item 3): fired by the spin on every value
// change (QM_CHANGED) - write the RAW entry the HUD parses, so the
// crosshair size moves at spin time instead of waiting for Save
void CMenuClassicCrosshair::SizeChanged()
{
	int i = (int)size.GetCurrentValue();

	if ( i >= 0 && i < (int)V_ARRAYSIZE( g_szCrosshairSizes ) )
		EngFuncs::CvarSetString( "cl_crosshair_size", g_szCrosshairSizes[i] );
}

int CMenuXhair::CMenuCrosshairPreview::ScaleForRes( float value, int height )
{
	/* "default" resolution is 640x480 */
	return rint( value * ( (float)height / 480.0f ) );
}

void CMenuXhair::CMenuCrosshairPreview::DrawCrosshairSection( int _x0, int _y0, int _x1, int _y1 )
{
	CMenuXhair *parent = (CMenuXhair *)Parent();

	float x0 = (float)_x0;
	float y0 = (float)_y0;
	float x1 = (float)_x1 - _x0;
	float y1 = (float)_y1 - _y0;

	EngFuncs::PIC_Set( hWhite, parent->r.GetCurrentValue(), parent->g.GetCurrentValue(), parent->b.GetCurrentValue(), parent->a.GetCurrentValue() );

	if ( parent->additive.bChecked )
		EngFuncs::PIC_DrawAdditive( x0, y0, x1, y1 );
	else
		EngFuncs::PIC_DrawTrans( x0, y0, x1, y1 );
}

void CMenuXhair::CMenuCrosshairPreview::DrawCrosshairPadding( int _pad, int _x0, int _y0, int _x1, int _y1 )
{
	CMenuXhair *parent = (CMenuXhair *)Parent();

	float pad = (float)_pad;
	float x0 = (float)_x0;
	float y0 = (float)_y0;
	float x1 = (float)_x1 - _x0;
	float y1 = (float)_y1 - _y0;

	EngFuncs::PIC_Set( hWhite, 0, 0, 0, parent->a.GetCurrentValue() );
	EngFuncs::PIC_DrawTrans( x0 - pad, y0 - pad, x1 + 2 * pad, pad ); // top part

	EngFuncs::PIC_Set( hWhite, 0, 0, 0, parent->a.GetCurrentValue() );
	EngFuncs::PIC_DrawTrans( x0 - pad, y0 + y1, x1 + 2 * pad, pad ); // bottom part

	EngFuncs::PIC_Set( hWhite, 0, 0, 0, parent->a.GetCurrentValue() );
	EngFuncs::PIC_DrawTrans( x0 - pad, y0, pad, y1 ); // left part

	EngFuncs::PIC_Set( hWhite, 0, 0, 0, parent->a.GetCurrentValue() );
	EngFuncs::PIC_DrawTrans( x0 + x1, y0, pad, y1 ); // right part
}

void CMenuXhair::CMenuCrosshairPreview::Draw()
{
	CMenuXhair *parent = (CMenuXhair *)Parent();

	int center_x, center_y;
	int gap, length, thickness;
	int y0, y1, x0, x1;
	wrect_t inner;
	wrect_t outer;

	/* calculate the coordinates */
	center_x = m_scPos.x + ( m_scSize.w / 2 );
	center_y = m_scPos.y + ( m_scSize.h / 2 );

	gap = ScaleForRes( 4 + parent->gap.GetCurrentValue(), ScreenHeight );
	length = ScaleForRes( parent->size.GetCurrentValue(), ScreenHeight );
	thickness = ScaleForRes( parent->thick.GetCurrentValue(), ScreenHeight );
	thickness = Q_max( 1, thickness );

	inner.left = ( center_x - gap - thickness / 2 );
	inner.right = ( inner.left + 2 * gap + thickness );
	inner.top = ( center_y - gap - thickness / 2 );
	inner.bottom = ( inner.top + 2 * gap + thickness );

	outer.left = ( inner.left - length );
	outer.right = ( inner.right + length );
	outer.top = ( inner.top - length );
	outer.bottom = ( inner.bottom + length );

	y0 = ( center_y - thickness / 2 );
	x0 = ( center_x - thickness / 2 );
	y1 = ( y0 + thickness );
	x1 = ( x0 + thickness );

	// APAMk2 - Draw Crosshair's BG before Crosshair itself
	if ( !hImage )
	{
		UI_FillRect( m_scPos, m_scSize, uiPromptBgColor );
	}
	else
	{
		EngFuncs::PIC_Set( hImage, 255, 255, 255 );
		EngFuncs::PIC_DrawTrans( m_scPos, m_scSize );
	}

	int textHeight = m_scPos.y - ( m_scChSize * 1.5f );
	UI_DrawString( font, m_scPos.x, textHeight, m_scSize.w, m_scChSize, szName, uiColorText, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_NOSIZELIMIT | ETF_FORCECOL );

	// A7 round 147 item 7: the vector HUD reads cl_crosshair_type every
	// frame - show the SAME shape in the preview the player will get
	int shape = (int)EngFuncs::GetCvarFloat( "cl_crosshair_type" );
	if ( shape < 0 || shape > 3 )
		shape = 0;

	if ( shape == 2 )
	{
		/* circle: ring of small squares at the same radius the in-game
		 * DrawCrosshairCircle uses (gap + length / 2); outline padding is
		 * skipped here - the preview only approximates the ring */
		float r = (float)( gap + length / 2 );
		float half = (float)Q_max( 1, thickness ) * 0.5f;
		const float pi = 3.14159265f;

		for ( int i = 0; i < 16; i++ )
		{
			float a = 2.0f * pi * (float)i / 16.0f;
			int px = center_x + (int)( cosf( a ) * r + 0.5f );
			int py = center_y + (int)( sinf( a ) * r + 0.5f );

			DrawCrosshairSection( px - (int)half, py - (int)half, px + (int)half + 1, py + (int)half + 1 );
		}

		if ( parent->dot.bChecked )
			DrawCrosshairSection( x0, y0, x1, y1 );
	}
	else
	{
	/* draw padding if wanted */
	if ( parent->pad.GetCurrentValue() )
	{
		/* don't scale this */
		int pad = (int)parent->pad.GetCurrentValue();

		if ( parent->dot.bChecked || shape == 3 )
			DrawCrosshairPadding( pad, x0, y0, x1, y1 );

		if ( !parent->xhairT.bChecked && shape != 1 )
			DrawCrosshairPadding( pad, x0, outer.top, x1, inner.top );

		DrawCrosshairPadding( pad, x0, inner.bottom, x1, outer.bottom );
		DrawCrosshairPadding( pad, outer.left, y0, inner.left, y1 );
		DrawCrosshairPadding( pad, inner.right, y0, outer.right, y1 );
	}

	if ( parent->dot.bChecked || shape == 3 )
		DrawCrosshairSection( x0, y0, x1, y1 );

	if ( !parent->xhairT.bChecked && shape != 1 )
		DrawCrosshairSection( x0, outer.top, x1, inner.top );

	DrawCrosshairSection( x0, inner.bottom, x1, outer.bottom );
	DrawCrosshairSection( outer.left, y0, inner.left, y1 );
	DrawCrosshairSection( inner.right, y0, outer.right, y1 );
	}

	// draw the rectangle
	if ( eFocusAnimation == QM_HIGHLIGHTIFFOCUS && IsCurrentSelected() )
		UI_DrawRectangle( m_scPos, m_scSize, uiInputTextColor );
	else
		UI_DrawRectangle( m_scPos, m_scSize, uiInputFgColor );
}

void CMenuClassicCrosshair::CMenuCrosshairPreview::Draw()
{
	CMenuClassicCrosshair *parent = (CMenuClassicCrosshair *)Parent();

	int length;
	int x = m_scPos.x, y = m_scPos.y;
	int w = m_scSize.w, h = m_scSize.h;
	int delta;
	int i = parent->color.GetCurrentValue();
	int r = 0, g = 255, b = 0, a = 180;

	if ( i >= 0 && i < V_ARRAYSIZE( g_CrosshairColors ) )
	{
		r = g_CrosshairColors[i].r;
		g = g_CrosshairColors[i].g;
		b = g_CrosshairColors[i].b;
	}
	else
	{
		sscanf( EngFuncs::GetCvarString( "cl_crosshair_color" ), "%d %d %d", &r, &g, &b );
	}

	if ( !hImage )
	{
		UI_FillRect( m_scPos, m_scSize, uiPromptBgColor );
	}
	else
	{
		EngFuncs::PIC_Set( hImage, 255, 255, 255 );
		EngFuncs::PIC_DrawTrans( m_scPos, m_scSize );
	}

	switch ( (int)parent->size.GetCurrentValue() )
	{
	case 1:
		length = 10;
		break;
	case 2:
		length = 20;
		break;
	case 3:
		length = 30;
		break;
	case 0:
		if ( ScreenWidth < 640 )
			length = 30;
		else if ( ScreenWidth < 1024 )
			length = 20;
		else
			length = 10;
	default:
		break;
	}

	length *= ScreenHeight / 768.0f;
	delta = ( w / 2 - length ) * 0.5f;

	if ( !parent->translucent.bChecked )
	{
		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawTrans( x + w / 2, y + delta, 1, length );

		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawTrans( x + w / 2, y + h / 2 + delta, 1, length );

		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawTrans( x + delta, y + h / 2, length, 1 );

		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawTrans( x + w / 2 + delta, y + h / 2, length, 1 );
	}
	else
	{
		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawAdditive( x + w / 2, y + delta, 1, length );

		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawAdditive( x + w / 2, y + h / 2 + delta, 1, length );

		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawAdditive( x + delta, y + h / 2, length, 1 );

		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		EngFuncs::PIC_DrawAdditive( x + w / 2 + delta, y + h / 2, length, 1 );
	}

	int textHeight = m_scPos.y - ( m_scChSize * 1.5f );
	UI_DrawString( font, m_scPos.x, textHeight, m_scSize.w, m_scChSize, szName, uiColorText, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_NOSIZELIMIT | ETF_FORCECOL );

	// draw the rectangle
	if ( eFocusAnimation == QM_HIGHLIGHTIFFOCUS && IsCurrentSelected() )
		UI_DrawRectangle( m_scPos, m_scSize, uiInputTextColor );
	else
		UI_DrawRectangle( m_scPos, m_scSize, uiInputFgColor );
}

void CMenuClassicCrosshair::Reload()
{
	char colorstr[32];
	int rgb[3];
	char sizestr[32];
	int i, j;

	CMenuItemsHolder::Reload();

	strncpy( colorstr, EngFuncs::GetCvarString( "cl_crosshair_color" ), sizeof( colorstr ) );
	if ( sscanf( colorstr, "%d %d %d", &rgb[0], &rgb[1], &rgb[2] ) == 3 )
	{
		j = V_ARRAYSIZE( g_CrosshairColors );
		for ( i = 0; i <= j; i++ )
		{
			if ( i == j )
			{
				color.SetCurrentValue( colorstr );
				break;
			}

			if ( g_CrosshairColors[i].r == rgb[0] && g_CrosshairColors[i].g == rgb[1] && g_CrosshairColors[i].b == rgb[2] )
			{
				color.SetCurrentValue( i );
				break;
			}
		}
	}

	strncpy( sizestr, EngFuncs::GetCvarString( "cl_crosshair_size" ), sizeof( sizestr ) );
	j = V_ARRAYSIZE( g_szCrosshairSizes );
	for ( i = 0; i <= j; i++ )
	{
		if ( i == j )
		{
			// A7 build 143 (item 3): unknown value lands on Auto (index 0)
			// - reading the cvar as a float was the old CVAR_VALUE bug
			size.SetCurrentValue( 0.0f );
			break;
		}

		if ( !stricmp( sizestr, g_szCrosshairSizes[i] ) )
		{
			size.SetCurrentValue( i );
			break;
		}
	}
}

void CMenuXhair::Save()
{
	char colorstr[32];

	snprintf( colorstr, sizeof( colorstr ), "%d %d %d %d", (int)r.GetCurrentValue(), (int)g.GetCurrentValue(), (int)b.GetCurrentValue(), (int)a.GetCurrentValue() );
	EngFuncs::CvarSetString( "xhair_color", colorstr );

	additive.WriteCvar();
	dot.WriteCvar();
	dynamicMove.WriteCvar();
	dynamicScale.WriteCvar();
	gapUseWeaponValue.WriteCvar();
	gap.WriteCvar();
	pad.WriteCvar();
	size.WriteCvar();
	xhairT.WriteCvar();
	thick.WriteCvar();
	// A7 round 147 item 7: persist the shape spin with the rest
	shape.WriteCvar();
}

ADD_MENU( menu_crosshair, CMenuCrosshair, UI_Crosshair_Menu );