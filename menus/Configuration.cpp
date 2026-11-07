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

// A7 remake: Options is now the original CS 1.6 tabbed dialog - a single
// floating window with the tab strip Multiplayer | Keyboard | Mouse | Audio
// | Video and OK / Cancel / Apply on the bottom right. The Multiplayer tab
// carries the picture's content: player name, spraypaint image (logo
// dropdown + preview + colour) and crosshair appearance (size/colour/
// translucent + live preview).
// build 58: Touch tab removed on user request (touch settings stay reachable
// from the touch menus / console); every page is organized into labelled
// sections like the original GameUI grouped options, and the Video tab is
// split into Display (gamma/brightness) and Rendering (water ripples,
// detail textures, HUD scaling) groups.
// build 85: Voice tab removed (voice settings live in Player Setup) and
// Lock tab removed (ui_lockcode is a dead cvar), leaving five tabs. The
// Video tab now carries a NESTED tab view of its own with two sub-tabs:
// "Video options" (gamma/brightness/ripple/detailtex/HUD scale) and
// "Video modes" - the old fullscreen CMenuVidModes screen folded in as a
// page (resolution list, renderer, window mode, vsync, Apply with the
// 10-second test-mode safety revert). No setting opens a separate
// full-screen view anymore.

#include "Framework.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "CheckBox.h"
#include "Slider.h"
#include "Field.h"
#include "SpinControl.h"
#include "YesNoMessageBox.h"
#include "TabView.h"
#include "Table.h"
#include "Action.h"
#include "StringVectorModel.h"
#include "StringArrayModel.h"
#include "ColorPickerDialog.h"
#include "keydefs.h"
#include "q_client.h"       // build 88: kbutton_t for the mouse-look state read
#include "Utils.h" // build 88: RemapVal for the gamma/brightness remap
#include "MessageBox.h"     // build 88: key-grab dialog of the Key bindings sub-page
#include "KbActListModel.h" // build 88: same list model as menus/Controls.cpp
#include "BaseMenu.h"       // build 88: uiFileDialogGlobal / UI_DrawBevel (Touch buttons fold-in)
#include "mobility_int.h"   // build 88: TOUCH_FL_* button flags
#include "utlvector.h"      // build 88: touch button list storage

// build 85: a CMenuTabView strip is 1.5 bold-font lines tall (see
// CMenuTabView::CalcSizes); the nested Video view must subtract both
// its own strip and the outer one from the page height
#define TAB_STRIP_HEIGHT ( UI_MED_CHAR_HEIGHT * 3 / 2 )

static const char *g_szCrosshairSizes[] = { "auto", "small", "medium", "large" };

// A7 build 58, build 87: section header - a white label that groups a
// page's controls like the original GameUI's organised option sections
// (the reference screenshots use plain white group labels, no gold)
// build 90: header text moved to the sampled GameUI light text
// (216,222,211) so every group label matches the picture's colour
static void SectionHeader( CMenuAction &a, const char *text, int x, int y, int w )
{
	a.iFlags = QMF_INACTIVE;
	a.colorBase = uiColorText;
	a.szName = text;
	a.SetRect( x, y, w, 24 );
}

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

/*
=====================================================================
Multiplayer tab: player name, spraypaint image, crosshair appearance
=====================================================================
*/
class CMenuOptionsMPPage : public CMenuItemsHolder
{
public:
	CMenuOptionsMPPage() : msgBox( true ), m_stripeCount( 1 ),
		m_horizontal( false ), m_logoColorable( false )
	{
		m_stripes[0][0] = m_stripes[0][1] = m_stripes[0][2] = 255;

		logoImage.hImage = 0;
		logoImage.stripes = m_stripes;
		logoImage.stripeCount = m_stripeCount;
		logoImage.colorable = false;
		logoImage.horizontal = &m_horizontal;

		// A7 round 100 (user: "no choosing - just import image or connect
		// from steam"): the scoreboard logo is RESOLVED by the game now
		// (imported image -> my Steam photo -> *sid photo -> logos/
		// fallback), so this page only previews the picture that will be
		// used and offers the import button. Plain image, no spray stripes.
		avatarImage.hImage = 0;
		avatarImage.stripes = NULL;
		avatarImage.stripeCount = 0;
		avatarImage.colorable = false;
		avatarImage.horizontal = NULL;

		preview.hImage = 0;
		preview.hWhite = 0;
	}

	// returns false when the player name is invalid (msgbox shown)
	bool Save();
	void UpdateLogo();

	// A7 round 100: no picker - show the picture the scoreboard will pick
	// by itself, and offer the one action that adds a new one
	void UpdateAvatar();
	void OnAvatarImport();
	// build 101: in-app file list callback (PC-style "search the bmps")
	static void AvatarImportCb( bool success );
	void EnsurePlayerLogo();
	void ShowColorPicker();
	void OnColorPickerOk();
	void WriteNewLogo();
	void ParseLogoColorCvar();
	void WriteLogoColorCvar();

	// A7 build 136 (user: "when it [steam login] is connected it should
	// change [to the] id - steam id with correct logo ... in options"):
	// the Steam account row under the crosshair column shows the stored
	// SteamID64 (friends/steamid.txt, written by the SteamFriends web
	// screen) plus the Steam avatar when one exists, and clicking it runs
	// the engine's "steamfriends" command
	void UpdateSteam();
	CMenuAction secSteam;
	CMenuAction steamRow;
	CMenuBitmap steamPic;

	// same shape as CMenuPlayerSetup's list: logos/*.bmp|*.png, no remapped
	class CLogosListModel : public CStringVectorModel
	{
	public:
		void Update() override;

		int GetFullPath( char *buf, size_t size, int pos )
		{
			const char *file = Element( pos ).String();
			const char *ext = IsPng( pos ) ? "png" : "bmp";
			return snprintf( buf, size, "logos/%s.%s", file, ext );
		}

		bool IsPng( int pos )
		{
			return m_isPngs[pos];
		}

	private:
		CUtlVector<bool> m_isPngs;
	} logosModel;

private:
	void _Init() override;
	void Reload() override;

	CMenuField name;

	CMenuSpinControl logo;
	class CMenuLogoPreview : public CMenuBaseItem
	{
	public:
		void Draw() override;
		HIMAGE hImage;
		const byte ( *stripes )[3];
		int stripeCount;
		bool colorable;
		const bool *horizontal;
	} logoImage;
	CMenuPicButton btnChooseColor;
	CMenuColorPickerDialog colorPickerDlg;
	byte m_stripes[MAX_LOGO_STRIPES][3];
	int m_stripeCount;
	bool m_horizontal;
	bool m_logoColorable;

	// A7 round 100: automatic scoreboard logo - the preview shows the
	// effective picture, the button runs the picker, nothing is stored
	// by hand anymore (cl_playerlogo is only the logos/ fallback the
	// menu fills in when there is nothing better)
	CMenuPicButton btnAvatarImport;
	CMenuLogoPreview avatarImage;
	CMenuAction secAvatar;

	CMenuSpinControl size;
	CMenuSpinControl color;
	CMenuCheckBox translucent;
	class CMenuXhairPreview : public CMenuBaseItem
	{
	public:
		void Draw() override;
		HIMAGE hImage;
		HIMAGE hWhite;
	} preview;

	CMenuAction sprayNote1;
	CMenuAction crosshairTitle;
	CMenuYesNoMessageBox msgBox;
};

void CMenuOptionsMPPage::CLogosListModel::Update( )
{
	char	**filenames;
	int numFiles, i;

	m_isPngs.RemoveAll();
	RemoveAll();

	// Get file list
	filenames = EngFuncs::GetFilesList( "logos/*.*", &numFiles, false );

	// build the model list
	for( i = 0; i < numFiles; i++ )
	{
		CUtlString logoFileName = filenames[i];
		char temp[256];
		bool png = logoFileName.BEndsWithCaseless( ".png" );

		if( png || logoFileName.BEndsWithCaseless( ".bmp" ))
		{
			COM_FileBase( logoFileName.String(), temp, sizeof( temp ));

			if( !stricmp( temp, "remapped" ))
				continue;

			AddToTail( temp );
			m_isPngs.AddToTail( png );
		}
	}
}

// A7 build 113: forward decl - the resolver is defined further down
// (next to UpdateAvatar), the Draw below needs it for the live refresh
static const char *AvatarEffectivePath( const char *playerlogo );

void CMenuOptionsMPPage::CMenuLogoPreview::Draw()
{
	// A7 build 113 (user: "logo avatar still too late to update in
	// options"): this page's preview never watched @cl_avatargen - only
	// the PlayerSetup copy did - so a picture imported from THIS page
	// (Options -> Import image...) kept its stale texture until the page
	// was re-entered. Same live refresh as PlayerSetup: on every engine
	// bump drop the cached picture and re-read the new bytes from disk.
	static int avGenSeen = -1;
	int avGen = (int)EngFuncs::GetCvarFloat( "@cl_avatargen" );

	if( avGen != avGenSeen )
	{
		avGenSeen = avGen;

		const char *path = AvatarEffectivePath( EngFuncs::GetCvarString( "cl_playerlogo" ));

		if( path )
		{
			EngFuncs::PIC_Free( path );
			hImage = EngFuncs::PIC_Load( path, 0 );
		}
	}

	if( !hImage )
	{
		// draw the background
		UI_FillRect( m_scPos, m_scSize, uiPromptBgColor );

		UI_DrawString( font, m_scPos, m_scSize, L( "No logo" ), colorBase, m_scChSize, QM_CENTER, ETF_SHADOW );
	}
	else if( !colorable || stripeCount <= 1 )
	{
		byte r = colorable ? stripes[0][0] : 255;
		byte g = colorable ? stripes[0][1] : 255;
		byte b = colorable ? stripes[0][2] : 255;
		EngFuncs::PIC_Set( hImage, r, g, b );
		EngFuncs::PIC_DrawTrans( m_scPos, m_scSize );
	}
	else
	{
		const Size img_sz = EngFuncs::PIC_Size( hImage );
		const bool hz = horizontal && *horizontal;

		const double tex_per_stripe = ( hz ? img_sz.w  : img_sz.h  ) / (double)stripeCount;
		const double scr_per_stripe = ( hz ? m_scSize.w : m_scSize.h ) / (double)stripeCount;

		for( int i = 0; i < stripeCount; i++ )
		{
			int scr_start = (int)( i * scr_per_stripe + 0.5 );
			int scr_next  = (int)(( i + 1 ) * scr_per_stripe + 0.5 );
			int tex_start = (int)( i * tex_per_stripe + 0.5 );
			int tex_next  = (int)(( i + 1 ) * tex_per_stripe + 0.5 );

			wrect_t rc = {};
			if( hz )
			{
				rc.left   = tex_start;
				rc.right  = tex_next;
				rc.bottom = img_sz.h;
			}
			else
			{
				rc.right  = img_sz.w;
				rc.top    = tex_start;
				rc.bottom = tex_next;
			}

			Point ui_pt = hz ? Point( m_scPos.x + scr_start, m_scPos.y ) : Point( m_scPos.x, m_scPos.y + scr_start );
			Size  ui_sz = hz ? Size( scr_next - scr_start, m_scSize.h ) : Size( m_scSize.w, scr_next - scr_start );

			EngFuncs::PIC_Set( hImage, stripes[i][0], stripes[i][1], stripes[i][2] );
			EngFuncs::PIC_DrawTrans( ui_pt, ui_sz, &rc );
		}
	}

	int textHeight = m_scPos.y - (m_scChSize * 1.5f);
	uint textflags = ( iFlags & QMF_DROPSHADOW ) ? ETF_SHADOW : 0;
	UI_DrawString( font, m_scPos.x, textHeight, m_scSize.w, m_scChSize, szName, uiColorText, m_scChSize, QM_LEFT, textflags | ETF_FORCECOL | ETF_NOSIZELIMIT );

	// draw the rectangle
	if( eFocusAnimation == QM_HIGHLIGHTIFFOCUS && IsCurrentSelected() )
		UI_DrawRectangle( m_scPos, m_scSize, uiInputTextColor );
	else
		UI_DrawRectangle( m_scPos, m_scSize, uiInputFgColor );
}

