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
#include "Action.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"
#include "MenuStrings.h"
#include "PlayerIntroduceDialog.h"
#include "gameinfo.h"
#include "AnimatedBanner.h"
#include "MovieBanner.h"
#include "FontRenderer.h"

// classic Counter-Strike game-menu text style: flat Tahoma, no glow.
// build 91: the reference menu rows are ~16px font with ~27px spacing
// (user: menu items "too big in xash") - keep the hit box tall enough that
// a full line can never wrap onto the next row.
static void ApplyMainMenuTextStyle( CMenuPicButton &button )
{
	button.SetCustomTextStyle( CFontBuilder( "Tahoma", 16, 400 ).Create(), 16 );
	button.charSize = 16;
	// text-sized hit box, so the tight classic stack never overlaps
	button.SetSize( 560, 26 );
}

// A7 user request: the Friends button opens the Steam sign-in/friend-list
// screen - an engine command that raises a WebView over the running game.
// The screen saves friends/names.txt for the menu and the scoreboard.
static void UI_SteamFriendsScreen( void )
{
	EngFuncs::ClientCmd( false, "steamfriends\n" );
}

// A7 build 91: the Console entry raises the engine's floating console
// window (toggleconsole opens the window in the menu / paused game)
static void UI_ConsoleWindow( void )
{
	EngFuncs::ClientCmd( false, "toggleconsole\n" );
}

class CMenuMain: public CMenuFramework
{
public:
	CMenuMain() : CMenuFramework( "CMenuMain" ) { }

	bool KeyDown( int key ) override;

	// A7 build 87: the only window that keeps the GameUI sound set -
	// every other window uses the original GoldSource launch_* sounds
	bool IsMainMenuRoot() const override { return true; }

	void QuitDialogCb();

private:
	void _Init() override;
	void _VidInit( ) override;

	void VidInit( bool connected );

	void DisconnectCb();
	void DisconnectDialogCb();

	CMenuPicButton	resumeGame;
	CMenuPicButton	disconnect;
	// build 90: in-game "Player list" entry (reference screenshot 3:
	// Resume game / Disconnect / Player list ...)
	CMenuPicButton	playerList;
	CMenuPicButton	newGame;
	CMenuPicButton	findServers;
	CMenuPicButton	randomServer;
	CMenuPicButton	configuration;
	// A7 build 91: "Console" entry - raises the engine's floating console
	// window (the Android equivalent of the PC `~` key)
	CMenuPicButton	consoleButton;
	CMenuPicButton	quit;

	// A7 build 63: real-menu furniture (video reference): the CS logo
	// bottom-left and the Friends button bottom-right
	CMenuBitmap	csLogo;
	CMenuPicButton	friendsButton;

	// quit dialog
	CMenuYesNoMessageBox dialog;
};

void CMenuMain::QuitDialogCb()
{
	if( CL_IsActive() && EngFuncs::GetCvarFloat( "host_serverstate" ) && EngFuncs::GetCvarFloat( "maxplayers" ) == 1.0f )
		dialog.SetMessage( L( "StringsList_235" ) );
	else
		dialog.SetMessage( L( "GameUI_QuitConfirmationText" ) );

	dialog.onPositive.SetCommand( false, "quit \"menu dialog\"\n" );
	dialog.Show();
	// A7 build 127 (round 128): Show() re-arms the open fade every time, so
	// drop it right after - the confirm box must be on screen the instant it
	// is asked for (GoldSource shows it flat, no animation) - user: "ingame
	// disconnect are of them too late like freeze".
	dialog.DisableTransition();
	// build 127: one-shot breadcrumb proving the instant-confirm build ships.
	static bool bInstantDialogWarned;
	if( !bInstantDialogWarned )
	{
		bInstantDialogWarned = true;
		Con_Printf( "[A7] build127: confirm dialogs open with no transition\n" );
	}
}

void CMenuMain::DisconnectCb()
{
	EngFuncs::ClientCmd( false, "disconnect\n" );
	VidInit( false );
	CalcPosition();
	CalcSizes();
	VidInitItems();
}

void CMenuMain::DisconnectDialogCb()
{
	dialog.onPositive = VoidCb( &CMenuMain::DisconnectCb );
	dialog.SetMessage( L( "Really disconnect?" ) );
	dialog.Show();
	// A7 build 127 (round 128): no open animation on the disconnect confirm
	// either - one less frame of perceived lag before the disconnect starts
	// (see QuitDialogCb).
	dialog.DisableTransition();
}

