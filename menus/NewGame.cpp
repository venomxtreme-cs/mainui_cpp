/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// build 90: NEW GAME, GoldSource style. The old page was three difficulty
// picture buttons plus a banner; it is now one floating window titled
// "New Game" (the float frame derives that title from the class name - this
// dialog has NO banner item), with the reference screenshot's tab strip
// Map | Game Options | Bots Options and a right-aligned Start/Cancel footer.
//
//   Map          - the map selector only. The reference's Filter and
//                  Location rows are explicitly out of scope; no extra
//                  controls are put on this page.
//   Game Options - Server name (hostname), Max players (maxplayers) and
//                  Password server (sv_password) with menus/CreateGame.cpp's
//                  exact field wiring (LinkCvar + onCvarGet via
//                  UI_GetScriptCvar + WriteCvar()/UI_SetScriptCvar), plus an
//                  Advanced options entry opening UI_AdvServerOptions_Menu()
//                  (greyed out when UI_AdvServerOptions_IsAvailable() says
//                  there is no settings.scr to edit).
//   Bots         - "Include CPU players (Bots) in this game", "Number of CPU
//                  players" and the difficulty levels.
//
// LAYOUT (build 90): authored in layout coords - ApplyFloatLayout wraps the
// visible items and CENTRES the float frame itself, so what is authored here
// is the proportion: tab strip x300..1272 (nothing authored left of 300, the
// main menu list lives at x36..200 behind this dialog), y168..207 for the
// strip (1.5 bold-font lines - TAB_STRIP_HEIGHT in Configuration.cpp), body
// down to y608 = a 401px page, footer right-aligned ending at x1272
// (Start 1060,640,100,34 | Cancel 1172,640,100,34). Inside the 972px page
// every control obeys rel-x + w <= 960, and rows are pitched >= 50px because
// a field draws its label 39px ABOVE the box (1.5 * UI_MED_CHAR_HEIGHT,
// controls/Field.cpp CalcSizes) and it must clear whatever sits above it.
// Checkbox labels are drawn at x + w * 1.25 (CheckBox::CalcSizes), so a
// label-right row needs x + w*1.25 + label width <= 960 too.
//
// SLOW-OPEN FIX (user: "New Game is too late to show"). Findings on the open
// path and what each costs now:
//   (a) fade - CMenuBaseWindow::Show() ends in EnableTransition(ANIM_OPENING)
//       and the window only reaches full alpha after TTT_PERIOD (200 ms,
//       BaseWindow::DrawAnimation), so even an already-built dialog looked
//       late. Show() now calls DisableTransition(): it is drawn on the click
//       frame.
//   (b) banner - there is no banner item anymore (the float title bar carries
//       "New Game" and CMenuFramework::ApplyFloatLayout picks it from
//       szName when banner.Parent() != this), so no title art/text work runs
//       inside _Init.
//   (c) map list build - the map selector naturally wants
//       EngFuncs::CreateMapsList(), and the naive port would do what
//       CreateGame does on every open: Cmd_CheckMapsList(1) -> an
//       FS_Search("maps/*.bsp") over the whole game dir plus a maps.lst
//       rewrite. CMenuNewGameMapModel::Update() is the cached/lazy copy of
//       CreateGame's model: it returns immediately while the list is fresh,
//       reuses the maps.lst already on disk (Cmd_CheckMapsList(0) bails out
//       before the search) and only rescans when uiStatic.needMapListUpdate
//       is set - the engine/mainui raise it after every game
//       (menus/ConnectionProgress.cpp). Offline it is one cheap file parse per
//       session, then Update() is a no-op.
//
// build 90: bot cvars. `bot_quota` and `bot_difficulty` are real cvars, but
// they belong to the game DLL (ReGameDLL dlls/bot/cs_bot_init.cpp,
// Bot_RegisterCVars) and do NOT exist while the menu runs - binding them here
// would read "" and a write would print fwgs cvar.c "variable 'bot_quota'
// not found" and be dropped. So nothing on the Bots page is LinkCvar'd: the
// number lives in settings.scr through UI_SetScriptCvar and is pushed to the
// console from StartGame(), AFTER the server is up (see the comment there).
//
// build 135 (item 8): `bot_add_allowed` joins that push - it is the game
// DLL's master gate for CPU players (console bot_add/bot_add_t/bot_add_ct
// are refused and quota maintenance clamps to 0 while it reads 0), so the
// "Include CPU players" tick now holds even against stale bot_quota values
// in configs and against script calls, which bot_quota 0 alone did not.

#include "CheckBox.h"
#include "EventSystem.h"
#include "Framework.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"
#include "MenuStrings.h"
#include "Action.h"     // build 90: page section headers
#include "Field.h"      // build 90: Server name / Max players / Password
#include "Table.h"      // build 90: map list
#include "TabView.h"    // build 90: Map | Game Options | Bots Options strip
#include "BaseMenu.h"   // build 90: UI_AdvServerOptions_*, UI_*ScriptCvar

// build 90: section header - a white inactive label that groups a page's
// controls, exactly like menus/Configuration.cpp's SectionHeader
static void SectionHeader( CMenuAction &a, const char *text, int x, int y, int w )
{
	a.iFlags = QMF_INACTIVE;
	a.colorBase = uiColorText;
	a.szName = text;
	a.SetRect( x, y, w, 24 );
}

