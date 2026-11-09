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
// A7 round 145 (item B, user: "make loading java version we did for
// android now do for pc"): the PC loading view is now a port of the ANDROID
// Java loading window (XashActivity.java, builds 97-144) instead of the old
// xash-styled 640x192 box with its Console/Cancel/Skip pict-buttons. The
// Java design, element for element:
//
//   collage backdrop (BackgroundLayout tiles) + 0x66000000 dim
//   centered olive panel #4c5844 with a raised light/dark bevel
//   title row: CS logo + "Loading..." (bold, bright #eaf0e6)
//   gold caption rule #c4b550 (2dp, 6dp under the title)
//   status line #d8ded3 (wraps, top-aligned context: "Connecting to X...",
//       "Starting local game server...", "Precaching resources...")
//   bar row: 18-segment gold progress bar over a #282e22 trough with a
//       sunken GoldSource bevel + the Cancel button (#3e4637 raised) beside
//       it on the left margin of the row
//   transfer block (hidden until a server file is fetched): dlName 13sp
//       #eaf0e6 -> dlStatus 12sp -> dlRow (16dp bar + gold bold file
//       counter, Cancel MOVES beside this bar) -> dlFooter 11sp. No percent
//       number next to the bar (Java build 106), no Skip/Console buttons.
//
// All GameUI_* L() tokens are gone from this file: round 144 proved
// L("GameUI_EstablishingConnection") / L("GameUI_Cancel") render their raw
// key while L("GameUI_StartingServer") resolves, so the port uses the plain
// English literals the Java window itself uses (they match
// resource/gameui_english.txt verbatim) - no localization can leak through.
// The state machine, the exports, the Esc "Really disconnect?" confirm box
// and the Android suppression guard (Android shows the Java overlay, this
// dialog stays down there) are unchanged.
#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"

#include "ItemsHolder.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "FontManager.h"

enum EState
{
	STATE_NONE,
	STATE_MENU, // do not hide when disconnected or in game
	STATE_DOWNLOAD, // transfer block row is on screen
	STATE_CONNECTING, // showing single progress
	STATE_CONSOLE // do not show until state reset
};

enum ESource
{
	SOURCE_CONSOLE,
	SOURCE_SERVERBROWSER,
	SOURCE_CREATEGAME
};

// A7 round 145: the Android loading-window palette (XashActivity "REAL CS
// 1.6 GameUI palette" tokens) - identical values to the menu defaults.
static const unsigned int A7_LOADING_PANEL   = 0xFF4C5844; // panel bg
static const unsigned int A7_LOADING_ACCENT  = 0xFFC4B550; // gold rule/fill
static const unsigned int A7_LOADING_TITLE   = 0xFFEAF0E6; // caption/name
static const unsigned int A7_LOADING_TEXT    = 0xFFD8DED3; // status labels

// dialog frame in 1024x768 menu units (Java's 340dp/12dp frame @ ~45% of
// the usable width, vertical rhythm kept from the Java LinearLayout stack)
#define A7_DLG_W    480
#define A7_DLG_H    164 // title + rule + status + main bar row + pad
#define A7_DLG_H_DL 284 // + transfer block (name/status/dlRow/footer)
#define A7_PAD      16

// Title row of the Java loading window (XashActivity titleRow): an 18dp
// SQUARE ImageView holding R.drawable.cs_logo - the GOLD soldier
// silhouette (cs_logo.png, 256x331, FIT_CENTER) - with an 8dp right
// margin, caption right next to it. Round 146 (item B): the PC port had
// the retail 256x16 "Counter-Strike" WORDMARK pushed far left with the
// title starting at x=280, which does not match Android at all (user:
// "not same like android in logo and others"). dp -> menu units keeps the
// same ~1.45 factor as the bar row (Java 18dp bar = our 26 units):
// box 18dp = 26 units square, margin 8dp = 11 units, FIT_CENTER of the
// 256x331 art inside the square = 20x26 (aspect preserved, centered).
#define A7_LOGO_X   16              // logo box, left inner pad
#define A7_LOGO_Y   17              // centered in the 16..44 title row
#define A7_LOGO_W   26              // Java 18dp square
#define A7_LOGO_H   26
#define A7_LOGO_IW  20              // art inside the box after FIT_CENTER
#define A7_TITLE_X  ( A7_LOGO_X + A7_LOGO_W + 11 ) // 53: box + 8dp margin