void CMenuOptionsMPPage::CMenuXhairPreview::Draw()
{
	CMenuOptionsMPPage *parent = (CMenuOptionsMPPage *)Parent();

	int length;
	int x = m_scPos.x, y = m_scPos.y;
	int w = m_scSize.w, h = m_scSize.h;
	int delta;
	int i = parent->color.GetCurrentValue();
	int r = 0, g = 255, b = 0, a = 180;

	if ( i >= 0 && i < (int)V_ARRAYSIZE( g_CrosshairColors ) )
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

void CMenuOptionsMPPage::UpdateLogo()
{
	const int pos = logo.GetCurrentValue();
	bool colorable = false;

	if( pos < 0 )
	{
		logoImage.hImage = 0;
	}
	else
	{
		char filename[1024];
		const int temp = logosModel.GetFullPath( filename, sizeof( filename ), pos );
		if(( temp < 0 ) || ( temp > (int)sizeof( filename )))
		{
			logoImage.hImage = 0;
		}
		else
		{
			logoImage.hImage = EngFuncs::PIC_Load( filename, 0 );

			if( !logosModel.IsPng( pos ))
			{
				CBMP *bmpFile = CBMP::LoadFile( filename );
				if( bmpFile->GetBitmapHdr()->bitsPerPixel == 8 )
					colorable = true;
				delete bmpFile;
			}
		}
	}

	m_logoColorable         = colorable;
	logoImage.colorable     = m_logoColorable;
	logoImage.stripes       = m_stripes;
	logoImage.stripeCount   = m_stripeCount;
	logoImage.horizontal    = &m_horizontal;

	btnChooseColor.SetGrayed( !m_logoColorable );
}

// A7 round 100: the picture the scoreboard will pick for me, in the
// same order the draw code uses - my Steam photo, the imported image,
// the logos/ fallback stored in cl_playerlogo, then any logos/ picture
// so a fresh install still shows something. "" = nothing to show yet.
// A7 build 139 (item 4, "if he use steam account don't let him change
// avatar"): avatar_self.png now comes FIRST - a signed-in account's
// photo beats a picture imported earlier, and while signed in the
// import path itself is refused (UpdateSteam hides the button).
static const char *AvatarEffectivePath( const char *playerlogo )
{
	static const char *cands[] = {
		"media/avatars/avatar_self.png",     // my Steam photo (wins)
		"media/avatars/avatar_custom.png",   // "Import image..." (Options)
	};

	for( size_t i = 0; i < V_ARRAYSIZE( cands ); i++ )
	{
		int len = 0;
		void *buf = EngFuncs::COM_LoadFile( cands[i], &len );
		if( buf )
		{
			EngFuncs::COM_FreeFile( buf );
			return cands[i];
		}
	}

	if( playerlogo && *playerlogo )
	{
		int len = 0;
		void *buf = EngFuncs::COM_LoadFile( playerlogo, &len );
		if( buf )
		{
			EngFuncs::COM_FreeFile( buf );
			return playerlogo;
		}
	}

	return NULL;
}

void CMenuOptionsMPPage::UpdateAvatar()
{
	int len = 0;
	void *buf;

	// an imported picture wins over anything stored in the cvars
	const char *path = AvatarEffectivePath( EngFuncs::GetCvarString( "cl_playerlogo" ));

	if( !path && logosModel.GetRows() > 0 )
	{
		// nothing better: show the first logos/ picture, the same one
		// EnsurePlayerLogo() stores as the fallback
		static char fb[1024];
		logosModel.GetFullPath( fb, sizeof( fb ), 0 );

		buf = EngFuncs::COM_LoadFile( fb, &len );
		if( buf )
		{
			EngFuncs::COM_FreeFile( buf );
			path = fb;
		}
	}

	avatarImage.hImage = path ? EngFuncs::PIC_Load( path, 0 ) : 0;
}

// "Import image..." - build 101 (user: "just like search bmp file like in
// pc, not search in my phone"): open the mainui file list with the
// pictures the game folders hold (logos/, the gamedir root, media/) -
// the PC-style open dialog with preview/Use selected file/Cancel, no
// Android gallery. The engine stores the choice as
// media/avatars/avatar_custom.png and the scoreboard picks it up (it
// re-checks while drawn).
void CMenuOptionsMPPage::OnAvatarImport()
{
	// A7 build 139 (item 4, "if he use steam account don't let him
	// change avatar"): while friends/steamid.txt exists the Steam photo
	// is the avatar - the button is hidden (UpdateSteam) and this is the
	// functional belt for a dialog that was already open
	if( UI_SteamSignedIn() )
	{
		UI_ShowMessageBox( "Steam account connected - your Steam photo is used as the avatar." );
		return;
	}

	static const char *pats[] = {
		"logos/*",
		"*.bmp",
		"*.png",
		"*.jpg",
		"*.jpeg",
		"*.tga",
		"media/*",
	};

	for( int i = 0; i < (int)V_ARRAYSIZE( pats ); i++ )
		Q_strncpy( uiFileDialogGlobal.patterns[i], pats[i], sizeof( uiFileDialogGlobal.patterns[i] ));

	uiFileDialogGlobal.npatterns = (int)V_ARRAYSIZE( pats );
	uiFileDialogGlobal.preview = true;
	uiFileDialogGlobal.valid = true;
	uiFileDialogGlobal.callback = AvatarImportCb;
	UI_FileDialog_Menu();
}

void CMenuOptionsMPPage::AvatarImportCb( bool success )
{
	if( !success || !uiFileDialogGlobal.result[0] )
		return;

	char cmd[512];
	V_snprintf( cmd, sizeof( cmd ), "avatar_import \"%s\"\n", uiFileDialogGlobal.result );
	EngFuncs::ClientCmd( true, cmd );
}

// A7 round 100: when there is no stored fallback at all, fill it in
// from logos/ so the logo shows on every server without the player
// having to choose anything
void CMenuOptionsMPPage::EnsurePlayerLogo()
{
	const char *value = EngFuncs::GetCvarString( "cl_playerlogo" );

	if( value && *value )
		return;

	if( logosModel.GetRows() <= 0 )
		return;

	char path[1024];
	logosModel.GetFullPath( path, sizeof( path ), 0 );
	EngFuncs::CvarSetString( "cl_playerlogo", path );
}

void CMenuOptionsMPPage::ShowColorPicker()
{
	if( !m_logoColorable )
		return;

	colorPickerDlg.Show( m_stripes, m_stripeCount, m_horizontal, logoImage.hImage );
}

void CMenuOptionsMPPage::OnColorPickerOk()
{
	colorPickerDlg.GetStripes( m_stripes, m_stripeCount, m_horizontal );
	EngFuncs::CvarSetValue( "ui_logohorizontal", m_horizontal ? 1.f : 0.f );
	logoImage.colorable = m_logoColorable;
	logoImage.stripes = m_stripes;
	logoImage.stripeCount = m_stripeCount;
	logoImage.horizontal = &m_horizontal;
}

void CMenuOptionsMPPage::ParseLogoColorCvar()
{
	m_stripeCount = 1;
	m_stripes[0][0] = m_stripes[0][1] = m_stripes[0][2] = 255;

	const char *logoColor = EngFuncs::GetCvarString( "cl_logocolor" );
	if( !logoColor || !*logoColor )
		return;

	int parsed = 0;
	const char *p = logoColor;
	while( parsed < MAX_LOGO_STRIPES )
	{
		int r, g, b, n = 0;
		while( *p == ' ' || *p == '\t' || *p == ',' )
			p++;
		if( !*p )
			break;
		if( sscanf( p, "%d %d %d%n", &r, &g, &b, &n ) != 3 || n <= 0 )
			break;
		m_stripes[parsed][0] = (byte)Q_max( 0, Q_min( 255, r ));
		m_stripes[parsed][1] = (byte)Q_max( 0, Q_min( 255, g ));
		m_stripes[parsed][2] = (byte)Q_max( 0, Q_min( 255, b ));
		parsed++;
		p += n;
	}

	if( parsed > 0 )
		m_stripeCount = parsed;
}

void CMenuOptionsMPPage::WriteLogoColorCvar()
{
	CUtlString s;
	for( int i = 0; i < m_stripeCount; i++ )
	{
		if( i > 0 )
			s += " ";
		CUtlString triple;
		triple.Format( "%d %d %d", m_stripes[i][0], m_stripes[i][1], m_stripes[i][2] );
		s += triple;
	}
	EngFuncs::CvarSetString( "cl_logocolor", s.String() );
}

void CMenuOptionsMPPage::WriteNewLogo( void )
{
	char filename[1024];
	int pos = logo.GetCurrentValue();

	if( pos < 0 )
		return;

	EngFuncs::DeleteFile( "logos/remapped.png" );
	EngFuncs::DeleteFile( "logos/remapped.bmp" );

	logosModel.GetFullPath( filename, sizeof( filename ), pos );

	if( logosModel.IsPng( pos ))
	{
		int len;
		void *afile = EngFuncs::COM_LoadFile( filename, &len );

		if( !afile )
			return;

		// just copy file, nothing special
		EngFuncs::COM_SaveFile( "logos/remapped.png", afile, len );

		EngFuncs::COM_FreeFile( afile );

		EngFuncs::CvarSetString( "cl_logoext", "png" );
	}
	else
	{
		CBMP *bmpFile = CBMP::LoadFile( filename );

		// not valid logo BMP file
		if( !bmpFile )
			return;

		// remap logo if needed
		if( m_logoColorable )
			bmpFile->RemapLogo( m_stripeCount, &m_stripes[0][0], m_horizontal );

		bmpFile->Save( "logos/remapped.bmp" );
		EngFuncs::CvarSetString( "cl_logoext", "bmp" );

		delete bmpFile;
	}

	if( m_logoColorable )
		WriteLogoColorCvar();

	logo.WriteCvar();

	EngFuncs::CvarSetValue( "@cl_logoupdate", !EngFuncs::GetCvarFloat( "@cl_logoupdate" ));
}

bool CMenuOptionsMPPage::Save()
{
	char colorstr[32];

	if( !UI::Names::CheckIsNameValid( name.GetBuffer() ))
	{
		msgBox.Show();
		return false;
	}

	name.WriteCvar();
	WriteNewLogo();

	// A7 round 100: cl_playerlogo is no longer picked by hand - the
	// scoreboard resolves imported/steam/sid pictures itself and the
	// cvar only carries the logos/ fallback, filled in on demand. It is
	// registered FCVAR_ARCHIVE on the client side, so the engine
	// persists it with the rest of config.cfg
	EnsurePlayerLogo();

	int i = size.GetCurrentValue();
	if( i >= 0 && i < (int)V_ARRAYSIZE( g_szCrosshairSizes ))
		EngFuncs::CvarSetString( "cl_crosshair_size", g_szCrosshairSizes[i] );

	i = color.GetCurrentValue();
	if( i >= 0 && i < (int)V_ARRAYSIZE( g_CrosshairColors ))
	{
		snprintf( colorstr, sizeof( colorstr ), "%i %i %i",
			g_CrosshairColors[i].r, g_CrosshairColors[i].g, g_CrosshairColors[i].b );
		EngFuncs::CvarSetString( "cl_crosshair_color", colorstr );
	}

	translucent.WriteCvar();

	return true;
}

// A7 build 136 (item 1, user: "when it is connected it should change to
// the Steam id with correct logo ... in options"): open the Steam
// sign-in/friends screen from the Options row. The engine registers the
// command on Android only (engine/platform/android/android.c,
// Cmd_AddCommand "steamfriends" -> XashActivity.openSteamFriends)
static void SteamFriendsOpen( void )
{
	EngFuncs::ClientCmd( false, "steamfriends\n" );
}

// Fill the Steam account row from the two files the launcher-side
// SteamFriends screen writes on sign-in: friends/steamid.txt (the id)
// and media/avatars/avatar_self.png (that account's avatar - the
// "correct logo" beside the id). Not signed in: plain prompt text, no
// picture, so the row never pretends.
void CMenuOptionsMPPage::UpdateSteam()
{
	char id[64];
	int len = 0;
	id[0] = '\0';

	void *buf = EngFuncs::COM_LoadFile( "friends/steamid.txt", &len );
	if( buf )
	{
		char *s = (char *)buf;

		while( *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' )
			s++;

		size_t n = strlen( s );
		while( n && ( s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n' ))
			n--;
		if( n >= sizeof( id ))
			n = sizeof( id ) - 1;
		memcpy( id, s, n );
		id[n] = '\0';

		EngFuncs::COM_FreeFile( buf );
	}

	// reload-safe: drop the old handle before re-binding below
	EngFuncs::PIC_Free( "media/avatars/avatar_self.png" );

	// CMenuBitmap has no hImage member (build 136 first attempt used my
	// own preview class' field) - it binds through SetPicture, and when
	// there is no picture to show colorBase = 0 keeps Draw() from
	// painting an empty box (Bitmap.cpp skips the fill then)
	bool haveAvatar = EngFuncs::FileExists( "media/avatars/avatar_self.png" );
	steamPic.SetPicture( "media/avatars/avatar_self.png" );
	steamPic.colorBase = ( id[0] && haveAvatar ) ? (unsigned int)uiColorWhite : 0u;

	if( id[0] )
	{
		// A7 build 142 (round 142 item 1, user: "don't show in options
		// game , just only photo from steam and name changed"): the raw
		// Steam ID64 no longer draws on this row - the only visual that
		// stays here is the Steam photo (and the screen the row opens).
		// The identity change he sees in-game is the persona name
		// (UI_ApplySteamPersona pushes friends/name.txt into the name
		// cvar), not a number; the id still lives on disk for every gate
		// that needs it and shows on the Steam screen itself.
		steamRow.SetText( "Steam connected - click to open" );

		Con_Printf( "[A7] build142: steam row connected (photo only, id hidden)\n" );

		// A7 build 139 (item 4, "if he use steam account don't let him
		// change avatar"): the Steam photo owns the scoreboard logo - the
		// import button goes away while the account is connected (the
		// command is refused in OnAvatarImport too, and Java refuses the
		// write itself - three gates on purpose)
		btnAvatarImport.Hide();
	}
	else
	{
		steamRow.SetText( "Steam: not signed in - click to sign in with Steam" );
		Con_Printf( "[A7] build136: steam row not signed in\n" );

		btnAvatarImport.Show();
	}

	// A7 build 139 (items 3+4): a persona captured by the sign-in screen
	// while the game runs reaches the name cvar here (the sign-in starts
	// from this very row, so the next open of Options is the earliest
	// reliable sync point - see UI_ApplySteamPersona for the once-per-
	// persona rule)
	UI_ApplySteamPersona();
}

void CMenuOptionsMPPage::_Init()
{
	static const char *itemlist[V_ARRAYSIZE( g_CrosshairColors )];
	static CStringArrayModel colors( itemlist, V_ARRAYSIZE( g_CrosshairColors ));
	for( size_t i = 0; i < V_ARRAYSIZE( g_CrosshairColors ); i++ )
		itemlist[i] = L( g_CrosshairColors[i].name );

	static const char *sizelist[] = { L( "GameUI_Auto" ), L( "GameUI_Small" ), L( "GameUI_Medium" ), L( "GameUI_Large" ) };
	static CStringArrayModel sizes( sizelist, V_ARRAYSIZE( sizelist ));

	// A7 build 88: name area shrunk per user ("name are too big") - the
	// spray and player-logo previews grow 56 -> 88 (small, like the
	// reference), and the two hint lines merge into one so both groups
	// still fit the 401px page with >= 6px gaps.
	// build 90: re-spaced - every above-label row cleared the control
	// above it by >= 14px (name -> logo label was a 3px gap, "stuck")
	name.szName = L( "GameUI_PlayerName" );
	name.iMaxLength = 32;
	name.LinkCvar( "name" );
	name.SetRect( 16, 44, 400, 32 );

	logoImage.szName = L( "GameUI_SpraypaintImage" );
	logoImage.SetRect( 16, 132, 88, 88 );

	logosModel.Update();
	logo.Setup( &logosModel );
	logo.LinkCvar( "cl_logofile", CMenuEditable::CVAR_STRING );
	logo.onChanged = VoidCb( &CMenuOptionsMPPage::UpdateLogo );
	logo.SetRect( 140, 132, 340, 32 );

	// A7 round 100: no picker. The preview shows the picture the
	// scoreboard resolves on its own, and the button opens the system
	// image chooser (engine "avatar_pick"). Both labels are the section
	// headers, so the controls draw no text above them.
	EnsurePlayerLogo();

	btnAvatarImport.SetNameAndStatus( "Import image...",
		"Pick a picture from your gallery - it shows next to your name on the scoreboard" );
	btnAvatarImport.SetRect( 140, 306, 340, 32 );
	btnAvatarImport.onReleased = VoidCb( &CMenuOptionsMPPage::OnAvatarImport );

	avatarImage.szName = "";
	avatarImage.SetRect( 16, 306, 88, 88 );

	SectionHeader( secAvatar, "Player logo - set up automatically", 16, 270, 480 );

	// refresh unconditionally so an imported/stored picture shows on
	// first open
	UpdateAvatar();

	btnChooseColor.szName = L( "Choose logo color" );
	btnChooseColor.SetRect( 140, 184, 340, 32 );
	btnChooseColor.onReleased = VoidCb( &CMenuOptionsMPPage::ShowColorPicker );

	colorPickerDlg.Link( this );
	colorPickerDlg.onOk = VoidCb( &CMenuOptionsMPPage::OnColorPickerOk );

	// build 88: one merged hint line (was two - the second line pushed
	// the player-logo section off the page bottom); 560px fits the text
	sprayNote1.iFlags = QMF_INACTIVE;
	sprayNote1.colorBase = uiColorHelp;
	sprayNote1.szName = "Changes apply when you join a new server.";
	sprayNote1.SetRect( 16, 230, 560, 24 );

	SectionHeader( secSteam, "Steam account", 600, 300, 440 );

	steamPic.SetPicture( "media/avatars/avatar_self.png" );
	steamPic.colorBase = 0; // until UpdateSteam() decides (just below)
	steamPic.SetRect( 600, 332, 56, 56 );

	steamRow.SetRect( 668, 344, 372, 32 );
	steamRow.szStatusText = "Open the Steam sign-in / friends screen";
	steamRow.onReleased = SteamFriendsOpen;
	UpdateSteam();

	// right column: crosshair appearance (picture's Multiplayer tab).
	// A7 build 87: re-pitched so every label clears the control above it
	// (header 16..40, size label 52..78, color label 135..161)
	// build 90: opened up again - 17/19/16px gaps between the rows
	SectionHeader( crosshairTitle, "Crosshair appearance", 600, 16, 340 );

	size.Setup( &sizes );
	size.szName = "Size";
	size.LinkCvar( "cl_crosshair_size", CMenuEditable::CVAR_VALUE );
	size.SetRect( 600, 96, 260, 32 );

	color.Setup( &colors );
	color.szName = "Color";
	color.LinkCvar( "cl_crosshair_color", CMenuEditable::CVAR_STRING );
	color.SetRect( 600, 186, 260, 32 );

	// explicit box: without a width the label lands on the box itself
	translucent.SetNameAndStatus( L( "GameUI_Translucent" ), NULL );
	translucent.LinkCvar( "cl_crosshair_translucent" );
	translucent.SetRect( 600, 234, 32, 32 );

	preview.szName = ""; // title is the crosshairTitle label now
	preview.SetRect( 880, 96, 160, 160 );
	preview.hImage = EngFuncs::PIC_Load( "gfx/vgui/crosshair.tga", 0 );
	preview.hWhite = EngFuncs::PIC_Load( "*white" );

	msgBox.SetMessage( L( "Please, choose another player name" ) );
	msgBox.Link( this );

	AddItem( name );
	AddItem( logoImage );
	AddItem( logo );
	AddItem( btnChooseColor );
	AddItem( sprayNote1 );
	AddItem( secAvatar );
	AddItem( avatarImage );
	AddItem( btnAvatarImport );
	AddItem( crosshairTitle );
	AddItem( size );
	AddItem( color );
	AddItem( translucent );
	AddItem( preview );
	// A7 build 136 (item 1): the Steam account row
	AddItem( secSteam );
	AddItem( steamPic );
	AddItem( steamRow );
}

void CMenuOptionsMPPage::Reload()
{
	BaseClass::Reload();

	ParseLogoColorCvar();
	logoImage.stripes = m_stripes;
	logoImage.stripeCount = m_stripeCount;
	logoImage.horizontal = &m_horizontal;
	logoImage.colorable = false;
	UpdateLogo();

	// player logo: automatic now - re-list logos/ (the fallback pick may
	// have changed) and re-resolve the effective picture, which picks up
	// an image imported while the dialog was closed
	logosModel.Update();
	avatarImage.colorable = false;
	avatarImage.stripes = NULL;
	avatarImage.stripeCount = 0;
	avatarImage.horizontal = NULL;
	EnsurePlayerLogo();
	UpdateAvatar();

	// A7 build 136 (item 1): re-read friends/steamid.txt so signing in
	// (or out) through the Steam screen shows up on the next open
	UpdateSteam();
}

/*
=====================================================================
Keyboard tab (build 89: the notes/settings sub-page and the nested
kbTabs strip are GONE per user - "just add one bindings that's it".
The bindings table below IS the Keyboard tab; Use Defaults lives on
it now)
=====================================================================
*/
class CMenuOptionsKeysPage;

// same list model as menu_controls: action | key | alternate
class CMenuOptionsKeysModel : public CMenuKbActListModel
{
public:
	CMenuOptionsKeysModel( CMenuOptionsKeysPage *parent ) : CMenuKbActListModel( VIEW_BINDINGS ), parent( parent ) { }

	void OnActivateEntry( int line ) override;
	void OnDeleteEntry( int line ) override;

private:
	CMenuOptionsKeysPage *parent;
};

class CMenuOptionsKeysPage : public CMenuItemsHolder
{
public:
	CMenuOptionsKeysPage() : keysListModel( this ) { }

	void Save() { } // binds apply immediately

	// grab/unbind flow copied from menus/Controls.cpp (CMenuControls);
	// the fullscreen Hide() paths are dropped - the Options dialog stays
	void EnterGrabMode( void );
	void UnbindEntry( void );

	CMenuTable keysList;
	CMenuOptionsKeysModel keysListModel;

private:
	void _Init() override;
	void Reload() override;

	void UnbindCommand( const char *command );

	CMenuAction secBindings;
	CMenuAction hint1;
	CMenuAction hint2;
	CMenuAction hint3;

	// build 89: used to live on the removed notes/settings sub-page
	CMenuPicButton useDefaults;
	CMenuYesNoMessageBox defaultsMsgBox;

	void OnUseDefaults()
	{
		EngFuncs::ClientCmd( true, "exec keyboard\n" );
	}

	// redefine key wait dialog (was CMenuControls::CGrabKeyMessageBox)
	class CGrabKeyMessageBox : public CMenuMessageBox
	{
	public:
		bool KeyUp( int key ) override;
		bool KeyDown( int key ) override;
	} msgBox1;
};

void CMenuOptionsKeysModel::OnActivateEntry( int line )
{
	parent->EnterGrabMode();
}

void CMenuOptionsKeysModel::OnDeleteEntry( int line )
{
	parent->UnbindEntry();
}

void CMenuOptionsKeysPage::UnbindCommand( const char *command )
{
	const size_t command_len = strlen( command );

	for( int i = 0; ; i++ )
	{
		const char *str = EngFuncs::KeynumToString( i );

		if( !strcmp( str, "<OUT OF RANGE>" ))
			break;

		const char *b = EngFuncs::KEY_GetBinding( i );
		if( !b )
			continue;

		if( !strncmp( b, command, command_len ))
			EngFuncs::KEY_SetBinding( i, "" );
	}
}

bool CMenuOptionsKeysPage::CGrabKeyMessageBox::KeyUp( int key )
{
	EUISounds sound;
	CMenuOptionsKeysPage *parent = ((CMenuOptionsKeysPage *)m_pParent);

	// defining a key
	// escape is special, should allow rebind all keys on gamepad
	if( UI::Key::IsConsole( key ) || key == K_ESCAPE
		|| !parent->keysListModel.entries.IsValidIndex( parent->keysList.GetCurrentIndex( )))
	{
		sound = SND_BUZZ;
	}
	else
	{
		const char *bindName = parent->keysListModel.entries[parent->keysList.GetCurrentIndex( )].bind;

		EngFuncs::ClientCmdF( true, "bind \"%s\" \"%s\"\n", EngFuncs::KeynumToString( key ), bindName );

		sound = SND_LAUNCH;
	}

	parent->keysListModel.Update();
	Hide();
	PlayLocalSound( uiStatic.sounds[sound] );

	return true;
}

bool CMenuOptionsKeysPage::CGrabKeyMessageBox::KeyDown( int key )
{
	return true;
}

void CMenuOptionsKeysPage::UnbindEntry()
{
	if( !keysListModel.IsLineUsable( keysList.GetCurrentIndex( )))
	{
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_BUZZ] );
		return; // not a key
	}

	const char *bindName = keysListModel.entries[keysList.GetCurrentIndex( )].bind;

	UnbindCommand( bindName );
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_REMOVEKEY] );
	keysListModel.Update();
}