// build 90: same helper as CreateGame.cpp (static, no cross-TU aliasing) -
// the Max players field clamps itself with it
static const char *MAX_CLIENTS_STRING()
{
	static CUtlString maxClientsStr;
	maxClientsStr.Format( "%d", MAX_CLIENTS );
	return maxClientsStr.String();
}

class CMenuNewGame;

// build 90: map list model - copied from menus/CreateGame.cpp
// (CMenuMapListModel) and adapted: no "Random map" row here (New Game has to
// start the level picked on the Map tab; the random entry is CreateGame's
// multiplayer flow) and Update() carries the slow-open cache, see below.
struct map_t
{
	char name[CS_SIZE];
	char desc[CS_SIZE];
};

class CMenuNewGameMapModel : public CMenuBaseModel, public CUtlVector<map_t>
{
public:
	CMenuNewGameMapModel( CMenuNewGame *parent ) : m_bBuilt( false ), parent( parent ) { }

	void Update() override;
	int GetColumns() const override { return 2; }
	int GetRows() const override { return Count(); }
	const char *GetCellText( int line, int column ) override
	{
		switch( column )
		{
		case 0: return Element( line ).name;
		case 1: return Element( line ).desc;
		}

		return NULL;
	}

	// build 90: true after this session parsed a list - a no-refresh open
	// never touches the filesystem again
	bool m_bBuilt;

	CMenuNewGame *parent;
};

/*
=====================================================================
Map tab: map selector only (no Filter, no Location)
=====================================================================
*/
class CMenuNewGameMapPage : public CMenuItemsHolder
{
public:
	CMenuNewGameMapPage( CMenuNewGame *parent ) : mapsModel( parent ),
		m_bScanPending( false ), m_bScanWasForced( false ) { }

	// build 90: the level Start will launch - NULL when nothing selectable
	// is on the tab (empty list / invalid entry)
	const char *SelectedMap( void );

	// A7 build 139 (round 139 items 1+2): also driven from
	// CMenuNewGame::Draw - TabView only draws the SELECTED page, so an
	// armed scan on a dialog opened on another tab never ran and the model
	// stayed built-but-empty ("maps hidden")
	void RunPendingScan( void );

private:
	void _Init() override;
	void Reload() override;
	void Draw() override;
	bool ArmDeferredScan( void );

	CMenuAction secMap;

	CMenuTable           mapsList;
	CMenuNewGameMapModel mapsModel;

	// A7 build 138 (round 137 item 3, user: "fix new game also start show
	// menu too late"): the session-first maps/*.bsp walk used to run in
	// Init (SetModel -> model Update), i.e. INSIDE the tap that opens the
	// dialog - the window could not be presented until the whole FUSE walk
	// had finished. Armed, _Init suppresses that first walk and the real
	// scan runs one frame later out of Draw(), while the New Game window
	// is already on screen (SelectedMap() runs it inline if a tap beats
	// that first frame).
	bool m_bScanPending;
	bool m_bScanWasForced;
};

/*
=====================================================================
Game Options tab: hostname / maxplayers / sv_password + Adv. options
=====================================================================
*/
class CMenuNewGameOptionsPage : public CMenuItemsHolder
{
public:
	CMenuNewGameOptionsPage() { }

	// build 90: CreateGame.cpp::SaveCvars equivalent - the three values go
	// to the engine AND to settings.scr, in that order
	void SaveCvars( void );

	// build 90: what StartGame feeds `maxplayers` with (clamped like the
	// CreateGame field keeps it, between 1 and MAX_CLIENTS)
	int MaxPlayersValue( void );

private:
	void _Init() override;

	CMenuAction secOpts;

	CMenuField hostName;
	CMenuField maxClients;
	CMenuField password;

	CMenuPicButton advOptions;
};

/*
=====================================================================
Bots tab: include-CPU-players tick, player count, difficulty levels
=====================================================================
*/
class CMenuNewGameBotsPage : public CMenuItemsHolder
{
public:
	CMenuNewGameBotsPage() : m_iDifficulty( 0 ) { }

	// build 90: 0..3 = Easy/Normal/Hard/Expert; CMenuNewGame::StartGame
	// turns it into `skill` (easy=1 normal=2 hard=3, expert=4 - ReGameDLL
	// reads bot_difficulty 0..3 and the engine clamps skill to 3 when the
	// server starts, fwgs sv_init.c bound()) and into bot_difficulty
	int  Difficulty( void ) const { return m_iDifficulty; }

	// bot_quota semantics: 0 while the box is unticked, else the count
	int  BotQuota( void );

	// A7 build 135 (item 8, user: "include CPU players off must really
	// keep bots out"): the raw "Include CPU players" tick - StartGame needs
	// it independently of the quota (the gate is bot_add_allowed, which
	// blocks console bot_add* too, not only the quota count)
	bool IncludeBots( void ) const { return includeBots.bChecked; }

	// build 90: settings.scr ONLY - never an engine cvar (see the file
	// header: bot_quota/bot_difficulty do not exist in the menu)
	void SaveSettings( void );

private:
	void _Init() override;
	void Reload() override;

