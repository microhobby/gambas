/***************************************************************************

  gdrag.cpp

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
#include "gapplication.h"
#include "gmainwindow.h"
#include "gmouse.h"
#include "gclipboard.h"
#include "gdrag.h"

//#define DEBUG_ME 1

#ifdef GTK3
struct _GtkTargetList
{
  GList *list;
  guint ref_count;
};

#if GTK_CHECK_VERSION(3, 12, 0)
#else
struct _GtkTargetPair
{
  GdkAtom   target;
  guint     flags;
  guint     info;
};
#endif

typedef
	struct _GtkTargetPair GtkTargetPair;
#endif

static int _current_clipboard = gClipboard::Clipboard;
static GtkClipboard *_clipboard = NULL;
static GtkClipboard *_selection = NULL;
static bool _clipboard_has_changed[2] = { FALSE };

static char *convert_format(char *fmt)
{
	if (!strcmp(fmt, "STRING"))
		return (char *)"text/plain";
	if (!strcmp(fmt, "UTF8_STRING"))
		return (char *)"text/plain;charset=utf-8";
	return fmt;
}

#if GTK_CHECK_VERSION(2, 14, 0)
#else
static gint gtk_selection_data_get_length(const GtkSelectionData *sel)
{
	return sel->length;
}

static const guchar *gtk_selection_data_get_data(const GtkSelectionData *sel)
{
	return sel->data;
}
#endif

#if GTK_CHECK_VERSION(2, 22, 0)
#else
static GList *gdk_drag_context_list_targets(GdkDragContext *context)
{
	return context->targets;
}
#endif

/***********************************************************************

	Clipboard

************************************************************************/

static void cb_change(GtkClipboard *clipboard, GdkEvent *, gpointer)
{
	_clipboard_has_changed[clipboard == _selection ? gClipboard::Clipboard : gClipboard::Selection] = TRUE;
}


static GtkClipboard *get_clipboard()
{
	if (_current_clipboard == gClipboard::Selection)
	{
		if (!_selection)
		{
			_selection = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
			g_signal_connect(G_OBJECT(_selection), "owner-change", G_CALLBACK(cb_change), (gpointer)gClipboard::Clipboard);
		}
		return _selection;
	}
	else
	{
		if (!_clipboard)
		{
			_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
			g_signal_connect(G_OBJECT(_clipboard), "owner-change", G_CALLBACK(cb_change), (gpointer)gClipboard::Selection);
		}
		return _clipboard;
	}
}

bool gClipboard::hasChanged()
{
	return _clipboard_has_changed[_current_clipboard];
}

void gClipboard::setCurrent(int clipboard)
{
	_current_clipboard = clipboard;
}

int gClipboard::getCurrent()
{
	return _current_clipboard;
}

void gClipboard::clear()
{
	if (gApplication::isInit())
		gtk_clipboard_clear(get_clipboard());
}

char *gClipboard::getFormat(int n)
{
	int i;
	gint n_tg;
	GdkAtom *targets;
	char *fmt, *cfmt;

	if (!gtk_clipboard_wait_for_targets(get_clipboard(), &targets, &n_tg))
		return NULL;

	for (i = 0; i < n_tg; i++)
	{
		fmt = gdk_atom_name(targets[i]);
		cfmt = convert_format(fmt);
		if (!islower(cfmt[0]))
		{
			g_free(fmt);
			continue;
		}
		if (n == 0)
		{
			gt_free_later(fmt);
			return cfmt;
		}
		n--;
	}

	return NULL;
}

static void cb_clear_text(GtkClipboard *clipboard, gpointer text)
{
	if (text) g_free(text);
}

static void cb_get_text(GtkClipboard *clipboard, GtkSelectionData *selection, guint info, gpointer text)
{
	gtk_selection_data_set(selection, gtk_selection_data_get_target(selection), 8, (guchar *)text, strlen((char *)text));
}

int gClipboard::getType()
{
	if (gtk_clipboard_wait_is_image_available(get_clipboard())) return Image;
	if (gtk_clipboard_wait_is_text_available(get_clipboard())) return Text;
	return Nothing;
}

