/*
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "Bitmap.h"
#include "PicButton.h"
#include "Slider.h"
#include "CheckBox.h"
#include "YesNoMessageBox.h"

#define ART_BANNER	  	"gfx/shell/head_vidoptions"
#define ART_GAMMA		"gfx/shell/gamma"

#define LEGACY_VIEWSIZE 0

class CMenuVidOptions : public CMenuFramework
{
private:
	void _Init() override;
	void _VidInit() override;
	void Reload() override;

public:
	CMenuVidOptions() : CMenuFramework( "CMenuVidOptions" ) { }
	void SaveAndPopMenu() override;
	void UpdateConfig();
	void GetConfig();

	int		outlineWidth;

	class CMenuVidPreview : public CMenuBitmap
	{
		void Draw() override;
	} testImage;

	CMenuPicButton	done;

#if LEGACY_VIEWSIZE
	CMenuSlider	screenSize;
#endif
	CMenuSlider	gammaIntensity;
	CMenuSlider	brightness;
	CMenuCheckBox	vbo;
	CMenuCheckBox	swwater;
	CMenuCheckBox	overbright;
	CMenuCheckBox	filtering;
	CMenuCheckBox	detailtex;
	CMenuCheckBox	hudscale;
	CMenuYesNoMessageBox	msgBox;

	HIMAGE		hTestImage;
};

/*
=================
CMenuVidOptions::UpdateConfig
=================
*/
void CMenuVidOptions::UpdateConfig( void )
{
	float val1 = RemapVal( gammaIntensity.GetCurrentValue(), 0.0, 1.0, 1.8, 3.0 );
	float val2 = RemapVal( brightness.GetCurrentValue(), 0.0, 1.0, 0.0, 3.0 );
	EngFuncs::CvarSetValue( "gamma", val1 );
	EngFuncs::CvarSetValue( "brightness", val2 );
	if( hTestImage )
		EngFuncs::ProcessImage( hTestImage, val1, val2 );
}

void CMenuVidOptions::GetConfig( void )
{
	float val1 = EngFuncs::GetCvarFloat( "gamma" );
	float val2 = EngFuncs::GetCvarFloat( "brightness" );

	gammaIntensity.SetCurrentValue( RemapVal( val1, 1.8f, 3.0f, 0.0f, 1.0f ) );
	brightness.SetCurrentValue( RemapVal( val2, 0.0f, 3.0f, 0.0f, 1.0f ) );
	if( hTestImage )
		EngFuncs::ProcessImage( hTestImage, val1, val2 );

	gammaIntensity.SetOriginalValue( val1 );
	brightness.SetOriginalValue( val2 );
}

void CMenuVidOptions::SaveAndPopMenu( void )
{
#if LEGACY_VIEWSIZE
	screenSize.WriteCvar();
#endif
	detailtex.WriteCvar();
	vbo.WriteCvar();
	swwater.WriteCvar();
	overbright.WriteCvar();
	filtering.WriteCvar();
	hudscale.WriteCvar();
	// gamma and brightness is already written

	CMenuFramework::SaveAndPopMenu();
}

/*
=================
CMenuVidOptions::Ownerdraw
=================
*/
void CMenuVidOptions::CMenuVidPreview::Draw( )
{
	int		color = 0xFFFF0000; // 255, 0, 0, 255 - classic GoldSrc preview frame

	// A7-full-port: the gfx/shell/gamma test picture is xash3d art (user
	// report: "other menus still look xash3d style extras bmp image").
	// Draw the classic grayscale gamma ramp instead: black -> white steps
	// still preview gamma/brightness because the engine applies both to
	// everything drawn on screen.
	UI_FillRect( m_scPos, m_scSize, uiColorBlack );

	const int steps = 16;
	int rampX = m_scPos.x + (int)( 4 * uiStatic.scaleX );
	int rampY = m_scPos.y + (int)( 4 * uiStatic.scaleY );
	int rampW = m_scSize.w - (int)( 8 * uiStatic.scaleX );
	int rampH = m_scSize.h - (int)( 8 * uiStatic.scaleY );

	for( int i = 0; i < steps; i++ )
	{
		int g = i * 255 / ( steps - 1 );
		unsigned int c = 0xFF000000 | ( g << 16 ) | ( g << 8 ) | g;
		int x0 = rampX + rampW * i / steps;
		int x1 = rampX + rampW * ( i + 1 ) / steps;

		UI_FillRect( x0, rampY, x1 - x0, rampH, c );
	}

	UI_DrawRectangleExt( m_scPos, m_scSize, color, ((CMenuVidOptions*)Parent())->outlineWidth );
}