void CMenuOptionsKeysPage::EnterGrabMode()
{
	if( !keysListModel.IsLineUsable( keysList.GetCurrentIndex( )))
	{
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_REMOVEKEY] );
		return;
	}

	// entering to grab-mode
	const char *bindName = keysListModel.entries[keysList.GetCurrentIndex( )].bind;

	int keys[2];

	CMenuKbActListModel::LookupBoundKeys( bindName, keys );
	if( keys[1] != -1 )
		UnbindCommand( bindName );

	msgBox1.Show();

	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_KEY] );
}

void CMenuOptionsKeysPage::_Init()
{
	// build 89: full outer-tab space now (1120 x 401 - no inner strip),
	// so the table gets back the 40px it lost in the build-88 nesting
	SectionHeader( secBindings, "Key bindings", 16, 16, 340 );

	// build 90: the three hint lines get 4px of line spacing (pitch 24 ->
	// 28) and Use Defaults drops to y184 so its y-39 label zone clears
	// the paragraph instead of touching it
	hint1.iFlags = QMF_INACTIVE;
	hint1.colorBase = uiColorHelp;
	hint1.szName = "Click an action, then press a";
	hint1.SetRect( 16, 56, 340, 24 );

	hint2.iFlags = QMF_INACTIVE;
	hint2.colorBase = uiColorHelp;
	hint2.szName = "key to bind it. The Delete key";
	hint2.SetRect( 16, 84, 340, 24 );

	hint3.iFlags = QMF_INACTIVE;
	hint3.colorBase = uiColorHelp;
	hint3.szName = "unbinds the action.";
	hint3.SetRect( 16, 112, 340, 24 );

	keysList.SetRect( 380, 16, 660, 378 );
	keysList.SetModel( &keysListModel );
	keysList.SetupColumn( 0, L( "GameUI_Action" ), 0.50f );
	keysList.SetupColumn( 1, L( "GameUI_KeyButton" ), 0.25f );
	keysList.SetupColumn( 2, L( "GameUI_Alternate" ), 0.25f );

	msgBox1.SetMessage( L( "Press a key or button" ) );
	msgBox1.Link( this );

	// build 89: Use Defaults now sits on the bindings tab itself (the
	// standalone notes page was removed - label draws at y-39)
	useDefaults.szName = L( "GameUI_UseDefaults" );
	useDefaults.SetRect( 16, 188, 300, 32 );
	useDefaults.onReleased = defaultsMsgBox.MakeOpenEvent();

	defaultsMsgBox.SetMessage( L( "GameUI_KeyboardSettingsText" ) );
	defaultsMsgBox.onPositive = VoidCb( &CMenuOptionsKeysPage::OnUseDefaults );
	defaultsMsgBox.Link( this );

	AddItem( secBindings );
	AddItem( hint1 );
	AddItem( hint2 );
	AddItem( hint3 );
	AddItem( useDefaults );
	AddItem( keysList );
}

// the model refills each time the dialog reopens (was Controls::_VidInit)
void CMenuOptionsKeysPage::Reload()
{
	BaseClass::Reload();
	keysListModel.Update();
}

/*
=====================================================================
Mouse tab
=====================================================================
*/
class CMenuOptionsMousePage : public CMenuItemsHolder
{
public:
	void Save();

private:
	void _Init() override;

	CMenuSlider sensitivity;
	CMenuCheckBox filter;
	CMenuAction secMouse; // build 58 section header
};

void CMenuOptionsMousePage::Save()
{
	sensitivity.WriteCvar();
	filter.WriteCvar();
}

void CMenuOptionsMousePage::_Init()
{
	SectionHeader( secMouse, "Mouse", 16, 8, 660 );

	sensitivity.szName = L( "GameUI_MouseSensitivity" );
	sensitivity.Setup( 0.0, 20.0f, 0.1 );
	sensitivity.LinkCvar( "sensitivity" );
	// build 58: sliders span the full page width - the old 660 left half
	// of the dialog empty ("size too bad"). build 87: moved to y88 so the
	// label at y-39 clears the section header
	// build 90: slider pulled to y96 (24px under the header), the filter
	// row dropped to y190 so the two controls breathe (was 88/160)
	sensitivity.SetRect( 16, 96, 1020, 32 );

	filter.SetNameAndStatus( "Mouse filtering", "Smooth out raw mouse movement" );
	filter.LinkCvar( "m_filter" );
	filter.SetRect( 16, 190, 32, 32 );

	AddItem( secMouse );
	AddItem( sensitivity );
	AddItem( filter );
}

/*
=====================================================================
Mouse tab - Advanced sub-page (build 88: xash Advanced-controls
features folded in as nested tabs; sensitivity stays on the Mouse
page - no duplication)
=====================================================================
*/
class CMenuOptionsAdvPage : public CMenuItemsHolder
{
public:
	void Save();

private:
	void _Init() override;
	void Reload() override;

	void GetConfig();
	void PitchInvert();
	void ToggleLookCheckboxes( bool write );

	CMenuAction secAdv;

	CMenuCheckBox crosshair;
	CMenuCheckBox invertMouse;
	CMenuCheckBox mouseLook;
	CMenuCheckBox lookSpring;
	CMenuCheckBox lookStrafe;
	CMenuCheckBox lookFilter;
	CMenuCheckBox autoaim;
	CMenuCheckBox rawinput;
};

void CMenuOptionsAdvPage::PitchInvert()
{
	bool invert = invertMouse.bChecked;
	float m_pitch = EngFuncs::GetCvarFloat( "m_pitch" );

	if(( invert && ( m_pitch > 0 )) || ( !invert && ( m_pitch < 0 )))
		EngFuncs::CvarSetValue( "m_pitch", -m_pitch );
}

void CMenuOptionsAdvPage::ToggleLookCheckboxes( bool write )
{
	lookSpring.SetGrayed( mouseLook.bChecked );
	lookStrafe.SetGrayed( mouseLook.bChecked );

	if( write )
	{
		if( mouseLook.bChecked )
			EngFuncs::ClientCmd( false, "+mlook\nbind _force_write\n" );
		else
			EngFuncs::ClientCmd( false, "-mlook\nbind _force_write\n" );
	}
}

// xash AdvancedControls::GetConfig - special reads for invert/mouselook
// plus the plain LinkCvar set. No sensitivity (lives on the Mouse page)
// and the standalone menu's m_filter cross-write is dropped - the Mouse
// page's own filter checkbox stays the single owner of m_filter
void CMenuOptionsAdvPage::GetConfig()
{
	kbutton_t *mlook;

	if( EngFuncs::GetCvarFloat( "m_pitch" ) < 0 )
		invertMouse.bChecked = true;

	mlook = (kbutton_t *)EngFuncs::KEY_GetState( "in_mlook" );
	if( mlook )
	{
		if( mlook && mlook->state & 1 )
			mouseLook.bChecked = true;
		else
			mouseLook.bChecked = false;
	}
	else
	{
		mouseLook.SetGrayed( true );
		mouseLook.bChecked = true;
	}

	crosshair.LinkCvar( "crosshair" );
	lookSpring.LinkCvar( "lookspring" );
	lookStrafe.LinkCvar( "lookstrafe" );
	lookFilter.LinkCvar( "look_filter" );
	autoaim.LinkCvar( "sv_aim" );
	rawinput.LinkCvar( "m_rawinput" );

	ToggleLookCheckboxes( false );
}

void CMenuOptionsAdvPage::Save()
{
	crosshair.WriteCvar();
	lookSpring.WriteCvar();
	lookStrafe.WriteCvar();
	lookFilter.WriteCvar();
	autoaim.WriteCvar();
	rawinput.WriteCvar();

	ToggleLookCheckboxes( true );
}

void CMenuOptionsAdvPage::Reload()
{
	BaseClass::Reload();
	GetConfig();
}

void CMenuOptionsAdvPage::_Init()
{
	// build 90: rows re-pitched 42 -> 46 so each checkbox row gets 14px
	// (header 8..32, first row 56); label-right rows, no above labels
	SectionHeader( secAdv, "Advanced", 16, 8, 1020 );

	// build 90: QMF_NOTIFY came off these eight rows - CheckBox draws the
	// console status string at x+40/y+7 on top of its own label (which
	// starts at x+40), so every row read as two stuck-on-top strings.
	// Without the flag CMenuFramework shows the status on the dialog's
	// bottom hint line instead of inside the row.
	crosshair.SetNameAndStatus( L( "Crosshair" ), L( "Enable the weapon aiming crosshair" ) );
	crosshair.SetRect( 16, 56, 32, 32 );

	invertMouse.SetNameAndStatus( L( "GameUI_ReverseMouse" ), L( "GameUI_ReverseMouseLabel" ) );
	invertMouse.onChanged = VoidCb( &CMenuOptionsAdvPage::PitchInvert );
	invertMouse.SetRect( 16, 102, 32, 32 );

	mouseLook.SetNameAndStatus( L( "GameUI_MouseLook" ), L( "GameUI_MouseLookLabel" ) );
	SET_EVENT( mouseLook.onChanged,
		((CMenuOptionsAdvPage *)pSelf->Parent())->ToggleLookCheckboxes( true ) );
	mouseLook.SetRect( 16, 148, 32, 32 );

	lookSpring.SetNameAndStatus( L( "Look spring" ), L( "Causes the screen to 'spring' back to looking straight ahead when you move forward" ) );
	lookSpring.SetRect( 16, 194, 32, 32 );

	lookStrafe.SetNameAndStatus( L( "Look strafe" ), L( "In combination with your mouse look modifier, causes left-right movements to strafe instead of turn" ) );
	lookStrafe.SetRect( 620, 56, 32, 32 );

	lookFilter.SetNameAndStatus( L( "GameUI_MouseFilter" ), L( "GameUI_MouseFilterLabel" ) );
	lookFilter.SetRect( 620, 102, 32, 32 );

	autoaim.SetNameAndStatus( L( "GameUI_AutoAim" ), L( "GameUI_AutoaimLabel" ) );
	autoaim.SetRect( 620, 148, 32, 32 );

	rawinput.SetNameAndStatus( L( "GameUI_RawInput" ), L( "GameUI_RawInputLabel" ) );
	rawinput.SetRect( 620, 194, 32, 32 );

	AddItem( secAdv );
	AddItem( crosshair );
	AddItem( invertMouse );
	AddItem( mouseLook );
	AddItem( lookSpring );
	AddItem( lookStrafe );
	AddItem( lookFilter );
	AddItem( autoaim );
	AddItem( rawinput );
}

/*
=====================================================================
Mouse tab - Gyroscope sub-page (build 88: menus/Gyro.cpp folded in,
device mode = the built-in phone gyro; self-contained read/write so
the fabs/sign convention stays consistent - "improve it without bugs")
=====================================================================
*/
class CMenuOptionsGyroPage : public CMenuItemsHolder
{
public:
	void Save();

private:
	void _Init() override;
	void Reload() override;

	void GetConfig();

	CMenuAction secGyro;
	CMenuAction secDead;

	CMenuCheckBox enable;
	CMenuSlider pitch, yaw, roll;
	CMenuCheckBox invPitch, invYaw, invRoll;
	CMenuSlider dPitch, dYaw, dRoll;
};

void CMenuOptionsGyroPage::GetConfig()
{
	// device gyro (the m_bGamepadMode == false path of CMenuGyro)
	enable.LinkCvar( "gyro_enable" );

	float _pitch = EngFuncs::GetCvarFloat( "gyro_pitch" );
	float _yaw   = EngFuncs::GetCvarFloat( "gyro_yaw" );
	float _roll  = EngFuncs::GetCvarFloat( "gyro_roll" );

	pitch.SetCurrentValue( fabs( _pitch ));
	yaw.SetCurrentValue( fabs( _yaw ));
	roll.SetCurrentValue( fabs( _roll ));

	invPitch.bChecked = _pitch < 0.0f;
	invYaw.bChecked = _yaw < 0.0f;
	invRoll.bChecked = _roll < 0.0f;

	dPitch.LinkCvar( "gyro_pitch_deadzone" );
	dYaw.LinkCvar( "gyro_yaw_deadzone" );
	dRoll.LinkCvar( "gyro_roll_deadzone" );
}

void CMenuOptionsGyroPage::Save()
{
	enable.WriteCvar();

	float _pitch = pitch.GetCurrentValue();
	if( invPitch.bChecked ) _pitch *= -1.0f;

	float _yaw = yaw.GetCurrentValue();
	if( invYaw.bChecked ) _yaw *= -1.0f;

	float _roll = roll.GetCurrentValue();
	if( invRoll.bChecked ) _roll *= -1.0f;

	EngFuncs::CvarSetValue( "gyro_pitch", _pitch );
	EngFuncs::CvarSetValue( "gyro_yaw", _yaw );
	EngFuncs::CvarSetValue( "gyro_roll", _roll );

	dPitch.WriteCvar();
	dYaw.WriteCvar();
	dRoll.WriteCvar();
}

void CMenuOptionsGyroPage::Reload()
{
	BaseClass::Reload();
	GetConfig();
}