	void SetDifficulty( int idx );
	void SyncIncludeTicked( void );

	static void DifficultyCb( CMenuBaseItem *pSelf, void *pExtra );
	static void IncludeBotsCb( CMenuBaseItem *pSelf, void *pExtra );

	// build 90: no "CPU players (bots)" section header - the reference has a
	// plain divider above the tick row, and the 401px page cannot hold a
	// header row AND a >= 50px-pitched four-level group (see _Init)
	CMenuAction secDifficulty;

	CMenuCheckBox includeBots;
	CMenuField    botCount;

	// build 90: mainui has no radio control (grep 'radio' over the project
	// only hits the HUD/menu key defs), so the four levels are an exclusive
	// group of checkboxes - the reference's round radios with the shared
	// cb_* checkbox art; SetDifficulty() keeps exactly one ticked
	CMenuCheckBox difficulty[4];

	int m_iDifficulty;
};

/*
=====================================================================
The New Game window itself
=====================================================================
*/
class CMenuNewGame : public CMenuFramework
{
public:
	CMenuNewGame() : CMenuFramework( "CMenuNewGame" ), startBtn( NULL ), mapPage( this ) { }

	void StartGame( uintptr_t skill );
	void Show() override
	{
		if( gMenu.m_gameinfo.flags & GFL_NOSKILLS )
		{
			// build 90: unchanged fast path - services without a skill scale
			// start the game right away and never see this dialog
			StartGame( 1.0f );
			return;
		}

		CMenuFramework::Show();

		// build 90 (SLOW-OPEN FIX): kill the ANIM_OPENING fade that took
		// TTT_PERIOD (200 ms) before the window reached full alpha - New Game
		// must be fully visible on the frame it was clicked
		DisableTransition();
	}

	// build 90: public - the map model greys it when no level is available
	// (the same owner->back-reference CreateGame keeps for its OK button)
	CMenuPicButton *startBtn;

	// A7 build 139 (item 2, user: "in new game menu, map dosen't show like
	// hiding"): the deferred scan was only driven by the Map page's own
	// Draw(), but CMenuTabView::Draw() draws ONLY the selected page - open
	// the dialog while another tab is current (the cursor sticks from the
	// last visit) and the armed model stayed built-but-empty forever
	// (m_bBuilt=true + needMapListUpdate=false from the arm = every later
	// Update() early-outs) - an empty list that never self-corrects. The
	// window itself now drains the pending scan on its first frame, no
	// matter which tab is showing.
	void Draw() override
	{
		CMenuFramework::Draw();
		mapPage.RunPendingScan();
	}

private:
	void _Init() override;

	static void StartGameCb( CMenuBaseItem *pSelf, void *pExtra );
	static void ShowDialogCb( CMenuBaseItem *pSelf, void *pExtra );

	CMenuYesNoMessageBox msgBox;
	CEventCallback startCallback;    // build 90: carried through msgBox (old flow)

	CMenuTabView tabs;
	CMenuNewGameMapPage     mapPage;
	CMenuNewGameOptionsPage gameOptions;
	CMenuNewGameBotsPage    botsPage;

	// build 90: the old difficulty page's demo-chapter tick is not part of the
	// reference UI, so it is never added; StartGame keeps its branch (the box
	// stays unchecked -> plain `newgame`, exactly the previous behaviour)
	CMenuCheckBox startDemoChapter;
};

/*
================
CMenuNewGameMapModel::Update

build 90: CreateGame's model, slow-open fixed (opens inside one session
serve from the m_bBuilt cache).

build 135 (item 7, user: "New Game doesn't find all maps"): the session-FIRST
build no longer reuses a maps.lst it finds on disk. That file lives in
/sdcard across installs and release updates while uiStatic.needMapListUpdate
only rises after an in-game BSP download - so the list used to freeze at
whatever was scanned once (or written by an older release) and every map
added afterwards was missing from this dialog for good. The first build of a
session now always rescans for real (CreateMapsList(1)), and every later open
in that session still hits the cache above, the slow-open fix intact.
================
*/
void CMenuNewGameMapModel::Update( void )
{
	char *afile;

	// build 90: cached - every open after the first one in this session
	// (and after an in-game download) does no work at all
	if( m_bBuilt && !uiStatic.needMapListUpdate )
		return;

	// session-first build (or a download raised the flag): this is the ONLY
	// place that rescans - a miss here means the game has no levels at all,
	// never "reuse whatever file is on disk"
	RemoveAll();

	if( !EngFuncs::CreateMapsList( 1 ) || ( afile = (char *)EngFuncs::COM_LoadFile( "maps.lst", NULL ) ) == NULL )
	{
		Con_Printf( "Cmd_GetMapsList: can't open maps.lst\n" );
		if( parent->startBtn )
			parent->startBtn->SetGrayed( true );
		return;
	}

	if( !m_bBuilt )
		Con_Printf( "[A7] build135: New Game map list rescanned (session-first scan, stale maps.lst dropped)\n" );

	// build 90: no "Random map" row here - New Game must start the level that
	// is selected on this tab (see the model comment)
	char *pfile = afile;
	char token[1024];

	while(( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))) != NULL )
	{
		map_t map;

		Q_strncpy( map.name, token, sizeof( map.name ));
		if(( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))) == NULL )
		{
			Q_strncpy( map.desc, map.name, sizeof( map.desc ));
			AddToTail( map );
			break; // unexpected end of file
		}
		Q_strncpy( map.desc, token, sizeof( map.desc ));

		// A7 build 135 (item 7): no prefix hiding any more - "New Game
		// doesn't find all maps" - every level maps.lst reports lands on
		// the list (Start still refuses an invalid one through
		// SelectedMap() -> IsMapValid)
		AddToTail( map );
	}

	// build 90: nothing to launch when the list is empty (CreateGame greys
	// its OK button the same way for Count() <= 1 - its 1 is the random row)
	if( parent->startBtn )
		parent->startBtn->SetGrayed( Count() == 0 );

	EngFuncs::COM_FreeFile( afile );

	m_bBuilt = true;
	uiStatic.needMapListUpdate = false;
}