void gClipboard::setImage(gPicture *image)
{
	gtk_clipboard_set_image(get_clipboard(), image->getPixbuf());
}

gPicture * gClipboard::getImage()
{
	_clipboard_has_changed[_current_clipboard] = FALSE;
  return new gPicture(gtk_clipboard_wait_for_image(get_clipboard()));
}

static void
gt_clipboard_set_text (GtkClipboard *clipboard, char *format, const gchar *text, gint len)
{
  GtkTargetList *list;
  GList *l;
  GtkTargetEntry *targets;
  gint n_targets, i;

  list = gtk_target_list_new (NULL, 0);
  if (format)
  	gtk_target_list_add (list, gdk_atom_intern(format, false), 0, 0);
  gtk_target_list_add_text_targets (list, 0);

  n_targets = g_list_length(list->list);
  targets = g_new0 (GtkTargetEntry, n_targets);
  for (l = list->list, i = 0; l; l = l->next, i++)
    {
      GtkTargetPair *pair = (GtkTargetPair *)l->data;
      targets[i].target = gdk_atom_name (pair->target);
    }

  if (len < 0)
    len = strlen (text);

  gtk_clipboard_set_with_data (clipboard,
			       targets, n_targets,
			       cb_get_text, cb_clear_text,
			       g_strndup (text, len));
  gtk_clipboard_set_can_store (clipboard, NULL, 0);

  for (i = 0; i < n_targets; i++)
    g_free (targets[i].target);
  g_free (targets);
  gtk_target_list_unref (list);
}

void gClipboard::setText(char *text, int len, char *format)
{
	if (!text)
		return;

	gt_clipboard_set_text(get_clipboard(), format, text, len);
}

char *gClipboard::getText(int *len, const char *format)
{
	GdkAtom target = GDK_NONE;
	gint n_tg;
	GdkAtom *targets;
	char *fmt;
	int i;
	GtkSelectionData *data;
	char *text;

	*len = 0;

	if (!gtk_clipboard_wait_for_targets(get_clipboard(), &targets, &n_tg) || n_tg <= 0)
		return NULL;
	
	if (format && !strcmp(format, "text/plain"))
		format = "text/plain;charset=utf-8";

	for (i = 0; i < n_tg; i++)
	{
		target = targets[i];
		fmt = convert_format(gt_free_later(gdk_atom_name(target)));
		if (!islower(fmt[0]))
			continue;
		if (!format && !strncasecmp(fmt, "text/", 5) && strcasecmp(fmt, "text/plain"))
			break;
		if (format && !strcasecmp(fmt, format))
			break;
	}

	if (i >= n_tg)
		return NULL;

	if (!gtk_clipboard_wait_is_target_available(get_clipboard(), target))
		return NULL;

	data = gtk_clipboard_wait_for_contents(get_clipboard(), target);
	*len = gtk_selection_data_get_length(data);
	text = (char *)g_malloc(*len);
	memcpy(text, gtk_selection_data_get_data(data), *len);

	gtk_selection_data_free(data);

	_clipboard_has_changed[_current_clipboard] = FALSE;

	return gt_free_later(text);
}

/***********************************************************************

	Drag & Drop

************************************************************************/

bool gDrag::_active = false;
gPicture *gDrag::_icon = NULL;
int gDrag::_icon_x = 0;
int gDrag::_icon_y = 0;
gControl *gDrag::_source = NULL;
gControl *gDrag::_destination = NULL;
gControl *gDrag::_dest = NULL;
int gDrag::_action = 0;
int gDrag::_type = 0;
gPicture *gDrag::_picture = NULL;
char *gDrag::_text = NULL;
int gDrag::_text_len = 0;
char *gDrag::_format = NULL;
int gDrag::_enabled = 0;
int gDrag::_x = -1;
int gDrag::_y = -1;
GdkDragContext *gDrag::_context = NULL;
guint32 gDrag::_time = 0;
bool gDrag::_local = false;
volatile bool gDrag::_got_data = false;
volatile bool gDrag::_end = false;
gControl *gDrag::_current = NULL;

void gDrag::setIcon(gPicture *vl)
{
	gPicture::assign(&_icon, vl);
}

