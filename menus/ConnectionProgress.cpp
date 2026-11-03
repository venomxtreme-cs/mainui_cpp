/*
ConnectionProgress.cpp -- connection progress window
Copyright (C) 2017 mittorn

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
#include "Utils.h"

#include "ItemsHolder.h"
#include "ProgressBar.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"

enum EState
{
	STATE_NONE,
	STATE_MENU, // do not hide when disconnected or in game
	STATE_DOWNLOAD, // enlarge your connectionprogress window
	STATE_CONNECTING, // showing single progress
	STATE_CONSOLE // do not show until state reset
};

enum ESource
{
	SOURCE_CONSOLE,
	SOURCE_SERVERBROWSER,
	SOURCE_CREATEGAME
};


class CMenuConnectionProgress : public CMenuBaseWindow
{
public:
	CMenuConnectionProgress();
	void _Init() override;
	void _VidInit() override;
	void Draw() override;
	bool DrawAnimation() override;
	bool KeyDown( int key ) override;
	void Disconnect();
	void HandleDisconnect( void );
	void HandlePrecache( void )
	{
		SetCommonText( L( "GameUI_PrecachingResources" ) );
		commonProgress.LinkCvar( "scr_loading", 0, 100 );
		m_iState = STATE_CONNECTING;
	}
	void HandleStufftext( float flProgress, const char *pszText )
	{
		SetCommonText( pszText );
		commonProgress.SetValue( flProgress );
		m_iState = STATE_CONNECTING;
	}
	void HandleDownload( const char *pszFileName, const char *pszServerName, int iCurrent, int iTotal, const char *comment )
	{
		if( strstr( pszFileName, ".bsp" ) )
			uiStatic.needMapListUpdate = true;
		snprintf( sDownloadString, sizeof( sDownloadString ) - 1, L( "Downloading %s \nfrom %s" ), pszFileName, pszServerName );
		snprintf( sCommonString, sizeof( sCommonString ) - 1, "%d of %d %s", iCurrent + 1, iTotal, comment );
		m_iState = STATE_DOWNLOAD;
		commonProgress.SetValue( (float)iCurrent/iTotal +  0.01f / iTotal * EngFuncs::GetCvarFloat("scr_download") );
	}
	void HandleConnect( const char *pszText )
	{
		if( !pszText )
		{
			m_iSource = SOURCE_CREATEGAME;
			SetServer( "" );
			SetCommonText( L( "GameUI_StartingServer" ) );
		}
		else
		{
			m_iSource = SOURCE_SERVERBROWSER;
			SetServer( pszText );
			SetCommonText( L( "GameUI_EstablishingConnection" ) );
		}

		commonProgress.LinkCvar( "scr_loading", 0, 100 );
	}

	void SetCommonText( const char *pszText )
	{
		Q_strncpy( sCommonString, pszText, sizeof( sCommonString ) );
		// snprintf( sCommonString, sizeof( sCommonString ) - 1, "%s", pszText );
	}
	void SetServer( const char *pszName )
	{
		if( m_iSource == SOURCE_CREATEGAME )
		{
			Q_strncpy( sTitleString, L( "GameUI_StartingServer" ), sizeof( sTitleString ) );
		}
		else if( !pszName[0] )
		{
			Q_strncpy( sTitleString, L( "GameUI_StartingServer" ), sizeof( sTitleString ) );
		}
		else
		{
			snprintf( sTitleString, sizeof( sTitleString ) - 1,
				"%s %s", L( "GameUI_EstablishingConnection" ), pszName );
		}

		commonProgress.SetValue( 0 );
	}

	// A7 build 96: guaranteed visible + ACTIVE dialog on every load-phase
	// call. IsVisible() alone is not enough: CWindowStack::Remove leaves a
	// removed-as-last-window instance sitting in the vector flagged
	// QMF_CLOSING with no Update() ever purging it (IsActive() is false),
	// so the old IsVisible() ? VidInit() pattern kept re-laying-out a
	// window that CWindowStack::Update never draws - the screen stayed
	// black for the whole first phase. Show() -> Add() clears QMF_CLOSING
	// and re-selects it as active (Init() inside is idempotent).
	void EnsureShown( void )
	{
		if( IsVisible( ) && m_pStack->Current( ) == this && !FBitSet( iFlags, QMF_CLOSING ))
			VidInit( ); // already on top: re-layout only (192 -> 256 box)
		else Show( );   // stale/closing/not-top: (re)activate strictly
	}

	EState m_iState;
	ESource m_iSource;
private:
	CMenuBackgroundBitmap background;
	// A7 build 94: full-screen game backdrop behind the dialog - the PC
	// reference loading screen shows the CS background collage
	// (resource/*_loading.tga tiles) with the dialog centered on top,
	// never a black screen. not an item: it must cover the whole screen
	// beyond this dialog's 640x192 rect, so it stays parentless and is
	// VidInit'd/Drawn from _VidInit()/Draw() directly
	CMenuBackgroundBitmap screenBg;
	CMenuProgressBar commonProgress;
	CMenuProgressBar downloadProgress;
	CMenuPicButton consoleButton;
	CMenuPicButton disconnectButton;
	CMenuPicButton skipButton;
	CMenuYesNoMessageBox dialog;
	CMenuAction title;
	CMenuAction downloadText;
	CMenuAction commonText;
	char sTitleString[256];
	char sDownloadString[512];
	char sCommonString[512];
};

CMenuConnectionProgress::CMenuConnectionProgress() : CMenuBaseWindow( "ConnectionProgress" )
{
	sDownloadString[0] = sCommonString[0] = sTitleString[0] = '\0';
	m_iSource = SOURCE_CONSOLE;
	m_iState = STATE_NONE;
	szName = "ConnectionProgress";
}

bool CMenuConnectionProgress::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		dialog.Show();
		PlayLocalSound( uiStatic.sounds[SND_OUT] );
	}
	else if( UI::Key::IsConsole( key ))
	{
		consoleButton.onReleased( &consoleButton );
		PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
	}
	else if( key == 'A' )
	{
		HandleDisconnect();
	}

	return CMenuItemsHolder::KeyDown( key );
}

void CMenuConnectionProgress::HandleDisconnect( void )
{
	if( m_iState == STATE_NONE )
		return;

	if( m_iState == STATE_CONSOLE )
	{
		m_iState = STATE_NONE;
		return;
	}

	if( UI_IsVisible() && m_pStack->Current() == this )
	{
		Hide();
		if( m_iSource != SOURCE_CONSOLE && m_iState != STATE_MENU )
		{
			UI_CloseMenu();
			UI_SetActiveMenu( true );
			UI_Main_Menu();
			UI_ServerBrowser_Menu();
			if( m_iSource == SOURCE_CREATEGAME )
				UI_CreateGame_Menu();
			if( m_iState == STATE_DOWNLOAD )
			{
				Show();
				return;
			}
			m_iSource = SOURCE_CONSOLE;
		}
	}
	
	SetCommonText( L( "GameUI_Disconnected" ) );

	m_iState = STATE_NONE;
	VidInit();
}

void CMenuConnectionProgress::Disconnect()
{
	if( m_iState == STATE_DOWNLOAD )
	{
		EngFuncs::ClientCmd( true, "http_clear\n" );
		m_iState = STATE_CONNECTING;
		HandleDisconnect();
	}

	EngFuncs::ClientCmd( false, "disconnect\n");
}

void CMenuConnectionProgress::_Init( void )
{
	iFlags |= QMF_DIALOG;

	background.bForceColor = true;
	background.colorBase = uiPromptBgColor;

	consoleButton.SetPicture( PC_CONSOLE );
	consoleButton.szName = L( "GameUI_Console" );
	SET_EVENT_MULTI( consoleButton.onReleased,
	{
		CMenuConnectionProgress *parent = (CMenuConnectionProgress *)pSelf->Parent();
		EngFuncs::KEY_SetDest( KEY_CONSOLE );
		parent->m_iState = STATE_CONSOLE;
		parent->m_iSource = SOURCE_CONSOLE;
		UI_CloseMenu();
		UI_SetActiveMenu( false );
	});
	consoleButton.bEnableTransitions = false;

	disconnectButton.SetPicture( PC_DISCONNECT );
	disconnectButton.szName = L( "GameUI_Cancel" );
	disconnectButton.onReleased = VoidCb( &CMenuConnectionProgress::Disconnect );
	disconnectButton.bEnableTransitions = false;

	dialog.SetMessage( L( "Really disconnect?" ) );
	dialog.Link( this );
	dialog.onPositive = VoidCb( &CMenuConnectionProgress::Disconnect );

	title.iFlags = QMF_INACTIVE|QMF_DROPSHADOW;
	// A7 build 63: video reference - the real VGUI loading dialog has a
	// "Loading..." caption top-left; the live status stays in commonText
	title.eTextAlignment = QM_LEFT;
	title.szName = L( "Loading..." );

	skipButton.szName = L( "Skip" );
	skipButton.onReleased.SetCommand( true, "http_skip\n" );
	skipButton.bEnableTransitions = false;

	downloadText.iFlags = commonText.iFlags = QMF_INACTIVE;
	downloadText.szName = sDownloadString;
	commonText.szName = sCommonString;

	downloadProgress.LinkCvar( "scr_download", 0.0f, 100.0f );

	AddItem( background );
	AddItem( consoleButton );
	AddItem( disconnectButton );
	AddItem( downloadProgress );
	AddItem( commonProgress );
	AddItem( title );
	AddItem( skipButton );
	AddItem( downloadText );
	AddItem( commonText );
}

void CMenuConnectionProgress::_VidInit( void )
{
	int dlg_h = ( m_iState == STATE_DOWNLOAD )?256:192;
	int dlg_y = 768 / 2 - dlg_h / 2;
	int cursor = dlg_h;

	SetRect( DLG_X + 192, dlg_y, 640, dlg_h );
	pos.x += uiStatic.xOffset;
	pos.y += uiStatic.yOffset;

	title.SetCharSize( QM_DEFAULTFONT );
	title.SetRect( 20, 16, 600, 20 );

	cursor -= 44;
	consoleButton.SetRect( 188, cursor, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );
	disconnectButton.SetRect( 338, cursor, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );

	// A7 build 94: size the full-screen backdrop (parentless -> its
	// VideInit picks ScreenWidth/Height and the collage layout itself)
	screenBg.VidInit();

	if( gpGlobals->developer )
		consoleButton.Hide();

	cursor -= 30;
	commonProgress.SetRect( 20, cursor, 600, 20 );

	cursor -= 50;
	commonText.SetCharSize( QM_SMALLFONT );
	commonText.SetRect( 20, cursor, 500, 40 );

	if( m_iState == STATE_DOWNLOAD )
	{
		cursor -= 30;
		downloadProgress.Show();
		downloadProgress.SetRect( 20, cursor, 500, 20 );
		skipButton.SetRect( 540, cursor, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );
		skipButton.Show();

		cursor -= 50;
		downloadText.Show();
		downloadText.SetCharSize( QM_SMALLFONT );
		downloadText.SetRect( 20, cursor, 500, 40 );
	}
	else
	{
		downloadProgress.Hide();
		skipButton.Hide();
		downloadText.Hide();
	}

	CalcPosition();
	CalcSizes();
}

bool CMenuConnectionProgress::DrawAnimation()
{
	// We don't have animation
	return true;
}

void CMenuConnectionProgress::Draw( void )
{
	if( ( m_iState != STATE_MENU && CL_IsActive() ) || ( m_iState == STATE_NONE && m_pStack->Current() == this ) )
	{
		m_iState = STATE_NONE;
		// A7 build 93: take the Esc/"Really disconnect?" confirmation box
		// down with us - it lives in its own window stack and would
		// otherwise survive over the game after the dialog closes
		if( dialog.IsVisible( ))
			dialog.Hide();
		Hide();
		return;
	}
	// A7 build 94: the game's background collage fills the screen behind
	// the dialog (reference: PC loading screen = art + centered box, not
	// a black screen); the light GameUI dim stays on top of the art
	screenBg.Draw();
	UI_FillRect( 0,0, gpGlobals->scrWidth, gpGlobals->scrHeight, 0x40000000 );
	CMenuBaseWindow::Draw();
}

ADD_MENU3( menu_connectionprogress, CMenuConnectionProgress, UI_ConnectionProgress_f );

// A7 build 126 (item 5, user: "menu of after finish download with cancel
// original xash, remove it"): on Android the platform loading window
// (XashActivity overlay - opaque, with its own Cancel) IS the join view.
// This xash-styled dialog (Cancel/Skip/Console buttons + the Esc "Really
// disconnect?" box) used to sit on top of it after the download finished.
// Every door funnels through these exports - the engine wrappers in
// cl_gameui.c, ServerBrowser.cpp's direct call and the connectionprogress
// console command - so one guard removes it everywhere. Disconnect stays:
// with the state machine never leaving STATE_NONE, HandleDisconnect is a
// no-op, and the engine reports connection failures itself.
#if XASH_ANDROID
static bool a7_conn_dialog_logged = false;

static void A7_ConnDialogOff( void )
{
	if( !a7_conn_dialog_logged )
	{
		a7_conn_dialog_logged = true;
		Con_Printf( "[A7] build126: xash connection dialog suppressed - Android join view is the platform loading window\n" );
	}
}
#define A7_CONN_DIALOG_OFF( ) do { A7_ConnDialogOff(); return; } while( 0 )
#else
#define A7_CONN_DIALOG_OFF( ) do { } while( 0 )
#endif

// exports
void UI_ConnectionProgress_Disconnect( void )
{
	menu_connectionprogress->HandleDisconnect();
}

void UI_ConnectionProgress_Download( const char *pszFileName, const char *pszServerName, int iCurrent, int iTotal, const char *comment )
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	menu_connectionprogress->HandleDownload( pszFileName, pszServerName, iCurrent, iTotal, comment );

	// A7 build 94: engine-driven flow - HandleDisconnect's error branch
	// must never pop the main menu / server browser under the dialog
	// (the engine opens the right window itself on failure), and a
	// per-queued-file Show() would restart the open animation every time
	menu_connectionprogress->m_iSource = SOURCE_CONSOLE;

	// A7 build 96: strictly visible+active (re-layouts 192 -> 256 box)
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_DownloadEnd( void )
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	// A7 build 94: only meaningful while the download box is the one on
	// screen (the dialog's own Cancel already moved it out of this state,
	// so the http_clear that follows becomes a no-op here)
	if( menu_connectionprogress->m_iState != STATE_DOWNLOAD )
		return;

	// downloads over - back to the resource-loading state so scr_loading
	// drives the bar again. NO HandleDisconnect on purpose: its restore
	// branch would pop Main + ServerBrowser over a still-running load
	menu_connectionprogress->HandlePrecache();
	menu_connectionprogress->m_iSource = SOURCE_CONSOLE;

	// A7 build 96: EnsureShown keeps the dialog strictly visible+active
	// (see its comment - a stale closing instance never drew, black load)
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_Precache( void )
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	menu_connectionprogress->HandlePrecache();
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_Connect( const char *server ) // NULL for local server
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	menu_connectionprogress->m_iState = STATE_MENU;
	menu_connectionprogress->HandleConnect( server );

	// A7 build 94: engine-driven flow (see UI_ConnectionProgress_Download)
	menu_connectionprogress->m_iSource = SOURCE_CONSOLE;

	// A7 build 96: strictly visible+active - the single pre-load frame
	// SCR_BeginLoadingPlaque presents before the synchronous server
	// spawn must draw it, or the screen is black until svc_serverdata
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_ChangeLevel( void )
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	menu_connectionprogress->m_iState = STATE_MENU;
	menu_connectionprogress->SetCommonText( L( "Changing level on server" ) );
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_ParseServerInfo( const char *server )
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	menu_connectionprogress->SetServer( server );
	menu_connectionprogress->m_iState = STATE_CONNECTING;
	menu_connectionprogress->SetCommonText( L( "GameUI_ParseServerInfo" ) );
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_f( void )
{
	if( !strcmp( EngFuncs::CmdArgv(1), "disconnect" ) )
	{
		UI_ConnectionProgress_Disconnect();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "dl" ) )
	{
		UI_ConnectionProgress_Download( EngFuncs::CmdArgv( 2 ), EngFuncs::CmdArgv( 3 ), atoi(EngFuncs::CmdArgv( 4 )), atoi(EngFuncs::CmdArgv( 5 )), EngFuncs::CmdArgv( 6 ) );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "dlend" ) )
	{
		UI_ConnectionProgress_DownloadEnd();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "stufftext" ) )
	{
		// A7 build 126 (item 5): the only branch that bypasses the
		// guarded exports - same suppression, dialog stays down on Android
		A7_CONN_DIALOG_OFF( );
		menu_connectionprogress->HandleStufftext( atof( EngFuncs::CmdArgv( 2 ) ), EngFuncs::CmdArgv( 3 ) );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "precache" ) )
	{
		UI_ConnectionProgress_Precache();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "menu" ) )
	{
		UI_ConnectionProgress_Connect( EngFuncs::CmdArgv(2) );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "localserver" ) )
	{
		UI_ConnectionProgress_Connect( NULL );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "changelevel" ) )
	{
		UI_ConnectionProgress_ChangeLevel();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "serverinfo" ) )
	{
		UI_ConnectionProgress_ParseServerInfo( EngFuncs::CmdArgv(2) );
	}
}