/*
=================
CMenuMain::Key
=================
*/
bool CMenuMain::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ) )
	{
		if ( CL_IsActive( ))
		{
			if( !dialog.IsVisible() )
				UI_CloseMenu();
		}
		else
		{
			QuitDialogCb( );
		}
		return true;
	}
	return CMenuFramework::KeyDown( key );
}

/*
=================
CMenuMain::_Init
=================
*/
void CMenuMain::_Init( void )
{
	// A7 remake: the original CS 1.6 game menu is a plain left-aligned text
	// list over the collage - New Game, Find Servers, Change game, Options,
	// Quit (plus Resume/Disconnect while connected). No banner art, no logo
	// bitmap, no movie - those are xash3d extras. Player setup lives only
	// in the Options dialog's Multiplayer tab (user request).
	resumeGame.SetNameAndStatus( L( "GameUI_GameMenu_ResumeGame" ), NULL );
	resumeGame.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( resumeGame );
	resumeGame.onReleased = UI_CloseMenu;

	disconnect.SetNameAndStatus( L( "GameUI_GameMenu_Disconnect" ), NULL );
	disconnect.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( disconnect );
	disconnect.onReleased = VoidCb( &CMenuMain::DisconnectDialogCb );

	// build 90: "Current players" window (reference picture 3) - shown in
	// the in-game list only, right under Disconnect like in GoldSource
	playerList.SetNameAndStatus( L( "Player list" ), NULL );
	playerList.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( playerList );
	playerList.onReleased = UI_PlayerList_Menu;

	newGame.SetNameAndStatus( L( "GameUI_NewGame" ), NULL );
	newGame.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( newGame );
	newGame.onReleased = UI_NewGame_Menu; // build 90: tabbed New Game dialog (ref picture 2)

	findServers.SetNameAndStatus( L( "Find Servers" ), NULL );
	findServers.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( findServers );
	findServers.onReleased = UI_InternetGames_Menu;

	// A7 build 63: video reference - the real menu's third item is
	// "Random server" (join a random entry of the loaded internet list);
	// the Change game dialog is gone, like in the original GameMenu.res.
	// build 90: no QMF_NOTIFY status string - its status text drew at a
	// fixed offset and ran/stuck into the neighbour rows (user: "remove
	// random"); the reference menu shows the entry with no status hint.
	randomServer.SetNameAndStatus( L( "Random server" ), NULL );
	randomServer.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( randomServer );
	randomServer.onReleased = UI_RandomServer;

	// build 90: reference has the Counter-Strike word-mark bottom-left,
	// plain over the collage - NO black tint/background (user: "logo
	// doesn't have black color background"). colorBase stays default so
	// CMenuBitmap::VidInit's SetDefault(white) applies.
	csLogo.SetPicture( "resource/game_menu.tga" );
	// A7 build 93: QM_DRAWNORMAL goes to PIC_Draw with no blending, which
	// paints the TGA's transparent pixels black - the word-mark needs the
	// alpha-blended path (user: "logo has black background - fix it")
	csLogo.SetRenderMode( QM_DRAWTRANS );
	csLogo.iFlags |= QMF_INACTIVE; // art, not an action
	friendsButton.szName = L( "Friends" );
	ApplyMainMenuTextStyle( friendsButton );
	// A7 user request: friends is a real screen - the engine command opens
	// the Steam login WebView, which saves the friend list for the menu and
	// the scoreboard
	friendsButton.onReleased = UI_SteamFriendsScreen;

	configuration.SetNameAndStatus( L( "GameUI_Options" ), NULL );
	configuration.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( configuration );
	configuration.onReleased = UI_Options_Menu;

	// A7 build 91: opens the engine's floating console window (toggleconsole
	// now raises the window instead of the full-screen console) - the
	// Android equivalent of the PC `~` key, an Options-like menu entry
	consoleButton.SetNameAndStatus( L( "Console" ), NULL );
	consoleButton.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( consoleButton );
	consoleButton.onReleased = UI_ConsoleWindow;

	quit.SetNameAndStatus( L( "GameUI_GameMenu_Quit" ), NULL );
	quit.iFlags |= QMF_NOTIFY;
	ApplyMainMenuTextStyle( quit );
	quit.onReleased = VoidCb( &CMenuMain::QuitDialogCb );

	// server.dll is needed to start a new game
	if( !EngFuncs::CheckGameDll( ))
		newGame.SetGrayed( true );

	dialog.Link( this );

	AddItem( resumeGame );
	AddItem( disconnect );
	AddItem( playerList ); // build 90: in-game-only, visibility gated in VidInit
	AddItem( newGame );
	AddItem( findServers );
	AddItem( randomServer );
	AddItem( configuration );
	AddItem( consoleButton );
	AddItem( quit );
	// A7 build 64: build 63 set the members up but never registered
	// them - the CS logo (bottom-left) and the Friends button
	// (bottom-right) from the video were configured yet never shown.
	AddItem( csLogo );
	AddItem( friendsButton );
}

