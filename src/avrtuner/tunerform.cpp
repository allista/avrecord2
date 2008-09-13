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

#include <qcheckbox.h>
#include <qtextedit.h>
#include <qlineedit.h>
#include <qwidgetstack.h>
#include <qpushbutton.h>
#include <qmessagebox.h>
#include <qprogressbar.h>
#include <qlcdnumber.h>
#include <qapp.h>
extern QApplication* qApp;

#include <fstream>
#include <iostream>
#include <string>
#include <stdarg.h>
using namespace std;

#include "tunerform.h"
#include "kdetvview.h"

#include <common.h>


/* ---------------------------------------------------------------------- */
#include <errno.h>

void TunerForm::xperror(const char *msg)
{
	char text[256];

	sprintf(text,"%s: %s", msg, strerror(errno));
	QMessageBox::warning(this, "Error", text);
}

/* ---------------------------------------------------------------------- */


TunerForm::TunerForm(QWidget* parent, const char* name, WFlags fl)
		: TunerFormBase(parent,name,fl)
{
	config_modified = false;
	fdialog = new QFileDialog(NULL,"*.conf",NULL,"fdialog",TRUE);
	this->VideoScreen->setPaletteBackgroundColor( QColor( 0, 0, 0 ) );
	this->VideoScreen->setFixedAspectRatio( ASPECT_RATIO_NORMAL );
	this->VideoScreen->hide();
	this->ClearLogBtn->hide();
}

TunerForm::~TunerForm()
{
	monitor.stop();
	delete fdialog;
}


void TunerForm::showLog( int state )
{
	if(state)	this->MainStack->raiseWidget(1);
	else      this->MainStack->raiseWidget(0);
}

void TunerForm::clearLog( )
{
	this->ErrorLog->clear();
}

void TunerForm::log_message(QString &str)
{
	this->ErrorLog->append(str);
}


void TunerForm::startStopCapturing(int state)
{
	if(!config_file)
		return this->StartStopBtn->setOn(false);

	if(state)
	{
		fileSave();
		monitor.Init(this, config_file, this->WithMotionBox->isOn());
		monitor.start();
		if(!this->ShowLogBtn->isOn())
		{
			this->MainStack->raiseWidget(1);
			this->ClearLogBtn->show();
		}
		this->VideoScreen->show();
	}
	else
	{
		monitor.stop();
		if(!this->ShowLogBtn->isOn())
		{
			this->MainStack->raiseWidget(0);
			this->ClearLogBtn->hide();
		}
		updateMeters(0, 0);
		ErrorLog->append("\n");
		this->VideoScreen->hide();
	}
}

void TunerForm::helpAbout()
{}

void TunerForm::editFind()
{}

void TunerForm::closeEvent(QCloseEvent *e)
{
	monitor.stop();

	if(!config_modified) { e->accept(); return; }

	switch(QMessageBox::question(this, "Save configuration?",
				 "Configuration file was modified.\nSave it?",
				 QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel))
	{
		case QMessageBox::Yes:
			fileSave();
			if(!config_modified)
				e->accept();
			break;

		case QMessageBox::No:
			e->accept();
			break;

		case QMessageBox::Cancel:
			break;
	}
	return;
}


void TunerForm::fileSaveAs()
{
	if(!(config_file = fdialog->getSaveFileName(NULL,"*.conf",NULL,"fdialog")))
		return;

	config_modified = true;
	fileSave();
}

void TunerForm::fileSave()
{
	if(!config_modified) return;
	if(!config_file) return fileSaveAs();

	ofstream cfg;
	cfg.open(config_file, ios_base::out);
	if(!cfg.is_open())
	{
		xperror("Unable to open the file for writing");
		return;
	}

	QString text = this->ConfigEditor->text();
	cfg.write((const char*)text, text.length());

	config_changed(false);

	cfg.close();
}

void TunerForm::fileOpen(const char* fname)
{
	if(fname) config_file = fname;
	else
	{
		QString old_config = config_file;
		if(!(config_file = fdialog->getOpenFileName(NULL,"*.conf",NULL,"fdialog")))
		{
			config_file = old_config;
			return;
		}
	}

	fileReload();
}

void TunerForm::fileReload()
{
	if(!config_file) return;

	ifstream cfg;
	cfg.open(config_file, ios_base::in);
	if(!cfg.is_open())
	{
		config_file.truncate(0);
		xperror("Unable to open the file");
		return;
	}
	cfg.seekg(0);

	QString text;
	char    line[1024];
	while(cfg)
	{
		cfg.getline((char*)line, 1024);
		text += line;
		if(cfg) text += '\n';
	}
  this->ConfigEditor->setText(text);
	config_changed(false);

	cfg.close();
}

void TunerForm::restore(int& state)
{
	setWindowState(windowState() & ~WindowMinimized | WindowActive);
}

void TunerForm::configChanged()
{ config_changed(true); }

void TunerForm::config_changed(bool modified)
{
	config_modified = modified;
	QString fline   = (config_file)? config_file : "buffer";

	if(modified)
		this->FileNameLine->setText(fline + " [modified]");
	else this->FileNameLine->setText(fline);

	QFont fnt = this->FileNameLine->font();
	fnt.setBold(modified);
  this->FileNameLine->setFont(fnt);
}

void TunerForm::updateMeters(uint diffs, uint noise)
{
	this->Diffs->display(int(diffs));
	this->Noise->display(int(noise));

	diffs = (diffs > DiffsPrg->totalSteps())? DiffsPrg->totalSteps() : diffs;
	noise = (noise > NoisePrg->totalSteps())? NoisePrg->totalSteps() : noise;
	this->DiffsPrg->setProgress(diffs);
	this->NoisePrg->setProgress(noise);
}

void TunerForm::updateImage(QImage *img)
{
	if(!img) return;
	if(VideoScreen->width() != img->width())
	 {
		 this->VideoScreen->setMinimumSize(img->size());
		 this->VideoScreen->setMaximumHeight(img->height());
		 this->MainStack->update();
	 }

	bitBlt(this->VideoScreen, 0, 0, img, 0, Qt::CopyROP);
}

void TunerForm::customEvent( QCustomEvent * e )
{
	MonitorThread::TunerData *data;
	switch(e->type())
	{
		case MonitorThread::DATA_EVENT_TYPE:
			data = (MonitorThread::TunerData*)e->data();
			qApp->processEvents(200);
			updateMeters(data->diffs, data->noise);
			updateImage(data->image);
			delete data;
			break;

		default: break;
	}
}

#include "tunerform.moc"