// ---------------------------------------------------------------------------
// the Java LoadingBar, reborn on the PC: an 18-cell gold segmented bar over
// the dark trough with the sunken GoldSource bevel frame (javax View.onDraw
// port).  Links the same cvars the old CMenuProgressBar did.
// ---------------------------------------------------------------------------
class CMenuLoadingBar : public CMenuBaseItem
{
public:
	typedef CMenuBaseItem BaseClass;

	CMenuLoadingBar() : BaseClass()
	{
		m_flMin = 0.0f;
		m_flMax = 100.0f;
		m_flValue = 0.0f;
		m_szCvarName = NULL;
		colorBase.SetDefault( A7_LOADING_ACCENT );
		iFlags |= QMF_INACTIVE;
	}

	void LinkCvar( const char *cvName, float flMin, float flMax )
	{
		m_szCvarName = cvName;
		m_flMax = flMax;
		m_flMin = flMin;
	}

	void SetValue( float flValue )
	{
		m_flValue = bound( 0.0f, flValue, 1.0f );
		m_szCvarName = NULL;
	}

	void Draw( void ) override
	{
		float flProgress;

		if( m_szCvarName )
		{
			flProgress = bound( m_flMin, EngFuncs::GetCvarFloat( m_szCvarName ), m_flMax );
			flProgress = ( flProgress - m_flMin ) / ( m_flMax - m_flMin );
		}
		else flProgress = m_flValue;

		// Java LoadingBar.onDraw: #282e22 trough first ...
		UI_FillRect( m_scPos, m_scSize, 0xFF282E22 );

		// ... 18 gold cells filled left to right up to the percent,
		// gap = max(2, w * 0.012), insetY = max(2, h * 0.16)
		const int segments = 18;
		int gap = Q_max( 2, (int)((float)m_scSize.w * 0.012f ));
		int segW = (int)(( m_scSize.w - gap * ( segments + 1 )) / segments );
		int insetY = Q_max( 2, (int)((float)m_scSize.h * 0.16f ));
		int lit = (int)( bound( 0.0f, flProgress, 1.0f ) * segments + 0.5f );

		if( m_scSize.h - insetY * 2 > 0 )
		{
			for( int i = 0; i < lit && segW > 0; i++ )
			{
				int x = m_scPos.x + gap + i * ( segW + gap );
				if( x + segW > m_scPos.x + m_scSize.w ) break;
				UI_FillRect( x, m_scPos.y + insetY, segW, m_scSize.h - insetY * 2, colorBase );
			}
		}

		// sunken GoldSource trough: dark top/left, light bottom/right
		// (Java: LOADING_BORDER_DK top+left, LOADING_BAR_BORDER bottom+right)
		UI_DrawBevel( m_scPos, m_scSize, true );
	}

private:
	float m_flMin, m_flMax, m_flValue;
	const char *m_szCvarName;
};

