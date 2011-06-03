/***************************************************************************

  gspinbox.cpp

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
	MA 02110-1301, USA.

***************************************************************************/

#include "widgets.h"
#include "widgets_private.h"
#include "gspinbox.h"

void  spin_change(GtkSpinButton *spinbutton,gSpinBox *data)
{
	data->selectAll();
	if (data->onChange) data->onChange(data);
}

gSpinBox::gSpinBox(gContainer *parent) : gControl(parent)
{
	g_typ=Type_gSpinBox;
	have_cursor = true;
	use_base = true;
	
	_min = 0;
	_max = 100;
	
	border = gtk_spin_button_new_with_range(_min, _max, 1);
	widget = border;
	
	realize();
	
	onChange = NULL;
	
	g_signal_connect(G_OBJECT(widget),"value-changed",G_CALLBACK(spin_change),(gpointer)this);
}

/*
int gSpinBox::background()
{
	return get_gdk_base_color(widget);
}

void gSpinBox::setBackground(int color)
{
	set_gdk_base_color(widget,color);	
	if (!border->window) gtk_widget_realize(border);
	gdk_window_process_updates(border->window,true);
}

int gSpinBox::foreground()
{
	return get_gdk_text_color(widget);
}

void gSpinBox::setForeground(int color)
{	
	set_gdk_text_color(widget,color);
	if (!border->window) gtk_widget_realize(border);
	gdk_window_process_updates(border->window,true);
}
*/

int gSpinBox::step()
{
	gdouble step;
	
	gtk_spin_button_get_increments(GTK_SPIN_BUTTON(widget),&step,NULL);
	return (int)step;
}

int gSpinBox::value()
{
	gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
	return (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

bool gSpinBox::wrap()
{
	return gtk_spin_button_get_wrap(GTK_SPIN_BUTTON(widget));
}
	
void gSpinBox::setMaxValue(int vl)
{
	_max = vl;
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(widget),_min, _max);
}

void gSpinBox::setMinValue(int vl)
{
	_min = vl;
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(widget), _min, _max);
}

void gSpinBox::setStep(int vl)
{
	gdouble step = (gdouble)vl;
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(widget), step, step);
}

void gSpinBox::setValue(int vl)
{
	if (vl == value())
		return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),(gdouble)vl);
}

void gSpinBox::setWrap(bool vl)
{
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(widget),vl);
}

void gSpinBox::updateCursor(GdkCursor *cursor)
{
  GdkWindow *win;
  
  gControl::updateCursor(cursor);
  	
  win = GTK_ENTRY(widget)->text_area;
  if (!win)
  	return;
  	
  if (cursor)
    gdk_window_set_cursor(win, cursor);
  else
  {
    cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget), GDK_XTERM);
    gdk_window_set_cursor(win, cursor);
    gdk_cursor_unref(cursor);
  }
}

void gSpinBox::selectAll()
{
	gtk_editable_select_region(GTK_EDITABLE(widget), 0, -1);
}

bool gSpinBox::hasBorder() const
{
	return gtk_entry_get_has_frame(GTK_ENTRY(widget));
}

void gSpinBox::setBorder(bool vl)
{
	gtk_entry_set_has_frame(GTK_ENTRY(widget), vl);
}