void gDrag::cancel()
{
	#if DEBUG_ME
		fprintf(stderr, "gDrag::cancel\n");
	#endif

	hide();
	setIcon(NULL);
	setDropText(NULL);
	setDropImage(NULL);
	g_free(_format);
	_format = NULL;
	_source = NULL;
	_destination = NULL;
	_dest = NULL;
	_type = Nothing;
	_x = _y = -1;
	_time = 0;
	_got_data = false;
	_local = false;
	_active = false;

	gApplication::setButtonGrab(NULL);

#if DEBUG_ME
	fprintf(stderr, "_active -> false\n");
#endif
}

void gDrag::exit()
{
	cancel();
}

gControl *gDrag::drag(gControl *source, GtkTargetList *list)
{
	GdkDragContext *ct;
	gControl *dest;
	int button;

	#if DEBUG_ME
		fprintf(stderr, "gDrag::drag: source = %p list = %p event = %p\n", source, list, gApplication::lastEvent());
	#endif

	button = gMouse::left() ? 1 : gMouse::middle() ? 2 : gMouse::right() ? 3 : 0;

#if GTK_CHECK_VERSION(3, 10, 0)
	ct = gtk_drag_begin_with_coordinates(source->border, list, GDK_ACTION_MOVE, button, gApplication::lastEvent(), -1, -1);
#else
	ct = gtk_drag_begin(source->border, list, GDK_ACTION_MOVE, button, gApplication::lastEvent());
#endif
	if (!ct)
		return NULL;

	_local = true;
	_active = true;

#if DEBUG_ME
	fprintf(stderr, "gDrag::drag: begin\n");
#endif

	if (_icon)
	{
		GdkPixbuf *icon = _icon->getIconPixbuf();
		gtk_drag_set_icon_pixbuf(ct, icon, _icon_x, _icon_y);
		if (icon != _icon->getPixbuf())
			g_object_unref(G_OBJECT(icon));
	}
	
	source->_dragging = true;
	
	_end = false;
	while (!_end)
		MAIN_do_iteration(true);
	
	source->_dragging = false;
	
	gtk_target_list_unref(list);	
	
	dest = _destination;
	cancel();

#if DEBUG_ME
	fprintf(stderr, "gDrag::drag: end\n");
#endif

	return dest;
}

gControl *gDrag::dragText(gControl *source, char *text, char *format)
{
	GtkTargetList *list;

	//cancel();

	#if DEBUG_ME
		fprintf(stderr, "gDrag::dragText: %s\n", text);
	#endif

	setDropText(text);

	list = gtk_target_list_new (NULL, 0);
	if (format)
		gtk_target_list_add(list, gdk_atom_intern(format, false), 0, 0);
	gtk_target_list_add_text_targets(list, 0);

	//gtk_target_list_add (list,gdk_atom_intern("UTF8_STRING",false),0,0);
	//gtk_target_list_add (list,gdk_atom_intern("COMPOUND_TEXT",false),0,0);
	//gtk_target_list_add (list,gdk_atom_intern("TEXT",false),0,0);
	//gtk_target_list_add (list,GDK_TARGET_STRING,0,0);
	//gtk_target_list_add (list,gdk_atom_intern("text/plain;charset=utf-8",false),0,0);
	//gtk_target_list_add (list,gdk_atom_intern("text/plain",false),0,0);

	setDropInfo(Text, format);

	return drag(source, list);
}

gControl *gDrag::dragImage(gControl *source, gPicture *image)
{
	GtkTargetList *list;

	setDropImage(image);

	list = gtk_target_list_new (NULL,0);

	gtk_target_list_add(list, gdk_atom_intern("image/png", false), 0, 0);
	gtk_target_list_add(list, gdk_atom_intern("image/jpg", false), 0, 0);
	gtk_target_list_add(list, gdk_atom_intern("image/jpeg", false), 0, 0);
	gtk_target_list_add(list, gdk_atom_intern("image/gif", false), 0, 0);

	setDropInfo(Image, NULL);

	return drag(source, list);
}

void gDrag::setDropInfo(int type, char *format)
{
	_type = type;
	g_free(_format);
	_format = g_strdup(format);
}


