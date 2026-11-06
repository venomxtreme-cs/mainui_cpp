/*
Table.cpp
Copyright (C) 2010 Uncle Mike
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

#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Table.h"
#include "Utils.h"
#include "Scissor.h"


CMenuTable::CMenuTable() : BaseClass(),
	bFramedHintText( false ),
	bAllowSorting( false ),
	bShowScrollBar( true ),
	szBackground(),
	szUpArrow( UI_UPARROW ), szUpArrowFocus( UI_UPARROWFOCUS ), szUpArrowPressed( UI_UPARROWPRESSED ),
	szDownArrow( UI_DOWNARROW ), szDownArrowFocus( UI_DOWNARROWFOCUS ), szDownArrowPressed( UI_DOWNARROWPRESSED ),
	iTopItem( 0 ),
	iScrollBarSliding( false ),
	iHighlight( -1 ), iCurItem( 0 ), iNumRows( 0 ),
	m_iLastItemMouseChange( 0 ),
	m_iSortingColumn( -1 ),
	m_pModel( NULL )
{
	memset( szHeaderTexts, 0, sizeof(szHeaderTexts) );
	memset( columns, 0, sizeof(columns) );
	eFocusAnimation = QM_HIGHLIGHTIFFOCUS;
	SetCharSize( QM_SMALLFONT );
	bDrawStroke = true;
}

void CMenuTable::VidInit()
{
	BaseClass::VidInit();

	iBackgroundColor.SetDefault( uiColorSecondaryBg ); // A7: cs16 secondary bg
	iHeaderColor.SetDefault( uiColorHelp );
	colorStroke.SetDefault( uiInputFgColor );
	iStrokeFocusedColor.SetDefault( uiInputTextColor );

	iStrokeWidth = uiStatic.outlineWidth;

	iNumRows = ( m_scSize.h - iStrokeWidth * 2 ) / m_scChSize - 1;

	if( !iCurItem )
	{
		if( iCurItem < iTopItem )
			iTopItem = iCurItem;
		if( iCurItem > iTopItem + iNumRows - 1 )
			iTopItem = iCurItem - iNumRows + 1;
		if( iTopItem > m_pModel->GetRows() - iNumRows )
			iTopItem = m_pModel->GetRows() - iNumRows;
		if( iTopItem < 0 )
			iTopItem = 0;
	}

	RefreshGeometryAndSums();
}

// A7 build 62: box/arrow/header geometry is derived from the RENDER
// position - the float layout (and dragging) move m_scPos AFTER VidInit,
// so caching this only in VidInit drew the table panel at the pre-shift
// place (the Create Game map list hung outside the frame; the Touch
// Options presets list collided with the Delete button). Recompute on
// every CalcSizes, which both VidInit and RecalcRenderTree call.
// Idempotent: the arrow size is reset to 24 before each Scale().
void CMenuTable::CalcSizes( void )
{
	BaseClass::CalcSizes();

	RefreshGeometryAndSums();
}

// A7 build 138 (round 137 item 5, user: the scoreboard header labels sit
// ~190px right of their value columns and the header band overflows the
// frame): everything below is derived state, and it used to be split -
// VidInit computed the column sums, CalcSizes the box/header/arrow, and
// each only when somebody ran them. The header line is drawn at the LIVE
// m_scPos, but the frame, scrollbar and every row are drawn from the
// cached boxPos/boxSize, so any float-layout/drag position change (or a
// SetupColumn between VidInit and Draw - the Scoreboard re-sizes its two
// static columns per row-height tier change) left the header on one origin
// and the rows on another. Now VidInit, CalcSizes AND Draw() all funnel
// through here, so header, frame and rows can never disagree.
void CMenuTable::RefreshGeometryAndSums( void )
{
	// column sums from the CURRENT column widths (SetupColumn may have
	// changed them after VidInit; CalcSizes can still run before a model
	// is attached, hence the guard)
	flFixedSumm = 0.0f;
	flDynamicSumm = 0.0f;

	if( m_pModel )
	{
		for( int i = 0; i < m_pModel->GetColumns(); i++ )
		{
			// this isn't valid behaviour, but still enough for tables
			// without set columns width
			if( !columns[i].flWidth )
			{
				SetColumnWidth( i, 1 / m_pModel->GetColumns(), false );
			}

			if( columns[i].fStaticWidth )
				flFixedSumm += columns[i].flWidth;
			else
				flDynamicSumm += columns[i].flWidth;
		}
	}

	flFixedSumm *= uiStatic.scaleX;

	iStrokeWidth = uiStatic.outlineWidth;

	// header height
	headerSize.h = m_scChSize * HEADER_HEIGHT_FRAC;

	// arrow position and sizes (idempotent: the size is reset to 24
	// before every Scale(), so re-running this never doubles the arrow)
	if( bShowScrollBar )
		arrow.w = arrow.h = 24;
	else
		arrow.w = arrow.h = 0;
	arrow = arrow.Scale();

	downArrow.x = upArrow.x = m_scPos.x + m_scSize.w - arrow.w + iStrokeWidth * 1;
	upArrow.y = m_scPos.y - iStrokeWidth;
	downArrow.y = upArrow.y + m_scSize.h - arrow.h + iStrokeWidth * 2;
	if( !bFramedHintText )
	{
		upArrow.y += headerSize.h;
	}

	// header size (position is the table position)
	headerSize.w = m_scSize.w - arrow.w + iStrokeWidth;

	// box is lower than the header
	boxPos.x = m_scPos.x;
	boxPos.y = m_scPos.y + headerSize.h;
	boxSize.w = headerSize.w;
	boxSize.h = m_scSize.h - headerSize.h;
}

bool CMenuTable::MouseMove( int x, int y )
{
	if( !iScrollBarSliding && FBitSet( iFlags, QMF_HASMOUSEFOCUS ))
	{
		float step = Step();

		if( UI_CursorInRect( boxPos, boxSize ))
		{
			static float ac_y = 0;

			ac_y += cursorDY;
			cursorDY = 0;

			// keep the remainder, so slow dragging still scrolls the list
			int rows = ac_y / m_scChSize;

			if( rows )
			{
				iTopItem -= rows;
				ac_y -= rows * m_scChSize;
			}
		}
		else if( UI_CursorInRect( sbarPos, sbarSize ))
		{
			static float ac_y = 0;

			ac_y += cursorDY;
			cursorDY = 0;

			int rows = ac_y / step;

			if( rows )
			{
				iTopItem += rows;
				ac_y -= rows * step;
			}
		}
	}

	if( iScrollBarSliding )
	{
		int dist = uiStatic.cursorY - sbarPos.y - (sbarSize.h / 2);

		if((((dist / 2) > (m_scChSize / 2)) || ((dist / 2) < (m_scChSize / 2))) && iTopItem <= (m_pModel->GetRows() - iNumRows ) && iTopItem >= 0)
		{
			//_Event( QM_CHANGED );

			if((dist / 2) > ( m_scChSize / 2 ) && iTopItem < ( m_pModel->GetRows() - iNumRows ))
			{
				iTopItem++;
			}

			if((dist / 2) < -(m_scChSize / 2) && iTopItem > 0 )
			{
				iTopItem--;
			}
		}
	}

	iTopItem = bound( 0, iTopItem, m_pModel->GetRows() - iNumRows );

	return true;
}

int CMenuTable::CalcVisibleRowCount( int startItem ) const
{
	if( !m_pModel || m_pModel->GetRows() <= 0 )
		return 0;

	startItem = bound( 0, startItem, m_pModel->GetRows() - 1 );

	int count = 0;
	int y = boxPos.y;

	const int bottom = boxPos.y + boxSize.h;

	for( int i = startItem; i < m_pModel->GetRows(); i++ )
	{
		const int rowHeight = m_pModel->GetRowHeight( i, m_scChSize );

		if( y + rowHeight > bottom )
			break;

		y += rowHeight;
		count++;
	}

	return count;
}

bool CMenuTable::MoveView(int delta )
{
	iTopItem += delta;

	if( iTopItem < abs(delta) )
	{
		iTopItem = 0;
		return false;
	}
	else if( iTopItem > m_pModel->GetRows() - iNumRows )
	{
		if( m_pModel->GetRows() - iNumRows < 0 )
			iTopItem = 0;
		else
			iTopItem = m_pModel->GetRows() - iNumRows;
		return false;
	}

	return true;
}

bool CMenuTable::MoveCursor(int delta)
{
	iCurItem += delta;

	if( iCurItem < 0 )
	{
		iCurItem = 0;
		return false;
	}
	else if( iCurItem > m_pModel->GetRows() - 1 )
	{
		iCurItem = m_pModel->GetRows() - 1;
		return false;
	}
	return true;
}

void CMenuTable::SetCurrentIndex( int idx )
{
	iCurItem = bound( 0, idx, m_pModel->GetRows() );

	if( iCurItem < iTopItem )
		iTopItem = iCurItem;
	if( iNumRows ) // check if already vidinit
	{
		if( iCurItem > iTopItem + iNumRows - 1 )
			iTopItem = iCurItem - iNumRows + 1;
		if( iTopItem > m_pModel->GetRows() - iNumRows )
			iTopItem = m_pModel->GetRows() - iNumRows;
		if( iTopItem < 0 )
			iTopItem = 0;
	}
	else
	{
		iTopItem = 0; // will be recalculated on vidinit
	}
}

float CMenuTable::Step()
{
	float step = (m_pModel->GetRows() <= 1 ) ? 1 : (downArrow.y - upArrow.y - arrow.h) / (float)(m_pModel->GetRows() - 1);

	return step;
}

bool CMenuTable::KeyUp( int key )
{
	const char *sound = 0;
	int i;
	bool noscroll = false;

	iScrollBarSliding = false;

	if( UI::Key::IsLeftMouse( key ))
	{
		noscroll = true; // don't scroll to current when mouse used

		if( FBitSet( iFlags, QMF_HASMOUSEFOCUS ))
		{
			// test for arrows
			if( UI_CursorInRect( upArrow, arrow ) )
			{
				if( MoveView( -5 ) )
					sound = uiStatic.sounds[SND_MOVE];
				else sound = uiStatic.sounds[SND_BUZZ];
			}
			else if( UI_CursorInRect( downArrow, arrow ))
			{
				if( MoveView( 5 ) )
					sound = uiStatic.sounds[SND_MOVE];
				else sound = uiStatic.sounds[SND_BUZZ];
			}
			else if( UI_CursorInRect( boxPos, boxSize ))
			{
				// test for item select
				int starty = boxPos.y + iStrokeWidth;
				int endy = starty + iNumRows * m_scChSize;
				if( uiStatic.cursorY > starty && uiStatic.cursorY < endy )
				{
					int offsety = uiStatic.cursorY - starty;
					int newCur = iTopItem + offsety / m_scChSize;

					if( newCur < m_pModel->GetRows() )
					{
						if( newCur == iCurItem )
						{
							if( uiStatic.realTime - m_iLastItemMouseChange < 200 ) // 200 msec to double click
							{
								m_pModel->OnActivateEntry( iCurItem );
							}
						}
						else
						{
							iCurItem = newCur;
							sound = uiStatic.sounds[SND_NULL];
						}

						m_iLastItemMouseChange = uiStatic.realTime;
					}
				}
			}
			else if( bAllowSorting && UI_CursorInRect( m_scPos, headerSize ))
			{
				Point p = m_scPos;
				Size sz;
				sz.h = headerSize.h;

				for( i = 0; i < m_pModel->GetColumns(); i++, p.x += sz.w )
				{
					if( columns[i].fStaticWidth )
						sz.w = columns[i].flWidth * uiStatic.scaleX;
					else
						sz.w = ((float)headerSize.w - flFixedSumm) * columns[i].flWidth / flDynamicSumm;

					if( UI_CursorInRect( p, sz ))
					{
						if( GetSortingColumn() != i )
						{
							SetSortingColumn( i );
						}
						else
						{
							SwapOrder();
						}

						_Event( QM_CHANGED );
					}
				}
			}
		}
	}
	else if( UI::Key::IsEnter( key ))
	{
		if( m_pModel->GetRows() )
			m_pModel->OnActivateEntry( iCurItem ); // activate only on release
		else
			sound = uiStatic.sounds[SND_BUZZ]; // list is empty, can't activate anything
	}

	if( !noscroll )
	{
		if( iCurItem < iTopItem )
			iTopItem = iCurItem;
		if( iCurItem > iTopItem + iNumRows - 1 )
			iTopItem = iCurItem - iNumRows + 1;
		if( iTopItem > m_pModel->GetRows() - iNumRows )
			iTopItem = m_pModel->GetRows() - iNumRows;
		if( iTopItem < 0 )
			iTopItem = 0;
	}

	if( sound )
	{
		if( sound != uiStatic.sounds[SND_BUZZ] )
			_Event( QM_CHANGED );

		PlayLocalSound( sound );
	}

	return sound != NULL;
}

bool CMenuTable::KeyDown( int key )
{
	const char *sound = 0;
	bool noscroll = false;

	if( UI::Key::IsUpArrow( key ))
		sound = MoveCursor( -1 ) ? uiStatic.sounds[SND_MOVE] : 0;
	else if( UI::Key::IsDownArrow( key ))
		sound = MoveCursor( 1 ) ? uiStatic.sounds[SND_MOVE] : 0;
	else if( key == K_MWHEELUP )
		sound = MoveCursor( -1 ) ? uiStatic.sounds[SND_MOVE] : uiStatic.sounds[SND_BUZZ];
	else if( key == K_MWHEELDOWN )
		sound = MoveCursor( 1 ) ? uiStatic.sounds[SND_MOVE] : uiStatic.sounds[SND_BUZZ];
	else if( UI::Key::IsPageUp( key ))
		sound = MoveCursor( -2 ) ? uiStatic.sounds[SND_MOVE] : uiStatic.sounds[SND_BUZZ];
	else if( UI::Key::IsPageDown( key ))
		sound = MoveCursor( 2 ) ? uiStatic.sounds[SND_MOVE] : uiStatic.sounds[SND_BUZZ];
	else if( UI::Key::IsHome( key ))
	{
		sound = iCurItem > 0 ? uiStatic.sounds[SND_MOVE] : uiStatic.sounds[SND_BUZZ];
		iCurItem = 0;
	}
	else if( UI::Key::IsEnd( key ))
	{
		int lastItem = Q_max( m_pModel->GetRows() - 1, 0 );
		sound = iCurItem < lastItem ? uiStatic.sounds[SND_MOVE] : uiStatic.sounds[SND_BUZZ];
		iCurItem = lastItem;
	}
	else if( UI::Key::IsDelete( key ))
	{
		if( m_pModel->GetRows() )
			m_pModel->OnDeleteEntry( iCurItem ); // allow removing entries on repeating
		else
			sound = uiStatic.sounds[SND_BUZZ];
	}
	else if( UI::Key::IsLeftMouse( key ))
	{
		noscroll = true; // don't scroll to current when mouse used

		if( FBitSet( iFlags, QMF_HASMOUSEFOCUS ))
		{
			// test for scrollbar
			if( UI_CursorInRect( sbarPos, sbarSize ))
				iScrollBarSliding = true;
		}
	}

	if( !noscroll )
	{
		if( iCurItem < iTopItem )
			iTopItem = iCurItem;
		if( iCurItem > iTopItem + iNumRows - 1 )
			iTopItem = iCurItem - iNumRows + 1;
		if( iTopItem > m_pModel->GetRows() - iNumRows )
			iTopItem = m_pModel->GetRows() - iNumRows;
		if( iTopItem < 0 )
			iTopItem = 0;
	}

	if( sound )
	{
		if( sound != uiStatic.sounds[SND_BUZZ] )
			_Event( QM_CHANGED );

		PlayLocalSound( sound );
	}

	return sound != NULL;
}

void CMenuTable::DrawLine( Point p, const char **psz, size_t size, uint textColor, bool forceCol, uint fillColor )
{
	size_t i;
	Size sz;
	uint textflags = 0;

	textflags |= iFlags & QMF_DROPSHADOW ? ETF_SHADOW : 0;
	textflags |= forceCol ? ETF_FORCECOL : 0;

	sz.h = headerSize.h;

	if( fillColor )
	{
		sz.w = headerSize.w;
		UI_FillRect( p, sz, fillColor );
	}

	for( i = 0; i < size; i++, p.x += sz.w )
	{
		Point pt = p;

		if( columns[i].fStaticWidth )
			sz.w = columns[i].flWidth * uiStatic.scaleX;
		else
			sz.w = ((float)headerSize.w - flFixedSumm) * columns[i].flWidth / flDynamicSumm;

		if( !psz[i] || !sz.w ) // headers may be null, cells too
			continue;

		if( bAllowSorting && i == GetSortingColumn() )
		{
			// A7-full-port: drawn sort caret instead of the gfx/shell up/down
			// art (that art ships in the APK extras and looks xash3d)
			Size tri;
			tri.w = tri.h = Q_max( 6, (int)( sz.h * 0.4f ) );
			Point triPos;
			triPos.x = pt.x + sz.w - tri.w - (int)( 4 * uiStatic.scaleX );
			triPos.y = pt.y + ( sz.h - tri.h ) / 2;
			UI_DrawTriangle( triPos, tri, IsAscend() != 0, textColor );
		}

		UI_DrawString( font, pt, sz, psz[i], textColor, m_scChSize,
			m_pModel->GetAlignmentForColumn( i ), textflags );
	}
}

void CMenuTable::DrawLine( Point p, int line, uint textColor, bool forceCol, uint fillColor, uint rowHeight )
{
	int i;
	Size sz;

	// A7 build 116 (user log: SIGSEGV at UI_DrawString string[0] with a
	// stale CUtlString garbage pointer): never hand a row outside the
	// model's [0, GetRows()) range to GetCellText - the model can't be
	// trusted while an async response is rebuilding its vector, so a row
	// that no longer exists renders as nothing instead of freed-memory
	// bytes being drawn as a string pointer.
	if( !m_pModel || line < 0 || line >= m_pModel->GetRows() )
		return;

	sz.h = rowHeight;
	int contentOffsetY = ( rowHeight - m_scChSize ) / 2;

	unsigned int newFillColor;
	bool forceFillColor = false;
	if( m_pModel->GetLineColor( line, newFillColor, forceFillColor ))
	{
		if( !fillColor || forceFillColor )
			fillColor = newFillColor;
	}

	if( fillColor )
	{
		sz.w = headerSize.w;
		UI_FillRect( p, sz, fillColor );
	}

	for( i = 0; i < m_pModel->GetColumns(); i++, p.x += sz.w )
	{
		uint textflags = 0;

		textflags |= iFlags & QMF_DROPSHADOW ? ETF_SHADOW : 0;

		if( columns[i].fStaticWidth )
			sz.w = columns[i].flWidth * uiStatic.scaleX;
		else
			sz.w = ((float)boxSize.w - flFixedSumm) * columns[i].flWidth / flDynamicSumm;

		const char *str = m_pModel->GetCellText( line, i );
		const ECellType type = m_pModel->GetCellType( line, i );

		if( !str /* && type != CELL_ITEM  */) // headers may be null, cells too
			continue;

		bool useCustomColors = m_pModel->GetCellColors( line, i, newFillColor, forceFillColor );

		if( useCustomColors )
		{
			if( forceFillColor || forceCol )
			{
				textflags |= ETF_FORCECOL;
			}
			textColor = newFillColor;
		}

		switch( type )
		{
		case CELL_TEXT:
		{
			Point contentPos = p;
			contentPos.y += contentOffsetY;

			Size contentSize = sz;
			contentSize.h = m_scChSize;

			// A7 build 134 (round 135, user: "fix find servers texts stucks
			// ... correct names ... just like goldsource"): cells draw the
			// WHOLE string and are clipped at the column edge, exactly like
			// the original browser. The default path handed UI_DrawString the
			// column width (contentSize.h == one line, so the word-wrap
			// branch was dead) and it therefore ellipsised - and because the
			// cut point is remembered per character, the cut landed MID-WORD
			// ("whatever fits...") or, for a column too narrow even for "...",
			// hard-cut with no dots at all - that is the "stuck" text.
			// The per-column scissor keeps a long name inside its own column
			// (the body scissor at the end of Draw() only covers the table).
			//
			// A7 build 138 (round 137 item 4, user: "fix fonts text _
			// not show in menus"): the cell scissor used to be sized
			// EXACTLY at the row height, and on the STB backend the Tahoma
			// raster derives its scale from the hhea win metrics
			// (ascent 2049 / descent -423), which puts the baseline at
			// ~0.83 * (tall + 4) - i.e. the underscore lands 1-4 device px
			// BELOW the m_scChSize line box (descender tails lose the same
			// pixels). A row-height scissor therefore cut the entire glyph
			// away: list entries like "de_dust2" / "config_backup" rendered
			// "de dust2" with a gap. 5px of bottom allowance covers the
			// worst case at every font tier; the next row's glyphs only
			// start ~7px into their own row, so the slack can never touch
			// them, and the body scissor at the end of Draw() still caps
			// the table.
			UI::Scissor::PushScissor( p, Size( sz.w, sz.h + 5 ));

			UI_DrawString(
				font,
				contentPos,
				contentSize,
				str,
				textColor,
				m_scChSize,
				m_pModel->GetAlignmentForColumn( i ),
				textflags | ETF_NOSIZELIMIT
			);

			UI::Scissor::PopScissor();

			break;
		}
		case CELL_IMAGE_ADDITIVE:
		case CELL_IMAGE_DEFAULT:
		case CELL_IMAGE_HOLES:
		case CELL_IMAGE_TRANS:
		{
			HIMAGE pic = EngFuncs::PIC_Load( str );

			if( !pic )
				continue;

			Point picPos = p;
			picPos.y += ( rowHeight - m_scChSize ) / 2;
			Size picSize = EngFuncs::PIC_Size( pic );
			float scale = (float)m_scChSize/(float)picSize.h;

			// A7 build 111: aspect-fit into the column as well - a wide
			// picture scaled by height alone ran past the cell edge into
			// the next column's text
			if( picSize.w > 0 && picSize.w * scale > (float)sz.w )
				scale = (float)sz.w / (float)picSize.w;

			picSize = picSize * scale;

			switch( m_pModel->GetAlignmentForColumn( i ) )
			{
			case QM_RIGHT: picPos.x += ( sz.w - picSize.w ); break;
			case QM_CENTER: picPos.x += ( sz.w - picSize.w ) / 2; break;
			default: break;
			}

			if( useCustomColors )
			{
				int r, g, b, a;
				UnpackRGBA( r, g, b, a, newFillColor );
				EngFuncs::PIC_Set( pic, r, g, b, a );
			}
			else
			{
				EngFuncs::PIC_Set( pic, 255, 255, 255 );
			}

			switch( type )
			{
			case CELL_IMAGE_ADDITIVE:
				EngFuncs::PIC_DrawAdditive( picPos, picSize );
				break;
			case CELL_IMAGE_DEFAULT:
				EngFuncs::PIC_Draw( picPos, picSize );
				break;
			case CELL_IMAGE_HOLES:
				EngFuncs::PIC_DrawHoles( picPos, picSize );
				break;
			case CELL_IMAGE_TRANS:
				EngFuncs::PIC_DrawTrans( picPos, picSize );
				break;
			default: break; // shouldn't happen
			}

			break;
		}
		case CELL_IMAGE_ROWSCALED:
		{
			// Like CELL_IMAGE_TRANS, this draws with transparency, but scales to the row's height
			// (rowHeight, as computed by the model's GetRowHeight())
			// rather than to the text char height (m_scChSize). This
			// is what lets a model grow avatar-style cells purely by
			// varying row height -- e.g. based on how many rows need
			// to fit -- without needing a larger font for the whole
			// row just to get a bigger image.
			HIMAGE pic = EngFuncs::PIC_Load( str );

			if( !pic )
				continue;

			Size picSize = EngFuncs::PIC_Size( pic );

			// Small fixed margin so the avatar doesn't touch the row's
			// top/bottom edge even when rowHeight == picSize after scale.
			int avatarMargin = Q_max( 0, (int)( 2 * uiStatic.scaleY ));
			int targetHeight = Q_max( 1, (int)rowHeight - avatarMargin * 2 );

			// Never let the (assumed square-ish) avatar grow wider than
			// its own column -- callers size columns independently of
			// row height, so this is the one place that can enforce it
			// regardless of what the caller's column width happens to be.
			targetHeight = Q_min( targetHeight, (int)sz.w );

			// A7 build 111: aspect-FIT, not height-only - a wide/tall
			// logo scaled purely by height overflowed the avatar column
			// and overlapped the name text ("sometime in score board
			// size not looking good, so fix it"). Both axes now stay
			// inside the cell; the picture keeps its real aspect.
			float scale = (float)targetHeight / (float)picSize.h;
			if( picSize.w > 0 && picSize.w * scale > (float)sz.w )
				scale = (float)sz.w / (float)picSize.w;
			picSize = picSize * scale;

			Point picPos = p;
			picPos.y += ( rowHeight - picSize.h ) / 2;

			switch( m_pModel->GetAlignmentForColumn( i ) )
			{
			case QM_RIGHT: picPos.x += ( sz.w - picSize.w ); break;
			case QM_CENTER: picPos.x += ( sz.w - picSize.w ) / 2; break;
			default: break;
			}

			if( useCustomColors )
			{
				int r, g, b, a;
				UnpackRGBA( r, g, b, a, newFillColor );
				EngFuncs::PIC_Set( pic, r, g, b, a );
			}
			else
			{
				EngFuncs::PIC_Set( pic, 255, 255, 255 );
			}

			// Row-scaled images use the same transparent draw mode as CELL_IMAGE_TRANS.
			EngFuncs::PIC_DrawTrans( picPos, picSize );

			break;
		}
		}
	}
}