/*
=================
CMenuVidOptions::Init
=================
*/
void CMenuVidOptions::_Init( void )
{
	// A7-full-port: no xash gamma test art - the preview draws a grayscale
	// ramp, which the engine gamma/brightness applies to directly
	hTestImage = 0;

	banner.SetPicture( ART_BANNER );

	testImage.iFlags = QMF_INACTIVE;
	testImage.SetRect( 390, 225, 480, 450 );

	int height = 280;

#if LEGACY_VIEWSIZE
	screenSize.szName = L( "Screen size" );
	screenSize.SetCoord( 72, height );
	screenSize.Setup( 30, 120, 10 );
	screenSize.onChanged = CMenuEditable::WriteCvarCb;

	height += 60;
#endif

	gammaIntensity.szName = L( "GameUI_Gamma" );
	gammaIntensity.SetCoord( 72, height );
	gammaIntensity.Setup( 0.0, 1.0, 0.025 );
	gammaIntensity.onChanged = VoidCb( &CMenuVidOptions::UpdateConfig );
	gammaIntensity.onCvarGet = VoidCb( &CMenuVidOptions::GetConfig );
	height += 60;

	brightness.SetCoord( 72, height );
	brightness.szName = L( "GameUI_Brightness" );
	brightness.Setup( 0, 1.0, 0.025 );
	brightness.onChanged = VoidCb( &CMenuVidOptions::UpdateConfig );
	brightness.onCvarGet = VoidCb( &CMenuVidOptions::GetConfig );
	height += 60;

	done.szName = L( "GameUI_OK" );
	done.SetCoord( 72, height );
	done.SetPicture( PC_DONE );
	done.onReleased = VoidCb( &CMenuVidOptions::SaveAndPopMenu );
	height += 60;

	detailtex.szName = L( "Detail textures" );
	detailtex.SetCoord( 72, height );
	height += 50;

	vbo.szName = L( "Use VBO" );
	vbo.SetCoord( 72, height );
	height += 50;

	swwater.szName = L( "Water ripples" );
	swwater.SetCoord( 72, height );
	height += 50;

	overbright.szName = L( "Overbrights" );
	overbright.SetCoord( 72, height );
	height += 50;

	filtering.szName = L( "Texture filtering" );
	filtering.SetCoord( 72, height );
	height += 50;

	hudscale.szName = L( "Auto scale HUD" );
	hudscale.SetCoord( 72, height );
	height += 50;

	msgBox.SetMessage( L( "^1WARNING: This is an experimental option and turning it on might break mods!^7\n\nReload the game or reconnect to the server to apply the settings."));
	msgBox.onNegative.pExtra = &hudscale;
	SET_EVENT_MULTI( msgBox.onNegative,
	{
		CMenuCheckBox *cb = (CMenuCheckBox *)pExtra;

		cb->bChecked = false;
		cb->SetCvarValue( 0.0f );
	});

	SET_EVENT_MULTI( hudscale.onChanged,
	{
		CMenuVidOptions *parent = (CMenuVidOptions *)pSelf->Parent();

		if( EngFuncs::ClientInGame( ))
		{
			// bring up warning message box
			// FIXME: try to save the game, apply the settings, and then load it back. This will be useful when changing resolution too.
			parent->msgBox.Show();
		}
	});

	SET_EVENT_MULTI( hudscale.onCvarWrite,
	{
		CMenuCheckBox *cb = (CMenuCheckBox *)pSelf;

		if( cb->bChecked )
		{
			// automatically scale HUD as if we have 1024x768 screen
			// (saving aspect ratio)
			// FIXME: allow configuring this value?
			EngFuncs::CvarSetValue( cb->CvarName(), 1024.0f );
		}
		else
		{
			EngFuncs::CvarSetValue( cb->CvarName(), 0.0f );
		}
	});

	SET_EVENT_MULTI( hudscale.onCvarGet,
	{
		CMenuCheckBox *cb = (CMenuCheckBox *)pSelf;

		cb->bChecked = EngFuncs::GetCvarFloat( cb->CvarName() ) >= 640.0f;
	});

	AddItem( banner );
	AddItem( done );
#if LEGACY_VIEWSIZE
	AddItem( screenSize );
#endif
	AddItem( gammaIntensity );
	AddItem( brightness );
	AddItem( detailtex );
	AddItem( vbo );
	AddItem( swwater );
	AddItem( overbright );
	AddItem( filtering );
	AddItem( hudscale );
	AddItem( testImage );

#if LEGACY_VIEWSIZE
	screenSize.LinkCvar( "viewsize" );
#endif

	gammaIntensity.LinkCvar( "gamma" );
	brightness.LinkCvar( "brightness" );

	detailtex.LinkCvar( "r_detailtextures" );
	swwater.LinkCvar( "r_ripple" );
	vbo.LinkCvar( "gl_vbo" );
	overbright.LinkCvar( "gl_overbright" );
	// skip filtering.LinkCvar, different names in ref_gl and ref_soft
	hudscale.LinkCvar( "hud_scale" );
}

