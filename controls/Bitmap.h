/*
Bitmap.h - bitmap menu item
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

#ifndef MENU_BITMAP_H
#define MENU_BITMAP_H

#include "BaseItem.h"

class CMenuBitmap : public CMenuBaseItem
{
public:
	typedef CMenuBaseItem BaseClass;

	CMenuBitmap();
	bool bKeepAspectRatio;

	void VidInit( void ) override;
	bool KeyUp( int key ) override;
	bool KeyDown( int key ) override;
	void Draw( void ) override;
	void SetPicture( const char *pic, const char *focusPic = NULL, const char *pressPic = NULL)
	{
		szPic = pic;
		szFocusPic = focusPic;
		szPressPic = pressPic;
	}

	// A7 build 143 (round 143 item 6): true when SetPicture's load really
	// materialized a texture - callers gate white placeholder fills on
	// this (Draw() paints colorBase whenever the handle is missing, so a
	// file the decoder rejects showed as a solid white plate)
	bool IsPictureValid( void ) const { return szPic.IsValid(); }

	void SetRenderMode( ERenderMode renderMode, ERenderMode focusRenderMode = QM_DRAWNORMAL, ERenderMode pressRenderMode = QM_DRAWNORMAL )
	{
		eRenderMode = renderMode;
		eFocusRenderMode = focusRenderMode;
		ePressRenderMode = pressRenderMode;
	}

protected:
	CImage szPic;
	ERenderMode eRenderMode;

	CImage szFocusPic;
	ERenderMode eFocusRenderMode;

	CImage szPressPic;
	ERenderMode ePressRenderMode;
};

#endif // MENU_BITMAP_H