/*
================
CMenuNewGameMapPage
================
*/
const char *CMenuNewGameMapPage::SelectedMap( void )
{
	// A7 build 138 (item 3): a tap can beat the deferred first build
	// (Start pressed while the list is still empty) - run the scan right
	// here rather than refusing a valid Start on an empty model
	RunPendingScan();

	int item = mapsList.GetCurrentIndex();

	if( !mapsModel.IsValidIndex( item ))
		return NULL;

	const char *name = mapsModel[item].name;

	if( !EngFuncs::IsMapValid( name ))
		return NULL; // bad map

	return name;
}

// build 90: same shape as CreateGame::Reload - builds after a game (the
// engine raises uiStatic.needMapListUpdate in ConnectionProgress); between
// those opens Update() is a plain "list is fresh" return.
//
// A7 build 139 (item 1, user: "fix when i click new game it be too late"):
// BaseWindow::Show() runs Reload() on EVERY open, and this used to call
// mapsModel.Update() straight - so after any game or download the tap paid
// the full maps/*.bsp FUSE walk before the window could appear (build 138
// only deferred the _Init path, which runs once per process). Arm the same
// deferred scan instead: Show() returns instantly, Draw() pays the walk one
// frame later while the window is already on screen.
void CMenuNewGameMapPage::Reload( void )
{
	BaseClass::Reload();
	ArmDeferredScan();
}

void CMenuNewGameMapPage::_Init( void )
{
	// build 90: the ONLY control of this tab - the reference's Filter and
	// Location rows are dropped on purpose
	SectionHeader( secMap, L( "GameUI_Map" ), 16, 4, 940 );

	// build 90: header at y4..28, table row starts 54 (50px pitch) and ends
	// at 382 inside the 401px page (19px of bottom margin); width 940 keeps
	// rel-x + w = 956 <= 960 so the column headers stay inside the frame
	mapsList.SetCharSize( QM_SMALLFONT );
	mapsList.SetupColumn( 0, L( "GameUI_Map" ), 0.5f );
	mapsList.SetupColumn( 1, L( "Title" ), 0.5f );

	// A7 build 138 (round 137 item 3, "new game also start show menu too
	// late"): SetModel() calls Update(), and Update()'s session-first rule
	// means that FIRST build is a full maps/*.bsp walk through Android
	// FUSE - it used to stall this very Init(), so the New Game window
	// could not appear before it ended. Arm the model's own cache flags
	// just for this call (both for a session-first build and for a raised
	// uiStatic.needMapListUpdate after a download) so SetModel returns
	// instantly; Draw() runs the real scan one frame later, when the
	// window is already on screen, restoring both flags first so the
	// "always rescan first, never trust a stale maps.lst" contract of
	// build 135 stays intact.
	//
	// A7 build 139 (items 1+2): the same arming now also runs from
	// Reload() (see ArmDeferredScan) and the pending scan is driven by
	// CMenuNewGame::Draw(), not only by this page's own Draw() - TabView
	// skips non-selected pages, which left the armed model built-but-empty
	// forever when the dialog opened on another tab.
	ArmDeferredScan();
	mapsList.SetModel( &mapsModel );   // may Update() here, footer exists already
	mapsList.SetRect( 16, 54, 940, 328 );

	AddItem( secMap );
	AddItem( mapsList );
}

// A7 build 139 (items 1+2): one arming rule for every open path (_Init's
// SetModel and Show()'s Reload). Returns true when a real scan is now
// pending; the flags are consumed/restored by RunPendingScan().
bool CMenuNewGameMapPage::ArmDeferredScan( void )
{
	// Show() runs Init() (_Init arms) and then Reload() in the SAME call,
	// and a dialog closed before its first Draw re-opens with the state
	// still armed - recomputing here would see the armed cache flags and
	// clear the pending scan (and m_bScanWasForced) that must survive
	if( m_bScanPending )
		return true;

	m_bScanPending = !( mapsModel.m_bBuilt && !uiStatic.needMapListUpdate );
	m_bScanWasForced = m_bScanPending && uiStatic.needMapListUpdate;
	if( m_bScanPending )
	{
		mapsModel.m_bBuilt = true;             // arms Update()'s early-out
		uiStatic.needMapListUpdate = false;    // consumed here, restored in RunPendingScan()
		Con_Printf( "[A7] build139: New Game map scan deferred (open path, %s)\n",
			m_bScanWasForced ? "post-download rescan" : "session-first scan" );
	}
	return m_bScanPending;
}