void CMenuOptionsGyroPage::_Init()
{
	// sensitivity sliders left (label draws at y-39, 80px pitch), dead-
	// zones right - same two-column split as CMenuGyro, re-pitched so
	// every label clears the control above it
	// build 90: slider pitch 80 -> 88 (17px between a row and the next
	// label zone) - the third row still ends at y348 inside the 362px
	SectionHeader( secGyro, "Gyroscope", 16, 8, 546 );
	SectionHeader( secDead, "Deadzones", 620, 8, 416 );

	enable.SetNameAndStatus( L( "Enable Built-in Gyroscope" ), L( "Use device motion sensors for aiming" ) );
	enable.SetRect( 16, 48, 32, 32 );

	pitch.Setup( 0.0f, 20.0f, 0.1f );
	pitch.SetNameAndStatus( L( "Pitch" ), L( "Vertical gyro sensitivity" ) );
	invPitch.SetNameAndStatus( L( "Invert" ), L( "Invert vertical gyro axis" ) );
	pitch.SetRect( 16, 140, 360, 32 );
	invPitch.SetRect( 400, 140, 32, 32 );

	yaw.Setup( 0.0f, 20.0f, 0.1f );
	yaw.SetNameAndStatus( L( "Yaw" ), L( "Horizontal gyro sensitivity" ) );
	invYaw.SetNameAndStatus( L( "Invert" ), L( "Invert horizontal gyro axis" ) );
	yaw.SetRect( 16, 228, 360, 32 );
	invYaw.SetRect( 400, 228, 32, 32 );

	roll.Setup( 0.0f, 20.0f, 0.1f );
	roll.SetNameAndStatus( L( "Roll" ), L( "Sideways tilt gyro sensitivity" ) );
	invRoll.SetNameAndStatus( L( "Invert" ), L( "Invert roll axis" ) );
	roll.SetRect( 16, 316, 360, 32 );
	invRoll.SetRect( 400, 316, 32, 32 );

	dPitch.Setup( 0.0f, 10.0f, 0.1f );
	dPitch.SetNameAndStatus( L( "Pitch" ), L( "Gyro pitch deadzone" ) );
	dPitch.SetRect( 620, 140, 416, 32 );

	dYaw.Setup( 0.0f, 10.0f, 0.1f );
	dYaw.SetNameAndStatus( L( "Yaw" ), L( "Gyro yaw deadzone" ) );
	dYaw.SetRect( 620, 228, 416, 32 );

	dRoll.Setup( 0.0f, 10.0f, 0.1f );
	dRoll.SetNameAndStatus( L( "Roll" ), L( "Gyro roll deadzone" ) );
	dRoll.SetRect( 620, 316, 416, 32 );

	AddItem( secGyro );
	AddItem( secDead );
	AddItem( enable );
	AddItem( pitch );
	AddItem( invPitch );
	AddItem( yaw );
	AddItem( invYaw );
	AddItem( roll );
	AddItem( invRoll );
	AddItem( dPitch );
	AddItem( dYaw );
	AddItem( dRoll );
}

/*
=====================================================================
Mouse tab - Gamepad sub-page (build 88: menus/Gamepad.cpp folded in;
its Controls / Gyroscope / Done buttons are dropped - they duplicate
this dialog's tabs and footer - "don't add them")
=====================================================================
*/
enum engineAxis_t
{
	JOY_AXIS_SIDE = 0,
	JOY_AXIS_FWD,
	JOY_AXIS_PITCH,
	JOY_AXIS_YAW,
	JOY_AXIS_RT,
	JOY_AXIS_LT,
	JOY_AXIS_NULL
};

static const char *axisNamesPad[7] =
{
	"Side",
	"Forward",
	"Yaw",
	"Pitch",
	"Right Trigger",
	"Left Trigger",
	"NOT BOUND"
};

class CMenuOptionsGamepadPage : public CMenuItemsHolder
{
public:
	void Save();

private:
	void _Init() override;
	void Reload() override;

	void GetConfig();

	CMenuAction secJoy;
	CMenuAction axisBindLabel;
	CMenuAction secAxes;

	CMenuSlider side, forward, pitch, yaw;
	CMenuCheckBox invSide, invFwd, invPitch, invYaw;
	CMenuSpinControl axisBind[6];
	CMenuCheckBox enableOsk;
};

void CMenuOptionsGamepadPage::GetConfig()
{
	char binding[7] = { 0 };

	enableOsk.LinkCvar( "osk_enable" );

	Q_strncpy( binding, EngFuncs::GetCvarString( "joy_axis_binding" ), sizeof( binding ));

	float _side    = EngFuncs::GetCvarFloat( "joy_side" );
	float _forward = EngFuncs::GetCvarFloat( "joy_forward" );
	float _pitch   = EngFuncs::GetCvarFloat( "joy_pitch" );
	float _yaw     = EngFuncs::GetCvarFloat( "joy_yaw" );

	side.SetCurrentValue( fabs( _side ));
	forward.SetCurrentValue( fabs( _forward ));
	pitch.SetCurrentValue( fabs( _pitch ));
	yaw.SetCurrentValue( fabs( _yaw ));

	invSide.bChecked = _side < 0.0f;
	invFwd.bChecked = _forward < 0.0f;
	invPitch.bChecked = _pitch < 0.0f;
	invYaw.bChecked = _yaw < 0.0f;

	for( unsigned int i = 0; i < sizeof( binding ) - 1; i++ )
	{
		switch( binding[i] )
		{
		case 's':
			axisBind[i].ForceDisplayString( L( axisNamesPad[JOY_AXIS_SIDE] ) );
			axisBind[i].SetCurrentValue( JOY_AXIS_SIDE );
			break;
		case 'f':
			axisBind[i].ForceDisplayString( L( axisNamesPad[JOY_AXIS_FWD] ) );
			axisBind[i].SetCurrentValue( JOY_AXIS_FWD );
			break;
		case 'p':
			axisBind[i].ForceDisplayString( L( axisNamesPad[JOY_AXIS_PITCH] ) );
			axisBind[i].SetCurrentValue( JOY_AXIS_PITCH );
			break;
		case 'y':
			axisBind[i].ForceDisplayString( L( axisNamesPad[JOY_AXIS_YAW] ) );
			axisBind[i].SetCurrentValue( JOY_AXIS_YAW );
			break;
		case 'r':
			axisBind[i].ForceDisplayString( L( axisNamesPad[JOY_AXIS_RT] ) );
			axisBind[i].SetCurrentValue( JOY_AXIS_RT );
			break;
		case 'l':
			axisBind[i].ForceDisplayString( L( axisNamesPad[JOY_AXIS_LT] ) );
			axisBind[i].SetCurrentValue( JOY_AXIS_LT );
			break;
		default:
			axisBind[i].ForceDisplayString( L( axisNamesPad[JOY_AXIS_NULL] ) );
			axisBind[i].SetCurrentValue( JOY_AXIS_NULL );
		}
	}
}

void CMenuOptionsGamepadPage::Save()
{
	float _side = side.GetCurrentValue();
	if( invSide.bChecked ) _side *= -1;

	float _forward = forward.GetCurrentValue();
	if( invFwd.bChecked ) _forward *= -1;

	float _pitch = pitch.GetCurrentValue();
	if( invPitch.bChecked ) _pitch *= -1;

	float _yaw = yaw.GetCurrentValue();
	if( invYaw.bChecked ) _yaw *= -1;

	char binding[7] = { 0 };

	for( int i = 0; i < 6; i++ )
	{
		switch((int)axisBind[i].GetCurrentValue( ))
		{
		case JOY_AXIS_SIDE:  binding[i] = 's'; break;
		case JOY_AXIS_FWD:   binding[i] = 'f'; break;
		case JOY_AXIS_PITCH: binding[i] = 'p'; break;
		case JOY_AXIS_YAW:   binding[i] = 'y'; break;
		case JOY_AXIS_RT:    binding[i] = 'r'; break;
		case JOY_AXIS_LT:    binding[i] = 'l'; break;
		default:             binding[i] = '0'; break;
		}
	}

	EngFuncs::CvarSetValue( "joy_side", _side );
	EngFuncs::CvarSetValue( "joy_forward", _forward );
	EngFuncs::CvarSetValue( "joy_pitch", _pitch );
	EngFuncs::CvarSetValue( "joy_yaw", _yaw );
	EngFuncs::CvarSetString( "joy_axis_binding", binding );

	enableOsk.WriteCvar();
}

void CMenuOptionsGamepadPage::Reload()
{
	BaseClass::Reload();
	GetConfig();
}

void CMenuOptionsGamepadPage::_Init()
{
	static CStringArrayModel axisModel( axisNamesPad, V_ARRAYSIZE( axisNamesPad ));

	// left: binding map (spins draw no top label, szName is empty);
	// right: axis sensitivities with labels at y-39
	SectionHeader( secJoy, "Gamepad", 16, 8, 520 );
	SectionHeader( axisBindLabel, "Axis binding map", 16, 48, 520 );

	for( int i = 0; i < 6; i++ )
	{
		axisBind[i].szStatusText = L( "Set axis binding" );
		axisBind[i].Setup( &axisModel );
		axisBind[i].SetRect( 16, 84 + i * 40, 340, 32 );
	}

	enableOsk.SetNameAndStatus( L( "Builtin on-screen keyboard" ), L( "Enable builtin on-screen keyboard in case your platform doesn't have any" ));
	enableOsk.SetRect( 16, 324, 32, 32 );

	SectionHeader( secAxes, "Axis sensitivity", 620, 8, 340 );

	side.Setup( 0.0f, 1.0f, 0.1f );
	side.SetNameAndStatus( L( "Side" ), L( "Side movement sensitivity" ) );
	invSide.SetNameAndStatus( L( "Invert" ), L( "Invert side movement axis" ) );
	side.SetRect( 620, 84, 264, 32 );
	invSide.SetRect( 892, 84, 32, 32 );

	forward.Setup( 0.0f, 1.0f, 0.1f );
	forward.SetNameAndStatus( L( "Forward" ), L( "Forward movement sensitivity" ) );
	invFwd.SetNameAndStatus( L( "Invert" ), L( "Invert forward movement axis" ) );
	// build 90: right-column slider pitch 78 -> 80 (13/9px clearances)
	// - four labeled sliders plus the 24px header do not fit 362px with
	// a full 12px grid, this is the largest even pitch available
	forward.SetRect( 620, 164, 264, 32 );
	invFwd.SetRect( 892, 164, 32, 32 );

	pitch.Setup( 0.0f, 200.0f, 0.1f );
	pitch.SetNameAndStatus( L( "Look X" ), L( "Horizontal look sensitivity" ) );
	invPitch.SetNameAndStatus( L( "Invert" ), L( "Invert pitch axis" ) );
	pitch.SetRect( 620, 244, 264, 32 );
	invPitch.SetRect( 892, 244, 32, 32 );

	yaw.Setup( 0.0f, 200.0f, 0.1f );
	yaw.SetNameAndStatus( L( "Look Y" ), L( "Vertical look sensitivity" ) );
	invYaw.SetNameAndStatus( L( "Invert" ), L( "Invert yaw axis" ) );
	yaw.SetRect( 620, 324, 264, 32 );
	invYaw.SetRect( 892, 324, 32, 32 );

	AddItem( secJoy );
	AddItem( axisBindLabel );
	AddItem( secAxes );
	AddItem( enableOsk );
	AddItem( side );
	AddItem( invSide );
	AddItem( forward );
	AddItem( invFwd );
	AddItem( pitch );
	AddItem( invPitch );
	AddItem( yaw );
	AddItem( invYaw );
	for( int i = 0; i < 6; i++ )
		AddItem( axisBind[i] );
}

/*
=====================================================================
Audio tab
=====================================================================
*/
class CMenuOptionsAudioPage : public CMenuItemsHolder
{
public:
	void Save();

private:
	void _Init() override;

	CMenuSlider soundVolume;
	CMenuSlider musicVolume;
	CMenuSlider suitVolume;
	CMenuCheckBox muteFocusLost;
	CMenuAction secVolume; // build 58 section header
};

void CMenuOptionsAudioPage::Save()
{
	soundVolume.WriteCvar();
	musicVolume.WriteCvar();
	suitVolume.WriteCvar();
	muteFocusLost.WriteCvar();
}

void CMenuOptionsAudioPage::_Init()
{
	SectionHeader( secVolume, "Volume", 16, 8, 1020 );

	// A7 build 87: 88px pitch - each slider label at y-39 clears the
	// control/header above it (the old y52 put the first label on the
	// "Volume" header - "stuck text")
	// build 90: whole stack moved down 8px (96/184/272) with the mute
	// row at 330 - the Audio page has room, so the rows spread to ~19px
	soundVolume.szName = L( "GameUI_SoundEffectVolume" );
	soundVolume.Setup( 0.0, 1.0, 0.05f );
	soundVolume.LinkCvar( "volume" );
	soundVolume.SetRect( 16, 96, 1020, 32 );

	musicVolume.szName = L( "GameUI_MP3Volume" );
	musicVolume.Setup( 0.0, 1.0, 0.05f );
	musicVolume.LinkCvar( "MP3Volume" );
	musicVolume.SetRect( 16, 184, 1020, 32 );

	suitVolume.szName = L( "GameUI_HEVSuitVolume" );
	suitVolume.Setup( 0.0, 1.0, 0.05f );
	suitVolume.LinkCvar( "suitvolume" );
	suitVolume.SetRect( 16, 272, 1020, 32 );

	muteFocusLost.SetNameAndStatus( L( "Mute when inactive" ), NULL );
	muteFocusLost.LinkCvar( "snd_mute_losefocus" );
	muteFocusLost.SetRect( 16, 330, 32, 32 );

	AddItem( secVolume );
	AddItem( soundVolume );
	AddItem( musicVolume );
	AddItem( suitVolume );
	AddItem( muteFocusLost );
}

/*
=====================================================================
Video tab - "Video options" sub-tab (build 85 nested tabs)
=====================================================================
*/
class CMenuOptionsVideoOptsPage : public CMenuItemsHolder
{
public:
	void Save();

	// build 88: gamma/brightness are REMAPPED cvars (slider 0..1 ->
	// gamma 1.8..3.0, brightness 0..3.0) - same callbacks as the
	// original xash VideoOptions menu. Without them the page pushed
	// raw 0..1 slider values into gamma ("cvars should work").
	void UpdateConfig();
	void GetConfig();

private:
	void _Init() override;
	void Reload() override; // build 88: renderer-dependent toggles

	CMenuAction secDisplay;   // build 58 section header
	CMenuAction secRendering; // build 58 section header
	CMenuSlider gammaIntensity;
	CMenuSlider brightness;
	CMenuCheckBox ripple;
	CMenuCheckBox hudscale;
	CMenuCheckBox detailtex;
	// build 88: the missing xash VideoOptions controls
	CMenuCheckBox vbo;
	CMenuCheckBox overbright;
	CMenuCheckBox filtering;
};

void CMenuOptionsVideoOptsPage::Save()
{
	// gamma/brightness are already written live by UpdateConfig
	// (onChanged) with the 1.8..3.0 / 0..3.0 remap - WriteCvar here
	// would push raw slider values into the cvars
	detailtex.WriteCvar();
	vbo.WriteCvar();
	ripple.WriteCvar();
	overbright.WriteCvar();
	filtering.WriteCvar();
	hudscale.WriteCvar();
}

void CMenuOptionsVideoOptsPage::UpdateConfig()
{
	float val1 = RemapVal( gammaIntensity.GetCurrentValue(), 0.0, 1.0, 1.8, 3.0 );
	float val2 = RemapVal( brightness.GetCurrentValue(), 0.0, 1.0, 0.0, 3.0 );
	EngFuncs::CvarSetValue( "gamma", val1 );
	EngFuncs::CvarSetValue( "brightness", val2 );
}

void CMenuOptionsVideoOptsPage::GetConfig()
{
	float val1 = EngFuncs::GetCvarFloat( "gamma" );
	float val2 = EngFuncs::GetCvarFloat( "brightness" );

	gammaIntensity.SetCurrentValue( RemapVal( val1, 1.8f, 3.0f, 0.0f, 1.0f ) );
	brightness.SetCurrentValue( RemapVal( val2, 0.0f, 3.0f, 0.0f, 1.0f ) );
	gammaIntensity.SetOriginalValue( val1 );
	brightness.SetOriginalValue( val2 );
}

