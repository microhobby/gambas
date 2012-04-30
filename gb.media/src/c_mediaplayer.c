/***************************************************************************

  c_mediaplayer.c

  gb.media component

  (c) 2012 Benoît Minisini <gambas@users.sourceforge.net>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 1, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
  MA 02110-1301, USA.

***************************************************************************/

#define __C_MEDIAPLAYER_C

#include "c_mediaplayer.h"

//---- MediaPlayer -------------------------------------------------------


//-------------------------------------------------------------------------

GB_DESC MediaPlayerDesc[] = 
{
	GB_DECLARE("MediaPlayer", sizeof(CMEDIAPLAYER)),
	GB_INHERITS("MediaPipeline"),
	
	GB_END_DECLARE
};