// A7 build 138 (item 3): the deferred session-first scan. Called after the
// window has been presented (first Draw) or inline if a tap races it.
void CMenuNewGameMapPage::RunPendingScan( void )
{
	if( !m_bScanPending )
		return;

	m_bScanPending = false;

	const int t0 = uiStatic.realTime;

	mapsModel.m_bBuilt = false;                // the cache arm from _Init
	if( m_bScanWasForced )
		uiStatic.needMapListUpdate = true;     // restore the download flag
	m_bScanWasForced = false;

	mapsModel.Update();

	Con_Printf( "[A7] build138: New Game window was presented first, deferred map scan ran (+%d ms, %d maps)\n",
		uiStatic.realTime - t0, mapsModel.Count());
}

void CMenuNewGameMapPage::Draw( void )
{
	BaseClass::Draw();

	// A7 build 138 (item 3): frame 1 shows the dialog shell without
	// blocking the tap; the walked rows land on the flip after this - the
	// scan's cost is paid VISIBLE (a brief fill after the window is up)
	// instead of invisible (nothing at all while the FUSE walk runs)
	RunPendingScan();
}

/*
================
CMenuNewGameOptionsPage

The three fields are CreateGame.cpp hostName/maxClients/password verbatim:
same LinkCvar, same onCvarGet via UI_GetScriptCvar (the buffer follows
settings.scr, the menu may run without the engine cvar holding the saved
value), same iMaxLength / bNumbersOnly / hidden password, same SaveCvars
_WriteCvar + UI_SetScriptCvar pair.
================
*/
int CMenuNewGameOptionsPage::MaxPlayersValue( void )
{
	int players = atoi( maxClients.GetBuffer() );

	if( players < 1 )
		return 1; // the value the dialog was written with before

	return bound( 1, players, MAX_CLIENTS );
}

void CMenuNewGameOptionsPage::SaveCvars( void )
{
	hostName.WriteCvar();
	maxClients.WriteCvar();
	password.WriteCvar();

	// build 90: update script variables so they are saved to settings.scr
	UI_SetScriptCvar( "hostname", hostName.GetBuffer() );
	UI_SetScriptCvar( "maxplayers", maxClients.GetBuffer() );
	UI_SetScriptCvar( "sv_password", password.GetBuffer() );
}

void CMenuNewGameOptionsPage::_Init( void )
{
	SectionHeader( secOpts, "Server settings", 16, 4, 940 );

	// build 90: rows pitched 84px - each label (drawn 39px above the box,
	// y-1.5 x char height) clears the control/result above it by 9..13px,
	// nothing touches; fields are 240 wide as asked (205..240), the whole
	// group runs 4..368 inside the 401px page
	hostName.szName = L( "GameUI_ServerName" );
	hostName.szStatusText = "Name shown in the server browser for this game";
	hostName.iMaxLength = 28;
	SET_EVENT_MULTI( hostName.onCvarGet,
	{
		CMenuField *self = (CMenuField *)pSelf;
		self->SetBuffer( UI_GetScriptCvar( self->CvarName() ) );
	});
	hostName.LinkCvar( "hostname" );
	hostName.SetRect( 16, 76, 240, 32 );

	maxClients.iMaxLength = 3;
	maxClients.bNumbersOnly = true;
	maxClients.szName = L( "GameUI_MaxPlayers" );
	maxClients.szStatusText = "Maximum number of players in this game";
	SET_EVENT_MULTI( maxClients.onChanged,
	{
		CMenuField *self = (CMenuField *)pSelf;
		const char *buf = self->GetBuffer();
		if( buf[0] == 0 ) return;

		int players = atoi( buf );
		if( players > MAX_CLIENTS )
			self->SetBuffer( MAX_CLIENTS_STRING() );
	});
	SET_EVENT_MULTI( maxClients.onCvarGet,
	{
		CMenuField *self = (CMenuField *)pSelf;
		const char *val = UI_GetScriptCvar( self->CvarName() );
		self->SetBuffer( val );

		const char *buf = self->GetBuffer();

		int players = atoi( buf );
		if( players <= 1 )
			self->SetBuffer( "16" );
		else if( players > MAX_CLIENTS )
			self->SetBuffer( MAX_CLIENTS_STRING() );
	});
	maxClients.LinkCvar( "maxplayers" );
	maxClients.SetRect( 16, 164, 240, 32 );

	// build 90: the reference's "Password server" row (GameOptions tab).
	// Not L("GameUI_Password") - that key renders as plain "Password" and
	// the spec's label differs from it; L() passes a literal through, the
	// same way the non-key labels ("Server settings", "Difficulty") are used
	// in this file. Everything else is CreateGame's wiring verbatim.
	password.szName = L( "Password server" );
	password.szStatusText = "Password asked when joining this game";
	password.iMaxLength = 16;
	password.eTextAlignment = QM_CENTER;
	password.bHideInput = true;
	SET_EVENT_MULTI( password.onCvarGet,
	{
		CMenuField *self = (CMenuField *)pSelf;
		self->SetBuffer( UI_GetScriptCvar( self->CvarName() ) );
	});
	password.LinkCvar( "sv_password" );
	password.SetRect( 16, 248, 240, 32 );

	// build 90: the Advanced options entry of the reference dialog - the same
	// no-atlas chrome button Configuration keeps in its pages (no picture is
	// set, so CMenuPicButton::Draw takes the float-dialog chrome path)
	advOptions.SetNameAndStatus( L( "Advanced options" ), L( "Edit the rest of the server configuration (settings.scr)" ));
	advOptions.SetRect( 16, 332, 320, 32 );
	advOptions.onReleased = UI_AdvServerOptions_Menu;
	advOptions.SetGrayed( !UI_AdvServerOptions_IsAvailable() );

	AddItem( secOpts );
	AddItem( hostName );
	AddItem( maxClients );
	AddItem( password );
	AddItem( advOptions );
}

