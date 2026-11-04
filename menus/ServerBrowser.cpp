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
#include "YesNoMessageBox.h"
#include "Table.h"
#include "keydefs.h"
#include "Switch.h"
#include "Field.h"
#include "utlvector.h"
#include "CheckBox.h"
#include "SpinControl.h"
#include "StringArrayModel.h"
#include "DropDown.h"
#include "Action.h" // A7: friendsHint label in the Friends tab

#define ART_BANNER_INET     "gfx/shell/head_inetgames"
#define ART_BANNER_LAN      "gfx/shell/head_lan"
#define ART_BANNER_LOCK     "gfx/shell/lock"
#define ART_BANNER_FAVORITE "gfx/shell/favorite"

#define MAX_PING 9.999f
#define FILTER_MAX_MAPS 16
#define INTERNET_TAB_COUNT 6  // original GameUI row: Internet/Favorites/History/Spectate/LAN/Friends

class CMenuServerBrowser;

// A7 build 127 (round 128): display order now matches the original
// GoldSource "Servers" window (user: "improve find servers like original
// goldsource, check pictures"):
//   [lock][star] Players | Servers (N) | Game | Map | Latency | (ip)
// COLUMN_* stays the identity of a column everywhere; only the numbers
// (which ARE the on-screen order) moved, plus the new COLUMN_GAME.
enum
{
	COLUMN_PASSWORD = 0,
	COLUMN_FAVORITE,
	COLUMN_PLAYERS,
	COLUMN_NAME,     // header is set dynamically: "Servers (<count>)"
	COLUMN_GAME,
	COLUMN_MAP,
	COLUMN_PING,     // header "Latency", value = plain ms number
	COLUMN_IP,
	COLUMN_LAST
};

struct server_t
{
	netadr_t adr;
	char  info[512];
	float ping;
	int  numcl;
	int  maxcl;
	char name[64];
	char mapname[64];
	char gamestr[64]; // A7 build 127: GoldSource "Game" column (mod name)
	char clientsstr[64];
	char pingstr[64];
	char ipstr[64];
	bool favorite;
	bool havePassword;
	bool isGoldSrc;
	bool pending_info;

	float prevPing;
	float pingChangedTime;
	int   prevNumcl;
	float playersChangedTime;
	float mapChangedTime;

	server_t( netadr_t adr, const char *info, bool is_favorite, bool pending_info = false );
	void UpdateData();
	void SetPing( float ping );

	const char *ToProtocol( void )
	{
		if( isGoldSrc ) return "gs";
		return "49";
	}

	int NameCmp( const server_t &other ) const
	{
		return colorstricmp( name, other.name );
	}

	int AdrCmp( const server_t &other ) const
	{
		return EngFuncs::NET_CompareAdr( &adr, &other.adr );
	}

	int MapCmp( const server_t &other ) const
	{
		return stricmp( mapname, other.mapname );
	}

	int ClientCmp( const server_t &other ) const
	{
		if( numcl > other.numcl ) return 1;
		else if( numcl < other.numcl ) return -1;
		return 0;
	}

	int PingCmp( const server_t &other ) const
	{
		if( ping > other.ping ) return 1;
		else if( ping < other.ping ) return -1;
		return 0;
	}

	bool IsEmpty( ) const
	{
		return numcl == 0;
	}

	bool IsFull( ) const
	{
		return numcl >= maxcl;
	}

#define GENERATE_COMPAR_FN( method ) \
	static int method ## Ascend( const void *a, const void *b ) \
	{\
		return (( const server_t *)a)->method( *(( const server_t *)b) );\
	}\
	static int method ## Descend( const void *a, const void *b ) \
	{\
		return (( const server_t *)b)->method( *(( const server_t *)a) );\
	}\

	GENERATE_COMPAR_FN( NameCmp )
	GENERATE_COMPAR_FN( AdrCmp )
	GENERATE_COMPAR_FN( MapCmp )
	GENERATE_COMPAR_FN( ClientCmp )
	GENERATE_COMPAR_FN( PingCmp )
#undef GENERATE_COMPAR_FN
};

struct favlist_entry_t
{
	favlist_entry_t( const char *sadr, const char *prot, bool favorited = true )
	{
		Q_strncpy( this->sadr, sadr, sizeof( this->sadr ));
		Q_strncpy( this->prot, prot, sizeof( this->prot ));
		this->favorited = favorited;
	}

	void GenerateDummyInfoString( CUtlString &info ) const
	{
		info.AppendFormat( "\\host\\%s", sadr );
		info.AppendFormat( "\\gamedir\\%s", gMenu.m_gameinfo.gamefolder );
		info.Append( "\\map\\unknown\\numcl\\0\\maxcl\\0" );
		if( !stricmp( prot, "current" ) || !strcmp( prot, "49" ))
		{
			info.Append( "\\p\\49" );
		}
		else if( !stricmp( prot, "goldsrc" ) || !stricmp( prot, "gs" ))
		{
			info.Append( "\\p\\48\\gs\\1" );
		}
		else
		{
			UI_ShowMessageBox( "Invalid protocol while generating dummy info string" );
			info.Clear();
		}
	}

	void QueryServer( void ) const
	{
		EngFuncs::ClientCmdF( false, "ui_queryserver \"%s\" \"%s\"", sadr, prot );
	}

	char sadr[128];
	char prot[16];
	bool favorited;
};

struct filterMap_t
{
	char name[64];

	filterMap_t( ): filterMap_t( "" ) {}

	filterMap_t( const filterMap_t &obj )
	{
		count = obj.count;
		Q_strncpy( name, obj.name, sizeof( name ) );
		Q_strncpy( display, obj.display, sizeof( display ) );
	}

	filterMap_t( const char *s, unsigned short c = 0 )
	{
		count = c;
		Q_strncpy( name, s, sizeof( name ) );
		UpdateDisplay( );
	}

	inline bool operator==( const char *s ) const
	{
		return colorstricmp( name, s ) == 0;
	}

	void AddCount( unsigned short c )
	{
		count += c;
		UpdateDisplay();
	}

	const char *GetDisplay( ) const
	{
		return display;
	}

	static int CmpName( const filterMap_t *a, const filterMap_t *b )
	{
		return colorstricmp( a->name, b->name );
	}

	static int CmpCount( const filterMap_t *a, const filterMap_t *b )
	{
		unsigned short ca = a->count;
		unsigned short cb = b->count;
		return ca < cb ? -1 : (ca > cb ? 1 : 0);
	}

	static int CmpCountInvert( const filterMap_t *a, const filterMap_t *b )
	{
		unsigned short ca = a->count;
		unsigned short cb = b->count;
		return ca < cb ? 1 : (ca > cb ? -1 : 0);
	}

private:
	char display[128];
	unsigned short count;

	void UpdateDisplay()
	{
		snprintf( display, sizeof( display ), "(%d) %s", count, name );
	}
};

class CMenuGameListModel : public CMenuBaseModel
{
public:
	CMenuGameListModel( CMenuServerBrowser *parent ) :
		CMenuBaseModel(), parent( parent ), m_iSortingColumn(-1)
	{
		filterPing = MAX_PING * 1000.0f;
		filterEmpty = 0;
		filterFull = 0;
		filterProtocol = 0;
	}

	void Update() override;

	int GetColumns() const override
	{
		return COLUMN_LAST; // havePassword, game, mapname, maxcl, ping, (hidden)ip
	}

	int GetRows() const override
	{
		return servers.Count();
	}

	ECellType GetCellType( int line, int column ) override
	{
		if( column == COLUMN_PASSWORD || column == COLUMN_FAVORITE )
			return CELL_IMAGE_ADDITIVE;
		return CELL_TEXT;
	}

	const char *GetCellText( int line, int column ) override
	{
		// A7 build 116 (same belt as ServerInfo's 112 guard): the list is
		// repopulated/filtered while the browser draws - an out-of-range
		// row must render empty, not read past the vector.
		if( line < 0 || line >= servers.Count( ))
			return NULL;

		switch( column )
		{
		case COLUMN_PASSWORD: return servers[line].havePassword ? ART_BANNER_LOCK : NULL;
		case COLUMN_FAVORITE: return servers[line].favorite ? ART_BANNER_FAVORITE : NULL;
		case COLUMN_NAME: return servers[line].name;
		case COLUMN_GAME: return servers[line].gamestr;
		case COLUMN_MAP: return servers[line].mapname;
		case COLUMN_PLAYERS: return servers[line].clientsstr;
		case COLUMN_PING: return servers[line].pingstr;
		case COLUMN_IP: return servers[line].ipstr;
		default: return NULL;
		}
	}

