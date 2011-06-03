/***************************************************************************

  cwebview.h

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
	MA 02110-1301, USA.

***************************************************************************/

#ifndef __CWEBVIEW_H
#define __CWEBVIEW_H

#include <QUrl>
#include <QAuthenticator>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebFrame>
#include <QWebView>

#include "cwebdownload.h"
#include "main.h"

#ifndef __CWEBVIEW_CPP

extern GB_DESC CWebViewAuthDesc[];
extern GB_DESC CWebViewDesc[];
extern GB_DESC CWebViewDownloadsDesc[];

#else

#define THIS      ((CWEBVIEW *)_object)
#define WIDGET    ((MyWebView *)((QT_WIDGET *)_object)->widget)

#endif

class MyWebView : public QWebView
{
	Q_OBJECT

public:
	
	MyWebView(QWidget *parent);

protected:
		
	virtual QWebView *createWindow(QWebPage::WebWindowType type);
};

typedef
	struct 
	{
		QT_WIDGET widget;
		void *new_view;
		double progress;
		char *status;
		QT_PICTURE icon;
		QNetworkReply *reply;
		QAuthenticator *authenticator;
		bool cached;
	}
	CWEBVIEW;

class CWebView : public QObject
{
  Q_OBJECT

public:

  static CWebView manager;

public slots:

	void iconChanged();
	//void linkClicked(const QUrl &url);
	void loadFinished(bool ok);
	void loadProgress(int progress);
	void loadStarted();
	void selectionChanged();
	void statusBarMessage(const QString &text);
	void titleChanged(const QString &title);
	void linkHovered(const QString &link, const QString &title, const QString &textContent);
	//void downloadRequested(const QNetworkRequest &);
	void frameCreated(QWebFrame *);
	void authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
	void urlChanged(const QUrl &);
	void downloadRequested(const QNetworkRequest &);
	void handleUnsupportedContent(QNetworkReply*);
};

#endif
