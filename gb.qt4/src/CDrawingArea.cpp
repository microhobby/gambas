/***************************************************************************

  CDrawingArea.cpp

  (c) 2000-2009 Benoît Minisini <gambas@users.sourceforge.net>

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

#define __CDRAWINGAREA_CPP

#include <QApplication>
#include <QPaintEvent>
#include <QPixmap>
#include <QPainter>

#include "CDraw.h"
#include "cpaint_impl.h"
#include "CDrawingArea.h"

#ifndef NO_X_WINDOW
#include <QX11Info>
#include <X11/Xlib.h>
#endif

DECLARE_EVENT(EVENT_draw);


/***************************************************************************

	class MyDrawingArea

***************************************************************************/

MyDrawingArea::MyDrawingArea(QWidget *parent) : MyContainer(parent)
{
	drawn = 0;
	cache = 0;
	_background = 0;
	_frozen = false;
	_event_mask = 0;
	_use_paint = false;
	_set_background = false;
	_cached = false;
	_transparent = false;
	
	setMerge(false);
	setCached(false);
	setAllowFocus(false);
	
	setAttribute(Qt::WA_KeyCompression, false);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_DontCreateNativeAncestors, true);
	
	//setAttribute(Qt::WA_NoSystemBackground, true);
	
	setTransparent(false);
}


MyDrawingArea::~MyDrawingArea()
{
	if (_background)
	{
		delete _background;
		_background = 0;
	}
}

void MyDrawingArea::setAllowFocus(bool f)
{
	if (f)
	{
		setFocusPolicy(Qt::WheelFocus);
		setAttribute(Qt::WA_InputMethodEnabled, true);
	}
	else
	{
		setFocusPolicy(Qt::NoFocus);
	}
}


void MyDrawingArea::setMerge(bool m)
{
	_merge = m;
	/*if (_merge)
		clearWFlags(Qt::WPaintClever);
	else
		setWFlags(Qt::WPaintClever);*/
}

void MyDrawingArea::setFrozen(bool f)
{
	if (f == _frozen)
		return;

	#ifndef NO_X_WINDOW
	XWindowAttributes attr;

	if (f)
	{
		//setBackgroundMode(Qt::NoBackground);
		XGetWindowAttributes(QX11Info::display(), winId(), &attr);
		_event_mask = attr.your_event_mask;
		XSelectInput(QX11Info::display(), winId(), ExposureMask);
		//clearWFlags(Qt::WPaintClever);
		//qDebug("frozen");
	}
	else
	{
		//setBackgroundMode(Qt::PaletteBackground);
		XSelectInput(QX11Info::display(), winId(), _event_mask);
		setMerge(_merge);
		//qDebug("unfrozen");
	}
	#endif
	
	_frozen = f;
}


void MyDrawingArea::paintEvent(QPaintEvent *event)
{
	if (_background)
	{
		if (_set_background)
		{
			XSetWindowBackgroundPixmap(QX11Info::display(), winId(), _background->handle());
			_set_background = false;
		}
		QPainter paint( this );
		drawFrame(&paint);
		//MyContainer::paintEvent(event);
	}
	else
	{
		QPainter paint( this );
		QRect r;

		r = event->rect().intersect(rect());
		if (r.isValid())
		{
			QPainter *p;
			void *_object = CWidget::getReal(this);
			
			if (!_object)
				return;
			
			if (!isTransparent())
			{
				cache = new QPixmap(r.width(), r.height());
				cache->fill(this, r.x(), r.y());
			}

			//qDebug("paint: %d %d %d %d", event->rect().x(), event->rect().y(), event->rect().width(), event->rect().height());

			if (_use_paint)
			{
				PAINT_begin(THIS);
				p = PAINT_get_current();
			}
			else
			{
				DRAW_begin(THIS);
				p = DRAW_get_current();
			}
				
			if (!isTransparent())
			{
				p->translate(-r.x(), -r.y());
			}
			
			if (!_use_paint)
			{
			  p->setBrushOrigin(-r.x(), -r.y());
				DRAW_clip(r.x(), r.y(), r.width(), r.height());
			}
			else
				PAINT_clip(r.x(), r.y(), r.width(), r.height());
			
			//p->setClipRegion(event->region().intersect(contentsRect()));
			//p->setBrushOrigin(-r.x(), -r.y());
			
			p->save();
			
			GB.Raise(THIS, EVENT_draw, 0);
				
			p->restore();
			p->setRenderHint(QPainter::Antialiasing, false);
			drawFrame(p);
				
			if (_use_paint)
				PAINT_end();
			else
				DRAW_end();
			
			if (!isTransparent())
			{
				paint.drawPixmap(r.x(), r.y(), *cache);
				delete cache;
				cache = 0;
			}
		}
	}
}