	bool GetCellColors( int line, int column, unsigned int &textColor, bool &force) const override
	{
		// A7 build 116: parity with GetCellText's bounds guard
		if( line < 0 || line >= servers.Count( ))
			return false;

		const server_t &s = servers[line];
		const float fadeTime = 1.5f;
		const uint green = PackRGB( 64, 255, 64 );
		const uint red = PackRGB( 255, 64, 64 );

		float changedTime = 0.0f;
		uint flashColor = green;

		switch( column )
		{
		case COLUMN_PING:
			changedTime = s.pingChangedTime;
			flashColor = s.ping < s.prevPing ? green : red;
			break;
		case COLUMN_PLAYERS:
			changedTime = s.playersChangedTime;
			flashColor = s.numcl > s.prevNumcl ? green : red;
			break;
		case COLUMN_MAP:
			changedTime = s.mapChangedTime;
			flashColor = green;
			break;
		default:
			return false;
		}

		if( changedTime <= 0.0f )
			return false;

		float age = EngFuncs::DoubleTime() - changedTime;
		if( age >= fadeTime )
			return false;

		textColor = InterpColor( flashColor, uiPromptTextColor, age / fadeTime );
		force = true;
		return true;
	}

	void OnActivateEntry( int line ) override;

	void Flush()
	{
		filterMaps.RemoveAll();
		servers.RemoveAll();
		serversRefreshTime = gpGlobals->time;
	}

	bool IsHavePassword( int line )
	{
		if( line < 0 || line >= servers.Count( ))
			return false;
		return servers[line].havePassword;
	}

	void AddServerToList( netadr_t adr, const char *info, bool is_favorite );

	bool Sort( int column, bool ascend ) override;

	void SetFilterMap( const char *mapname )
	{
		filterMap = filterMap_t( mapname );
	}

	float serversRefreshTime;
	float filterPing;
	char filterEmpty;
	char filterFull;
	char filterProtocol;
	CUtlVector<server_t> servers;

	filterMap_t filterMap;
	CUtlVector<filterMap_t> filterMaps;
private:
	CMenuServerBrowser *parent;

	int m_iSortingColumn;
	bool m_bAscend;
};

class CMenuServerBrowser: public CMenuFramework
{
public:
	CMenuServerBrowser() : CMenuFramework( "CMenuServerBrowser" ), gameListModel( this ), m_iLastRefreshTab( -1 ) { }
	void Draw() override;
	void Show() override;
	void Hide() override;
	bool KeyUp( int key ) override;

	void SetLANOnly( bool lanOnly )
	{
		m_bLanOnly = lanOnly;
	}
	void GetGamesList( void );
	void ClearList( void );
	void RefreshList( void );
	void JoinGame( void );
	void ResetPing( void )
	{
		gameListModel.serversRefreshTime = EngFuncs::DoubleTime();
	}
	void ViewGameInfo( void );
	void OnTabSwitch( void );

	void ParseServerListFromFile( const char *filename, CUtlVector<favlist_entry_t> &list );
	void SaveServerListToFile( const char *filename, const CUtlVector<favlist_entry_t> &list );
	void QueryServerList( const CUtlVector<favlist_entry_t> &list );

	// A7: the original GameUI Friends tab, filled from the file the
	// Steam friends WebView screen writes (<gamedir>/friends/servers.txt);
	// returns true when the file changed since the previous read
	bool LoadFriendsTab( void );

	// shows the empty-tab caption only while the Friends tab has no rows
	void UpdateFriendsHint( void );
	void FavoriteServer( void );
	void MaybeEnableFavoriteButton( void );
	void ToggleFavoriteButton( bool en );
	void OnChangeSelectedServer( void );
	void SaveLists( void );

	void ShowAddServerBox( void );
	void AddServer( void );

	void AddServerToList( netadr_t adr, const char *info );
	bool IsAddressInTab( int tab, const netadr_t &adr ) const;

	static void Connect( server_t &server );

	CMenuPicButton *joinGame;
	CMenuPicButton *refresh;
	CMenuPicButton *viewGameInfo;
	CMenuPicButton *favorite;
	CMenuPicButton *addServer;
	CMenuSwitch tabSwitch; // not actually tabs

	CMenuDropDownFloat filterPing;
	CMenuDropDownInt filterEmpty;
	CMenuDropDownInt filterFull;
	CMenuDropDownStr filterMap;
	CMenuDropDownInt filterProtocol;

	CMenuYesNoMessageBox msgBox;
	CMenuTable	gameList;
	CMenuGameListModel gameListModel;
	// A7: caption inside the Friends tab while it is still empty (shows up
	// until the Steam friends screen has saved servers.txt)
	CMenuAction friendsHint;
	// last bytes read from friends/servers.txt - the file itself is the
	// change oracle for the Friends tab (no second server_t list needed)
	CUtlString m_sFriendsRaw;
	CMenuCheckBox showip;

	CMenuYesNoMessageBox askPassword;
	CMenuField password;

	CMenuYesNoMessageBox addServerBox;
	CMenuField addressField;
	CMenuSpinControl serverProtocol;

	int	  refreshTime;
	int   refreshTime2;

	bool m_bLanOnly;

	CUtlVector<favlist_entry_t> favoritesList;
	CUtlVector<favlist_entry_t> historyList;
	int m_iLastRefreshTab;
	CUtlVector<server_t> m_InternetServers[INTERNET_TAB_COUNT];
	CUtlVector<server_t> m_LanServers;
private:
	void _Init() override;
	void _VidInit() override;
};

static server_t staticServerSelect( netadr_t(), "", false );
static bool staticWaitingPassword = false;

ADD_MENU3( menu_internetgames, CMenuServerBrowser, UI_InternetGames_Menu );
ADD_MENU4( menu_langame, NULL, UI_LanGame_Menu, NULL );

/*
=================
CMenuServerBrowser::Menu
=================
*/
void UI_ServerBrowser_Menu( void )
{
	if ( gMenu.m_gameinfo.gamemode == GAME_SINGLEPLAYER_ONLY )
		return;

	// stop demos to allow network sockets to open
	if ( gpGlobals->demoplayback && EngFuncs::GetCvarFloat( "cl_background" ))
	{
		uiStatic.m_iOldMenuDepth = uiStatic.menu.Count();
		EngFuncs::ClientCmd( false, "stop\n" );
		uiStatic.m_fDemosPlayed = true;
	}

	menu_internetgames->Show();
}

void UI_InternetGames_Menu( void )
{
	menu_internetgames->SetLANOnly( false );

	UI_ServerBrowser_Menu();
}

/*
=================
UI_RandomServer

A7 build 63: "Random server" main-menu item (matches the real menu in
the user's video reference) - joins a random entry of the internet list
that is already loaded; opens the server browser when the list is still
empty so the user can refresh it.
=================
*/
void UI_RandomServer( void )
{
	CUtlVector<server_t> &list = menu_internetgames->gameListModel.servers;

	if( !list.Count( ))
	{
		UI_InternetGames_Menu();
		return;
	}

	CMenuServerBrowser::Connect( list[EngFuncs::RandomLong( 0, list.Count() - 1 )] );
}

void UI_LanGame_Menu( void )
{
	menu_internetgames->SetLANOnly( true );

	UI_ServerBrowser_Menu();
}

server_t::server_t( netadr_t adr, const char *info, bool is_favorite, bool pending_info ) :
	adr( adr ), favorite( is_favorite ), pending_info( pending_info ),
	prevPing( 0.0f ), pingChangedTime( 0.0f ),
	prevNumcl( 0 ), playersChangedTime( 0.0f ), mapChangedTime( 0.0f )
{
	Q_strncpy( this->info, info, sizeof( this->info ));
}

