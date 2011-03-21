/***************************************************************************

  c_image.h

  (c) 2000-2011 Benoît Minisini <gambas@users.sourceforge.net>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

***************************************************************************/

#ifndef __C_IMAGE_H
#define __C_IMAGE_H

#include "main.h"

typedef 
  struct 
  {
    GB_IMG img;
  }
  CIMAGE;

#ifndef __C_IMAGE_C
extern GB_DESC CImageDesc[];
#else

#define THIS ((GB_IMG *)_object)

#endif

#endif /* __CIMAGE_H */