void gDrag::setDropData(int action, int x, int y, gControl *source, gControl *dest)
{
	#if DEBUG_ME
		fprintf(stderr, "gDrag::setDropData: action = %d x = %d y = %d source = %p\n", action, x, y, source);
	#endif

	_x = x;
	_y = y;
	_action = action;
	_source = source;
	_destination = dest;
	_active = true;
}

void gDrag::setDropText(char *text, int len)
{
	#if DEBUG_ME
		fprintf(stderr, "gDrag::setDropText: text = '%s' %d\n", text ? text : "(null)", len);
	#endif

	g_free(_text);
	if (text)
	{
		if (len < 0) len = strlen(text);
		_text_len = len;
		_text = (char *)g_malloc(len);
		memcpy(_text, text, len);
	}
	else
	{
		_text = NULL;
		_text_len = 0;
	}
}

void gDrag::setDropImage(gPicture* image)
{
	//g_debug("gDrag::setDropImage: image = %p\n", image);
	gPicture::assign(&_picture, image);
}

void gDrag::setDropImage(char *buf, int len)
{
	GdkPixbufLoader *ld;
	GdkPixbuf *pixbuf = NULL;

	//g_debug("gDrag::setDropImage: buf = %p len = %d\n", buf, len);

	if (buf && len > 0)
	{
		ld = gdk_pixbuf_loader_new ();
		if (gdk_pixbuf_loader_write(ld, (const guchar*)buf, len, NULL))
		{
			gdk_pixbuf_loader_close (ld, NULL);
			pixbuf = gdk_pixbuf_loader_get_pixbuf(ld);
		}
		g_object_unref(G_OBJECT(ld));
	}

	if (pixbuf)
		setDropImage(new gPicture(pixbuf));
	else
		setDropImage(NULL);
}

bool gDrag::checkThreshold(gControl *control, int x, int y, int sx, int sy)
{
	if (_active)
		return false;
	else
		return gtk_drag_check_threshold(control->border, sx, sy, x, y);
}

GdkDragContext *gDrag::enable(GdkDragContext *context, gControl *control, guint32 time)
{
	GdkDragContext *old = _context;

	#if DEBUG_ME
		fprintf(stderr, "gDrag::enable: %p -> %p\n", old, context);
	#endif

	_enabled++;
	_context = context;
	_time = time;
	_dest = control;
	return old;
}

GdkDragContext *gDrag::disable(GdkDragContext *context)
{
	GdkDragContext *old = _context;

	#if DEBUG_ME
		fprintf(stderr, "gDrag::disable: %p -> %p\n", old, context);
	#endif

	_context = context;
	_enabled--;
	return old;
}

void gDrag::show(gControl *control, int x, int y, int w, int h)
{
	static GB_FUNCTION func;
	static bool init = FALSE;

	if (!init)
	{
		GB.GetFunction(&func, (void *)GB.FindClass("_Gui"), "_ShowDNDFrame", NULL, NULL);
		init = TRUE;
	}

	GB.Push(5, GB_T_OBJECT, control->hFree, GB_T_INTEGER, x, GB_T_INTEGER, y, GB_T_INTEGER, w, GB_T_INTEGER, h);
	GB.Call(&func, 5, FALSE);
}

void gDrag::hide(gControl *control)
{
	static GB_FUNCTION func;
	static bool init = FALSE;

	if (!init)
	{
		GB.GetFunction(&func, (void *)GB.FindClass("_Gui"), "_HideDNDFrame", NULL, NULL);
		init = TRUE;
	}

	GB.Push(1, GB_T_OBJECT, control ? control->hFree : NULL);
	GB.Call(&func, 1, FALSE);
}


char *gDrag::getFormat(int n)
{
	GList *tg;
	gchar *format, *cformat;

	//if (gDrag::getType()) // local DnD
	//	return;

	//g_debug("set_from_context: non local\n");

	if (_format)
		return n == 0 ? _format : NULL;

	if (!_context)
		return NULL;

	tg = g_list_first(gdk_drag_context_list_targets(_context));

	while (tg)
	{
		format = gdk_atom_name((GdkAtom)tg->data);
		cformat = convert_format(format);

		if (islower(cformat[0]))
		{
			if (n <= 0)
			{
				gt_free_later(format);
				return cformat;;
			}
			n--;
		}

		g_free(format);
		tg = g_list_next(tg);
	}

	return NULL;
}