// build 88: same renderer-dependent logic as the original xash
// VideoOptions menu - the toggles only write cvars the loaded ref
// really has, and texture filtering flips between ref_soft's sw_texfilt
// and ref_gl's inverted gl_texture_nearest
void CMenuOptionsVideoOptsPage::Reload()
{
	bool gl_active = !strnicmp( EngFuncs::GetCvarString( "r_refdll_loaded" ), "gl", 2 );
	bool soft_active = !stricmp( EngFuncs::GetCvarString( "r_refdll_loaded" ), "soft" );

	detailtex.SetGrayed( !gl_active );
	detailtex.SetInactive( !gl_active );

	vbo.SetGrayed( !gl_active );
	vbo.SetInactive( !gl_active );

	ripple.SetGrayed( !gl_active );
	ripple.SetInactive( !gl_active );

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
				cb->bChecked = EngFuncs::GetCvarFloat( cb->CvarName( )) == 0.0f;
			});
			SET_EVENT_MULTI( filtering.onCvarWrite,
			{
				CMenuCheckBox *cb = (CMenuCheckBox *)pSelf;
				EngFuncs::CvarSetValue( cb->CvarName(), cb->bChecked ? 0.0f : 1.0f );
			});

			filtering.LinkCvar( "gl_texture_nearest" );
		}
	}
	else
	{
		filtering.SetGrayed( true );
		filtering.SetInactive( true );
	}

	BaseClass::Reload();
}

void CMenuOptionsVideoOptsPage::_Init()
{
	// build 88: ALL the xash VideoOptions controls (user: "some of them
	// are missed, add all of them"), laid out in two columns to fit the
	// 362px nested-page height. Labels draw at y-39 for the sliders;
	// checkboxes label to their right, so the right column is a plain
	// 42px stack. Content: Display - gamma, brightness, HUD scale;
	// Rendering - water ripples, VBO, overbrights, detail textures,
	// texture filtering.
	SectionHeader( secDisplay, "Display", 16, 8, 520 );

	gammaIntensity.szName = L( "GameUI_Gamma" );
	gammaIntensity.Setup( 0.0, 1.0, 0.025 );
	gammaIntensity.LinkCvar( "gamma" );
	// build 88: remapped cvar - see UpdateConfig/GetConfig
	gammaIntensity.onChanged = VoidCb( &CMenuOptionsVideoOptsPage::UpdateConfig );
	gammaIntensity.onCvarGet = VoidCb( &CMenuOptionsVideoOptsPage::GetConfig );
	gammaIntensity.SetRect( 16, 88, 520, 32 );

	brightness.szName = L( "GameUI_Brightness" );
	brightness.Setup( 0, 1.0, 0.025 );
	brightness.LinkCvar( "brightness" );
	brightness.onChanged = VoidCb( &CMenuOptionsVideoOptsPage::UpdateConfig );
	brightness.onCvarGet = VoidCb( &CMenuOptionsVideoOptsPage::GetConfig );
	brightness.SetRect( 16, 178, 520, 32 );

	hudscale.SetNameAndStatus( L( "Auto scale HUD" ), NULL );
	hudscale.LinkCvar( "hud_scale" );
	hudscale.SetRect( 16, 240, 32, 32 );

	SectionHeader( secRendering, "Rendering", 560, 8, 480 );

	// build 90: checkbox column re-pitched 42 -> 46 (48/94/140/186/232)
	ripple.SetNameAndStatus( L( "Water ripples" ), L( "Rippling water surfaces" ));
	ripple.LinkCvar( "r_ripple" );
	ripple.SetRect( 560, 48, 32, 32 );

	vbo.SetNameAndStatus( L( "Use VBO" ), L( "Vertex buffer objects (faster rendering)" ));
	vbo.LinkCvar( "gl_vbo" );
	vbo.SetRect( 560, 94, 32, 32 );

	overbright.SetNameAndStatus( L( "Overbrights" ), L( "Brighter lightmaps" ));
	overbright.LinkCvar( "gl_overbright" );
	overbright.SetRect( 560, 140, 32, 32 );

	detailtex.SetNameAndStatus( L( "Detail textures" ), NULL );
	detailtex.LinkCvar( "r_detailtextures" );
	detailtex.SetRect( 560, 186, 32, 32 );

	filtering.SetNameAndStatus( L( "Texture filtering" ), NULL );
	// cvar wired in Reload() - ref_gl and ref_soft use different names
	filtering.SetRect( 560, 232, 32, 32 );

	AddItem( secDisplay );
	AddItem( gammaIntensity );
	AddItem( brightness );
	AddItem( hudscale );
	AddItem( secRendering );
	AddItem( ripple );
	AddItem( vbo );
	AddItem( overbright );
	AddItem( detailtex );
	AddItem( filtering );
}

/*
====================================================================
Video tab - "Video modes" sub-tab (build 85: the old fullscreen
CMenuVidModes screen folded into Options)
====================================================================
*/
class CMenuOptionsVidModesModel : public CMenuBaseArrayModel
{
public:
	void Update() override;
	int GetRows() const override
	{
		return m_szModes.Count();
	}

	const char *GetText( int line ) override
	{
		return m_szModes[line];
	}

private:
	CUtlVector<const char *> m_szModes;
};

class CMenuOptionsRenderersModel : public CMenuBaseArrayModel
{
public:
	void Update() override;
	int GetRows() const override
	{
		return m_refs.Count();
	}

	const char *GetText( int i ) override
	{
		return m_refs[i].readable;
	}

	const char *GetShortName( int i )
	{
		return m_refs[i].shortName;
	}

	void Add( const char *str )
	{
		refdll temp;

		Q_strncpy( temp.shortName, str, sizeof( temp.shortName ));
		Q_strncpy( temp.readable, str, sizeof( temp.readable ));

		m_refs.AddToTail( temp );
	}
private:
	struct refdll
	{
		char shortName[64];
		char readable[64];
	};

	CUtlVector<refdll> m_refs;
};

class CMenuOptionsVideoModesPage : public CMenuItemsHolder
{
public:
	CMenuOptionsVideoModesPage() : restartMsgBox( true ),
		prevMode( 0 ), prevModeX( 0 ), prevModeY( 0 ), prevFullscreen( 0 ),
		testModeTimer( 0 )
	{ }

	// build 87: the footer's global OK / Apply drives the FULL apply -
	// resolution, window mode, renderer and vsync in one SetConfig (with
	// the test-mode safety revert). The build-85 sub-page-local Apply is
	// gone (user: "remove apply, there's two for it"). Returns false
	// while a test-mode/restart box is pending, so OK keeps the dialog
	// open and TickTestMode can still revert.
	bool Save()
	{
		SetConfig();
		return !testModeMsgBox.IsVisible( ) && !restartMsgBox.IsVisible( );
	}

	void SetMode( int mode );
	void SetMode( int w, int h );
	void SetConfig();
	void RevertChanges();
	void ApplyChanges();
	void FinalizeChanges();
	void GetConfig();

	void GetRendererConfig();
	void WriteRendererConfig();
	bool IsRendererChanged();

	// build 85: test-mode countdown, ticked from CMenuOptions::Draw every
	// frame while the dialog is open (was CMenuVidModes::Draw)
	void TickTestMode();

private:
	void _Init() override;
	void Reload() override;

	// "restart required" acknowledged - close the whole Options dialog
	// (the old standalone screen just hid itself at this point)
	void CloseDialog();

	CMenuCheckBox vsync;

	CMenuTable vidList;
	CMenuOptionsVidModesModel vidListModel;

	CMenuYesNoMessageBox testModeMsgBox;
	CMenuYesNoMessageBox restartMsgBox;

	CMenuOptionsRenderersModel renderersModel;
	CMenuSpinControl renderers;
	CMenuSpinControl windowMode;

	int prevMode;
	int prevModeX;
	int prevModeY;
	int prevFullscreen;
	float testModeTimer;
	char testModeMsg[256];
};

void CMenuOptionsVidModesModel::Update()
{
	m_szModes.Purge();

	for( int i = 0; ; i++ )
	{
		const char *mode = EngFuncs::GetModeString( i );

		if( !mode )
			break;

		m_szModes.AddToTail( mode );
	}
}

void CMenuOptionsRenderersModel::Update()
{
	const char *r_refdll_loaded = EngFuncs::GetCvarString( "r_refdll_loaded" );
	m_refs.Purge();

	for( int i = 0; ; i++ )
	{
		refdll temp;

		if( !EngFuncs::GetRenderers( i, temp.shortName, sizeof( temp.shortName ), temp.readable, sizeof( temp.readable )))
			break;

		// append asterisk to currently loaded renderer
		if( !stricmp( r_refdll_loaded, temp.shortName ))
			strncat( temp.readable, "*", sizeof( temp.readable ) - strlen( temp.readable ) - 1 );

		m_refs.AddToTail( temp );
	}
}

void CMenuOptionsVideoModesPage::GetRendererConfig()
{
	// get current _configured_ renderer
	const char *refdll = EngFuncs::GetCvarString( "r_refdll" );

	if( !refdll[0] )
	{
		renderers.SetCurrentValue( 0.0f );
		return;
	}

	int i;
	for( i = 0; i < renderersModel.GetRows(); i++ )
	{
		if( !stricmp( renderersModel.GetShortName( i ), refdll ))
		{
			renderers.SetCurrentValue( i );
			break;
		}
	}

	if( i == renderersModel.GetRows( ))
	{
		renderersModel.Add( refdll ); // add unknown user selection
		renderers.SetCurrentValue( i );
	}
}

void CMenuOptionsVideoModesPage::WriteRendererConfig()
{
	int i = renderers.GetCurrentValue();
	EngFuncs::CvarSetString( "r_refdll", renderersModel.GetShortName( i ));
}

// the renderer library is only loaded on startup
bool CMenuOptionsVideoModesPage::IsRendererChanged()
{
	int i = renderers.GetCurrentValue();

	if( i < 0 || i >= renderersModel.GetRows( ))
		return false;

	return stricmp( renderersModel.GetShortName( i ), EngFuncs::GetCvarString( "r_refdll_loaded" )) != 0;
}

void CMenuOptionsVideoModesPage::GetConfig()
{
	float fullscreen = EngFuncs::GetCvarFloat( "fullscreen" );
	fullscreen = bound( 0, fullscreen, 2 ); // windowed, fullscreen, borderless

	float vid_mode = EngFuncs::GetCvarFloat( "vid_mode" );
	vid_mode = bound( 0, vid_mode, vidListModel.GetRows( ));

	windowMode.SetCurrentValue( fullscreen );
	vidList.SetCurrentIndex( vid_mode );

	renderers.UpdateCvar( true );
	vsync.UpdateCvar( true );

	ApplyChanges();
}

void CMenuOptionsVideoModesPage::SetMode( int w, int h )
{
	// only possible on Xash3D FWGS!
	EngFuncs::ClientCmdF( true, "vid_setmode %i %i\n", w, h );
}

void CMenuOptionsVideoModesPage::SetMode( int mode )
{
	// vid_setmode is a new command, which does not depends on
	// static resolution list but instead uses dynamic resolution
	// list provided by video backend
	EngFuncs::ClientCmdF( true, "vid_setmode %i\n", mode );
}

/*
================
UI_VidModes_SetConfig
================
*/
void CMenuOptionsVideoModesPage::SetConfig( )
{
	bool testMode = false;
	int  currentWindowModeIndex = windowMode.GetCurrentValue();
	int  currentModeIndex = vidList.GetCurrentIndex();
	bool isVidModeChanged = prevMode != currentModeIndex;
	bool isWindowedModeChanged = prevFullscreen != currentWindowModeIndex;

	/*
	checking windowed mode first because it'll be checked next in
	screen resolution changing code, otherwise when user try to
	change screen resolution and windowed flag at same time,
	only resolution will be changed.
	*/

	if( isWindowedModeChanged ) // moved to fullscreen, enable test mode
		testMode |= currentWindowModeIndex > 0;

	if( isVidModeChanged ) // have changed resolution, but enable test mode only in fullscreen
		testMode |= currentWindowModeIndex > 0;

	if( testMode ) // show this dialog before changing any settings
	{
		testModeMsgBox.Show();
		testModeTimer = gpGlobals->time + 10.0f; // ten seconds should be enough
	}

	vsync.WriteCvar();
	renderers.WriteCvar();

	if( isWindowedModeChanged )
		EngFuncs::CvarSetValue( "fullscreen", currentWindowModeIndex );

	if( isVidModeChanged )
	{
		SetMode( currentModeIndex );
		EngFuncs::CvarSetValue( "vid_mode", currentModeIndex );
		vidList.SetCurrentIndex( currentModeIndex );
	}

	// let the test mode dialog have the screen to itself, it's on a timer
	if( testMode )
		return;

	// build 85: the old standalone screen hid itself here; inside Options
	// the dialog stays open (changes are already applied), it only nags
	// about a renderer change
	if( IsRendererChanged( ))
		restartMsgBox.Show();
}

void CMenuOptionsVideoModesPage::FinalizeChanges()
{
	prevMode = EngFuncs::GetCvarFloat( "vid_mode" );
	prevFullscreen = EngFuncs::GetCvarFloat( "fullscreen" );
	prevModeX = EngFuncs::GetCvarFloat( "width" );
	prevModeY = EngFuncs::GetCvarFloat( "height" );
}

void CMenuOptionsVideoModesPage::ApplyChanges()
{
	if( testModeMsgBox.IsVisible( ))
		return;

	FinalizeChanges();
}

void CMenuOptionsVideoModesPage::RevertChanges()
{
	bool fullscreenChanged = false;

	// if we switched FROM fullscreen TO windowed, then we must get rid of fullscreen mode
	// so we can easily change the window size, without jerking display mode
	if( prevFullscreen == false && EngFuncs::GetCvarFloat( "fullscreen" ) != 0.0f )
	{
		EngFuncs::CvarSetValue( "fullscreen", prevFullscreen );
		fullscreenChanged = true;
	}

	SetMode( prevModeX, prevModeY );

	// otherwise, we better to set window size at first, then switch TO fullscreen
	if( !fullscreenChanged )
		EngFuncs::CvarSetValue( "fullscreen", prevFullscreen );
}

// build 85: per-frame test-mode countdown; driven from CMenuOptions::Draw
// so it keeps ticking even when another tab or sub-tab is in front
void CMenuOptionsVideoModesPage::TickTestMode()
{
	if( testModeMsgBox.IsVisible( ) && !FBitSet( testModeMsgBox.iFlags, QMF_CLOSING ))
	{
		if( testModeTimer - gpGlobals->time > 0 )
		{
			snprintf( testModeMsg, sizeof( testModeMsg ), L( "Keep this resolution? %i seconds remaining" ), (int)(testModeTimer - gpGlobals->time) );
		}
		else
		{
			testModeMsgBox.Hide();
			RevertChanges();
		}
	}
}

void CMenuOptionsVideoModesPage::CloseDialog()
{
	// climb to the root window (the Options framework) and close it -
	// VoidCb dispatches on the message box's parent, which is this page
	CMenuBaseItem *root = this;

	while( root->Parent( ))
		root = root->Parent( );

	root->Hide();
}

/*
================
UI_VidModes_Init (folded into the Video tab, build 85)
================
*/
void CMenuOptionsVideoModesPage::_Init( void )
{
	static const char *windowModeStr[] =
	{
		L( "Windowed" ),
		L( "Fullscreen" ),
		L( "Borderless" ),
	};
	static CStringArrayModel windowModeModel( windowModeStr, V_ARRAYSIZE( windowModeStr ));

	// right side: resolution list (main element of the old fullscreen
	// Video modes screen); explicit width because a page is a 0x0 holder,
	// so the negative-width trick would resolve to nothing
	vidList.SetRect( 380, 16, 660, 308 );
	vidList.SetupColumn( 0, L( "GameUI_Resolution" ), 1.0f );
	vidList.SetModel( &vidListModel );

	vsync.szName = L( "GameUI_VSync" );
	vsync.SetRect( 16, 232, 32, 32 );
	vsync.LinkCvar( "gl_vsync" );

	testModeMsgBox.SetMessage( testModeMsg );
	testModeMsgBox.onPositive = VoidCb( &CMenuOptionsVideoModesPage::FinalizeChanges );
	testModeMsgBox.onNegative = VoidCb( &CMenuOptionsVideoModesPage::RevertChanges );
	testModeMsgBox.Link( this );

	restartMsgBox.SetMessage( L( "Restart the game to apply changes" ) );
	restartMsgBox.onPositive = VoidCb( &CMenuOptionsVideoModesPage::CloseDialog );
	restartMsgBox.Link( this );

	renderersModel.Update();
	renderers.szName = L( "GameUI_Renderer" );
	renderers.Setup( &renderersModel );
	renderers.SetRect( 16, 60, 320, 32 );
	renderers.SetCharSize( QM_SMALLFONT );
	renderers.onCvarGet = VoidCb( &CMenuOptionsVideoModesPage::GetRendererConfig );
	renderers.onCvarWrite = VoidCb( &CMenuOptionsVideoModesPage::WriteRendererConfig );

	// build 90: window mode dropped to y148 (small-font label clears the
	// renderer box by 26px) and vsync to y232 - its normal-font label
	// zone (y-39) used to start 1px under the window-mode box ("stuck")
	windowMode.szName = L( "Window mode" );
	windowMode.Setup( &windowModeModel );
	windowMode.SetRect( 16, 148, 320, 32 );
	windowMode.SetCharSize( QM_SMALLFONT );
	SET_EVENT_MULTI( windowMode.onChanged,
	{
		// disable vid modes list when borderless is used
		CMenuOptionsVideoModesPage *parent = pSelf->GetParent( CMenuOptionsVideoModesPage );
		parent->vidList.SetGrayed( parent->windowMode.GetCurrentValue( ) == 2 );
		parent->vidList.SetInactive( parent->windowMode.GetCurrentValue( ) == 2 );
	});

	// build 87: no local Apply anymore - the footer OK / Apply runs the
	// full SetConfig (resolution, window mode, renderer, vsync)

	AddItem( renderers );
	AddItem( windowMode );
	AddItem( vsync );
	AddItem( vidList );

	renderers.LinkCvar( "r_refdll", CMenuEditable::CVAR_STRING );
}