/*
================
CMenuNewGameBotsPage
================
*/
int CMenuNewGameBotsPage::BotQuota( void )
{
	if( !includeBots.bChecked )
		return 0; // bot_quota 0 = no CPU players (engine/gamedll semantics)

	int count = atoi( botCount.GetBuffer() );

	return bound( 0, count, MAX_CLIENTS );
}

void CMenuNewGameBotsPage::SaveSettings( void )
{
	// build 90: settings.scr only - bot_quota is a game-DLL cvar and does not
	// exist right now, so pfnCvarSetValue would be dropped ("variable
	// 'bot_quota' not found") and pop a console warning. StartGame() does the
	// real console assignment once the server has booted.
	if( !WasInit() )
		return;

	char buf[16];
	snprintf( buf, sizeof( buf ), "%d", BotQuota() );
	UI_SetScriptCvar( "bot_quota", buf );
}

void CMenuNewGameBotsPage::SetDifficulty( int idx )
{
	idx = bound( 0, idx, 3 );
	m_iDifficulty = idx;

	// exclusive group: exactly one level ticked (no event recursion - bChecked
	// is written directly and onChanged only fires on user input)
	for( int i = 0; i < 4; i++ )
		difficulty[i].bChecked = ( i == idx );
}

void CMenuNewGameBotsPage::DifficultyCb( CMenuBaseItem *pSelf, void *pExtra )
{
	CMenuNewGameBotsPage *page = (CMenuNewGameBotsPage *)pSelf->Parent();

	page->SetDifficulty( (int)(uintptr_t)pExtra );
}

void CMenuNewGameBotsPage::SyncIncludeTicked( void )
{
	// the count only takes effect when the CPU players are included (it is
	// greyed, not hidden - the value the player typed stays visible)
	botCount.SetGrayed( !includeBots.bChecked );
}

void CMenuNewGameBotsPage::IncludeBotsCb( CMenuBaseItem *pSelf, void *pExtra )
{
	CMenuNewGameBotsPage *page = (CMenuNewGameBotsPage *)pSelf->Parent();

	page->SyncIncludeTicked();
}

void CMenuNewGameBotsPage::Reload( void )
{
	BaseClass::Reload();   // linked items re-read their cvars (none here)

	// bot_quota from settings.scr, or from the engine when a game DLL is
	// loaded and has it, or the reference's default of 5 with the box off:
	// an empty/0 value means "no bots were asked for yet" (the engine's own
	// default is 0), so keep the box unticked and the count at 5
	const char *quotaStr = UI_GetScriptCvar( "bot_quota" );
	int quota = quotaStr ? atoi( quotaStr ) : 0;

	if( quota > 0 )
	{
		includeBots.bChecked = true;
		botCount.SetBuffer( quotaStr );
	}
	else
	{
		includeBots.bChecked = false;
		botCount.SetBuffer( "5" );
	}

	SyncIncludeTicked();

	// build 90: `skill` is an engine cvar (fwgs sv_main.c CVAR_DEFINE_AUTO
	// skill "1") so the radios can simply follow it: easy=1 normal=2 hard=3,
	// expert=4 (the engine clamps it to 3 when the server starts)
	int skillLevel = (int)( EngFuncs::GetCvarFloat( "skill" ) + 0.5f );
	SetDifficulty( bound( 1, skillLevel, 4 ) - 1 );
}