// The Java caption is 17sp BOLD against a 13sp status line; menu fonts are
// all weight 500 except the 16px bold, so one ad-hoc Tahoma 20/1000 face
// carries the size+weight gap. Built once and kept valid across resolution
// changes by FontManager::VidInit's ad-hoc rebuild (build 145 item C).
static HFont A7LoadingTitleFont( void )
{
	static HFont hFont = -1;

	if( hFont < 0 )
	{
		hFont = CFontBuilder( "Tahoma", 20, 1000 ).Create();
		if( hFont < 0 )
			hFont = QM_BIGFONT + 1; // belt: never hand handle 0 to a label
	}

	return hFont;
}

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
	// Java window chrome (so the Cancel PicButton draws button chrome)
	bool IsFloatDialog() const override { return true; }
	void HandleDisconnect( void );
	void HandlePrecache( void )
	{
		SetCommonText( "Precaching resources..." );
		loadBar.LinkCvar( "scr_loading", 0, 100 );
		m_iState = STATE_CONNECTING;
	}
	void HandleStufftext( float flProgress, const char *pszText )
	{
		SetCommonText( pszText );
		loadBar.SetValue( flProgress );
		// A7 round 147 item 1b: m_iState is NOT touched here any more.
		// The engine now calls this EVERY FRAME (Android loadingStatus
		// cadence), and stomping the state live would collapse a running
		// transfer block (STATE_DOWNLOAD) or flip the show-path's STATE_MENU
		// mid-load. State stays the property of Connect/Download/DownloadEnd/
		// Precache, exactly like before - this only moves the status line
		// and the main bar, like the Java window's per-frame setText.
	}
	void HandleDownload( const char *pszFileName, const char *pszServerName, int iCurrent, int iTotal, const char *comment );
	void HandleConnect( const char *pszText );

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
		if( IsVisible( ) && m_pStack->Current( ) == this && !FBitSet( iFlags, QMF_CLOSING  ))
			VidInit( ); // already on top: re-layout only
		else Show( );   // stale/closing/not-top: (re)activate strictly
	}

	void SetCommonText( const char *pszText )
	{
		Q_strncpy( sCommonString, pszText, sizeof( sCommonString ));
	}

	EState m_iState;
	ESource m_iSource;
private:
	CMenuBackgroundBitmap background;
	// full-screen game collage behind the panel (never a black screen)
	CMenuBackgroundBitmap screenBg;
	CMenuLoadingBar loadBar;        // main phase bar (scr_loading)
	CMenuLoadingBar dlBar;          // transfer bar (scr_download)
	CMenuPicButton cancelButton;    // Java "Cancel" beside whichever bar runs
	CMenuPicButton consoleButton;   // keyboard console path only, never drawn
	CMenuYesNoMessageBox dialog;
	CMenuAction title;               // [logo] "Loading..."
	CMenuAction statusText;          // live status line
	CMenuAction dlName;              // transfer file name
	CMenuAction dlStatus;            // "Downloading from server..."
	CMenuAction dlCount;             // gold bare file counter
	CMenuAction dlFooter;            // "N of M comment"
	HIMAGE logoPic;
	char sCommonString[512];
	char sDlFile[256];
	char sDlLastFile[256];
	char sDlStatus[128];
	char sDlFooter[256];
	char sDlCountString[16];
	int  m_iDlFiles;
};

