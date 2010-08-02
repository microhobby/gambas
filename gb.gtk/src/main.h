/***************************************************************************

  main.h

  (c) 2004-2006 - Daniel Campos Fernández <dcamposf@gmail.com>

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

#ifndef __MAIN_H
#define __MAIN_H

#include "gambas.h"
#include "gb.image.h"
#include "gb.gtk.h"
#include "widgets.h"
#include "CWidget.h"

#ifndef __MAIN_C
extern GB_INTERFACE GB;
extern IMAGE_INTERFACE IMAGE;

extern GB_CLASS CLASS_Picture;
extern GB_CLASS CLASS_Image;
extern GB_CLASS CLASS_DrawingArea;
extern GB_CLASS CLASS_Menu;
extern GB_CLASS CLASS_Window;
extern GB_CLASS CLASS_Printer;
extern GB_CLASS CLASS_SvgImage;

#endif

void do_iteration(bool do_not_block, bool do_not_sleep = false);

#endif