void server_t::UpdateData( void )
{
	Q_strncpy( name, Info_ValueForKey( info, "host" ), sizeof( name ));
	Q_strncpy( mapname, Info_ValueForKey( info, "map" ), sizeof( mapname ));
	Q_strncpy( ipstr, EngFuncs::NET_AdrToString( adr ), sizeof( ipstr ));
	numcl = atoi( Info_ValueForKey( info, "numcl" ));
	maxcl = atoi( Info_ValueForKey( info, "maxcl" ));
	// A7 build 127 (round 128): the reference GoldSource browser prints
	// "14 / 32" (slash, spaces), not the old "14\32" backslash form
	snprintf( clientsstr, sizeof( clientsstr ), "%d / %d", numcl, maxcl );
	havePassword = !strcmp( Info_ValueForKey( info, "password" ), "1" );
	isGoldSrc = !strcmp( Info_ValueForKey( info, "gs" ), "1" );

	// A7 build 127: the GoldSource "Game" column - the mod name the server
	// reports ("game" key, falling back to "gamedir"), with the well-known
	// folder names spelled out the way original GameUI shows them.
	{
		const char *g = Info_ValueForKey( info, "game" );

		if( !g || !g[0] )
			g = Info_ValueForKey( info, "gamedir" );

		if( !g || !g[0] )
			Q_strncpy( gamestr, "-", sizeof( gamestr ));
		else if( !stricmp( g, "cstrike" ))
			Q_strncpy( gamestr, "Counter-Strike", sizeof( gamestr ));
		else if( !stricmp( g, "czero" ))
			Q_strncpy( gamestr, "Condition Zero", sizeof( gamestr ));
		else if( !stricmp( g, "valve" ))
			Q_strncpy( gamestr, "Half-Life", sizeof( gamestr ));
		else if( !stricmp( g, "tfc" ))
			Q_strncpy( gamestr, "Team Fortress Classic", sizeof( gamestr ));
		else Q_strncpy( gamestr, g, sizeof( gamestr ));
	}
}

void server_t::SetPing( float ping )
{
	ping = bound( 0.0f, ping, MAX_PING );

	this->ping = ping;
	// A7 build 127: the reference Latency column is a bare number (header
	// says "Latency"), not "99 ms"
	snprintf( pingstr, sizeof( pingstr ), "%.f", ping * 1000 );
}

bool CMenuGameListModel::Sort(int column, bool ascend)
{
	m_iSortingColumn = column;
	if( column == -1 )
		return false; // disabled

	m_bAscend = ascend;
	switch( column )
	{
	case COLUMN_PASSWORD:
		return false;
	case COLUMN_NAME:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::NameCmpAscend : server_t::NameCmpDescend );
		return true;
	case COLUMN_MAP:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::MapCmpAscend : server_t::MapCmpDescend );
		return true;
	case COLUMN_PLAYERS:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::ClientCmpAscend : server_t::ClientCmpDescend );
		return true;
	case COLUMN_PING:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::PingCmpAscend : server_t::PingCmpDescend );
		return true;
	case COLUMN_IP:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::AdrCmpAscend : server_t::AdrCmpDescend );
		return true;
	}

	return false;
}

/*
=================
CMenuServerBrowser::GetGamesList
=================
*/
void CMenuGameListModel::Update( void )
{
	int		i;

	// A7 build 127 (round 128): the original GoldSource browser carries the
	// row count in the Name column header - "Servers (12)" in the user's
	// screenshots. Static buffer: the table only stores the pointer, so it
	// must outlive this call.
	{
		static char s_serversHeader[32];
		snprintf( s_serversHeader, sizeof( s_serversHeader ), "Servers (%d)", servers.Count() );
		parent->gameList.SetHeaderText( COLUMN_NAME, s_serversHeader );
	}

	// regenerate table data
	for( i = 0; i < servers.Count(); i++ )
		servers[i].UpdateData();

	if( servers.Count() )
	{
		parent->joinGame->SetGrayed( false );
		parent->MaybeEnableFavoriteButton();
		parent->OnChangeSelectedServer();
		if( m_iSortingColumn != -1 )
			Sort( m_iSortingColumn, m_bAscend );
	}
}

void CMenuGameListModel::OnActivateEntry( int line )
{
	CMenuServerBrowser::Connect( servers[line] );
}

void CMenuGameListModel::AddServerToList( netadr_t adr, const char *info, bool is_favorite )
{
	int i;
	int pos = -1;

	for( i = 0; i < servers.Count(); i++ )
	{
		if( !EngFuncs::NET_CompareAdr( &servers[i].adr, &adr ))
		{
			pos = i;
			break;
		}
	}

	// update existing entry's ping/info in place; skip filter re-check
	if( pos >= 0 )
	{
		float oldPing = servers[pos].ping;
		int oldNumcl = servers[pos].numcl;
		char oldMap[sizeof( servers[pos].mapname )];
		Q_strncpy( oldMap, servers[pos].mapname, sizeof( oldMap ));
		bool wasPending = servers[pos].pending_info;

		Q_strncpy( servers[pos].info, info, sizeof( servers[pos].info ));
		servers[pos].UpdateData();
		servers[pos].SetPing( EngFuncs::DoubleTime() - serversRefreshTime );
		servers[pos].pending_info = false;

		if( !wasPending )
		{
			float now = EngFuncs::DoubleTime();

			if( fabsf( servers[pos].ping - oldPing ) > 0.005f )
			{
				servers[pos].prevPing = oldPing;
				servers[pos].pingChangedTime = now;
			}
			if( servers[pos].numcl != oldNumcl )
			{
				servers[pos].prevNumcl = oldNumcl;
				servers[pos].playersChangedTime = now;
			}
			if( oldMap[0] && stricmp( servers[pos].mapname, oldMap ) != 0 )
				servers[pos].mapChangedTime = now;
		}

		if( m_iSortingColumn != -1 )
			Sort( m_iSortingColumn, m_bAscend );
		return;
	}

	server_t server( adr, info, is_favorite );

	server.UpdateData();
	server.SetPing( EngFuncs::DoubleTime() - serversRefreshTime );

	if( server.ping > filterPing )
		return;

	if( filterEmpty == '1' && !server.IsEmpty( ))
		return;
	if( filterEmpty == '0' && server.IsEmpty( ))
		return;

	if( filterFull == '1' && !server.IsFull( ))
		return;
	if( filterFull == '0' && server.IsFull( ))
		return;
	
	if( filterProtocol == '1' && server.isGoldSrc )
		return;
	if( filterProtocol == '2' && !server.isGoldSrc )
		return;

	if( filterMap.name[0] && colorstricmp( filterMap.name, server.mapname ) != 0 )
		return;

	if( server.mapname[0] != 0 )
	{
		bool foundMap = false;

		for( int i = 0; i < filterMaps.Count(); ++i )
		{
			foundMap = filterMaps[i] == server.mapname;

			if( foundMap )
			{
				filterMaps[i].AddCount( server.numcl );
				break;
			}
		}

		if( !foundMap )
		{
			filterMaps.AddToTail( filterMap_t( server.mapname, server.numcl ) );

			// sort by players count
			filterMaps.Sort( filterMap_t::CmpCountInvert );

			parent->filterMap.Clear();
			for( int i = Q_min( FILTER_MAX_MAPS, filterMaps.Count() ); i--; )
				parent->filterMap.AddItem( filterMaps[i].GetDisplay(), filterMaps[i].name );
			parent->filterMap.AddItem( L( "any map" ), "" );

			if( !filterMap.name[0] )
				parent->filterMap.SelectLast( false );
		}
	}

	servers.AddToTail( server );

	if( m_iSortingColumn != -1 )
		Sort( m_iSortingColumn, m_bAscend );
}

void CMenuServerBrowser::Connect( server_t &server )
{
	// prevent refresh during connect
	menu_internetgames->refreshTime = uiStatic.realTime + 999999;

	// ask user for password
	if( server.havePassword )
	{
		// if dialog window is still open, then user have entered the password
		if( !staticWaitingPassword )
		{
			// save current select
			staticServerSelect = server;
			staticWaitingPassword = true;

			// show password request window
			menu_internetgames->askPassword.Show();

			return;
		}
	}
	else
	{
		// remove password, as server don't require it
		EngFuncs::CvarSetString( "password", "" );
	}

	staticWaitingPassword = false;

	const char *sadr = EngFuncs::NET_AdrToString( server.adr );
	const char *prot = server.ToProtocol();

	if( menu_internetgames->m_bLanOnly == false )
	{
		bool serverExists = false;

		for( int i = 0; i < menu_internetgames->historyList.Count(); ++i )
		{
			if( strcmp( menu_internetgames->historyList[i].sadr, sadr ) == 0 && strcmp( menu_internetgames->historyList[i].prot, prot ) == 0 )
			{
				serverExists = true;
				break;
			}
		}

		if( !serverExists )
		{
			if( menu_internetgames->historyList.Count() > 20 ) // FIXME: make configurable
				menu_internetgames->historyList.FastRemove( 0 );
			menu_internetgames->historyList.AddToTail( favlist_entry_t( sadr, prot, true ) );


			menu_internetgames->SaveLists();
		}
	}

	EngFuncs::ClientCmdF( false, "connect \"%s\" \"%s\"\n", sadr, prot );

	UI_ConnectionProgress_Connect( "" );
}

