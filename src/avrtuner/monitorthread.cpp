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

#include <avconfig.h>
#include "monitorthread.h"

void CaptureThread::Init( QString cfg )
{
	if(recorder)
	{
		delete recorder;
		recorder = NULL;
	}

	AVConfig conf;
	if(!conf.Load(cfg)) return;

	recorder = new BaseRecorder<QMutex>();
	if(!recorder->Init(conf.getConfig()))
	{
		::log_message(1, "CaptureThread: Init: unable to initialize recorder");
		delete recorder;
		recorder = NULL;
	}
}


void CaptureThread::initImageBuffer( unsigned char *&buffer, uint &size, uint &w, uint &h, PixelFormat &in_fmt) const
{
	if(!recorder)	return;
	size   = recorder->getVBSize();
	w      = recorder->getWidth();
	h      = recorder->getHeight();
	in_fmt = av_pixel_formats[recorder->getPixelFormat()];
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





MonitorThread::MonitorThread()
{
	basket  = NULL;
	buffer  = NULL;
	screen  = NULL;
	overlay = NULL;
}

MonitorThread::~ MonitorThread()
{
	catcher.stop();
	if(buffer) delete[] buffer;
	SDL_Quit();
}

void MonitorThread::Init( QObject * bask, QString cfg, bool with_motion )
{
	basket = bask;
	catcher.Init(cfg);
	catcher.initImageBuffer(buffer, v_bsize, width, height, in_fmt);
	highlight_motion = with_motion;

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER))
		log_message(1, "MonitorThread: Could not initialize SDL - %s", SDL_GetError());
	else if(!(screen = SDL_SetVideoMode(width, height, 0, 0)))
		log_message(1, "MonitorThread: SDL: could not set video mode");
	else
	{
		screen_rect.x = 0;
		screen_rect.y = 0;
		screen_rect.w = width;
		screen_rect.h = height;

		overlay = SDL_CreateYUVOverlay(width, height, SDL_YV12_OVERLAY, screen);

		overlay_frame.data[0] = overlay->pixels[0];
		overlay_frame.data[1] = overlay->pixels[2]; //note switch between Y and U planes
		overlay_frame.data[2] = overlay->pixels[1];

		overlay_frame.linesize[0] = overlay->pitches[0];
		overlay_frame.linesize[1] = overlay->pitches[2]; //note switch between Y and U planes
		overlay_frame.linesize[2] = overlay->pitches[1];

		sws = sws_getContext(width, height, in_fmt,
						 width, height, PIX_FMT_YUV420P,
						 SWS_POINT, NULL, NULL, NULL);
		if(!sws)
			log_message(1, "MonitorThread: couldn't get SwsContext for pixel format conversion");
	}
}

void MonitorThread::run( )
{
	if(!catcher.inited())	return;

	catcher.start();
	while(basket)
	{
		catcher.lock();
		motion = catcher.getMotion();
		peak   = catcher.getPeak();
		catcher.getImage(buffer, highlight_motion);
		catcher.unlock();

		if(screen && overlay && sws)
		{
			SDL_LockYUVOverlay(overlay);

			// allocate input frame
			AVFrame *in_picture = avcodec_alloc_frame();
			if(!in_picture)
			{
				log_message(1, "MonitorThread: avcodec_alloc_frame - could not allocate frame for in_picture");
				continue;
			}

			//fill in the picture
			avpicture_fill((AVPicture*)in_picture, buffer, in_fmt,
							width, height);

			//Convert the image into YUV format that SDL uses
			sws_scale(sws, in_picture->data, in_picture->linesize, 0,
					  height, overlay_frame.data, overlay_frame.linesize);

			SDL_UnlockYUVOverlay(overlay);

			//display the overlay
			SDL_DisplayYUVOverlay(overlay, &screen_rect);

			//pool events (or they'll pile up and freez the app O_O)
			while(SDL_PollEvent(&event));
		}

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

	SDL_Quit();
}

void MonitorThread::updateTuner()
{
	if(!basket || !buffer) return;

	QCustomEvent *event = new QCustomEvent(DATA_EVENT_TYPE);
	TunerData    *data  = new TunerData;

	data->motion = motion;
	data->peak   = peak;

	event->setData((void*)data);
	qApp->postEvent(basket, event);
}