void CMenuOptionsVideoModesPage::Reload()
{
	BaseClass::Reload();
	GetConfig();
}

/*
=====================================================================
Touch tab - build 58 removed it from Options; build 88 puts it BACK
here as nested tabs (Touch options | Touch buttons), like Video
=====================================================================
*/

/*
=====================================================================
Touch tab - "Touch options" sub-page (build 88: menus/TouchOptions.cpp
folded in as nested tabs, like the Video tab. Fullscreen banner/Done
paths dropped - the dialog footer owns OK/Cancel/Apply)
=====================================================================
*/
class CMenuOptionsTouchOptsPage : public CMenuItemsHolder
{
public:
	CMenuOptionsTouchOptsPage()
	{
	} // CProfilesModel has no parent - same as the original TouchOptions model

	void Save();

	// profile actions (same flow as CMenuTouchOptions)
	void ApplyProfile();
	void SaveProfile();
	void DeleteMsgBox();
	void ResetMsgBox();
	void DeleteProfileCb();
	void ResetButtonsCb();
	void UpdateProfiles();

	// preset/profile list (same shape as CMenuTouchOptions::
	// CProfiliesListModel)
	class CProfilesModel : public CStringVectorModel
	{
	public:
		CProfilesModel() : CStringVectorModel(), iHighlight( 0 ), firstProfile( 0 ) { }
		void Update();
		int iHighlight;
		int firstProfile;
	} model;

private:
	void _Init() override;
	void Reload() override;

	void GetConfig();

	CMenuAction secSens;
	CMenuAction secTouch;
	CMenuAction secNonlin;
	CMenuAction secProfiles;

	CMenuSlider lookX, lookY, moveX, moveY;
	CMenuCheckBox enable, grid, nomouse, safearea;
	CMenuSpinControl gridsize;
	CMenuCheckBox acceleration;
	CMenuSlider power, multiplier, exponent;

	CMenuTable profiles;
	CMenuField profilename;
	CMenuPicButton reset, removeBtn, saveBtn, applyBtn;
	CMenuYesNoMessageBox msgBox;
};

void CMenuOptionsTouchOptsPage::CProfilesModel::Update( void )
{
	char **filenames;
	int numFiles;
	const char *curprofile;

	RemoveAll();

	filenames = EngFuncs::GetFilesList( "touch_presets/*.cfg", &numFiles, true );

	if( filenames && numFiles > 0 )
	{
		AddToTail( L( "Presets:" ));

		for( int j = 0; j < numFiles; j++ )
		{
			char profileDesc[128];
			COM_FileBase( filenames[j], profileDesc, sizeof( profileDesc ));
			AddToTail( profileDesc );
		}
	}

	filenames = EngFuncs::GetFilesList( "touch_profiles/*.cfg", &numFiles, true );
	curprofile = EngFuncs::GetCvarString( "touch_config_file" );

	AddToTail( L( "Profiles:" ));

	firstProfile = Count();
	AddToTail( "default" );

	for( int j = 0; j < numFiles; j++ )
	{
		char profileDesc[128];
		COM_FileBase( filenames[j], profileDesc, sizeof( profileDesc ));
		AddToTail( profileDesc );
		if( !strcmp( filenames[j], curprofile ))
			iHighlight = Count() - 1;
	}
}

void CMenuOptionsTouchOptsPage::Save()
{
	lookX.WriteCvar();
	lookY.WriteCvar();
	moveX.WriteCvar();
	moveY.WriteCvar();
	gridsize.WriteCvar();
	grid.WriteCvar();
	enable.WriteCvar();
	nomouse.WriteCvar();
	safearea.WriteCvar(); // the standalone menu forgot this one
	acceleration.WriteCvar();
	power.WriteCvar();
	multiplier.WriteCvar();
	exponent.WriteCvar();
}

void CMenuOptionsTouchOptsPage::GetConfig()
{
	grid.UpdateEditable();
	gridsize.UpdateEditable();
	lookX.UpdateEditable();
	lookY.UpdateEditable();
	moveX.UpdateEditable();
	moveY.UpdateEditable();
	enable.UpdateEditable();
	nomouse.UpdateEditable();
	safearea.UpdateEditable();
	acceleration.UpdateEditable();
	power.UpdateEditable();
	multiplier.UpdateEditable();
	exponent.UpdateEditable();
}

void CMenuOptionsTouchOptsPage::ResetMsgBox()
{
	msgBox.SetMessage( L( "Reset sensitivity options?" ) );
	msgBox.onPositive = VoidCb( &CMenuOptionsTouchOptsPage::ResetButtonsCb );
	msgBox.Show();
}

void CMenuOptionsTouchOptsPage::DeleteMsgBox()
{
	msgBox.SetMessage( L( "Delete selected profile?" ) );
	msgBox.onPositive = VoidCb( &CMenuOptionsTouchOptsPage::DeleteProfileCb );
	msgBox.Show();
}

void CMenuOptionsTouchOptsPage::ApplyProfile()
{
	int i = profiles.GetCurrentIndex();

	// preset selected
	if( i > 0 && i < model.firstProfile - 1 )
	{
		char command[256];
		const char *curconfig = EngFuncs::GetCvarString( "touch_config_file" );
		snprintf( command, sizeof( command ), "exec \"touch_presets/%s\"\n", model[i].Get());
		EngFuncs::ClientCmd( 1, command );

		while( EngFuncs::FileExists( curconfig, true ))
		{
			char copystring[256];
			char filebase[256];

			COM_FileBase( curconfig, filebase, sizeof( filebase ));

			if( snprintf( copystring, sizeof( copystring ), "touch_profiles/%s (new).cfg", filebase ) > (int)sizeof( copystring ) - 1 )
				break;

			EngFuncs::CvarSetString( "touch_config_file", copystring );
			curconfig = EngFuncs::GetCvarString( "touch_config_file" );
		}
	}
	else if( i == model.firstProfile )
		EngFuncs::ClientCmd( 1, "exec touch.cfg\n" );
	else if( i > model.firstProfile )
	{
		char command[256];
		snprintf( command, sizeof( command ), "exec \"touch_profiles/%s\"\n", model[i].Get());
		EngFuncs::ClientCmd( 1, command );
	}

	// try save config
	EngFuncs::ClientCmd( 1, "touch_writeconfig\n" );

	// check if it failed and reset profile to default if it is
	if( !EngFuncs::FileExists( EngFuncs::GetCvarString( "touch_config_file" ), true ))
	{
		EngFuncs::CvarSetString( "touch_config_file", "touch.cfg" );
		profiles.SetCurrentIndex( model.firstProfile );
	}
	model.Update();
	GetConfig();
}

void CMenuOptionsTouchOptsPage::SaveProfile()
{
	if( profilename.GetBuffer()[0] )
	{
		char name[512];

		EngFuncs::CvarSetStringF( "touch_config_file", "touch_profiles/%s.cfg", profilename.GetBuffer());

		Com_EscapeCommand( name, profilename.GetBuffer(), sizeof( name ));
		EngFuncs::ClientCmdF( 1, "touch_exportconfig \"touch_profiles/%s.cfg\"\n", name );
	}

	model.Update();
	profilename.Clear();
}

void CMenuOptionsTouchOptsPage::UpdateProfiles()
{
	char curprofile[256];
	bool isCurrent = false;
	int idx = profiles.GetCurrentIndex();

	COM_FileBase( EngFuncs::GetCvarString( "touch_config_file" ), curprofile, sizeof( curprofile ));

	if( model.IsValidIndex( idx ))
		isCurrent = !strcmp( curprofile, model[idx] );

	// list changed, update available options
	removeBtn.SetGrayed( true );
	if(( idx > model.firstProfile ) && !isCurrent )
		removeBtn.SetGrayed( false );

	applyBtn.SetGrayed( false );
	if( idx == 0 || idx == model.firstProfile - 1 )
		profiles.SetCurrentIndex( idx + 1 );
	if( isCurrent )
		applyBtn.SetGrayed( true );
}

void CMenuOptionsTouchOptsPage::DeleteProfileCb()
{
	char command[256];

	if( profiles.GetCurrentIndex() <= model.firstProfile )
		return;

	snprintf( command, sizeof( command ), "touch_deleteprofile \"%s\"\n", model[profiles.GetCurrentIndex()].Get());
	EngFuncs::ClientCmd( true, command );

	model.Update();
}

void CMenuOptionsTouchOptsPage::ResetButtonsCb()
{
	EngFuncs::ClientCmd( false, "touch_pitch 90\n" );
	EngFuncs::ClientCmd( false, "touch_yaw 120\n" );
	EngFuncs::ClientCmd( false, "touch_forwardzone 0.06\n" );
	EngFuncs::ClientCmd( false, "touch_sidezone 0.06\n" );
	EngFuncs::ClientCmd( false, "touch_grid 1\n" );
	EngFuncs::ClientCmd( false, "touch_grid_count 50\n" );
	EngFuncs::ClientCmd( false, "touch_nonlinear_look 0\n" );
	EngFuncs::ClientCmd( false, "touch_pow_factor 1.3\n" );
	EngFuncs::ClientCmd( false, "touch_pow_mult 400\n" );
	EngFuncs::ClientCmd( true,  "touch_exp_mult 0\n" );

	GetConfig();
}

void CMenuOptionsTouchOptsPage::_Init()
{
	// build 88: four columns inside 1120 x 362 - sensitivity sliders,
	// general toggles, nonlinear-look power sliders, profile list.
	// Sliders/fields label at y-39, checkboxes label to the right.
	SectionHeader( secSens, "Sensitivity", 16, 8, 260 );

	// build 90: all four sensitivity sliders re-pitched 72 -> 80 (rows
	// 84/164/244/324) - the old 72px pitch put each "Look/Side" label
	// 7px INSIDE the slider above it ("stuck")
	lookX.SetNameAndStatus( L( "Look X" ), L( "Horizontal look sensitivity" ));
	lookX.Setup( 50, 500, 5 );
	lookX.LinkCvar( "touch_yaw" );
	lookX.SetRect( 16, 84, 260, 32 );

	lookY.SetNameAndStatus( L( "Look Y" ), L( "Vertical look sensitivity" ));
	lookY.Setup( 50, 500, 5 );
	lookY.LinkCvar( "touch_pitch" );
	lookY.SetRect( 16, 164, 260, 32 );

	moveX.SetNameAndStatus( L( "Side" ), L( "Side movement sensitivity" ));
	moveX.Setup( 0.02, 1.0, 0.05 );
	moveX.LinkCvar( "touch_sidezone" );
	moveX.SetRect( 16, 244, 260, 32 );

	moveY.SetNameAndStatus( L( "Forward" ), L( "Forward movement sensitivity" ));
	moveY.Setup( 0.02, 1.0, 0.05 );
	moveY.LinkCvar( "touch_forwardzone" );
	moveY.SetRect( 16, 324, 260, 32 );

	SectionHeader( secTouch, "Touch controls", 296, 8, 260 );

	enable.SetNameAndStatus( L( "Enable touch" ), L( "Enable or disable touch controls" ));
	enable.LinkCvar( "touch_enable" );
	enable.SetRect( 296, 48, 32, 32 );

	grid.SetNameAndStatus( L( "Grid" ), L( "Enable or disable grid" ));
	grid.LinkCvar( "touch_grid_enable" );
	grid.SetRect( 296, 100, 32, 32 );

	gridsize.szStatusText = L( "Set grid size" );
	gridsize.Setup( 25, 100, 5 );
	gridsize.LinkCvar( "touch_grid_count", CMenuEditable::CVAR_VALUE );
	gridsize.SetRect( 440, 100, 116, 32 );

	nomouse.SetNameAndStatus( L( "Ignore mouse" ), L( "Ignore mouse input" ));
	nomouse.LinkCvar( "m_ignore" );
	nomouse.SetRect( 296, 152, 32, 32 );

	safearea.SetNameAndStatus( L( "Avoid screen notch" ), L( "Keep touch controls clear of the screen notch and camera cutout" ));
	safearea.LinkCvar( "touch_safearea" );
	safearea.SetRect( 296, 204, 32, 32 );

	SectionHeader( secNonlin, "Nonlinear look", 576, 8, 260 );

	acceleration.SetNameAndStatus( L( "Acceleration" ), L( "Nonlinear looking (touch_nonlinear_look)" ));
	acceleration.LinkCvar( "touch_nonlinear_look" );
	acceleration.SetRect( 576, 48, 32, 32 );

	// build 90: nonlinear sliders re-pitched 76 -> 80 (128/208/288) so
	// "Power/multiplier/exponent" labels clear the row above by ~9px
	power.SetNameAndStatus( L( "Power factor" ), L( "Power acceleration factor (touch_pow_factor)" ));
	power.Setup( 1, 1.7, 0.05 );
	power.LinkCvar( "touch_pow_factor" );
	power.SetRect( 576, 128, 200, 32 );

	multiplier.SetNameAndStatus( L( "Power multiplier" ), L( "Pre-multiplier for pow (touch_pow_mult)" ));
	multiplier.Setup( 100, 1000, 1 );
	multiplier.LinkCvar( "touch_pow_mult" );
	multiplier.SetRect( 576, 208, 200, 32 );

	exponent.SetNameAndStatus( L( "Exponent" ), L( "Exponent factor, more aggressive (touch_exp_mult)" ));
	exponent.Setup( 0, 100, 1 );
	exponent.LinkCvar( "touch_exp_mult" );
	exponent.SetRect( 576, 288, 200, 32 );

	SectionHeader( secProfiles, "Profiles", 784, 8, 248 );

	profiles.SetRect( 784, 44, 248, 128 );
	profiles.SetModel( &model );
	profiles.onChanged = VoidCb( &CMenuOptionsTouchOptsPage::UpdateProfiles );
	model.Update();

	// build 90: profile field pushed to y224 (13px under the list) and
	// the 2x2 button block to 268/312 - 12px between the button rows
	profilename.szName = L( "New Profile:" );
	profilename.iMaxLength = 16;
	profilename.SetRect( 784, 224, 248, 32 );

	applyBtn.SetNameAndStatus( L( "Activate" ), L( "Apply selected profile" ));
	applyBtn.SetPicture( PC_ACTIVATE );
	applyBtn.onReleased = VoidCb( &CMenuOptionsTouchOptsPage::ApplyProfile );
	applyBtn.SetRect( 784, 268, 116, 32 );

	saveBtn.SetNameAndStatus( L( "GameUI_Save" ), L( "Save new profile" ));
	saveBtn.SetPicture( PC_TOUCH_SAVE );
	saveBtn.onReleased = VoidCb( &CMenuOptionsTouchOptsPage::SaveProfile );
	saveBtn.SetRect( 912, 268, 116, 32 );

	removeBtn.SetNameAndStatus( L( "Delete" ), L( "Delete saved profile" ));
	removeBtn.SetPicture( PC_DELETE );
	removeBtn.onReleased = VoidCb( &CMenuOptionsTouchOptsPage::DeleteMsgBox );
	removeBtn.SetRect( 784, 312, 116, 32 );

	reset.SetNameAndStatus( L( "Reset" ), L( "Reset sensitivity options" ));
	reset.SetPicture( PC_TOUCH_RESET );
	reset.onReleased = VoidCb( &CMenuOptionsTouchOptsPage::ResetMsgBox );
	reset.SetRect( 912, 312, 116, 32 );

	msgBox.SetPositiveButton( L( "GameUI_OK" ), PC_OK );
	msgBox.Link( this );

	AddItem( secSens );
	AddItem( lookX );
	AddItem( lookY );
	AddItem( moveX );
	AddItem( moveY );
	AddItem( secTouch );
	AddItem( enable );
	AddItem( grid );
	AddItem( gridsize );
	AddItem( nomouse );
	AddItem( safearea );
	AddItem( secNonlin );
	AddItem( acceleration );
	AddItem( power );
	AddItem( multiplier );
	AddItem( exponent );
	AddItem( secProfiles );
	AddItem( profiles );
	AddItem( profilename );
	AddItem( applyBtn );
	AddItem( saveBtn );
	AddItem( removeBtn );
	AddItem( reset );
}