/*
=================
CMenuServerBrowser::JoinGame
=================
*/
void CMenuServerBrowser::JoinGame()
{
	gameListModel.OnActivateEntry( gameList.GetCurrentIndex() );
}

void CMenuServerBrowser::FavoriteServer()
{
	int i = gameList.GetCurrentIndex();

	if( !gameListModel.servers.IsValidIndex( i ))
		return;

	server_t &serv = gameListModel.servers[i];
	const char *sadr = EngFuncs::NET_AdrToString( serv.adr );

	serv.favorite = !serv.favorite;

	ToggleFavoriteButton( !serv.favorite );

	if( serv.favorite )
	{
		favlist_entry_t entry( sadr, serv.ToProtocol() );
		favoritesList.AddToTail( entry );
	}
	else
	{
		FOR_EACH_VEC( favoritesList, i )
		{
			if( !strcmp( favoritesList[i].sadr, sadr ))
			{
				favoritesList.FastRemove( i );
				break;
			}
		}
	}
}

void CMenuServerBrowser::MaybeEnableFavoriteButton( void )
{
	// favorites work the same in Direct and NAT modes
	favorite->SetGrayed( false );
}

void CMenuServerBrowser::ToggleFavoriteButton( bool en )
{
	if( en )
	{
		favorite->szName = L( "Favorite" );
		favorite->SetPicture( PC_FAVORITE, 'o' );
	}
	else
	{
		favorite->szName = L( "Unfavorite" );
		favorite->SetPicture( PC_UNFAVORITE, 'o' );
	}
}

void CMenuServerBrowser::ClearList()
{
	filterMap.Clear();
	if( gameListModel.filterMap.name[0] )
		filterMap.AddItem( gameListModel.filterMap.GetDisplay(), gameListModel.filterMap.name );
	filterMap.AddItem( L( "any map" ), "" );

	gameListModel.Flush();
	joinGame->SetGrayed( true );
	viewGameInfo->SetGrayed( true );
	favorite->SetGrayed( true );
}

void CMenuServerBrowser::ParseServerListFromFile( const char *filename, CUtlVector<favlist_entry_t> &list )
{
	byte *pfile = EngFuncs::COM_LoadFile( filename );
	char *afile = (char *)pfile;

	while( true )
	{
		favlist_entry_t entry( "", "", true );

		afile = EngFuncs::COM_ParseFile( afile, entry.sadr, sizeof( entry.sadr ));
		if( !afile )
			break;

		afile = EngFuncs::COM_ParseFile( afile, entry.prot, sizeof( entry.prot ));
		if( !afile )
			break;

		// ignore legacy protocol
		if( !strcmp( entry.prot, "48" ))
			continue;

		list.AddToTail( entry );
	}

	EngFuncs::COM_FreeFile( pfile );
}

void CMenuServerBrowser::SaveServerListToFile( const char *filename, const CUtlVector<favlist_entry_t> &list )
{
	CUtlString s;

	if( list.Count() == 0 )
	{
		EngFuncs::DeleteFile( filename );
		return;
	}

	FOR_EACH_VEC( list, i )
	{
		if( !list[i].favorited )
			continue;

		s.AppendFormat( "%s %s\n", list[i].sadr, list[i].prot );
	}

	EngFuncs::COM_SaveFile( filename, s.Get( ), s.Length( ));
}

void CMenuServerBrowser::QueryServerList( const CUtlVector<favlist_entry_t> &list )
{
	int curTab = tabSwitch.GetState();
	if( curTab < 0 || curTab >= INTERNET_TAB_COUNT )
		return;  // Safety: invalid tab index

	FOR_EACH_VEC( list, i )
	{
		netadr_t adr;

		if( !EngFuncs::textfuncs.pNetAPI->StringToAdr( (char *)list[i].sadr, &adr ))
			continue;

		bool found = false;

		FOR_EACH_VEC( m_InternetServers[curTab], j )
		{
			if( !EngFuncs::NET_CompareAdr( &m_InternetServers[curTab][j].adr, &adr ))
			{
				found = true;
				break;
			}
		}

		if( !found )
		{
			CUtlString fakeInfoString;
			list[i].GenerateDummyInfoString( fakeInfoString );

			server_t serv( adr, fakeInfoString, list[i].favorited, true );
			serv.UpdateData();
			serv.SetPing( 9.999f );

			// store dummy entry into per-tab buffer
			m_InternetServers[curTab].AddToTail( serv );

			// if this tab is currently visible, show the placeholder only when the
			// visible model is empty (avoid flashing many dummy entries on each refresh)
			if( curTab == tabSwitch.GetState() && gameListModel.GetRows() == 0 )
			{
				gameListModel.AddServerToList( adr, fakeInfoString, list[i].favorited );
				joinGame->SetGrayed( false );
				viewGameInfo->SetGrayed( false );
				MaybeEnableFavoriteButton();
				OnChangeSelectedServer();
			}
		}

		list[i].QueryServer();
	}

	UI_MenuResetPing_f();
}

/*
=================
CMenuServerBrowser::LoadFriendsTab

A7: the Friends tab of the original GameUI row. One entry per line of
<gamedir>/friends/servers.txt, format "ip:port<TAB>server name" - the
Steam friends screen picks the steam://connect link out of every friend's
page and saves it there.

Every line becomes a pending dummy server, the same placeholder path the
Favorites/History tabs use for a not-yet-queried entry: Join game and View
game info work right away, and the query issued afterwards fills in the
real ping, map and player counts.

Returns true when the file had actually changed since the last read: the
previous bytes are the change oracle, so the periodic refresh in Draw()
does not throw away the rows (and the user's selection) for nothing - and
no second server_t list has to be kept around, which a CUtlVector copy
would need (server_t has no default constructor).
=================
*/
bool CMenuServerBrowser::LoadFriendsTab( void )
{
	const int tab = INTERNET_TAB_COUNT - 1; // Friends is the last tab

	int len = 0;
	byte *pfile = EngFuncs::COM_LoadFile( "friends/servers.txt", &len );
	const char *raw = pfile ? (const char *)pfile : "";
	const int rawLen = pfile ? len : 0; // data, never assumed NUL-terminated

	const char *prev = m_sFriendsRaw.Get( );
	const int prevLen = prev ? (int)m_sFriendsRaw.Length( ) : 0;

	if( prevLen == rawLen && ( rawLen == 0 || !memcmp( raw, prev, rawLen )))
	{
		// nothing new since the last read - keep the list (and the rows
		// built from it) exactly as they are
		if( pfile )
			EngFuncs::COM_FreeFile( pfile );
		return false;
	}

	m_sFriendsRaw.Clear( );
	m_sFriendsRaw.Append( raw, rawLen );
	m_InternetServers[tab].RemoveAll( );

	const char *p = raw;
	const char *end = raw + rawLen;

	while( p < end )
	{
		const char *eol = (const char *)memchr( p, '\n', end - p );
		int ln = eol ? (int)( eol - p ) : (int)( end - p );

		while( ln > 0 && ( p[ln - 1] == '\r' || p[ln - 1] == '\n' ))
			ln--;

		if( ln > 0 )
		{
			char line[512];
			char addr[128];
			char name[128];
			char *sep;

			if( ln >= (int)sizeof( line ))
				ln = sizeof( line ) - 1;
			memcpy( line, p, ln );
			line[ln] = 0;

			// "ip:port \t server name" - an address never contains a tab or
			// a space, so the first separator ends it; the name may contain
			// both (server names very often contain spaces)
			sep = strchr( line, '\t' );
			if( !sep )
				sep = strchr( line, ' ' );

			if( sep )
			{
				char *nm;

				*sep = 0;
				Q_strncpy( addr, line, sizeof( addr ));

				nm = sep + 1;
				while( *nm == '\t' || *nm == ' ' )
					nm++;
				Q_strncpy( name, nm, sizeof( name ));
			}
			else
			{
				// address only - the row falls back to showing the address
				Q_strncpy( addr, line, sizeof( addr ));
				name[0] = 0;
			}

			netadr_t adr;

			if( addr[0] && EngFuncs::textfuncs.pNetAPI->StringToAdr( addr, &adr ))
			{
				bool found = false;

				FOR_EACH_VEC( m_InternetServers[tab], j )
				{
					if( !EngFuncs::NET_CompareAdr( &m_InternetServers[tab][j].adr, &adr ))
					{
						found = true;
						break;
					}
				}

				if( !found )
				{
					CUtlString info;

					info.AppendFormat( "\\host\\%s",
						name[0] ? name : EngFuncs::NET_AdrToString( adr ));
					info.AppendFormat( "\\gamedir\\%s", gMenu.m_gameinfo.gamefolder );
					info.Append( "\\map\\unknown\\numcl\\0\\maxcl\\0\\p\\49" );

					server_t serv( adr, info, false, true );
					serv.UpdateData();
					serv.SetPing( 9.999f ); // "no answer yet" placeholder

					m_InternetServers[tab].AddToTail( serv );

					// ask the entry for its real ping/map/player counts right
					// away; the reply arrives through UI_AddServerToList and
					// updates this tab's row in place (same path Favorites
					// and History entries take)
					EngFuncs::ClientCmdF( false, "ui_queryserver \"%s\" \"%s\"",
						EngFuncs::NET_AdrToString( adr ), serv.ToProtocol( ));
				}
			}
		}

		if( !eol )
			break;
		p = eol + 1;
	}

	if( pfile )
		EngFuncs::COM_FreeFile( pfile );

	return true;
}