CMenuConnectionProgress::CMenuConnectionProgress() : CMenuBaseWindow( "ConnectionProgress" )
{
	// Java: the status line starts as "Loading..." and the engine's phase
	// text (loadingStatus) overwrites it as the load advances
	Q_strncpy( sCommonString, "Loading...", sizeof( sCommonString ));
	sDlFile[0] = sDlLastFile[0] = '\0';
	sDlStatus[0] = sDlFooter[0] = sDlCountString[0] = '\0';
	Q_strncpy( sDlCountString, "0", sizeof( sDlCountString ));
	logoPic = 0;
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

	SetCommonText( "Disconnected" );

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

// Java loadingShow(serverAddr) -> status "Connecting to X...", or "Loading..."
// when there is no server (local game: the engine's own phase text follows,
// "Starting local game server...");
void CMenuConnectionProgress::HandleConnect( const char *pszText )
{
	if( pszText && pszText[0] )
	{
		m_iSource = SOURCE_SERVERBROWSER;
		snprintf( sCommonString, sizeof( sCommonString ), "Connecting to %s...", pszText );
	}
	else
	{
		m_iSource = SOURCE_CREATEGAME;
		SetCommonText( "Starting local game server..." );
	}

	// build 103: the file counter restarts per load (mLoadingDlFiles = 0
	// at the top of showLoadingOnUiThread)
	m_iDlFiles = 0;
	sDlLastFile[0] = sDlFile[0] = sDlStatus[0] = sDlFooter[0] = '\0';
	Q_strncpy( sDlCountString, "0", sizeof( sDlCountString ));

	loadBar.LinkCvar( "scr_loading", 0, 100 );
}

// Java loadingDownload(file, percent, status, footer): when file is null/empty
// the whole transfer block folds away (handled in _VidInit by state), while a
// file is fetched the block opens and the bare gold counter counts every file
// this load pulls - no percent sign (build 103/106).
void CMenuConnectionProgress::HandleDownload( const char *pszFileName, const char *pszServerName, int iCurrent, int iTotal, const char *comment )
{
	if( pszFileName && strstr( pszFileName, ".bsp" ))
		uiStatic.needMapListUpdate = true;

	if( pszFileName && pszFileName[0] )
	{
		Q_strncpy( sDlFile, pszFileName, sizeof( sDlFile ));

		if( strcmp( pszFileName, sDlLastFile ) != 0 )
		{
			Q_strncpy( sDlLastFile, pszFileName, sizeof( sDlLastFile ));
			m_iDlFiles++;
			snprintf( sDlCountString, sizeof( sDlCountString ), "%d", m_iDlFiles );
		}
	}

	// the engine's own transfer status text (cl_main: "Downloading from
	// server..."), verbatim - same literal the Java window receives
	Q_strncpy( sDlStatus, "Downloading from server...", sizeof( sDlStatus ));

	if( iTotal > 0 )
		snprintf( sDlFooter, sizeof( sDlFooter ), "%d of %d %s",
			iCurrent + 1, iTotal, comment ? comment : "" );
	else if( comment && comment[0] )
		Q_strncpy( sDlFooter, comment, sizeof( sDlFooter ));
	else sDlFooter[0] = '\0';

	m_iState = STATE_DOWNLOAD;
	// NOTE: the MAIN bar is untouched here - like the Java window it keeps
	// reading its own phase source (scr_loading); only dlBar shows bytes
}

void CMenuConnectionProgress::_Init( void )
{
	iFlags |= QMF_DIALOG;

	background.bForceColor = true;
	background.colorBase = A7_LOADING_PANEL;

	// the Java window draws no Console button, but the console key still
	// opens the console through this (hidden) item's handler - unchanged
	consoleButton.SetPicture( PC_CONSOLE );
	consoleButton.szName = "Console";
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
	consoleButton.Hide(); // no visible Console button (Java has neither)

	cancelButton.szName = "Cancel"; // Java: text button, #eaf0e6 on #3e4637
	cancelButton.onReleased = VoidCb( &CMenuConnectionProgress::Disconnect );
	cancelButton.bEnableTransitions = false;
	cancelButton.colorBase = A7_LOADING_TITLE;
	cancelButton.iFlags &= ~QMF_DROPSHADOW;

	dialog.SetMessage( "Really disconnect?" );
	dialog.Link( this );
	dialog.onPositive = VoidCb( &CMenuConnectionProgress::Disconnect );

	title.iFlags = QMF_INACTIVE|QMF_DROPSHADOW;
	title.eTextAlignment = QM_LEFT;
	title.szName = "Loading...";           // Java caption, verbatim
	title.colorBase = A7_LOADING_TITLE;
	title.font = A7LoadingTitleFont();
	title.charSize = 20;

	statusText.iFlags = QMF_INACTIVE;
	statusText.eTextAlignment = QM_LEFT;
	statusText.szName = sCommonString;
	statusText.SetCharSize( QM_DEFAULTFONT );
	statusText.colorBase = A7_LOADING_TEXT;

	dlName.iFlags = QMF_INACTIVE;
	dlName.eTextAlignment = QM_LEFT;
	dlName.szName = sDlFile;
	dlName.SetCharSize( QM_DEFAULTFONT );
	dlName.colorBase = A7_LOADING_TITLE;

	dlStatus.iFlags = QMF_INACTIVE;
	dlStatus.eTextAlignment = QM_LEFT;
	dlStatus.szName = sDlStatus;
	dlStatus.SetCharSize( QM_DEFAULTFONT );
	dlStatus.colorBase = A7_LOADING_TEXT;

	dlCount.iFlags = QMF_INACTIVE;
	dlCount.eTextAlignment = QM_LEFT;
	dlCount.szName = sDlCountString;
	dlCount.SetCharSize( QM_BOLDFONT );    // gold bare counter, bold
	dlCount.colorBase = A7_LOADING_ACCENT;

	dlFooter.iFlags = QMF_INACTIVE;
	dlFooter.eTextAlignment = QM_LEFT;
	dlFooter.szName = sDlFooter;
	dlFooter.SetCharSize( QM_TINYFONT );   // Java: 11sp footer
	dlFooter.colorBase = A7_LOADING_TEXT;

	loadBar.LinkCvar( "scr_loading", 0, 100 );
	dlBar.LinkCvar( "scr_download", 0.0f, 100.0f );

	// Round 146 (item B): the title-row logo is now the ANDROID one -
	// the GOLD soldier silhouette (res/drawable-nodpi/cs_logo.png, the
	// R.drawable.cs_logo the Java titleRow shows at 18dp FIT_CENTER),
	// shipped to the gamedir as resource/cs_logo.tga. The retail
	// resource/logo_game.tga WORDMARK above was the old PC-only choice
	// and is what the user called out as "not same like android".
	// belt: PIC_Load returns 0 when the asset is missing - an install
	// without it still gets a working window (the caption starts flush
	// at the inner pad and Draw() skips the logo; see _VidInit)
	logoPic = EngFuncs::PIC_Load( "resource/cs_logo.tga", PIC_NOFLIP_TGA );

	AddItem( background );
	AddItem( title );
	AddItem( statusText );
	AddItem( loadBar );
	AddItem( cancelButton );
	AddItem( dlName );
	AddItem( dlStatus );
	AddItem( dlBar );
	AddItem( dlCount );
	AddItem( dlFooter );
	AddItem( consoleButton );
}

void CMenuConnectionProgress::_VidInit( void )
{
	bool bDownload = ( m_iState == STATE_DOWNLOAD );
	int dlg_h = bDownload ? A7_DLG_H_DL : A7_DLG_H;
	int dlg_y = 768 / 2 - dlg_h / 2;

	// Java: FrameLayout.LayoutParams gravity CENTER - the frame sits in the
	// middle of the 1024x768 menu space (letterbox offset applied after)
	SetRect(( uiStatic.width - A7_DLG_W ) / 2, dlg_y, A7_DLG_W, dlg_h );
	pos.x += uiStatic.xOffset;
	pos.y += uiStatic.yOffset;

	// title row (Java titleRow): [gold soldier logo 18dp square @(16,17)]
	// "Loading..." right of it at the 8dp margin; without the logo the
	// caption sits flush at the inner pad instead of keeping a dead gap
	if( logoPic )
		title.SetRect( A7_TITLE_X, 16, A7_DLG_W - A7_TITLE_X - A7_PAD, 28 );
	else title.SetRect( A7_PAD, 16, A7_DLG_W - A7_PAD * 2, 28 );

	// gold caption rule (2dp tall, 6dp under the title) - drawn in Draw()
	// where it lands between the rows

	// status line: 8dp under the rule, wraps like setSingleLine(false)
	statusText.SetRect( A7_PAD, 66, A7_DLG_W - A7_PAD * 2, 40 );

	// main bar row: segmented bar fills the width, Cancel sits beside it
	// (Java barRow: bar weight=1, cancel 26dp tall with 8dp left margin)
	loadBar.SetRect( A7_PAD, 118, 330, 26 );
	cancelButton.SetRect( A7_DLG_W - A7_PAD - 110, 114, 110, 34 );

	if( bDownload )
	{
		// transfer block: name -> status -> dlRow(bar + bare counter +
		// Cancel, moved down beside THIS bar) -> footer (all hidden on
		// Android until loadingDownload supplies a file)
		dlName.Show();
		dlStatus.Show();
		dlBar.Show();
		dlCount.Show();
		dlFooter.SetVisibility( sDlFooter[0] != '\0' );
		dlName.SetRect( A7_PAD, 160, A7_DLG_W - A7_PAD * 2, 20 );
		dlStatus.SetRect( A7_PAD, 184, A7_DLG_W - A7_PAD * 2, 18 );
		dlBar.SetRect( A7_PAD, 214, 282, 26 );
		dlCount.SetRect( 306, 214, 42, 26 );
		dlFooter.SetRect( A7_PAD, 252, A7_DLG_W - A7_PAD * 2, 16 );

		// moveLoadingCancel(mLoadingDlRow) - Cancel rides beside the
		// transfer bar while one runs
		cancelButton.SetRect( A7_DLG_W - A7_PAD - 110, 210, 110, 34 );
	}
	else
	{
		dlName.Hide();
		dlStatus.Hide();
		dlBar.Hide();
		dlCount.Hide();
		dlFooter.Hide();
	}

	// no visible Console/Skip buttons on this window (Java has neither)
	consoleButton.Hide();

	// full-screen backdrop: collage + light dim pick up the real screen
	screenBg.VidInit();

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
	if( ( m_iState != STATE_MENU && CL_IsActive() ) || ( m_iState == STATE_NONE && m_pStack->Current() == this ))
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

	// Java backdrop: collage fills the frame, then the 0x66 dim on top so
	// the compact panel stays readable (was 0x40 before the port)
	screenBg.Draw();
	UI_FillRect( 0, 0, gpGlobals->scrWidth, gpGlobals->scrHeight, 0x66000000 );

	CMenuBaseWindow::Draw();

	// raised panel bevel: light top/left, dark bottom/right (Java
	// BevelDrawable raised = LOADING_BORDER_LT over LOADING_BORDER_DK)
	UI_DrawBevel( m_scPos, m_scSize, false );

	// gold caption rule: full inner width, 4 units tall, 8 under the title
	// row + 4 above the status line (Java ruleLp.topMargin 6dp, height 2dp)
	{
		int ruleX = m_scPos.x + (int)( A7_PAD * uiStatic.scaleX );
		int ruleW = m_scSize.w - (int)( A7_PAD * 2 * uiStatic.scaleX );
		int ruleY = m_scPos.y + (int)( 52 * uiStatic.scaleY );
		int ruleH = Q_max( 1, (int)( 4 * uiStatic.scaleY ));

		UI_FillRect( ruleX, ruleY, ruleW, ruleH, A7_LOADING_ACCENT );
	}

	// title-row logo: the ANDROID gold soldier (cs_logo.png), an 18dp
	// SQUARE FIT_CENTER ImageView in the Java titleRow - the 256x331 art
	// scales to A7_LOGO_IW x A7_LOGO_H inside the 26x26 box, centered.
	// Drawn ADDITIVE: this engine upload flattens the PNG's transparent
	// alpha to black, so a normal draw stamps a black box behind the
	// soldier - additive drops that black to nothing (the transparent
	// pixels are 0,0,0) and keeps the gold silhouette over the panel,
	// the same trick the old wordmark draw used for its white letters
	if( logoPic )
	{
		int lx = m_scPos.x + (int)(( A7_LOGO_X + ( A7_LOGO_W - A7_LOGO_IW ) / 2 ) * uiStatic.scaleX );
		int ly = m_scPos.y + (int)( A7_LOGO_Y * uiStatic.scaleY );
		int lw = (int)( A7_LOGO_IW * uiStatic.scaleX );
		int lh = (int)( A7_LOGO_H * uiStatic.scaleY );

		EngFuncs::PIC_Set( logoPic, 255, 255, 255, 255 );
		EngFuncs::PIC_DrawAdditive( lx, ly, lw, lh );
	}
}

ADD_MENU3( menu_connectionprogress, CMenuConnectionProgress, UI_ConnectionProgress_f );

// A7 build 126 (item 5): on Android the platform loading window (the Java
// XashActivity overlay - opaque, with its own Cancel) IS the join view, so
// every door funnels through these guards and this dialog stays down there.
// Unchanged from the pre-port build: Disconnect stays, with the state machine
// never leaving STATE_NONE HandleDisconnect is a no-op, and the engine
// reports connection failures itself.
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

	// A7 build 96: strictly visible+active (re-lays the frame out for the
	// transfer block - 164 -> 284 like Android's wrap-content box)
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_DownloadEnd( void )
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	// only meaningful while the transfer block is the one on screen (the
	// dialog's own Cancel already moved it out of this state, so the
	// http_clear that follows becomes a no-op here)
	if( menu_connectionprogress->m_iState != STATE_DOWNLOAD )
		return;

	// downloads over - back to the resource-loading state so scr_loading
	// drives the main bar again. NO HandleDisconnect on purpose: its
	// restore branch would pop Main + ServerBrowser over a still-running load
	menu_connectionprogress->HandlePrecache();
	menu_connectionprogress->m_iSource = SOURCE_CONSOLE;

	// A7 build 96: EnsureShown keeps the dialog strictly visible+active
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

	// changelevel keeps the ORIGINAL source semantics (HandleConnect would
	// switch the source to CREATEGAME and pop the create-game window on a
	// later disconnect) - only the phase text/state change, like before
	menu_connectionprogress->m_iState = STATE_MENU;
	menu_connectionprogress->SetCommonText( "Changing level on server" );
	menu_connectionprogress->EnsureShown();
}