void CMenuOptionsTouchOptsPage::Reload()
{
	BaseClass::Reload();
	GetConfig();
	model.Update();
	UpdateProfiles();
}

/*
=====================================================================
Touch tab - "Touch buttons" sub-page (build 88: compact fold-in of
menus/TouchButtons.cpp. The 4 RGBA sliders move to a bottom band, the
fullscreen banner/Done paths are dropped - the footer owns OK/Cancel)
=====================================================================
*/
class CMenuOptionsTouchBtnsPage;

// file-local clone of TouchButtons.cpp's CMenuCommandPickerDialog with
// this dialog's page as caller instead of the fullscreen editor
class CMenuOptionsCmdPicker : public CMenuBaseWindow
{
public:
	typedef CMenuBaseWindow BaseClass;
	CMenuOptionsCmdPicker() : BaseClass( "CMenuOptionsCmdPicker" ), model( this )
	{
		iFlags |= QMF_DIALOG;
	}

	void _Init() override;
	void _VidInit() override;
	void Draw() override;
	bool KeyDown( int key ) override;
	void Show() override;

	class CCommandListModel : public CMenuKbActListModel
	{
	public:
		CCommandListModel( CMenuOptionsCmdPicker *parent )
			: CMenuKbActListModel( VIEW_PICKER ), parent( parent ) { }

		void OnActivateEntry( int line ) override
		{
			if( IsLineUsable( line ))
				parent->Confirm();
		}

	private:
		CMenuOptionsCmdPicker *parent;
	} model;

	void Confirm();

	CMenuOptionsTouchBtnsPage *pCaller;
	CMenuAction title;
	CMenuTable list;
	CMenuPicButton ok;
	CMenuPicButton cancel;
};

class CMenuOptionsTouchBtnsPage : public CMenuItemsHolder
{
public:
	CMenuOptionsTouchBtnsPage() : model( this ), curflags( 0 )
	{
		selectedName[0] = 0;
		cmdPicker.pCaller = this;
	}

	void Save(); // footer OK/Apply persists button edits

	// fed by TouchButtons.cpp's engine AddTouchButtonToList hook
	void AddButtonToList( const char *name, const char *texture, const char *command, unsigned char *color, int flags )
	{
		model.AddButtonToList( name, texture, command, color, flags );
	}

	// same list model shape as CMenuTouchButtons::CButtonListModel
	class CTouchButtonsModel : public CMenuBaseArrayModel
	{
	public:
		CTouchButtonsModel( CMenuOptionsTouchBtnsPage *parent )
			: parent( parent ), gettingList( false ), initialized( false ) { }

		void Update() override;
		void AddButtonToList( const char *name, const char *texture, const char *command, unsigned char *color, int flags );

		const char *GetText( int line ) final override
		{
			return buttons[line].szName;
		}

		int GetRows() const final override
		{
			return buttons.Count();
		}

		struct button_t
		{
			char szName[128];
			char szTexture[128];
			char szCommand[128];
			byte bColors[4];
			int  iFlags;
		};

		CUtlVector<button_t> buttons;

		bool gettingList;
		bool initialized;
		CMenuOptionsTouchBtnsPage *parent;
	} model;

	// editor flow copied from CMenuTouchButtons
	void UpdateFields();
	void UpdateTexture();
	void UpdateCommandFlags();
	void UpdateSP();
	void UpdateMP();
	void SaveButton();
	void DeleteButton();
	void ResetButtons();
	void RemoveMsgBox();
	void ResetMsgBox();
	void OnCommandPicked( const char *cmd );
	void OpenFileDialog();
	void OpenCommandPicker()
	{
		cmdPicker.Show();
	}

	static void FileDialogCb( bool success );

	char selectedName[256];
	int curflags;

	class CMenuColor : public CMenuBaseItem
	{
	public:
		void Draw() override;
	} color;

	class CMenuBtnPreview : public CMenuBaseItem
	{
	public:
		CMenuBtnPreview() : CMenuBaseItem(), textureId( 0 ) { }
		void Draw() override;
		HIMAGE textureId;
	} preview;

private:
	void _Init() override;
	void Reload() override;

	CMenuAction secButtons;
	CMenuAction secFlags;

	CMenuTable buttonList;
	CMenuPicButton removeBtn, resetBtn, saveBtn, editorBtn;

	CMenuField name, command, texture;
	CMenuPicButton select, selectCmd;

	CMenuCheckBox hide, mp, sp, lock, additive, precision, stick, latch;
	CMenuSlider red, green, blue, alpha;

	CMenuYesNoMessageBox msgBox;
	CMenuOptionsCmdPicker cmdPicker;
};

// the engine callback in TouchButtons.cpp only knows its own fullscreen
// menu - it fans out to this page through this hook
static CMenuOptionsTouchBtnsPage *s_touchBtnsPage = nullptr;

// extern "C": matches the call site in menus/TouchButtons.cpp
extern "C" void UI_OptionsTouchButtons_AddButton( const char *name, const char *texture, const char *command, unsigned char *color, int flags )
{
	if( s_touchBtnsPage )
		s_touchBtnsPage->AddButtonToList( name, texture, command, color, flags );
}

void CMenuOptionsTouchBtnsPage::CTouchButtonsModel::AddButtonToList( const char *name, const char *texture, const char *command, unsigned char *color, int flags )
{
	if( !gettingList )
		return;

	button_t button;

	Q_strncpy( button.szName, name, sizeof( button.szName ));
	Q_strncpy( button.szTexture, texture, sizeof( button.szTexture ));
	Q_strncpy( button.szCommand, command, sizeof( button.szCommand ));
	memcpy( button.bColors, color, sizeof( button.bColors ));
	button.iFlags = flags;

	buttons.AddToTail( button );
}

void CMenuOptionsTouchBtnsPage::CTouchButtonsModel::Update()
{
	if( !initialized )
		return;

	buttons.RemoveAll();

	EngFuncs::ClientCmd( true, "" ); // perform Cbuf_Execute()

	gettingList = true;
	EngFuncs::ClientCmd( true, "touch_list\n" );
	gettingList = false;

	parent->UpdateFields();
}

void CMenuOptionsTouchBtnsPage::Save()
{
	// the fullscreen editor's Done button ran touch_writeconfig - the
	// footer OK / Apply propagates the edits the same way
	EngFuncs::ClientCmd( true, "touch_writeconfig\n" );
}

void CMenuOptionsTouchBtnsPage::UpdateFields()
{
	int i = buttonList.GetCurrentIndex();

	if( !model.buttons.IsValidIndex( i ))
		return;

	Q_strncpy( selectedName, model.buttons[i].szName, sizeof( selectedName ));
	red.SetCurrentValue( model.buttons[i].bColors[0] );
	green.SetCurrentValue( model.buttons[i].bColors[1] );
	blue.SetCurrentValue( model.buttons[i].bColors[2] );
	alpha.SetCurrentValue( model.buttons[i].bColors[3] );

	curflags = model.buttons[i].iFlags;
	mp.bChecked = !!( curflags & TOUCH_FL_MP );
	sp.bChecked = !!( curflags & TOUCH_FL_SP );
	lock.bChecked = !!( curflags & TOUCH_FL_NOEDIT );
	hide.bChecked = !!( curflags & TOUCH_FL_HIDE );
	additive.bChecked = !!( curflags & TOUCH_FL_DRAW_ADDITIVE );
	precision.bChecked = !!( curflags & TOUCH_FL_PRECISION );
	stick.bChecked = !!( curflags & TOUCH_FL_STICK );
	latch.bChecked = !!( curflags & TOUCH_FL_COMMAND_LATCH );

	name.Clear();
	texture.SetBuffer( model.buttons[i].szTexture );
	UpdateTexture();

	command.SetBuffer( model.buttons[i].szCommand );
	UpdateCommandFlags();
}

void CMenuOptionsTouchBtnsPage::UpdateCommandFlags()
{
	const char *cmd = command.GetBuffer();

	// these flags only affect specific commands
	stick.SetGrayed( strcmp( cmd, "_joy" ) != 0 );
	latch.SetGrayed( cmd[0] != '+' );
}

void CMenuOptionsTouchBtnsPage::OpenFileDialog()
{
	uiFileDialogGlobal.npatterns = 7;
	Q_strncpy( uiFileDialogGlobal.patterns[0], "touch/*", sizeof( uiFileDialogGlobal.patterns[0] ));
	Q_strncpy( uiFileDialogGlobal.patterns[1], "touch_default/*", sizeof( uiFileDialogGlobal.patterns[1] ));
	Q_strncpy( uiFileDialogGlobal.patterns[2], "gfx/touch/*", sizeof( uiFileDialogGlobal.patterns[2] ));
	Q_strncpy( uiFileDialogGlobal.patterns[3], "gfx/vgui/*", sizeof( uiFileDialogGlobal.patterns[3] ));
	Q_strncpy( uiFileDialogGlobal.patterns[4], "gfx/shell/*", sizeof( uiFileDialogGlobal.patterns[4] ));
	Q_strncpy( uiFileDialogGlobal.patterns[5], "*.tga", sizeof( uiFileDialogGlobal.patterns[5] ));
	Q_strncpy( uiFileDialogGlobal.patterns[6], "*.png", sizeof( uiFileDialogGlobal.patterns[6] ));
	uiFileDialogGlobal.preview = true;
	uiFileDialogGlobal.valid = true;
	uiFileDialogGlobal.callback = CMenuOptionsTouchBtnsPage::FileDialogCb;
	UI_FileDialog_Menu();
}

void CMenuOptionsTouchBtnsPage::FileDialogCb( bool success )
{
	if( success && s_touchBtnsPage )
	{
		s_touchBtnsPage->texture.SetBuffer( uiFileDialogGlobal.result );
		s_touchBtnsPage->UpdateTexture();
	}
}

void CMenuOptionsTouchBtnsPage::UpdateTexture()
{
	const char *buf = texture.GetBuffer();
	if( buf[0] && buf[0] != '#' )
		preview.textureId = EngFuncs::PIC_Load( buf );
	else
		preview.textureId = 0;
}

void CMenuOptionsTouchBtnsPage::UpdateSP()
{
	if( sp.bChecked )
	{
		curflags |= TOUCH_FL_SP;
		curflags &= ~TOUCH_FL_MP;
		mp.bChecked = false;
	}
	else
	{
		curflags &= ~TOUCH_FL_SP;
	}
}

void CMenuOptionsTouchBtnsPage::UpdateMP()
{
	if( mp.bChecked )
	{
		curflags |= TOUCH_FL_MP;
		curflags &= ~TOUCH_FL_SP;
		sp.bChecked = false;
	}
	else
	{
		curflags &= ~TOUCH_FL_MP;
	}
}

void CMenuOptionsTouchBtnsPage::SaveButton()
{
	char command[4096];
	char cmd[256];

	Com_EscapeCommand( cmd, this->command.GetBuffer(), 256 );

	if( name.GetBuffer()[0] )
	{
		snprintf( command, sizeof( command ), "touch_addbutton \"%s\" \"%s\" \"%s\"\n",
			name.GetBuffer(),
			texture.GetBuffer(),
			cmd );
		EngFuncs::ClientCmd( false, command );
		snprintf( command, sizeof( command ), "touch_setflags \"%s\" %i\n", name.GetBuffer(), curflags );
		EngFuncs::ClientCmd( false, command );
		snprintf( command, sizeof( command ), "touch_setcolor \"%s\" %u %u %u %u\n", name.GetBuffer(),
			(uint)red.GetCurrentValue(),
			(uint)green.GetCurrentValue(),
			(uint)blue.GetCurrentValue(),
			(uint)alpha.GetCurrentValue());
		EngFuncs::ClientCmd( true, command );
		name.Clear();
	}
	else
	{
		snprintf( command, sizeof( command ), "touch_settexture \"%s\" \"%s\"\n", selectedName, texture.GetBuffer());
		EngFuncs::ClientCmd( false, command );
		snprintf( command, sizeof( command ), "touch_setcommand \"%s\" \"%s\"\n", selectedName, cmd );
		EngFuncs::ClientCmd( false, command );
		snprintf( command, sizeof( command ), "touch_setflags \"%s\" %i\n", selectedName, curflags );
		EngFuncs::ClientCmd( false, command );
		snprintf( command, sizeof( command ), "touch_setcolor \"%s\" %u %u %u %u\n", selectedName,
			(uint)red.GetCurrentValue(),
			(uint)green.GetCurrentValue(),
			(uint)blue.GetCurrentValue(),
			(uint)alpha.GetCurrentValue());
		EngFuncs::ClientCmd( true, command );
	}

	model.Update();
}

void CMenuOptionsTouchBtnsPage::DeleteButton()
{
	int i = buttonList.GetCurrentIndex();
	if( i > 0 ) buttonList.SetCurrentIndex( i - 1 );
	char command[512];
	snprintf( command, sizeof( command ), "touch_removebutton \"%s\"\n", selectedName );
	EngFuncs::ClientCmd( true, command );
	model.Update();
}

void CMenuOptionsTouchBtnsPage::ResetButtons()
{
	EngFuncs::ClientCmd( false, "touch_removeall\n" );
	EngFuncs::ClientCmd( true, "touch_loaddefaults\n" );
	model.Update();
}

void CMenuOptionsTouchBtnsPage::RemoveMsgBox()
{
	msgBox.SetMessage( L( "Delete selected button?" ));
	msgBox.onPositive = VoidCb( &CMenuOptionsTouchBtnsPage::DeleteButton );
	msgBox.Show();
}

void CMenuOptionsTouchBtnsPage::ResetMsgBox()
{
	msgBox.SetMessage( L( "Reset all buttons?" ));
	msgBox.onPositive = VoidCb( &CMenuOptionsTouchBtnsPage::ResetButtons );
	msgBox.Show();
}

void CMenuOptionsTouchBtnsPage::OnCommandPicked( const char *cmd )
{
	command.SetBuffer( cmd );
	UpdateCommandFlags();
}

void CMenuOptionsTouchBtnsPage::CMenuColor::Draw()
{
	CMenuOptionsTouchBtnsPage *page = (CMenuOptionsTouchBtnsPage *)Parent();

	uint color = ((uint)page->blue.GetCurrentValue()) |
				(((uint)page->green.GetCurrentValue()) << 8) |
				(((uint)page->red.GetCurrentValue()) << 16) |
				(((uint)page->alpha.GetCurrentValue()) << 24);

	UI_FillRect( m_scPos, m_scSize, color );
}

void CMenuOptionsTouchBtnsPage::CMenuBtnPreview::Draw()
{
	CMenuOptionsTouchBtnsPage *page = (CMenuOptionsTouchBtnsPage *)Parent();

	UI_FillRect( m_scPos.x - 2, m_scPos.y - 2, m_scSize.w + 4, m_scSize.h + 4, 0xFFC0C0C0 );
	UI_FillRect( m_scPos, m_scSize, 0xFF808080 );

	EngFuncs::PIC_Set( textureId,
		(int)page->red.GetCurrentValue(),
		(int)page->green.GetCurrentValue(),
		(int)page->blue.GetCurrentValue(),
		(int)page->alpha.GetCurrentValue());

	if( page->additive.bChecked )
		EngFuncs::PIC_DrawAdditive( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h );
	else
		EngFuncs::PIC_DrawTrans( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h );
}