/*
=================
CMenuServerBrowser::UpdateFriendsHint

A7: the Friends tab reads a file that only exists after a Steam sign-in
through the Friends screen on the main menu, so an empty tab says that
instead of showing a blank frame.
=================
*/
void CMenuServerBrowser::UpdateFriendsHint( void )
{
	const bool friendsTab = ( tabSwitch.GetState( ) == INTERNET_TAB_COUNT - 1 );

	if( friendsTab && gameListModel.GetRows( ) == 0 )
		friendsHint.Show( );
	else
		friendsHint.Hide( );
}

void CMenuServerBrowser::RefreshList()
{
	gameListModel.serversRefreshTime = EngFuncs::DoubleTime();

	if( tabSwitch.GetState() == INTERNET_TAB_COUNT - 1 )
	{
		// A7: Friends - re-read what the Steam friends screen saved. Same
		// gate as every other tab here, so Draw()'s periodic refresh and the
		// Refresh button act identically; the rows are only rebuilt when the
		// file actually changed, which keeps the current selection alive
		// across the automatic refreshes
		if( uiStatic.realTime > refreshTime2 )
		{
			const int tab = INTERNET_TAB_COUNT - 1;
			bool changed;

			refreshTime2 = uiStatic.realTime + 1000;
			refresh->SetGrayed( true );

			changed = LoadFriendsTab( );

			// the visible table can also go empty without the file changing:
			// a filter's on-changed does ClearList() (flushes the model) and
			// then lands here - the rows have to come back in that case too
			if( !changed && m_InternetServers[tab].Count( ) > 0
				&& gameListModel.GetRows( ) == 0 )
				changed = true;

			if( changed )
			{
				gameListModel.Flush();

				FOR_EACH_VEC( m_InternetServers[tab], i )
				{
					gameListModel.AddServerToList( m_InternetServers[tab][i].adr,
						m_InternetServers[tab][i].info, m_InternetServers[tab][i].favorite );
				}

				joinGame->SetGrayed( gameListModel.GetRows( ) == 0 );
				viewGameInfo->SetGrayed( gameListModel.GetRows( ) == 0 );
				if( gameListModel.GetRows( ))
				{
					MaybeEnableFavoriteButton();
					OnChangeSelectedServer();
				}
			}
		}

		UpdateFriendsHint();
		return;
	}

	if( m_bLanOnly )
	{
		m_LanServers.RemoveAll();
		EngFuncs::ClientCmd( false, "localservers\n" );
	}
	else if( uiStatic.realTime > refreshTime2 )
	{
		m_iLastRefreshTab = tabSwitch.GetState();
		if( m_iLastRefreshTab >= 0 && m_iLastRefreshTab < INTERNET_TAB_COUNT )
			m_InternetServers[m_iLastRefreshTab].RemoveAll();

		if( tabSwitch.GetState() == 1 )
			QueryServerList( favoritesList );
		else if( tabSwitch.GetState() == 2 )
			QueryServerList( historyList );
		else
		{
			char filter[128] = "";
			char *buf = filter;
			int remaining = sizeof(filter);

			if( filterEmpty.GetItem( ) )
			{
				int n = snprintf(buf, remaining, "\\empty\\%c", (char) filterEmpty.GetItem( ) );
				remaining -= n;
				buf += n;
			}

			if( filterFull.GetItem( ) )
			{
				int n = snprintf(buf, remaining, "\\full\\%c", (char) filterFull.GetItem( ) );
				remaining -= n;
				buf += n;
			}

			if( filterMap.GetItem( )[0] )
			{
				int n = snprintf(buf, remaining, "\\map\\%s", filterMap.GetItem( ) );
				remaining -= n;
				buf += n;
			}

			EngFuncs::ClientCmdF( false, "internetservers %s\n", filter );
		}

		refreshTime2 = uiStatic.realTime + (EngFuncs::GetCvarFloat("cl_nat") ? 4000:1000);
		refresh->SetGrayed( true );
		if( uiStatic.realTime + 20000 < refreshTime )
			refreshTime = uiStatic.realTime + 20000;
	}
}

void CMenuServerBrowser::ViewGameInfo()
{
	int idx = gameList.GetCurrentIndex();

	if( idx < 0 || idx >= gameListModel.GetRows( ))
		return;

	UI_ServerInfo_Menu( gameListModel.servers[idx].adr, gameListModel.servers[idx].name );
}

void CMenuServerBrowser::OnTabSwitch()
{
	int idx = tabSwitch.GetState();

	// the original GameUI row has no NAT browser tab:
	// Internet/Spectate query the master directly, Favorites/History go
	// through their lists, LAN uses the local broadcast, Friends reads the
	// list the Steam friends screen saved
	EngFuncs::CvarSetValue( "cl_nat", 0.0f );
	m_bLanOnly = ( idx == 4 ); // LAN tab

	ClearList();
	// populate visible model from per-tab buffer so user sees cached results instantly
	int newTab = tabSwitch.GetState();
	if( newTab < 0 || newTab >= INTERNET_TAB_COUNT )
		newTab = 0;  // Safety: default to Internet tab if invalid

	// A7: the Friends tab has no cache worth keeping - the file the Steam
	// friends screen wrote is the source of truth; LoadFriendsTab only
	// touches the list when it really changed
	if( newTab == INTERNET_TAB_COUNT - 1 )
		LoadFriendsTab();

	for( int i = 0; i < m_InternetServers[newTab].Count(); ++i )
	{
		// re-add entries via model to apply filters and update UI controls
		gameListModel.AddServerToList( m_InternetServers[newTab][i].adr, m_InternetServers[newTab][i].info, m_InternetServers[newTab][i].favorite );
	}
	RefreshList();

	// the empty-state caption belongs to the Friends tab only; RefreshList
	// already placed it there and this keeps it in step for every other tab
	UpdateFriendsHint();
}

void CMenuServerBrowser::OnChangeSelectedServer( void )
{
	int i = gameList.GetCurrentIndex();

	if( !gameListModel.servers.IsValidIndex( i ))
		return;

	bool fav = gameListModel.servers[i].favorite;
	ToggleFavoriteButton( !fav );
}

void CMenuServerBrowser::ShowAddServerBox( void )
{
	addServerBox.Show();
}