/*
=================
UI_Main_Init
=================
*/
void CMenuMain::VidInit( bool connected )
{
	// A7 build 91: compact reference layout - ~16px rows with 28px spacing,
	// left-aligned column ending just above the word-mark row, so the stack
	// grows upward and always fits (user: menu items "too big in xash").
	int x = 36;
	int ygap = 28;

	// build 90: logo stays plain bottom-left over the collage; Friends on
	// the bottom bar (right)
	csLogo.SetRect( x, size.h - 72, 236, 37 );
	friendsButton.SetRect( size.w - 150, 706, 130, 28 );

	// A7 build 105 (user: "console menu that in main menu are in down of
	// options, move it to friends menu in up of him"): the Console entry
	// leaves the New game..Quit column and sits directly ABOVE the
	// Friends button on the right edge
	consoleButton.SetRect( size.w - 150, 706 - 34, 130, 28 );

	// rows: [Random server | blank] when out of a game, or [blank +
	// Resume/Disconnect/Player list] while connected, then one blank
	// spacer row, then the shared bottom group New Game..Quit
	// (build 91: Random server has no row while connected - its slot is
	// left blank like the reference in-game menu)
	// build 105: Console moved out of the column (it sits above Friends on
	// the right), so the stack is New Game, Find Servers, Options, Quit
	const int bottomRows = 4;
	int rows = bottomRows + 2 + ( connected ? 3 : 0 ); // + Random row + spacer
	int y = ( size.h - 84 ) - 44 - ( rows - 1 ) * ygap;

	if( connected )
	{
		y += ygap; // top slot stays blank while connected
		resumeGame.SetCoord( x, y );
		disconnect.SetCoord( x, y + ygap );
		playerList.SetCoord( x, y + ygap * 2 );
		// A7 build 92: skip PAST the player-list row and its blank spacer -
		// build 91 added only 2 gaps, so "New Game" landed on the exact
		// "Player list" row and the two entries drew on top of each other
		y += ygap * 4;
	}
	else
	{
		randomServer.SetCoord( x, y );
		y += ygap * 2; // one row + one blank spacer
	}

	newGame.SetCoord( x, y );
	findServers.SetCoord( x, y + ygap );
	configuration.SetCoord( x, y + ygap * 2 );
	// build 105: Console is parked above Friends (set at the top of this
	// function), the column now ends with Quit
	quit.SetCoord( x, y + ygap * 3 );

	// now figure out what's visible
	resumeGame.SetVisibility( connected );
	disconnect.SetVisibility( connected );
	playerList.SetVisibility( connected );
	newGame.SetVisibility( true );
	findServers.SetVisibility( true );
	// build 91: hidden while connected (blank slot in the in-game menu)
	randomServer.SetVisibility( !connected );
	configuration.SetVisibility( true );
	consoleButton.SetVisibility( true );
}

void CMenuMain::_VidInit()
{
	VidInit( CL_IsActive() );
}

ADD_MENU( menu_main, CMenuMain, UI_Main_Menu );

/*
=================
UI_QuitDialog_f

called by the engine when the OS asks to quit (window close button, Cmd+Q on macOS...)
=================
*/
static void UI_QuitDialog_f( void )
{
	// nothing to lose, quit immediately
	if( !CL_IsActive( ))
	{
		EngFuncs::ClientCmd( false, "quit \"menu\"\n" );
		return;
	}

	if( !UI_IsVisible( ))
		UI_Main_Menu();

	menu_main->QuitDialogCb();
}
ADD_COMMAND( menu_quit, UI_QuitDialog_f );