void MyDrawingArea::setBackground()
{
	if (_background)
	{
		_background->detach();

		#ifdef NO_X_WINDOW
		setErasePixmap(*_background);
		#else
		//if (isVisible())
		//	XSetWindowBackgroundPixmap(QX11Info::display(), winId(), _background->handle());
		//else
		_set_background = true;
		refreshBackground();
		#endif
	}
}

void MyDrawingArea::refreshBackground()
{
	if (_background)
	{
		#ifdef NO_X_WINDOW
		update();
		#else
		int fw = frameWidth();
		if (fw == 0)
			XClearWindow(QX11Info::display(), winId());
		else
		{
			XClearArea(QX11Info::display(), winId(), fw, fw, 0, 0, False);
			repaint();
		}
		#endif
	}
}


void MyDrawingArea::clearBackground()
{
	if (_background)
	{
		QPainter p(_background);
		p.fillRect(0, 0, _background->width(), _background->height(), palette().color(backgroundRole()));
		p.end();
		setBackground();
	}
}

void MyDrawingArea::resizeEvent(QResizeEvent *e)
{
	MyContainer::resizeEvent(e);
	updateBackground();
}

void MyDrawingArea::updateBackground()
{
	int wb, hb, w, h;

	if (drawn)
	{
		GB.Error("DrawingArea is being drawn");
		return;
	}

	if (_background)
	{
		w = QMAX(width(), 1);
		h = QMAX(height(), 1);

		if (w != _background->width() && h != _background->height())
		{		
			QPixmap *p = new QPixmap(w, h);
			p->fill(palette().color(backgroundRole()));

			wb = QMIN(w, _background->width());
			hb = QMIN(h, _background->height());

			QPainter pt(p);
			pt.drawPixmap(0, 0, *_background, 0, 0, wb, hb);
			pt.end();

			delete _background;
			_background = p;
			
			setBackground();
		}
	}
}

void MyDrawingArea::updateCache()
{
	if (_background)
		delete _background;

	if (_cached && !_transparent)
	{
		_background = new QPixmap(width(), height());
		clearBackground();

		setAttribute(Qt::WA_PaintOnScreen, true);
		setAttribute(Qt::WA_OpaquePaintEvent, true);
		setAttribute(Qt::WA_StaticContents, true);
	}
	else //if (_background)
	{
		_background = 0;
		setAttribute(Qt::WA_PaintOnScreen, false);
		setAttribute(Qt::WA_OpaquePaintEvent, false);
		setAttribute(Qt::WA_StaticContents, false);
		#ifdef NO_X_WINDOW
		setBackgroundMode(Qt::NoBackground);
		#else
		XSetWindowBackgroundPixmap(QX11Info::display(), winId(), None);
		XClearArea(QX11Info::display(), winId(), 0, 0, 0, 0, True);
		#endif
	}
}

void MyDrawingArea::setCached(bool c)
{
	_cached = c;
	updateCache();
}

void MyDrawingArea::setPalette(const QPalette &pal)
{
	MyContainer::setPalette(pal);
	repaint();
}

void MyDrawingArea::setTransparent(bool on)
{
	_transparent = on;
	updateCache();
}


/***************************************************************************

	DrawingArea

***************************************************************************/