void CMenuOptionsTouchBtnsPage::_Init()
{
	model.initialized = true;
	s_touchBtnsPage = this; // engine callback hook target

	// build 88: compact fold-in of the fullscreen button editor -
	// list left, fields middle, flags bottom-right, RGBA band bottom
	SectionHeader( secButtons, "Buttons", 16, 8, 314 );

	buttonList.SetRect( 16, 44, 314, 264 );
	buttonList.SetModel( &model );
	buttonList.onChanged = VoidCb( &CMenuOptionsTouchBtnsPage::UpdateFields );

	// build 90: action row 316 -> 320, 12px under the button list
	removeBtn.SetNameAndStatus( L( "Delete" ), L( "Delete selected button" ));
	removeBtn.SetPicture( PC_DELETE );
	removeBtn.onReleased = VoidCb( &CMenuOptionsTouchBtnsPage::RemoveMsgBox );
	removeBtn.SetRect( 16, 320, 86, 32 );

	resetBtn.SetNameAndStatus( L( "Reset" ), L( "Reset touch to default state" ));
	resetBtn.SetPicture( PC_TOUCH_RESET );
	resetBtn.onReleased = VoidCb( &CMenuOptionsTouchBtnsPage::ResetMsgBox );
	resetBtn.SetRect( 110, 320, 86, 32 );

	saveBtn.SetNameAndStatus( L( "GameUI_Save" ), L( "Save as new button" ));
	saveBtn.SetPicture( PC_TOUCH_SAVE );
	saveBtn.onReleased = VoidCb( &CMenuOptionsTouchBtnsPage::SaveButton );
	saveBtn.SetRect( 204, 320, 86, 32 );

	// fields column - each field's label sits 39px above it
	// build 90: command row 116 -> 128 and texture row 188 -> 212, so
	// each field's y-39 label zone clears the box above it by ~13px
	name.szName = L( "New Button:" );
	name.iMaxLength = 255;
	name.SetRect( 350, 44, 390, 32 );

	command.szName = L( "Command:" );
	command.iMaxLength = 255;
	command.onChanged = VoidCb( &CMenuOptionsTouchBtnsPage::UpdateCommandFlags );
	command.SetRect( 350, 128, 295, 32 );

	selectCmd.SetNameAndStatus( L( "Pick" ), L( "Pick command from key bindings list" ));
	selectCmd.SetPicture( PC_TOUCH_SELECT );
	selectCmd.onReleased = VoidCb( &CMenuOptionsTouchBtnsPage::OpenCommandPicker );
	selectCmd.SetRect( 660, 128, 85, 32 );

	texture.szName = L( "Texture:" );
	texture.iMaxLength = 255;
	texture.onChanged = VoidCb( &CMenuOptionsTouchBtnsPage::UpdateTexture );
	texture.eTextAlignment = QM_RIGHT;
	texture.SetRect( 350, 212, 295, 32 );

	select.SetNameAndStatus( L( "Select" ), L( "Select texture from list" ));
	select.SetPicture( PC_TOUCH_SELECT );
	select.onReleased = VoidCb( &CMenuOptionsTouchBtnsPage::OpenFileDialog );
	select.SetRect( 660, 212, 85, 32 );

	// build 90: preview shrunk 70 -> 60 and pushed to y256 so the RGBA
	// swatch fits under it (y328..354) instead of touching both
	preview.SetRect( 350, 256, 60, 60 );
	color.SetRect( 350, 328, 70, 26 );

	editorBtn.SetNameAndStatus( L( "Editor" ), L( "Open interactive editor" ));
	editorBtn.SetPicture( PC_TOUCH_EDITOR );
	editorBtn.onReleased = UI_TouchEdit_Menu;
	editorBtn.SetRect( 440, 320, 150, 32 );

	// flags - two columns, long labels carried by the status text so
	// every column stays <= 178px
	SectionHeader( secFlags, "Flags", 760, 8, 280 );

	// build 90: flag rows re-pitched 42 -> 46 (48/94/140/186)
	hide.SetNameAndStatus( L( "Hide" ), L( "Show/hide button" ));
	hide.iMask = TOUCH_FL_HIDE;
	hide.onChanged.pExtra = &curflags;
	hide.onChanged = CMenuCheckBox::BitMaskCb;
	hide.SetRect( 760, 48, 32, 32 );

	mp.SetNameAndStatus( L( "MP" ), L( "Show button only in multiplayer" ));
	mp.onChanged = VoidCb( &CMenuOptionsTouchBtnsPage::UpdateMP );
	mp.SetRect( 900, 48, 32, 32 );

	sp.SetNameAndStatus( L( "SP" ), L( "Show button only in singleplayer" ));
	sp.onChanged = VoidCb( &CMenuOptionsTouchBtnsPage::UpdateSP );
	sp.SetRect( 760, 94, 32, 32 );

	lock.SetNameAndStatus( L( "Lock" ), L( "Lock button editing" ));
	lock.iMask = TOUCH_FL_NOEDIT;
	lock.onChanged.pExtra = &curflags;
	lock.onChanged = CMenuCheckBox::BitMaskCb;
	lock.SetRect( 900, 94, 32, 32 );

	additive.SetNameAndStatus( L( "Additive" ), L( "Set button additive draw mode" ));
	additive.iMask = TOUCH_FL_DRAW_ADDITIVE;
	additive.onChanged.pExtra = &curflags;
	additive.onChanged = CMenuCheckBox::BitMaskCb;
	additive.SetRect( 760, 140, 32, 32 );

	precision.SetNameAndStatus( L( "Precision" ), L( "Look precision - increase look precision" ));
	precision.iMask = TOUCH_FL_PRECISION;
	precision.onChanged.pExtra = &curflags;
	precision.onChanged = CMenuCheckBox::BitMaskCb;
	precision.SetRect( 900, 140, 32, 32 );

	stick.SetNameAndStatus( L( "Stick" ), L( "Analog stick - move with an analog stick, for _joy buttons" ));
	stick.iMask = TOUCH_FL_STICK;
	stick.onChanged.pExtra = &curflags;
	stick.onChanged = CMenuCheckBox::BitMaskCb;
	stick.SetRect( 760, 186, 32, 32 );

	latch.SetNameAndStatus( L( "Toggle" ), L( "Press once to hold, press again to release, for + commands" ));
	latch.iMask = TOUCH_FL_COMMAND_LATCH;
	latch.onChanged.pExtra = &curflags;
	latch.onChanged = CMenuCheckBox::BitMaskCb;
	latch.SetRect( 900, 186, 32, 32 );

	// RGBA band along the bottom (was a 4-slider column in the
	// fullscreen editor) - labels at y239, controls at y278
	// build 90: band moved 290 -> 278 so it clears the Editor button
	// (the old 290..322 box was cut in half by Editor at y316)
	red.eFocusAnimation = QM_PULSEIFFOCUS;
	red.SetNameAndStatus( L( "Red" ), L( "Texture red channel" ));
	red.Setup( 0, 255, 1 );
	red.SetRect( 420, 278, 150, 32 );

	green.eFocusAnimation = QM_PULSEIFFOCUS;
	green.SetNameAndStatus( L( "Green" ), L( "Texture green channel" ));
	green.Setup( 0, 255, 1 );
	green.SetRect( 576, 278, 150, 32 );

	blue.eFocusAnimation = QM_PULSEIFFOCUS;
	blue.SetNameAndStatus( L( "Blue" ), L( "Texture blue channel" ));
	blue.Setup( 0, 255, 1 );
	blue.SetRect( 732, 278, 150, 32 );

	alpha.eFocusAnimation = QM_PULSEIFFOCUS;
	alpha.SetNameAndStatus( L( "Alpha" ), L( "Texture alpha channel" ));
	alpha.Setup( 0, 255, 1 );
	alpha.SetRect( 888, 278, 150, 32 );

	msgBox.Link( this );
	cmdPicker.Link( this );

	AddItem( secButtons );
	AddItem( buttonList );
	AddItem( removeBtn );
	AddItem( resetBtn );
	AddItem( saveBtn );
	AddItem( name );
	AddItem( command );
	AddItem( texture );
	AddItem( select );
	AddItem( selectCmd );
	AddItem( preview );
	AddItem( color );
	AddItem( editorBtn );
	AddItem( secFlags );
	AddItem( hide );
	AddItem( mp );
	AddItem( sp );
	AddItem( lock );
	AddItem( additive );
	AddItem( precision );
	AddItem( stick );
	AddItem( latch );
	AddItem( red );
	AddItem( green );
	AddItem( blue );
	AddItem( alpha );
}

void CMenuOptionsTouchBtnsPage::Reload()
{
	BaseClass::Reload();
	model.Update();
}

/*
=====================================================================
Touch tab - command picker dialog (clone of the file-local
CMenuCommandPickerDialog in menus/TouchButtons.cpp)
=====================================================================
*/
void CMenuOptionsCmdPicker::Confirm()
{
	int i = list.GetCurrentIndex();
	if( !model.IsLineUsable( i ))
		return;

	if( pCaller )
		pCaller->OnCommandPicked( model.entries[i].bind );

	Hide();
}

void CMenuOptionsCmdPicker::_Init()
{
	title.szName = L( "Pick command" );
	title.iFlags = QMF_INACTIVE | QMF_DROPSHADOW;
	title.eTextAlignment = QM_TOP;
	title.SetCharSize( QM_DEFAULTFONT );
	title.SetRect( 0, 16, 720, 32 );

	list.SetRect( 16, 56, 720 - 32, 480 - 56 - 64 );
	list.SetModel( &model );
	list.SetupColumn( 0, L( "GameUI_Action" ), 0.6f );
	list.SetupColumn( 1, L( "Command" ), 0.4f );

	ok.szName = L( "GameUI_OK" );
	ok.SetPicture( PC_OK );
	ok.SetRect( 720 / 2 - UI_BUTTONS_WIDTH / 2 - 8, 480 - 56, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );
	SET_EVENT_MULTI( ok.onReleased,
	{
		((CMenuOptionsCmdPicker *)pSelf->Parent())->Confirm();
	});

	cancel.szName = L( "GameUI_Cancel" );
	cancel.SetPicture( PC_CANCEL );
	cancel.SetRect( 720 / 2 + 8, 480 - 56, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );
	SET_EVENT_MULTI( cancel.onReleased,
	{
		((CMenuOptionsCmdPicker *)pSelf->Parent())->Hide();
	});

	AddItem( title );
	AddItem( list );
	AddItem( ok );
	AddItem( cancel );
}

void CMenuOptionsCmdPicker::_VidInit()
{
	SetRect(( uiStatic.width - 720 ) / 2, ( 768 - 480 ) / 2, 720, 480 );
	pos.x += uiStatic.xOffset;
	pos.y += uiStatic.yOffset;
	CalcPosition();
	CalcSizes();
}

void CMenuOptionsCmdPicker::Draw()
{
	UI_FillRect( 0, 0, gpGlobals->scrWidth, gpGlobals->scrHeight, 0x40000000 );

	// A7 retheme: olive panel + raised bevel (same as TouchButtons.cpp)
	EngFuncs::FillRGBA( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h, 74, 89, 66, 245 );
	UI_DrawBevel( m_scPos, m_scSize, false );

	EngFuncs::FillRGBA(
		m_scPos.x + 16 * uiStatic.scaleX,
		m_scPos.y + ( 16 + 32 ) * uiStatic.scaleY,
		m_scSize.w - 32 * uiStatic.scaleX,
		1, 255, 255, 255, 40 );

	BaseClass::Draw();
}

bool CMenuOptionsCmdPicker::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}
	return BaseClass::KeyDown( key );
}

void CMenuOptionsCmdPicker::Show()
{
	model.Update();
	BaseClass::Show();
}

/*
=====================================================================
The Options window itself
=====================================================================
*/
class CMenuOptions: public CMenuFramework
{
private:
	void _Init( void ) override;

public:
	typedef CMenuFramework BaseClass;
	CMenuOptions() : CMenuFramework("CMenuOptions") { }

	bool SaveAll();
	void OnOk();
	void OnApply()
	{
		SaveAll();
	}

	// build 85: the Video modes test-mode countdown ticks here, every
	// frame while the dialog is open (even when another tab is in front)
	void Draw() override
	{
		videoModesPage.TickTestMode();
		BaseClass::Draw();
	}

	CMenuTabView tabs;

	CMenuOptionsMPPage        mpPage;
	CMenuOptionsKeysPage      kbKeysPage;  // build 89: the Keyboard tab IS the bindings table
	CMenuTabView              mouseTabs;   // build 88: nested Mouse sub-tabs
	CMenuOptionsMousePage     mousePage;
	CMenuOptionsAdvPage       advPage;     // build 88: Advanced-controls fold-in
	CMenuOptionsGyroPage      gyroPage;    // build 88: Gyro.cpp fold-in
	CMenuOptionsGamepadPage   padPage;     // build 88: Gamepad.cpp fold-in
	CMenuOptionsAudioPage     audioPage;
	CMenuTabView              videoTabs;     // build 85: nested Video sub-tabs
	CMenuOptionsVideoOptsPage videoOptsPage;
	CMenuOptionsVideoModesPage videoModesPage;
	CMenuTabView              touchTabs;    // build 88: nested Touch sub-tabs
	CMenuOptionsTouchOptsPage touchOptsPage;
	CMenuOptionsTouchBtnsPage touchBtnsPage;

	CMenuPicButton *okBtn;
	CMenuPicButton *cancelBtn;
	CMenuPicButton *applyBtn;
};


bool CMenuOptions::SaveAll()
{
	bool ok = mpPage.Save();

	// pages write their controls through WriteCvar (LinkCvar only reads)
	audioPage.Save();
	videoOptsPage.Save();
	// build 87: the footer Apply/OK now runs the FULL modes apply
	// (resolution/window mode/renderer/vsync via SetConfig + the
	// test-mode safety revert); false = a test/restart box is open
	if( !videoModesPage.Save( ))
		ok = false;
	mousePage.Save();
	advPage.Save();
	gyroPage.Save();
	padPage.Save();
	touchOptsPage.Save();
	touchBtnsPage.Save();

	return ok;
}

void CMenuOptions::OnOk()
{
	if( SaveAll( ))
		Hide();
}

/*
=================
CMenuOptions::Init
=================
*/
void CMenuOptions::_Init( void )
{
	// tab strip like the original GameUI Options dialog
	// A7 build 56: 1120 = 8 tabs x 140px - "Multiplayer"/"Keyboard" fit
	// without ellipsis. build 58: Touch tab removed on user request.
	// build 85: Voice and Lock removed too - five tabs x 224px, and the
	// Video tab is itself a nested tab view (Video options | Video modes).
	// build 90: x1 152→224 (width 1048) so the main menu column at
	// x36..~200 stays fully visible BEHIND the dialog like reference
	// screenshot 2 (user choice); inner content re-flowed to ≤1040.
	tabs.SetRect( 224, 168, 1048, 440 );
	tabs.AddTabItem( mpPage,     L( "Multiplayer" ));
	tabs.AddTabItem( kbKeysPage, L( "Keyboard" ));
	tabs.AddTabItem( mouseTabs,  L( "Mouse" ));
	tabs.AddTabItem( audioPage,  L( "Audio" ));
	tabs.AddTabItem( videoTabs,  L( "Video" ));
	tabs.AddTabItem( touchTabs,  L( "Touch" ));

	// build 89: the nested Keyboard tabs are gone per user - kbKeysPage is
	// a direct tab now, its own space is the full 401px page (no inner strip)

	// build 88: nested Mouse tabs - Advanced (xash Advanced-controls),
	// Gyroscope and Gamepad folded in, no duplicated controls
	mouseTabs.SetRect( 0, 0, 1048, 440 - TAB_STRIP_HEIGHT );
	mouseTabs.AddTabItem( mousePage, L( "Mouse" ));
	mouseTabs.AddTabItem( advPage,   L( "Advanced" ));
	mouseTabs.AddTabItem( gyroPage,  L( "Gyroscope" ));
	mouseTabs.AddTabItem( padPage,   L( "Gamepad" ));

	// build 88: nested Touch tabs (the main menu's Touch entry now lives
	// here as content pages, like Video)
	touchTabs.SetRect( 0, 0, 1048, 440 - TAB_STRIP_HEIGHT );
	touchTabs.AddTabItem( touchOptsPage, L( "Touch options" ));
	touchTabs.AddTabItem( touchBtnsPage, L( "Touch buttons" ));

	// build 85: nested Video tabs. The inner strip sits at the top of the
	// Video page's area; its two sub-pages share the space below it
	// (440 - outer strip - inner strip = 362px of content height).
	videoTabs.SetRect( 0, 0, 1048, 440 - TAB_STRIP_HEIGHT );
	videoTabs.AddTabItem( videoOptsPage,  L( "Video options" ));
	videoTabs.AddTabItem( videoModesPage, L( "Video modes" ));

	AddItem( tabs );

	// build 90: right-aligned panel so the menu list stays visible left
	// of it (user choice: "Narrow the dialog right")
	m_bFloatRightAlign = true;

	// build 90: footer row rebuilt - OK/Cancel/Apply used to overlap
	// (OK 958..1058 ran INTO Cancel at 1050). Right-aligned group on
	// the dialog's right edge (x1272 = tab strip right, y640 = 32px
	// under the tab body which ends at 608): three 100x34 buttons with
	// an even 12px gap, same baseline, clear of the frame padding
	okBtn = AddButton( L( "GameUI_OK" ), nullptr, PC_OK, VoidCb( &CMenuOptions::OnOk ));
	okBtn->SetRect( 948, 640, 100, 34 );

	cancelBtn = AddButton( L( "GameUI_Cancel" ), nullptr, PC_CANCEL, VoidCb( &CMenuOptions::Hide ));
	cancelBtn->SetRect( 1060, 640, 100, 34 );

	applyBtn = AddButton( L( "GameUI_Apply" ), nullptr, PC_CANCEL, VoidCb( &CMenuOptions::OnApply ));
	applyBtn->SetRect( 1172, 640, 100, 34 );
}

ADD_MENU( menu_options, CMenuOptions, UI_Options_Menu );