void CMenuServerBrowser::AddServer( void )
{
	netadr_t adr;

	if( !EngFuncs::textfuncs.pNetAPI->StringToAdr( (char *)addressField.GetBuffer(), &adr ))
	{
		UI_ShowMessageBox( L( "Invalid address" ));
		return;
	}

	const char *proto;

	switch( (int)serverProtocol.GetCurrentValue( ))
	{
	case 0:
		proto = "49";
		break;
	case 1:
		proto = "gs";
		break;
	default:
		UI_ShowMessageBox( L( "Invalid protocol" ));
		return;
	}

	// FIXME: for now we can only show custom servers at favorites tab

	// Check for duplicates in favorites list
	FOR_EACH_VEC( favoritesList, i )
	{
		if( !strcmp( favoritesList[i].sadr, addressField.GetBuffer() ) &&
		    !strcmp( favoritesList[i].prot, proto ))
		{
			UI_ShowMessageBox( L( "Server already in favorites" ));
			return;
		}
	}

	favlist_entry_t entry( addressField.GetBuffer(), proto, false );
	favoritesList.AddToTail( entry );

	int previousTab = tabSwitch.GetState();
	if( previousTab != 1 ) // Favorites tab
	{
		tabSwitch.SetState( 1 );
	}

	CUtlString fakeInfoString;
	entry.GenerateDummyInfoString( fakeInfoString );

	server_t serv( adr, fakeInfoString, false, true );
	serv.UpdateData();
	serv.SetPing( MAX_PING );
	m_InternetServers[1].AddToTail( serv );
	if( tabSwitch.GetState() == 1 )
	{
		gameListModel.AddServerToList( adr, fakeInfoString, false );
	}

	entry.QueryServer();
	UI_MenuResetPing_f();
}

/*
=================
UI_Background_Ownerdraw
=================
*/
void CMenuServerBrowser::Draw( void )
{
	CMenuFramework::Draw();

	if( uiStatic.realTime > refreshTime )
	{
		RefreshList();
		refreshTime = uiStatic.realTime + 20000; // refresh every 10 secs
	}

	if( uiStatic.realTime > refreshTime2 )
	{
		refresh->SetGrayed( false );
	}
}

bool CMenuServerBrowser::KeyUp( int key )
{
	if( key == 'i' )
	{
		gameList.SetColumnWidth( COLUMN_IP, 300, true );

		gameList.VidInit();
		gameListModel.Update();
	}

	return CMenuFramework::KeyUp( key );
}

