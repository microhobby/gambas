/***************************************************************************

  main.cpp
  
  gb.poppler component

  (c) Benoît Minisini <g4mba5@gmail.com>

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

#define __MAIN_CPP

#include "c_pdf_document.h"
#include "main.h"

extern "C"
{
GB_INTERFACE GB EXPORT;
IMAGE_INTERFACE IMAGE EXPORT;

GB_DESC *GB_CLASSES[] EXPORT =
{
	PdfPageDesc,
	PdfDocumentDesc,
	NULL
};

int EXPORT GB_INIT(void)
{
	GB.GetInterface("gb.image", IMAGE_INTERFACE_VERSION, &IMAGE);
	
	return 0;
}

void EXPORT GB_EXIT()
{

}

}