void CMenuNewGameBotsPage::_Init( void )
{
	// build 90: re-spaced for the 972px/401px page. No "CPU players (bots)"
	// section header - the reference's Map page only has a divider above the
	// tick row, and a header row here would push the four-level group past
	// the page bottom (a header + 4 x 50px + the count row = 440+ of the 401).
	// Every consecutive row keeps the >= 50px pitch:
	//   20..52   tick row (label drawn to the RIGHT at x + w * 1.25 = 56)
	//   104..136 count field, its label at 65..104 clears the tick by 13
	//   164..188 "Difficulty" group header (60px under the count row top)
	//   216..240, 266..296, 316..346, 366..396 - the four levels, 50px pitch,
	//            last one 5px above the 401px page bottom
	SectionHeader( secDifficulty, "Difficulty", 16, 164, 400 );

	// build 90: label-right row (no above label) - 20..52, right under the
	// tab strip; the text runs from x56 and ends well inside x960
	includeBots.SetNameAndStatus( L( "Include CPU players (Bots) in this game" ), L( "Fill the free slots of this game with CPU players (bot_quota)" ));
	includeBots.onChanged = IncludeBotsCb;
	includeBots.SetRect( 16, 20, 32, 32 );

	// build 90: label at y-39 = 65 clears the tick box at 52 by 13px; box 300
	// wide so the 21-character label above it is not clipped (fields draw
	// their name inside box width), rel-x + w = 316 <= 960
	botCount.szName = L( "Number of CPU players" );
	botCount.szStatusText = "How many CPU players to add (bot_quota)";
	botCount.iMaxLength = 3;
	botCount.bNumbersOnly = true;
	SET_EVENT_MULTI( botCount.onChanged,
	{
		CMenuField *self = (CMenuField *)pSelf;
		const char *buf = self->GetBuffer();
		if( buf[0] == 0 ) return;

		int players = atoi( buf );
		if( players > MAX_CLIENTS )
			self->SetBuffer( MAX_CLIENTS_STRING() );
	});
	botCount.SetRect( 16, 104, 300, 32 );

	static const char *skillNames[4] = { "GameUI_Easy", "GameUI_Medium", "GameUI_Hard", "Expert" };

	for( int i = 0; i < 4; i++ )
	{
		// build 90: the reference shows Easy/Normal/Hard/Expert; the first
		// three keys are the ones the old difficulty page already used, and
		// Expert is what ReGameDLL's fourth bot level is called
		// (game_shared/bot/bot_constants.h BOT_EXPERT, matched from
		// bot_difficulty >= 2.9 in CCSBotManager::GetDifficultyLevel).
		// 50px pitch, label drawn to the right of the box like the tick row
		difficulty[i].SetNameAndStatus( L( skillNames[i] ), L( "Difficulty of the level and of the CPU players (skill cvar)" ) );
		difficulty[i].onChanged = DifficultyCb;
		difficulty[i].onChanged.pExtra = (void *)(uintptr_t)i;
		difficulty[i].SetRect( 16, 216 + i * 50, 32, 32 );
	}

	AddItem( includeBots );
	AddItem( botCount );
	AddItem( secDifficulty );
	for( int i = 0; i < 4; i++ )
		AddItem( difficulty[i] );
}

/*
================
CMenuNewGame::StartGame
================
*/
void CMenuNewGame::StartGame( uintptr_t skill )
{
	if( EngFuncs::GetCvarFloat( "host_serverstate" ) && EngFuncs::GetCvarFloat( "maxplayers" ) > 1 )
		EngFuncs::HostEndGame( "end of the game" );

	// build 90: the edited Game Options/Bots values reach the engine and
	// settings.scr BEFORE the game rules below are written, per the Create
	// Server order. Guarded because the GFL_NOSKILLS fast path calls
	// StartGame() without ever building this dialog - its buffers would be
	// empty and would wipe hostname/maxplayers/sv_password in settings.scr
	if( WasInit() )
	{
		gameOptions.SaveCvars();
		botsPage.SaveSettings();
		UI_SaveScriptConfig();      // push the settings.scr edits to disk
	}

	EngFuncs::CvarSetValue( "skill", skill );
	EngFuncs::CvarSetValue( "deathmatch", 0.0f );
	EngFuncs::CvarSetValue( "teamplay", 0.0f );
	// build 90: was "pausal" - a typo that does not exist anywhere in the
	// tree (fwgs sv_main.c registers CVAR_DEFINE( sv_pausable, "pausable",
	// "1", ...) and ReGameDLL writes "pausable"), so pfnCvar_Set would drop
	// it and singleplayer pause would stay off. Original NewGame.cpp wrote
	// "pausable", restored.
	EngFuncs::CvarSetValue( "pausable", 1.0f ); // singleplayer is always allowing pause
	// build 90: was a hard-coded 1.0f. The Max players value the player
	// edited in Game Options goes in instead (the field shows at least 2 for
	// a listen server, CreateGame's onCvarGet clamp), falling back to 1 when
	// the dialog was never built - and a fixed 1 would defeat the Bots tab,
	// since ReGameDLL clamps bot_quota to gpGlobals->maxClients
	EngFuncs::CvarSetValue( "maxplayers", (float)( WasInit() ? gameOptions.MaxPlayersValue() : 1 ));
	EngFuncs::CvarSetValue( "coop", 0.0f );

	EngFuncs::PlayBackgroundTrack( NULL, NULL );

	if( startDemoChapter.bChecked )
		EngFuncs::ClientCmdF( false, "newgame \"%s\"\n", gMenu.m_gameinfo.demomap );
	else
	{
		// build 90: the Map tab decides the level; with nothing usable
		// selected keep the original plain command (engine startmap)
		const char *mapName = WasInit() ? mapPage.SelectedMap() : NULL;

		if( mapName )
			EngFuncs::ClientCmdF( false, "newgame \"%s\"\n", mapName );
		else EngFuncs::ClientCmd( false, "newgame\n" );
	}

	// build 90: push the Bots values as PLAIN console assignments after the
	// level is queued. `newgame` only sets STATE_LOAD_LEVEL (fwgs
	// host_state.c COM_NewGame), so the three `wait`s - the same hack
	// CreateGame uses around "map" - let the next frames bring the server up
	// and the game DLL register bot_quota/bot_difficulty first, and then the
	// assignment lands on the real cvars. In a build without bots the names
	// are unknown commands and stay silent: no half-made cvar can block the
	// DLL registration (fwgs Cvar_Set never creates a variable).
	//
	// A7 build 135 (item 8, user: "include CPU players off must really keep
	// bots out - bot_add must not force bots while the box is off"): the
	// push now also carries bot_add_allowed, the ReGameDLL-side master gate.
	// While OFF: bot_add_allowed 0 + bot_quota 0 (double lock - the new
	// gate refuses console bot_add/bot_add_t/bot_add_ct and clamps quota
	// maintenance to 0, and the quota itself reads 0 anyway), so nothing
	// from a config or a script can push CPU players back in. While ON:
	// bot_add_allowed 1 + the chosen count/difficulty, restoring the gate
	// after a previous game turned it off. It is pushed AFTER admission is
	// handled on the server's console (Bot_RegisterCVars already ran by the
	// third wait), same handshake as bot_quota.
	if( WasInit() )
	{
		char botCmd[192];

		if( botsPage.IncludeBots() )
		{
			snprintf( botCmd, sizeof( botCmd ),
				"wait; wait; wait; bot_add_allowed 1; bot_quota %d; bot_difficulty %d\n",
				botsPage.BotQuota(), botsPage.Difficulty() );
			Con_Printf( "[A7] build135: bot gate ON (bot_add_allowed 1, bot_quota %d, bot_difficulty %d)\n",
				botsPage.BotQuota(), botsPage.Difficulty() );
		}
		else
		{
			snprintf( botCmd, sizeof( botCmd ),
				"wait; wait; wait; bot_add_allowed 0; bot_quota 0; bot_difficulty %d\n",
				botsPage.Difficulty() );
			Con_Printf( "[A7] build135: bot gate OFF (bot_add_allowed 0, bot_quota locked at 0 - console bot_add* refused too)\n" );
		}

		EngFuncs::ClientCmd( false, botCmd );
	}
}