void CMenuVidOptions::_VidInit()
{
	outlineWidth = 2;
	UI_ScaleCoords( NULL, NULL, &outlineWidth, NULL );
}

void CMenuVidOptions::Reload()
{
	bool gl_active = !strnicmp( EngFuncs::GetCvarString( "r_refdll_loaded" ), "gl", 2 );
	bool soft_active = !stricmp( EngFuncs::GetCvarString( "r_refdll_loaded" ), "soft" );

	detailtex.SetGrayed( !gl_active );
	detailtex.SetInactive( !gl_active );

	vbo.SetGrayed( !gl_active );
	vbo.SetInactive( !gl_active );

	if( gl_active )
	{
		if( EngFuncs::textfuncs.pfnIsCvarReadOnly( "gl_vbo" ) > 0 )
		{
			SET_EVENT_MULTI( vbo.onCvarChange,
			{
				CMenuCheckBox *cb = (CMenuCheckBox *)pSelf;
				cb->bChecked = false;
				UI_ShowMessageBox( L( "Not supported on your GPU" ));
			});

			vbo.onCvarWrite = CEventCallback::NoopCb;
		}
		else
		{
			vbo.onCvarWrite.Reset();
			vbo.onCvarChange.Reset();
		}
	}

	swwater.SetGrayed( !gl_active );
	swwater.SetInactive( !gl_active );

	overbright.SetGrayed( !gl_active );
	overbright.SetInactive( !gl_active );

	if( soft_active || gl_active )
	{
		filtering.SetGrayed( false );
		filtering.SetInactive( false );

		if( soft_active )
		{
			// no need to invert, 0 disables filtering
			filtering.LinkCvar( "sw_texfilt" );
		}
		else
		{
			SET_EVENT_MULTI( filtering.onCvarGet,
			{
				CMenuCheckBox *cb = (CMenuCheckBox *)pSelf;

				if( EngFuncs::GetCvarFloat( cb->CvarName( )))
					cb->bChecked = false;
				else cb->bChecked = true;
			});
			SET_EVENT_MULTI( filtering.onCvarWrite,
			{
				CMenuCheckBox *cb = (CMenuCheckBox *)pSelf;
				if( cb->bChecked )
					EngFuncs::CvarSetValue( cb->CvarName(), 0.0f );
				else EngFuncs::CvarSetValue( cb->CvarName(), 1.0f );
			});

			filtering.LinkCvar( "gl_texture_nearest" );
		}
	}
	else
	{
		filtering.SetGrayed( true );
		filtering.SetInactive( true );
	}

	CMenuFramework::Reload();
}

ADD_MENU( menu_vidoptions, CMenuVidOptions, UI_VidOptions_Menu );