/*
=================
CMenuServerBrowser::Init
=================
*/
void CMenuServerBrowser::_Init( void )
{
	AddItem( banner );

	joinGame = AddButton( L( "Join game" ), nullptr, PC_JOIN_GAME, VoidCb( &CMenuServerBrowser::JoinGame ), QMF_GRAYED );
	joinGame->onReleasedClActive = msgBox.MakeOpenEvent();

	// A7 build 93: no "Create server" button - the reference Find Servers
	// window doesn't have one (user request); RealignButtons flows the
	// remaining buttons up so Favorite/Refresh/Add server/Done keep their
	// clean left column

	viewGameInfo = AddButton( L( "View game info" ), nullptr, PC_VIEW_GAME_INFO, VoidCb( &CMenuServerBrowser::ViewGameInfo ), QMF_GRAYED );
	favorite = AddButton( L( "Favorite" ), nullptr, PC_FAVORITE, VoidCb( &CMenuServerBrowser::FavoriteServer ), 0, 'o' );
	refresh = AddButton( L( "Refresh" ), nullptr, PC_REFRESH, VoidCb( &CMenuServerBrowser::RefreshList ) );
	addServer = AddButton( L( "Add server" ), nullptr, PC_ADD_SERVER, VoidCb( &CMenuServerBrowser::ShowAddServerBox ));

	AddButton( L( "Done" ), nullptr, PC_DONE, VoidCb( &CMenuServerBrowser::Hide ) );

	msgBox.SetMessage( L( "Join a network game will exit any current game, OK to exit?" ) );
	msgBox.SetPositiveButton( L( "GameUI_OK" ), PC_OK );
	msgBox.HighlightChoice( CMenuYesNoMessageBox::HIGHLIGHT_YES );
	msgBox.onPositive = VoidCb( &CMenuServerBrowser::JoinGame );
	msgBox.Link( this );

	gameList.SetCharSize( QM_SMALLFONT );
	// A7 build 127 (round 128): GoldSource column order + headers taken
	// from the user's reference screenshots: [lock][star] | Players |
	// Servers (N) | Game | Map | Latency | (hidden ip). The Name header
	// text is refreshed with the live row count from the model's Update().
	gameList.SetupColumn( COLUMN_PASSWORD, NULL, 32.0f, true );
	gameList.SetupColumn( COLUMN_FAVORITE, NULL, 32.0f, true );
	// A7 build 134 (round 135, user: "fix find servers texts stucks ...
	// correct names ... just like goldsource"): the name column now takes
	// 0.40 of the free space instead of 0.34 (and 57% of it instead of
	// 51%) and two fixed columns give 40 design px back, so a real server
	// name fits before anything has to be cut at the column edge. "Players"
	// 110 -> 90 still holds "14 / 32" at the small font, "Latency" 90 -> 80
	// still holds 4 digits; Game 0.16 -> 0.14 because it only ever prints
	// "cstrike"/"valve".
	gameList.SetupColumn( COLUMN_PLAYERS, L( "Players" ), 90.0f, true );
	gameList.SetupColumn( COLUMN_NAME, "Servers (0)", 0.40f );
	gameList.SetupColumn( COLUMN_GAME, L( "Game" ), 0.14f );
	gameList.SetupColumn( COLUMN_MAP, L( "GameUI_Map" ), 0.16f );
	gameList.SetupColumn( COLUMN_PING, L( "Latency" ), 80.0f, true );
	gameList.SetupColumn( COLUMN_IP, L( "IP" ), 0, true );
	// A7 build 127 (round 128): one-shot proof the GoldSource column layout
	// shipped into libmenu (scan marker).
	{
		static int a7_browserColsLogged = 0;

		if( !a7_browserColsLogged )
		{
			a7_browserColsLogged = 1;
			Con_Printf( "[A7] build127: goldsource server columns (Players | Servers (N) | Game | Map | Latency)\n" );
		}
	}
	// A7 build 134 (round 135): one-shot proof the GoldSource cell-clip rule
	// shipped into libmenu (scan marker) - cells draw the whole string and
	// are cut at the column edge instead of being ellipsised mid-word, and
	// the name column carries the new 0.40/90/0.14/80 widths.
	{
		static int a7_cellClipLogged = 0;

		if( !a7_cellClipLogged )
		{
			a7_cellClipLogged = 1;
			Con_Printf( "[A7] build134: goldsource cell clip (full text cut at column edge, name col 0.40) + utf8 continuation guard\n" );
		}
	}
	gameList.SetModel( &gameListModel );
	gameList.bFramedHintText = true;
	gameList.bAllowSorting = true;
	gameList.onChanged = VoidCb( &CMenuServerBrowser::OnChangeSelectedServer );
	gameList.SetSize( -20, 465 );

	tabSwitch.SetRect( 360, 230, -20, 32 );
	tabSwitch.AddSwitch( L( "Internet" ));
	tabSwitch.AddSwitch( L( "Favorites" ));
	tabSwitch.AddSwitch( L( "History" ));
	tabSwitch.AddSwitch( L( "Spectate" ));
	tabSwitch.AddSwitch( "LAN" ); // intentionally not localized, like in GameUI
	tabSwitch.AddSwitch( L( "Friends" ));
	tabSwitch.eTextAlignment = QM_CENTER;
	tabSwitch.bMouseToggle = false;
	tabSwitch.bKeepToggleWidth = true;
	tabSwitch.iSelectColor = uiInputFgColor;
	tabSwitch.iFgTextColor = uiInputFgColor - 0x00151515; // bit darker
	tabSwitch.onChanged = VoidCb( &CMenuServerBrowser::OnTabSwitch );

	password.bHideInput = true;
	password.bAllowColorstrings = false;
	password.bNumbersOnly = false;
	password.szName = L( "GameUI_Password" );
	password.iMaxLength = 16;
	password.SetRect( 188, 140, 270, 32 );

	SET_EVENT_MULTI( askPassword.onPositive,
	{
		CMenuServerBrowser *parent = (CMenuServerBrowser*)pSelf->Parent();

		EngFuncs::CvarSetString( "password", parent->password.GetBuffer() );
		parent->password.Clear(); // we don't need entered password anymore
		CMenuServerBrowser::Connect( staticServerSelect );
	});

	SET_EVENT_MULTI( askPassword.onNegative,
	{
		CMenuServerBrowser *parent = (CMenuServerBrowser*)pSelf->Parent();

		EngFuncs::CvarSetString( "password", "" );
		parent->password.Clear(); // we don't need entered password anymore
		staticWaitingPassword = false;
	});

	askPassword.SetMessage( L( "GameUI_PasswordPrompt" ) );
	askPassword.Link( this );
	askPassword.Init();
	askPassword.AddItem( password );

	addressField.bAllowColorstrings = false;
	addressField.szName = NULL;
	addressField.eTextAlignment = QM_LEFT;
	addressField.SetRect( 64, 150, 512, 32 );

	static const char *protlist[] =
	{
		"Xash3D 49",
		"GoldSource 48",
	};
	static CStringArrayModel protlistModel( protlist, V_ARRAYSIZE( protlist ));

	serverProtocol.Setup( &protlistModel );
	serverProtocol.SetCurrentValue( 0.0f );
	serverProtocol.eTextAlignment = QM_LEFT;
	serverProtocol.SetRect( 64, 100, 512, 32 );\

	addServerBox.onPositive = VoidCb( &CMenuServerBrowser::AddServer );
	// A7 build 127 (round 128): the original GoldSource "Add Server"
	// wording + example addresses from the user's reference screenshot.
	// Kept to 3 lines: the message must stay above the protocol spin at
	// y=100 in this fixed 640x256 dialog (the original stacks the
	// examples under the field, we have no room for that here).
	addServerBox.SetMessage( L( "Enter the IP address of the server you wish to add.\nExamples: tfc.valvesoftware.com, counterstrike.speakeasy.net:27016\nor 205.158.143.200:27015" ));
	addServerBox.dlgMessage1.SetCoord( 8, 8 ); // a bit offset
	addServerBox.dlgMessage1.eTextAlignment = QM_TOPLEFT;
	addServerBox.Link( this );
	addServerBox.Init();
	addServerBox.AddItem( serverProtocol );
	addServerBox.AddItem( addressField );

	filterPing.AddItem( "500ms", 500.0f );
	filterPing.AddItem( "200ms", 200.0f );
	filterPing.AddItem( "100ms", 100.0f );
	filterPing.AddItem( "50ms", 50.0f );
	filterPing.AddItem( "20ms", 20.0f );
	filterPing.AddItem( L( "ping" ), MAX_PING * 1000.0f );
	filterPing.SelectLast( );
	filterPing.bDropUp = true;
	filterPing.eTextAlignment = QM_LEFT;
	filterPing.iFgTextColor = uiInputFgColor - 0x00151515;
	filterPing.SetCharSize( QM_SMALLFONT );
	filterPing.SetSize( 70, 30 );
	SET_EVENT_MULTI( filterPing.onChanged,
	{
		CMenuDropDownFloat *self = (CMenuDropDownFloat*)pSelf;
		CMenuServerBrowser *parent = (CMenuServerBrowser*)self->Parent();

		parent->gameListModel.filterPing = self->GetItem( ) / 1000.0f;
		parent->ClearList();
		parent->RefreshList();
	});

	filterEmpty.AddItem( L( "empty" ), '1' );
	filterEmpty.AddItem( L( "not empty" ), '0' );
	filterEmpty.AddItem( L( "any" ), 0 );
	filterEmpty.SelectLast( false );
	filterEmpty.bDropUp = true;
	filterEmpty.eTextAlignment = QM_LEFT;
	filterEmpty.iFgTextColor = uiInputFgColor - 0x00151515;
	filterEmpty.SetCharSize( QM_SMALLFONT );
	filterEmpty.SetSize( 120, 30 );
	SET_EVENT_MULTI( filterEmpty.onChanged,
	{
		CMenuDropDownInt *self = (CMenuDropDownInt*)pSelf;
		CMenuServerBrowser *parent = (CMenuServerBrowser*)self->Parent();

		parent->gameListModel.filterEmpty = self->GetItem( );
		parent->ClearList();
		parent->RefreshList();
	});

	filterFull.AddItem( L( "full" ), '1' );
	filterFull.AddItem( L( "not full" ), '0' );
	filterFull.AddItem( L( "any" ), 0 );
	filterFull.SelectLast( false );
	filterFull.bDropUp = true;
	filterFull.eTextAlignment = QM_LEFT;
	filterFull.iFgTextColor = uiInputFgColor - 0x00151515;
	filterFull.SetCharSize( QM_SMALLFONT );
	filterFull.SetSize( 120, 30 );
	SET_EVENT_MULTI( filterFull.onChanged,
	{
		CMenuDropDownInt *self = (CMenuDropDownInt*)pSelf;
		CMenuServerBrowser *parent = (CMenuServerBrowser*)self->Parent();

		parent->gameListModel.filterFull = self->GetItem( );
		parent->ClearList();
		parent->RefreshList();
	});

	filterMap.AddItem( L( "any map" ), "" );
	filterMap.bDropUp = true;
	filterMap.eTextAlignment = QM_LEFT;
	filterMap.iFgTextColor = uiInputFgColor - 0x00151515;
	filterMap.SetCharSize( QM_SMALLFONT );
	filterMap.SetSize( 200, 30 );
	SET_EVENT_MULTI( filterMap.onChanged,
	{
		CMenuDropDownStr *self = (CMenuDropDownStr*)pSelf;
		CMenuServerBrowser *parent = (CMenuServerBrowser*)self->Parent();

		parent->gameListModel.SetFilterMap( self->GetItem( ) );
		parent->ClearList();
		parent->RefreshList();
	});

	filterProtocol.AddItem( "Xash3D 49", '1' );
	filterProtocol.AddItem( "GoldSrc 48", '2' );
	filterProtocol.AddItem( L( "All" ), 0 );
	filterProtocol.SelectLast( false );
	filterProtocol.bDropUp = true;
	filterProtocol.eTextAlignment = QM_LEFT;
	filterProtocol.iFgTextColor = uiInputFgColor - 0x00151515;
	filterProtocol.SetCharSize( QM_SMALLFONT );
	filterProtocol.SetSize( 130, 30 );
	SET_EVENT_MULTI( filterProtocol.onChanged,
	{
		CMenuDropDownInt *self = (CMenuDropDownInt*)pSelf;
		CMenuServerBrowser *parent = (CMenuServerBrowser*)self->Parent();

		parent->gameListModel.filterProtocol = self->GetItem( );
		parent->ClearList();
		parent->RefreshList();
	});

	AddItem( gameList );
	AddItem( tabSwitch );

	// A7: empty-state caption of the Friends tab - drawn after the table
	// so it stays on top of the empty frame, hidden everywhere else
	friendsHint.iFlags = QMF_INACTIVE;
	friendsHint.colorBase = uiColorHelp;
	friendsHint.szName = "No servers yet - sign in through Friends on the main menu";
	friendsHint.eTextAlignment = QM_CENTER;
	friendsHint.SetRect( 420, 400, 560, 32 );
	friendsHint.Hide();
	AddItem( friendsHint );

	AddItem( filterPing );
	AddItem( filterEmpty );
	AddItem( filterFull );
	AddItem( filterMap );
	AddItem( filterProtocol );
}

/*
=================
CMenuServerBrowser::VidInit
=================
*/
void CMenuServerBrowser::_VidInit()
{
	refreshTime = uiStatic.realTime + 500; // delay before update 0.5 sec
	refreshTime2 = uiStatic.realTime + 500;

	if( m_bLanOnly )
	{
		gameList.SetCoord( 360, 230 );
	}
	else
	{
		gameList.SetCoord( 360, 230 + tabSwitch.size.h + uiStatic.outlineWidth );
	}

	int x = gameList.pos.x;
	int y = gameList.pos.y + gameList.size.h + UI_OUTLINE_WIDTH;
	filterPing.SetCoord( x, y );
	x += filterPing.size.w + 10;
	filterEmpty.SetCoord( x, y );
	x += filterEmpty.size.w + 10;
	filterFull.SetCoord( x, y );
	x += filterFull.size.w + 10;
	filterMap.SetCoord( x, y );
	x += filterMap.size.w + 10;
	filterProtocol.SetCoord( x, y );

	// force close all menus
	filterPing.MenuClose( );
	filterEmpty.MenuClose( );
	filterFull.MenuClose( );
	filterMap.MenuClose( );
	filterProtocol.MenuClose( );
}

