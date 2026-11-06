/*
PlayerList.cpp - in-game "Current players - Counter-Strike" player list dialog
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "BaseMenu.h"
#include "Framework.h"
#include "Table.h"
#include "CheckBox.h"
#include "PicButton.h"
#include "utlvector.h"
#include "utlstring.h"

// build 90: window caption. There is no localization key for it (and no
// "GameUI_Close"/"Mute in-game voice" key either - L() passes unknown keys
// through untouched, see MenuStrings.cpp), so English is hardcoded here.
#define UI_PLAYERLIST_TITLE		"Current players - Counter-Strike"

// y of the reference frame: the table sits directly under the title bar,
// mute-all box below it on the left, Close button bottom-right.
// A7 note: CMenuFramework::ApplyFloatLayout() wraps the visible items and
// CENTRES the frame itself, so these are authored sizes/relations, the
// final on-screen x is derived (dragging the title bar moves it afterwards).
#define UI_PLAYERLIST_X			560
#define UI_PLAYERLIST_Y			245
#define UI_PLAYERLIST_W			650
#define UI_PLAYERLIST_H			300

enum
{
	COLUMN_PLAYER_NAME = 0,
	COLUMN_PROPERTIES
};

// one row per connected player; the mute state is NOT cached here, it is
// queried from g_pClient on every draw (players can be muted elsewhere)
struct playerlist_entry_t
{
	int index;          // engine player slot, 1-based (matches g_pClient)
	CUtlString name;

	int NameCmp( const playerlist_entry_t &other ) const
	{
		return colorstricmp( name.String(), other.name.String() );
	}

	static int NameCmpAscend( const playerlist_entry_t *a, const playerlist_entry_t *b )
	{
		return a->NameCmp( *b );
	}
	static int NameCmpDescend( const playerlist_entry_t *a, const playerlist_entry_t *b )
	{
		return b->NameCmp( *a );
	}
};

class CMenuPlayerList;

// CMenuTable has no per-cell widgets (CELL_ITEM is commented out in
// model/BaseModel.h), so the accepted alternate layout is used:
//   column 1 "Player name"  - player name
//   column 2 "Properties"   - mute state indicator ("Mute in-game voice"
//                             while audible, "Muted" while blocked)
// double-clicking a row (or pressing Enter on it) toggles that player's mute.
//
// A7 build 140 (round 140 item 1, user: "fix in first picture, still show
// like that with no name player"): this class used to be named exactly like
// the FOUR-column player model in ServerInfo.cpp - two top-level classes,
// one name, two different vtable layouts (declaration order of Sort vs
// GetCellText swapped, plus this class' extra OnActivateEntry slot). The
// linker merged them weakly and ServerInfo's table dispatched through THIS
// class' vtable: its Name column ran this GetCellText(column 1) and printed
// the mute label "Mute in-game voice" instead of every player name, and its
// other virtuals (GetCellColors) landed on unrelated slots - the "weird
// color" rows in the same dialog. Renamed here (the sweep found no other
// cross-file duplicate in mainui) so each TU keeps its own vtable.
class CMenuPlayerListTableModel : public CMenuBaseModel
{
public:
	CMenuPlayerListTableModel() : CMenuBaseModel(), m_pMenu( nullptr ) {}

	void Update( void ) override { } // rows are pushed by the menu (RefreshPlayers)

	int GetColumns( void ) const override { return 2; }
	int GetRows( void ) const override { return entries.Count(); }

	const char *GetCellText( int line, int column ) override
	{
		if( !entries.IsValidIndex( line ))
			return "";

		switch( column )
		{
		case COLUMN_PLAYER_NAME:
			return entries[line].name.String();
		case COLUMN_PROPERTIES:
			if( g_pClient && g_pClient->IsPlayerGameVoiceMuted( entries[line].index ))
				return "Muted";
			return "Mute in-game voice";
		}
		return "";
	}

	bool Sort( int column, bool ascend ) override
	{
		if( column == COLUMN_PLAYER_NAME )
		{
			entries.Sort( ascend ? playerlist_entry_t::NameCmpAscend
			                     : playerlist_entry_t::NameCmpDescend );
			return true;
		}
		return false; // mute state is not sortable
	}

	// row selected/activated -> toggle that player's voice
	void OnActivateEntry( int line ) override;

	CUtlVector<playerlist_entry_t> entries;
	CMenuPlayerList *m_pMenu; // set once by CMenuPlayerList::_Init
};

class CMenuPlayerList : public CMenuFramework
{
public:
	CMenuPlayerList() : CMenuFramework( "CMenuPlayerList" ) {}

	void Show( void ) override;
	void Reload( void ) override;

	// public: called by CMenuPlayerListTableModel::OnActivateEntry
	void ToggleMute( int line );

private:
	void _Init( void ) override;

	void RefreshPlayers( void );           // rebuild rows + resync mute-all
	void SyncMuteAllCheckbox( void );
	void MuteAllChanged( void );           // mute-all / unmute-all

	CMenuTable players_list;
	CMenuPlayerListTableModel model;
	CMenuCheckBox muteAll;   // bottom-left "Mute in-game voice"
	CMenuPicButton close;    // bottom-right "Close"
};

ADD_MENU( menu_playerlist, CMenuPlayerList, UI_PlayerList_Menu );

void CMenuPlayerListTableModel::OnActivateEntry( int line )
{
	if( m_pMenu )
		m_pMenu->ToggleMute( line );
}

void CMenuPlayerList::Show( void )
{
	// only reachable from the in-game main menu list ("Player list");
	// never open it over the disconnected main menu
	if( !CL_IsActive() )
		return;

	// players may join/leave between opens - rebuild before the framework
	// runs Init/VidInit so the frame wraps the current rows
	RefreshPlayers();

	CMenuFramework::Show();
}

void CMenuPlayerList::Reload( void )
{
	BaseClass::Reload();
	RefreshPlayers();
}

void CMenuPlayerList::RefreshPlayers( void )
{
	model.entries.RemoveAll();

	if( g_pClient )
	{
		// engine player slots are 1-based: 0 is never a player
		const int maxPlayers = g_pClient->GetMaxPlayers();

		model.entries.EnsureCapacity( maxPlayers );

		for( int i = 1; i <= maxPlayers; i++ )
		{
			const char *name = g_pClient->GetPlayerName( i );

			if( !name || !name[0] )
				continue; // empty slot

			playerlist_entry_t entry;
			entry.index = i;
			entry.name = name; // copy: the engine pointer is not owned by us
			model.entries.AddToTail( entry );
		}
	}

	SyncMuteAllCheckbox();
}

void CMenuPlayerList::ToggleMute( int line )
{
	if( !g_pClient || !model.entries.IsValidIndex( line ))
		return;

	// same 1-based engine slot the voice ban manager expects
	const int idx = model.entries[line].index;

	if( g_pClient->IsPlayerGameVoiceMuted( idx ))
		g_pClient->UnmutePlayerGameVoice( idx );
	else
		g_pClient->MutePlayerGameVoice( idx );

	// column 2 is read live from g_pClient on draw, only the box needs sync
	SyncMuteAllCheckbox();
}

void CMenuPlayerList::MuteAllChanged( void )
{
	if( !g_pClient )
		return;

	for( int i = 0; i < model.entries.Count(); i++ )
	{
		const int idx = model.entries[i].index;

		if( muteAll.bChecked )
			g_pClient->MutePlayerGameVoice( idx );
		else
			g_pClient->UnmutePlayerGameVoice( idx );
	}
}

// mute-all box reflects "every listed player is muted"
void CMenuPlayerList::SyncMuteAllCheckbox( void )
{
	bool allMuted = g_pClient && model.entries.Count() > 0;

	for( int i = 0; i < model.entries.Count() && allMuted; i++ )
	{
		if( !g_pClient->IsPlayerGameVoiceMuted( model.entries[i].index ))
			allMuted = false;
	}

	muteAll.bChecked = allMuted; // plain field write: onChanged is NOT raised
}

void CMenuPlayerList::_Init( void )
{
	// build 90: right-aligned panel so the in-game menu column stays
	// visible on the left (reference screenshot 3)
	m_bFloatRightAlign = true;

	// the floating frame takes its caption from the banner name
	banner.szName = UI_PLAYERLIST_TITLE;

	// model must be set BEFORE AddItem (see controls/Table.h, rule 3)
	players_list.SetModel( &model );
	model.m_pMenu = this;
	players_list.bAllowSorting = true;
	players_list.SetCharSize( QM_SMALLFONT );
	players_list.SetupColumn( COLUMN_PLAYER_NAME, L( "Player name" ), 0.62f );
	players_list.SetupColumn( COLUMN_PROPERTIES, L( "Properties" ), 0.38f );
	players_list.SetRect( UI_PLAYERLIST_X, UI_PLAYERLIST_Y, UI_PLAYERLIST_W, UI_PLAYERLIST_H );

	// bottom-left checkbox: mute/unmute every listed player at once
	muteAll.szName = L( "Mute in-game voice" );
	muteAll.SetRect( UI_PLAYERLIST_X, UI_PLAYERLIST_Y + UI_PLAYERLIST_H + 12, 32, 32 );
	muteAll.onChanged = VoidCb( &CMenuPlayerList::MuteAllChanged );

	// bottom-right Close button (ctx menu: no GameUI_Close key in the dict).
	// no SetPicture(): in a float dialog a picture-less PicButton draws with
	// the GameUI "button" chrome around its own label (see PicButton::Draw)
	close.szName = L( "Close" );
	close.SetRect( UI_PLAYERLIST_X + UI_PLAYERLIST_W - 155,
		UI_PLAYERLIST_Y + UI_PLAYERLIST_H + 11, 155, 32 );
	close.onReleased = VoidCb( &CMenuPlayerList::Hide );

	AddItem( banner );
	AddItem( players_list );
	AddItem( muteAll );
	AddItem( close );
}