void CMenuNewGame::StartGameCb( CMenuBaseItem *pSelf, void *pExtra )
{
	CMenuNewGame *ui = (CMenuNewGame *)pSelf->Parent();

	// build 90: the old pic-buttons carried skill in pExtra (1/2/3); the
	// single Start button takes the Bots tab's selection instead - the same
	// easy=1, normal=2, hard=3 mapping CvarSetValue("skill") always got
	ui->StartGame( (uintptr_t)ui->botsPage.Difficulty() + 1 );
}

void CMenuNewGame::ShowDialogCb( CMenuBaseItem *pSelf, void *pExtra )
{
	CMenuNewGame *ui = (CMenuNewGame *)pSelf->Parent();

	ui->msgBox.onPositive = *(CEventCallback *)pExtra;
	ui->msgBox.Show();
}

/*
================
CMenuNewGame::_Init
================
*/
void CMenuNewGame::_Init( void )
{
	// build 90: right-aligned panel - the main menu list stays fully
	// visible left of this dialog (reference screenshot 2)
	m_bFloatRightAlign = true;

	// build 90: footer first - AddTabItem runs each page's _Init immediately,
	// and the map model grays Start while it builds (CreateGame keeps its OK
	// button in front of the map list for exactly this reason)
	startCallback = StartGameCb;

	CMenuPicButton *start = AddButton( L( "Start" ), L( "Start the selected map with these settings" ), PC_OK, startCallback );
	startBtn = start;
	startBtn->onReleasedClActive = ShowDialogCb;
	startBtn->onReleasedClActive.pExtra = &startCallback;

	CMenuPicButton *cancel = AddButton( L( "GameUI_Cancel" ), L( "Go back to the Main menu" ), PC_CANCEL, VoidCb( &CMenuNewGame::Hide ));

	// build 90: right-aligned pair on the dialog's right edge (x1272 = tab
	// strip right, y640 = 32px under the tab body) - the Options footer
	// geometry (948/1060/1172) with the third slot left empty
	startBtn->SetRect( 1060, 640, 100, 34 );
	cancel->SetRect( 1172, 640, 100, 34 );

	// build 90: GoldSource strip - tab row y168..207 (strip = 1.5 bold-font
	// lines, see CMenuTabView::CalcSizes), body down to y608; same setup as
	// menus/Configuration.cpp's tabs member, but authored 300..1272 so the
	// 972px strip (and its 401px pages, control rel-x + w <= 960) never
	// reaches into the main menu list column at x36..200
	tabs.SetRect( 300, 168, 972, 440 );
	tabs.AddTabItem( mapPage,     L( "GameUI_Map" ));
	tabs.AddTabItem( gameOptions, L( "Game Options" ));
	tabs.AddTabItem( botsPage,    L( "Bots Options" ));

	AddItem( tabs );

	// build 90: unchanged confirmation flow - Start goes through this box
	msgBox.SetMessage( L( "StringsList_240" ) );
	msgBox.HighlightChoice( CMenuYesNoMessageBox::HIGHLIGHT_NO );
	msgBox.Link( this );
}

ADD_MENU( menu_newgame, CMenuNewGame, UI_NewGame_Menu );