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


#include <klocale.h>

#include <qevent.h>
#include <qapp.h>
extern QApplication* qApp;

#include "monitorthread.h"

void CaptureThread::Init( QString cfg )
{
	if(recorder) delete recorder;

	recorder = new BaseRecorder<QMutex>();
	if(!recorder->Init(cfg))
	{
		::log_message(1, "CaptureThread: Init: unable to initialize recorder");
		delete recorder;
		recorder = NULL;
	}
}


void CaptureThread::initImageBuffer( unsigned char *&buffer, uint &size, uint &w, uint &h ) const
{
	if(!recorder)	return;
	size   = recorder->getVBSize();
	w      = recorder->getWidth();
	h      = recorder->getHeight();
	buffer = new unsigned char[size];
}

void CaptureThread::getImage( unsigned char *buffer, bool with_motion ) const
{
	if(!recorder || !buffer) return;
	const unsigned char *tmp;
	if(with_motion) tmp = recorder->getMBuffer();
	else tmp = recorder->getVBuffer();
	memcpy(buffer, tmp, recorder->getVBSize());
}

void CaptureThread::run( )
{
	if(!recorder) return;

	signal = SIG_RECORDING;
	recorder->IdleLoop(&signal);

	recorder->Close();
 	delete recorder;
 	recorder = NULL;
}

void CaptureThread::stop( )
{
	if(!recorder)
	{
		terminate();
		wait();
		return;
	}
	signal = SIG_QUIT;
	wait();
}




void MonitorThread::Init( QObject * bask, QString cfg, bool with_motion )
{
	basket = bask;
	catcher.Init(cfg);
	catcher.initImageBuffer(buffer, v_bsize, width, height);
	highlight_motion = with_motion;
}

void MonitorThread::run( )
{
	if(!catcher.inited())	return;

	catcher.start();
	while(basket)
	{
		catcher.lock();
		diffs = catcher.getDiffs();
		noise = catcher.getNoise();
		catcher.getImage(buffer, highlight_motion);
		catcher.unlock();

		updateTuner();
		msleep(50);
	}
}

void MonitorThread::stop( )
{
	basket = NULL;
	wait();
	catcher.stop();

	if(buffer)
		delete[] buffer;
	buffer  = NULL;
	v_bsize = 0;
}

void MonitorThread::updateTuner()
{
	if(!basket || !buffer) return;

	image.create(width, height, 32, QImage::IgnoreEndian);
	unsigned char *y, *u, *v;
	int c,d,e;
	QRgb *p;

	y = buffer;
	u = y + width*height;
	v = u + width*height/4;

	for(int yi=0; yi<height; yi++)
	{
		for(int x=0; x<width; x++)
		{
			c = (y[x])-16;
			d = (u[x>>1])-128;
			e = (v[x>>1])-128;

			int r = (298 * c           + 409 * e + 128)>>8;
			int g = (298 * c - 100 * d - 208 * e + 128)>>8;
			int b = (298 * c + 516 * d           + 128)>>8;

			if (r<0) r=0;   if (r>255) r=255;
			if (g<0) g=0;   if (g>255) g=255;
			if (b<0) b=0;   if (b>255) b=255;

			p  = (QRgb*)image.scanLine(yi)+x;
			*p = qRgba(r,g,b,255);
		}

		y += width;
		if(yi & 1)
		{
			u += width/2;
			v += width/2;
		}
	}

	QCustomEvent *event = new QCustomEvent(DATA_EVENT_TYPE);
	TunerData    *data  = new TunerData;

	data->diffs = diffs;
	data->noise = noise;
	data->image = &image;

	event->setData((void*)data);
	qApp->postEvent(basket, event);
}