BEGIN_METHOD(CDRAWINGAREA_new, GB_OBJECT parent)

	MyDrawingArea *wid = new MyDrawingArea(QCONTAINER(VARG(parent)));

	//THIS->widget.background = QColorGroup::Base;
	THIS->container = wid;
	THIS->widget.flag.fillBackground = false;

	CWIDGET_new(wid, (void *)_object);

END_METHOD


BEGIN_PROPERTY(CDRAWINGAREA_cached)

	if (READ_PROPERTY)
		GB.ReturnBoolean(WIDGET->isCached());
	else
		WIDGET->setCached(VPROP(GB_BOOLEAN));

END_PROPERTY


DECLARE_METHOD(CCONTROL_background);


BEGIN_METHOD_VOID(CDRAWINGAREA_clear)

	WIDGET->clearBackground();

END_METHOD


BEGIN_PROPERTY(CDRAWINGAREA_background)

	CCONTROL_background(_object, _param);

	if (!READ_PROPERTY)
		WIDGET->clearBackground();

END_PROPERTY

BEGIN_PROPERTY(CDRAWINGAREA_border)

	CCONTAINER_border(_object, _param);

	if (!READ_PROPERTY)
		WIDGET->clearBackground();

END_PROPERTY

BEGIN_PROPERTY(CDRAWINGAREA_enabled)

	CCONTROL_enabled(_object, _param);

	if (!READ_PROPERTY)
		WIDGET->setFrozen(!VPROP(GB_BOOLEAN));

END_PROPERTY

BEGIN_PROPERTY(CDRAWINGAREA_merge)

	if (READ_PROPERTY)
		GB.ReturnBoolean(WIDGET->isMerge());
	else
		WIDGET->setMerge(VPROP(GB_BOOLEAN));

END_PROPERTY

BEGIN_PROPERTY(CDRAWINGAREA_focus)

	if (READ_PROPERTY)
		GB.ReturnBoolean(WIDGET->isAllowFocus());
	else
		WIDGET->setAllowFocus(VPROP(GB_BOOLEAN));

END_PROPERTY

BEGIN_PROPERTY(CDRAWINGAREA_painted)

	if (READ_PROPERTY)
		GB.ReturnBoolean(WIDGET->isPaint());
	else
		WIDGET->setPaint(VPROP(GB_BOOLEAN));

END_PROPERTY

BEGIN_PROPERTY(CDRAWINGAREA_transparent)

	if (READ_PROPERTY)
		GB.ReturnBoolean(WIDGET->isTransparent());
	else
	{
		WIDGET->setTransparent(VPROP(GB_BOOLEAN));
		//THIS->widget.flag.fillBackground = !WIDGET->isTransparent();
		//CWIDGET_reset_color((CWIDGET *)THIS);
	}

END_PROPERTY

GB_DESC CDrawingAreaDesc[] =
{
	GB_DECLARE("DrawingArea", sizeof(CDRAWINGAREA)), GB_INHERITS("Container"),

	GB_METHOD("_new", NULL, CDRAWINGAREA_new, "(Parent)Container;"),

	GB_PROPERTY("Cached", "b", CDRAWINGAREA_cached),
	GB_PROPERTY("Merge", "b", CDRAWINGAREA_merge),
	
	GB_PROPERTY("Border", "i", CDRAWINGAREA_border),
	GB_PROPERTY("Background", "i", CDRAWINGAREA_background),
	
	GB_PROPERTY("Focus", "b", CDRAWINGAREA_focus),
	GB_PROPERTY("Enabled", "b", CDRAWINGAREA_enabled),
	GB_PROPERTY("Painted", "b", CDRAWINGAREA_painted),
	GB_PROPERTY("Transparent", "b", CDRAWINGAREA_transparent),

	GB_METHOD("Clear", NULL, CDRAWINGAREA_clear, NULL),

	GB_EVENT("Draw", NULL, NULL, &EVENT_draw),

	GB_INTERFACE("Draw", &DRAW_Interface),
	GB_INTERFACE("Paint", &PAINT_Interface),

	DRAWINGAREA_DESCRIPTION,

	GB_END_DECLARE
};