/*
=================
TableDrawScrollArrow

A7-full-port: always draw the triangle. The gfx/shell arrow art does ship
in the APK extras, so the "use art when present" path kept rendering the
xash3d arrows (user report: "other menus still look xash3d style extras
bmp image"). The drawn triangle matches the rest of the rethemed UI.
=================
*/
static void TableDrawScrollArrow( Point pos, Size size, bool bUp, unsigned int color, CImage &pic )
{
	(void)pic;
	UI_DrawTriangle( pos, size, bUp, color );
}

void CMenuTable::Draw()
{
	int i, y;
	// A7: cs16 gold-olive selection fill (accent blended over secondary bg)
	int selColor = PackRGB( 115, 114, 65 );
	int upFocus, downFocus, scrollbarFocus;

	// A7 build 138 (round 137 item 5, user: "in picture scoreboard command
	// it show like that": the header labels HP/Money/Score/Deaths/Latency
	// drew ~190px right of their value columns and overflowed the frame):
	// the header line at DrawLine(m_scPos, ...) is the ONLY thing drawn at
	// the live position - frame, scrollbar and rows all use the cached
	// boxPos/boxSize/arrow/headerSize. Re-derive both (and the column sums
	// SetupColumn may have changed) from wherever the table actually is
	// this frame, so header and rows can never disagree again.
	RefreshGeometryAndSums();

	static bool s_bGeometryRefreshLogged = false;
	if( !s_bGeometryRefreshLogged )
	{
		s_bGeometryRefreshLogged = true;
		Con_Printf( "[A7] build138: table geometry refreshed in Draw (header + box follow m_scPos)\n" );
		// round 137 item 4 proof: the cell scissor below carries the
		// 5px descender allowance that keeps the STB-Tahoma underscore
		// (drawn 1-4px below the line box) visible inside list rows
		Con_Printf( "[A7] build138: table cells keep a 5px descender slack so '_' prints in lists\n" );
	}

	// Calculate the actual number of rows that fit in the table.
	// Rows may have different heights, so using boxHeight / m_scChSize
	// is not valid here. (scoreboard port)
	if( m_pModel->GetRows() > 0 )
	{
		iTopItem = bound( 0, iTopItem, m_pModel->GetRows() - 1 );
		iNumRows = CalcVisibleRowCount( iTopItem );
	}
	else
	{
		iTopItem = 0;
		iNumRows = 0;
	}

	if( UI_CursorInRect( boxPos, boxSize ) )
	{
		int newCur = iTopItem + ( uiStatic.cursorY - boxPos.y ) / m_scChSize;

		if( newCur < m_pModel->GetRows() )
			iHighlight = newCur;
		else iHighlight = -1;
	}
	else iHighlight = -1;

	if( szBackground )
	{
		UI_DrawPic( m_scPos, m_scSize, uiColorWhite, szBackground );
	}
	else
	{
		// draw the opaque outlinebox first
		if( bFramedHintText )
		{
			UI_FillRect( m_scPos, headerSize.AddVertical( boxSize ), iBackgroundColor );
		}
		else
		{
			UI_FillRect( boxPos, boxSize, iBackgroundColor );
		}
	}

	int columns = Q_min( m_pModel->GetColumns(), MAX_TABLE_COLUMNS );

	DrawLine( m_scPos, szHeaderTexts, columns, iHeaderColor, true );

	if( !szBackground )
	{
		int color;

		if( eFocusAnimation == QM_HIGHLIGHTIFFOCUS && iFlags & QMF_HASKEYBOARDFOCUS )
			color = iStrokeFocusedColor;
		else
			color = colorStroke;

		if( bFramedHintText )
		{
			UI_DrawRectangleExt( m_scPos, headerSize, color, iStrokeWidth, QM_LEFT | QM_TOP | QM_RIGHT );
		}

		if( bDrawStroke )
			UI_DrawRectangleExt( boxPos, boxSize, color, iStrokeWidth );
	}


	sbarPos.x = upArrow.x + arrow.w * 0.125f;
	sbarSize.w = arrow.w * 0.75f;

	float step = Step();

	if(((downArrow.y - upArrow.y - arrow.h) - (((m_pModel->GetRows()-1)*m_scChSize)/2)) < 2)
	{
		sbarSize.h = (downArrow.y - upArrow.y - arrow.h) - (step * (m_pModel->GetRows() - iNumRows));
		sbarPos.y = upArrow.y + arrow.h + (step*iTopItem);
	}
	else
	{
		sbarSize.h = downArrow.y - upArrow.y - arrow.h - (((m_pModel->GetRows()- iNumRows) * m_scChSize) / 2);
		sbarPos.y = upArrow.y + arrow.h + (((iTopItem) * m_scChSize)/2);
	}

	// draw the arrows base
	UI_FillRect( upArrow.x, upArrow.y + arrow.h,
		arrow.w, downArrow.y - upArrow.y - arrow.h, uiInputFgColor );

	// ADAMIX
	if( iScrollBarSliding )
	{
		// Draw scrollbar background
		UI_FillRect( sbarPos.x, upArrow.y + arrow.h, sbarSize.w, downArrow.y - upArrow.y - arrow.h, uiColorBorderDark);
	}

	// ADAMIX END
	// draw the arrows
	if( iFlags & QMF_GRAYED )
	{
		TableDrawScrollArrow( upArrow, arrow, true,  uiColorDkGrey, szUpArrow );
		TableDrawScrollArrow( downArrow, arrow, false, uiColorDkGrey, szDownArrow );
	}
	else
	{
		scrollbarFocus = UI_CursorInRect( sbarPos, sbarSize );

		// special case if we sliding but lost focus
		if( iScrollBarSliding ) scrollbarFocus = true;

		// Draw scrollbar itself (A7: light thumb, accent while focused)
		UI_FillRect( sbarPos, sbarSize, scrollbarFocus ? uiColorAccent : uiColorBorderLight );

		if(this != m_pParent->ItemAtCursor())
		{
			TableDrawScrollArrow( upArrow, arrow, true,  uiColorWhite, szUpArrow );
			TableDrawScrollArrow( downArrow, arrow, false, uiColorWhite, szDownArrow );
		}
		else
		{
			// see which arrow has the mouse focus
			upFocus = UI_CursorInRect( upArrow, arrow );
			downFocus = UI_CursorInRect( downArrow, arrow );

			// special case if we sliding but lost focus
			if( iScrollBarSliding )
			{
				upFocus = downFocus = false;
			}

			if( eFocusAnimation == QM_HIGHLIGHTIFFOCUS )
			{
				TableDrawScrollArrow( upArrow, arrow, true,  upFocus   ? uiColorAccent : uiColorWhite, upFocus   ? szUpArrowFocus : szUpArrow );
				TableDrawScrollArrow( downArrow, arrow, false, downFocus ? uiColorAccent : uiColorWhite, downFocus ? szDownArrowFocus : szDownArrow );
			}
			else if( eFocusAnimation == QM_PULSEIFFOCUS )
			{
				int	color;

				color = PackAlpha( colorBase, 255 * (0.5f + 0.5f * sin( (float)uiStatic.realTime / UI_PULSE_DIVISOR )));

				TableDrawScrollArrow( upArrow, arrow, true,  upFocus   ? color : (int)colorBase, upFocus   ? szUpArrowFocus : szUpArrow );
				TableDrawScrollArrow( downArrow, arrow, false, downFocus ? color : (int)colorBase, downFocus ? szDownArrowFocus : szDownArrow );
			}
		}
	}


	// prevent the columns out of rectangle bounds
	UI::Scissor::PushScissor( boxPos, boxSize );
	y = boxPos.y;

	for( i = iTopItem; i < m_pModel->GetRows(); i++ )
	{
		int rowHeight = m_pModel->GetRowHeight( i, m_scChSize );

		if( y + rowHeight > boxPos.y + boxSize.h )
			break;

		int color = colorBase; // predict state
		bool forceCol = false;
		int fillColor = 0;

		if( iFlags & QMF_GRAYED )
		{
			color = uiColorDkGrey;
			forceCol = true;
		}
		else if( !(iFlags & QMF_INACTIVE) )
		{
			if( i == iCurItem )
			{
				// A7: keep row text readable on the selection fill
				fillColor = selColor;
			}
			else if( i == iHighlight )
			{
				fillColor = 0x604A5942; // A7: hover tint (panel color)
			}
		}

		DrawLine( Point( boxPos.x, y ), i, color, forceCol, fillColor, rowHeight );
		y += rowHeight;
	}

	UI::Scissor::PopScissor();
}