int gDrag::getType()
{
	int i;
	char *format;

	if (_type)
		return _type;

	for (i = 0;; i++)
	{
		format = getFormat(i);
		if (!format)
			return Nothing;
		if (strlen(format) >= 5 && !strncasecmp(format, "text/", 5))
			return Text;
		if (strlen(format) >= 6 &&! strncasecmp(format, "image/", 6))
			return Image;
	}
}

static void cb_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *sel, guint info, guint time, gControl *data)
{
#if DEBUG_ME
	fprintf(stderr, "cb_drag_data_received\n");
#endif

	if (gDrag::getType() == gDrag::Text)
	{
		if (gtk_selection_data_get_length(sel) != -1)
			gDrag::setDropText((char*)gtk_selection_data_get_data(sel), gtk_selection_data_get_length(sel));
		else
			gDrag::setDropText(NULL);
	}

	if (gDrag::getType() == gDrag::Image)
	{
		//fprintf(stderr, "Image\n");
		if (gtk_selection_data_get_length(sel) != -1)
			gDrag::setDropImage((char*)gtk_selection_data_get_data(sel), gtk_selection_data_get_length(sel));
		else
			gDrag::setDropImage(NULL);
	}

	gDrag::_got_data = true;
}


bool gDrag::getData(const char *prefix)
{
	GList *tg;
	gchar *format = NULL;
	char *cfmt;
	gulong id;
	static bool norec = false;
	gControl *dest;

#if DEBUG_ME
	fprintf(stderr, "getData: norec = %d _local = %d\n", norec, _local);
#endif

	if (norec || _local) // local DnD
		return false;

	tg = g_list_first(gdk_drag_context_list_targets(_context));

	while (tg)
	{
		g_free(format);
		format = gdk_atom_name((GdkAtom)tg->data);
		cfmt = convert_format(format);
		//fprintf(stderr, "getData: prefix = %s format = '%s'\n", prefix, format);

		if (strlen(cfmt) >= strlen(prefix) && !strncasecmp(cfmt, prefix, strlen(prefix)))
		{
			g_free(format);

			dest = _dest;

			id = g_signal_connect(dest->border, "drag-data-received", G_CALLBACK(cb_drag_data_received), (gpointer)dest);
			//fprintf(stderr, "gDrag::getData: g_signal_connect -> %p %ld\n", dest->border, id);

			_got_data = false;

			norec = true;

			gtk_drag_get_data (_dest->border, _context, (GdkAtom)tg->data, _time);

			while (!_got_data)
				MAIN_do_iteration(true);

			norec = false;

			//fprintf(stderr, "gDrag::getData: g_signal_disconnect -> %p %ld\n", dest->border, id);
			g_signal_handler_disconnect(dest->border, id);

			return false;
		}

		tg = g_list_next(tg);
	}

	g_free(format);
	return true;
}

char *gDrag::getText(int *len, const char *format, bool fromOutside)
{
	//setDropText(NULL);

	if (!format)
		format = "text/";

	if (!fromOutside && getData(format))
	{
		*len = 0;
		return NULL;
	}
	else
	{
		*len = _text_len;
		return _text;
	}
}

gPicture *gDrag::getImage(bool fromOutside)
{
	if (_picture)
		return _picture;

	if (!fromOutside && getData("image/"))
		return NULL;

	return _picture;
}

void gDrag::end()
{
	_end = true;
	_active = false;
}


bool gDrag::setCurrent(gControl *control)
{
	gControl *current;
	
	//fprintf(stderr, "gDrag::setCurrent: %s -> %s\n", _current ? _current->name() : "NULL", control ? control->name() : "NULL");

	if (_current == control)
		return false;
	
	if (_current)
	{
		current = _current;
		while (current)
		{
			CB_control_drag_leave(current);
			current = current->_proxy_for;
		}
	}
	
	_current = control;
	
	while (control)
	{
		if (CB_control_drag(control))
			return true;
		control = control->_proxy_for;
	}
	
	return false;
}