bool CMenuServerBrowser::IsAddressInTab( int tab, const netadr_t &adr ) const
{
	if( tab < 0 || tab >= INTERNET_TAB_COUNT )
		return false;

	FOR_EACH_VEC( m_InternetServers[tab], i )
	{
		if( !EngFuncs::NET_CompareAdr( &m_InternetServers[tab][i].adr, &adr ))
			return true;
	}

	return false;
}

void CMenuServerBrowser::Show()
{
	CMenuFramework::Show();

	if( m_bLanOnly )
	{
		banner.SetPicture( ART_BANNER_LAN );
		favorite->Hide();
		addServer->Hide();
		tabSwitch.Hide();
		filterProtocol.Hide();
		gameListModel.filterProtocol = 0;
	}
	else
	{
		banner.SetPicture( ART_BANNER_INET );
		favorite->Show();
		addServer->Show();
		tabSwitch.Show();
		filterProtocol.Show();
		gameListModel.filterProtocol = filterProtocol.GetItem( );

		favoritesList.RemoveAll();
		historyList.RemoveAll();
		if( EngFuncs::FileExists( "favorite_servers.lst", true ) )
			ParseServerListFromFile( "favorite_servers.lst", favoritesList );
		if( EngFuncs::FileExists( "history_servers.lst", true ) )
			ParseServerListFromFile( "history_servers.lst", historyList );
	}

	RealignButtons();

	// clear out server table
	staticWaitingPassword = false;
	gameListModel.Flush();
	gameList.SetSortingColumn( COLUMN_PING );
	joinGame->SetGrayed( true );
	viewGameInfo->SetGrayed( true );
	favorite->SetGrayed( true );

	// restore cached servers for current tab if they were already loaded
	if( m_bLanOnly )
	{
		for( int i = 0; i < m_LanServers.Count(); ++i )
		{
			gameListModel.AddServerToList( m_LanServers[i].adr, m_LanServers[i].info, m_LanServers[i].favorite );
		}
	}
	else
	{
		int curTab = tabSwitch.GetState();
		if( curTab < 0 || curTab >= INTERNET_TAB_COUNT )
			curTab = 0;  // Safety: default to Direct tab if invalid
		for( int i = 0; i < m_InternetServers[curTab].Count(); ++i )
		{
			gameListModel.AddServerToList( m_InternetServers[curTab][i].adr, m_InternetServers[curTab][i].info, m_InternetServers[curTab][i].favorite );
		}
	}
	m_iLastRefreshTab = -1;

	// visible tab may be Friends (cached from a previous visit)
	UpdateFriendsHint();
}

void CMenuServerBrowser::SaveLists()
{
	// TODO: we can actually cache master server response, so every time
	// player opens up this menu, they instantly get server list at first
	// and then re-sync with master server is just delayed
	// ... but I'm tired with this, so let's do it next time :)

	if( m_bLanOnly )
		return;

	SaveServerListToFile( "favorite_servers.lst", favoritesList );
	SaveServerListToFile( "history_servers.lst", historyList );
}

void CMenuServerBrowser::Hide()
{
	SaveLists();
	CMenuFramework::Hide();
}

void CMenuServerBrowser::AddServerToList( netadr_t adr, const char *info )
{
#ifndef XASH_ALL_SERVERS
	if( stricmp( gMenu.m_gameinfo.gamefolder, Info_ValueForKey( info, "gamedir" )) != 0 )
		return;
#endif

	if( !WasInit() )
		return;

	if( !IsVisible() )
		return;

	const char *s = EngFuncs::NET_AdrToString( adr );
	bool is_favorite = false;

	FOR_EACH_VEC( favoritesList, i )
	{
		if( !strcmp( favoritesList[i].sadr, s ))
		{
			is_favorite = favoritesList[i].favorited;
			break;
		}
	}

	// Handle LAN servers separately
	if( m_bLanOnly )
	{
		CUtlVector<server_t> &buf = m_LanServers;
		int pos = -1;
		for( int i = 0; i < buf.Count(); ++i )
		{
			if( !EngFuncs::NET_CompareAdr( &buf[i].adr, &adr ))
			{
				pos = i;
				break;
			}
		}

		if( pos >= 0 )
		{
			Q_strncpy( buf[pos].info, info, sizeof( buf[pos].info ));
			buf[pos].UpdateData();
			buf[pos].SetPing( EngFuncs::DoubleTime() - gameListModel.serversRefreshTime );
			buf[pos].pending_info = false;
		}
		else
		{
			server_t serv( adr, info, false );
			serv.UpdateData();
			serv.SetPing( EngFuncs::DoubleTime() - gameListModel.serversRefreshTime );
			buf.AddToTail( serv );
		}

		// Always update visible model for LAN
		gameListModel.AddServerToList( adr, info, false );
		joinGame->SetGrayed( false );
		viewGameInfo->SetGrayed( false );
		MaybeEnableFavoriteButton();
		OnChangeSelectedServer();
		return;
	}

	// Internet games logic
	int curTabVisible = tabSwitch.GetState();
	if( curTabVisible < 0 || curTabVisible >= INTERNET_TAB_COUNT )
		return;  // Safety: invalid tab index

	int targetTab = -1;

	if( IsAddressInTab( curTabVisible, adr ))
		targetTab = curTabVisible;
	else if( m_iLastRefreshTab != -1 && IsAddressInTab( m_iLastRefreshTab, adr ))
		targetTab = m_iLastRefreshTab;
	else
	{
		switch( curTabVisible )
		{
		case 1: // Favorites
		case 2: // History
			targetTab = curTabVisible;
			break;
		default:
			if( m_iLastRefreshTab != -1 )
				targetTab = m_iLastRefreshTab;
			else
				targetTab = curTabVisible;
			break;
		}
	}

	if( targetTab == 1 ) // Favorites (tab order: Internet/Favorites/History/Spectate/LAN/Friends)
	{
		bool found = false;
		FOR_EACH_VEC( favoritesList, j )
		{
			if( !strcmp( favoritesList[j].sadr, s ))
			{
				found = true;
				break;
			}
		}
		if( !found )
			return;
	}
	else if( targetTab == 2 ) // History
	{
		bool found = false;
		FOR_EACH_VEC( historyList, j )
		{
			if( !strcmp( historyList[j].sadr, s ))
			{
				found = true;
				break;
			}
		}
		if( !found )
			return;
	}

	// Add or update entry in per-tab buffer
	CUtlVector<server_t> &buf = m_InternetServers[targetTab];
	int pos = -1;
	for( int i = 0; i < buf.Count(); ++i )
	{
		if( !EngFuncs::NET_CompareAdr( &buf[i].adr, &adr ))
		{
			pos = i;
			break;
		}
	}

	if( pos >= 0 )
	{
		Q_strncpy( buf[pos].info, info, sizeof( buf[pos].info ));
		buf[pos].UpdateData();
		buf[pos].SetPing( EngFuncs::DoubleTime() - gameListModel.serversRefreshTime );
		buf[pos].pending_info = false;
	}
	else
	{
		server_t serv( adr, info, is_favorite );
		serv.UpdateData();
		serv.SetPing( EngFuncs::DoubleTime() - gameListModel.serversRefreshTime );
		buf.AddToTail( serv );
	}

	// If this response belongs to the currently visible tab, update visible model
	if( targetTab == curTabVisible )
	{
		gameListModel.AddServerToList( adr, info, is_favorite );

		joinGame->SetGrayed( false );
		viewGameInfo->SetGrayed( false );
		MaybeEnableFavoriteButton();
		OnChangeSelectedServer();
	}
}

/*
=================
UI_AddServerToList
=================
*/
void UI_AddServerToList( netadr_t adr, const char *info )
{
	if( !menu_internetgames )
		return;

	menu_internetgames->AddServerToList( adr, info );
}

/*
=================
UI_MenuResetPing_f
=================
*/
void UI_MenuResetPing_f( void )
{
	if( menu_internetgames )
		menu_internetgames->ResetPing();
}
ADD_COMMAND( menu_resetping, UI_MenuResetPing_f );
