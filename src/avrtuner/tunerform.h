/***************************************************************************
 *   Copyright (C) 2007 by Allis Tauri   *
 *   allista@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef TUNERFORM_H
#define TUNERFORM_H

#include <qstring.h>
#include <qfiledialog.h>
#include <qimage.h>

#include "tunerformbase.h"
#include "monitorthread.h"


///This is a QMainWindow child class.
///It implements form widget that is automaticaly generated
///from QtDesigner .ui file
class TunerForm : public TunerFormBase
{
	Q_OBJECT
public:
	TunerForm(QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
	~TunerForm();

public slots:
	virtual void restore(int& state);           ///< restores wiget window, if it is minimized
	virtual void configChanged();               ///< highlights filename line
	virtual void startStopCapturing(int state); ///< on start makes visible screen and errorlog wigets
	                                            ///< on stop makes visible config editor
	virtual void showLog(int state);            ///< shows and hides error log wiget
  virtual void clearLog();                    ///< clears content of error log wiget
	virtual void fileReload();                  ///< reloads current config file
	virtual void helpAbout();                   ///< shows about info
	virtual void editFind();                    ///< hmm... does nothig for now =)
	virtual void fileSaveAs();                  ///< saves config as different file
	virtual void fileSave();                    ///< saves config
	virtual void fileOpen() { fileOpen(NULL); };///< opens config
	virtual void fileOpen(const char* fname);   ///< opens config
  virtual void log_message(QString &str);     ///< appends string to errorlog line
	virtual void updateMeters(uint diffs, uint noise); ///< updates metters values
	virtual void updateImage(QImage *img);      ///< updates image

protected:
	QFileDialog  *fdialog; ///< dialog for fileselection
	QString   config_file; ///< configuration file name
	bool  config_modified; ///< true if config was modified

	MonitorThread monitor; ///< monitoring thread

	void config_changed(bool modified); ///< updates filename line
	void xperror(const char *msg);      ///< displays error dialog

	virtual void closeEvent(QCloseEvent * e);   ///< promts on unsaved config and closes main window
	virtual void customEvent(QCustomEvent * e); ///< meters values and image are updated through event queue
};

#endif