void UI_ConnectionProgress_ParseServerInfo( const char *server )
{
	A7_CONN_DIALOG_OFF( );

	if( menu_connectionprogress->m_iState == STATE_CONSOLE )
		return;

	menu_connectionprogress->SetCommonText( "Parsing server info..." );
	menu_connectionprogress->m_iState = STATE_CONNECTING;
	menu_connectionprogress->EnsureShown();
}

// A7 round 147 item 1b (user: "PC connect/dialog texts not same like
// android"): the engine's per-frame load-phase feed - the SAME sentences
// Android's loadingStatus writes into the Java window (cl_main
// CL_LoadingPhaseText), pushed at the same cadence with the same combined
// percent. Like Android_LoadingStatus this lands ONLY while the window is
// up (a browsing menu never collects phase updates), and the Android
// suppression guard keeps it dead where the Java overlay owns the view.
void UI_ConnectionProgress_StuffText( float flProgress, const char *pszText )
{
	A7_CONN_DIALOG_OFF( );

	if( !pszText || !pszText[0] )
		return;

	if( !menu_connectionprogress->IsVisible( ))
		return;

	menu_connectionprogress->HandleStufftext( flProgress, pszText );
}

void UI_ConnectionProgress_f( void )
{
	if( !strcmp( EngFuncs::CmdArgv(1), "disconnect" ))
	{
		UI_ConnectionProgress_Disconnect();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "dl" ))
	{
		UI_ConnectionProgress_Download( EngFuncs::CmdArgv( 2 ), EngFuncs::CmdArgv( 3 ), atoi(EngFuncs::CmdArgv( 4 )), atoi(EngFuncs::CmdArgv( 5 )), EngFuncs::CmdArgv( 6 ) );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "dlend" ))
	{
		UI_ConnectionProgress_DownloadEnd();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "stufftext" ))
	{
		A7_CONN_DIALOG_OFF( );
		menu_connectionprogress->HandleStufftext( atof( EngFuncs::CmdArgv( 2 ) ), EngFuncs::CmdArgv( 3 ) );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "precache" ))
	{
		UI_ConnectionProgress_Precache();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "menu" ))
	{
		UI_ConnectionProgress_Connect( EngFuncs::CmdArgv(2) );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "localserver" ))
	{
		UI_ConnectionProgress_Connect( NULL );
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "changelevel" ))
	{
		UI_ConnectionProgress_ChangeLevel();
	}
	else if( !strcmp( EngFuncs::CmdArgv(1), "serverinfo" ))
	{
		UI_ConnectionProgress_ParseServerInfo( EngFuncs::CmdArgv(2) );
	}
}