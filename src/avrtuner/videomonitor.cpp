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
#include "videomonitor.h"

VideoMonitor::VideoMonitor()
{
	signal_update_meters = NULL;
	capture_thread       = NULL;
	config   = NULL;
	recorder = NULL;
	signal   = SIG_QUIT;
	stop_monitor = true;

	in_picture = NULL;
	buffer  = NULL;
	screen  = NULL;
	overlay = NULL;
}


VideoMonitor::~VideoMonitor()
{
	stop();
	stop_capture();
	cleanup();
}

bool VideoMonitor::Init(Config *_config, Glib::Dispatcher *signal, bool with_motion)
{
	///if already inited, return
	if(!stop_monitor) return true;

	if(!_config) return false;
	config = _config;

	if(!signal) return false;
	signal_update_meters = signal;

	highlight_motion = with_motion;

	recorder = new BaseRecorder<Glib::Mutex>(true);
	if(!recorder->Init(config))
	{
			::log_message(1, "CaptureThread: Init: unable to initialize recorder");
			cleanup();
			return false;
	}

	uint size = recorder->getVBSize();
	width     = recorder->getWidth();
	height    = recorder->getHeight();
	in_fmt    = av_pixel_formats.find(recorder->getPixelFormat())->second;
	buffer    = new unsigned char[size];

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER))
	{
		log_message(1, "MonitorThread: Could not initialize SDL - %s", SDL_GetError());
		cleanup();
		return false;
	}
	else if(!(screen = SDL_SetVideoMode(width, height, 0, 0)))
	{
		log_message(1, "MonitorThread: SDL: could not set video mode");
		cleanup();
		return false;
	}
	else
	{
		SDL_WM_SetCaption("Video output", "Video output");

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
		{
			log_message(1, "MonitorThread: couldn't get SwsContext for pixel format conversion");
			cleanup();
			return false;
		}

		// allocate input frame
		in_picture = avcodec_alloc_frame();
		if(!in_picture)
		{
			log_message(1, "MonitorThread: avcodec_alloc_frame - could not allocate frame for in_picture");
			cleanup();
			return false;
		}
	}
	stop_monitor = false;
	return true;
}

void VideoMonitor::run()
{
	//if not inited or already running, return
	if(!recorder || stop_monitor || capture_thread) return;

	///start capture_thread
	capture_thread = Glib::Thread::create(sigc::mem_fun(*this, &VideoMonitor::capture), true);

	///monitor thread itself
	Glib::Dispatcher &emit_update_meters = *signal_update_meters;
	const unsigned char *tmp = NULL;
	while(true)
	{
		///check if we are to stop
		Glib::Mutex::Lock lock(mutex); ///< lock monitor
		if(stop_monitor) break;

		recorder->lock(); ///< lock recorder to get a picture and it's info
		///get info
		motion = recorder->getMotion();
		peak   = recorder->getPeak();
		lock.release(); ///< unlock monitor

		///get picture
		if(highlight_motion) tmp = recorder->getMBuffer();
		else tmp = recorder->getVBuffer();
		memcpy(buffer, tmp, recorder->getVBSize());
		recorder->unlock(); ///< unlock recorder

		///display grabbed image
		if(screen && overlay && sws)
		{
			SDL_LockYUVOverlay(overlay);

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
		emit_update_meters();
		usleep(recorder->getFrameInterval()*1000000);
	}

	stop_capture();
	cleanup();
}

void VideoMonitor::capture()
{
	if(!recorder) return;

	signal = SIG_RECORDING;
	recorder->RecordLoop(&signal);
	recorder->Close();
}

void VideoMonitor::stop_capture()
{
	if(!capture_thread) return;

	///stop CaptureThread
	recorder->lock();
	signal = SIG_QUIT;
	recorder->unlock();
	if(capture_thread->joinable())
		capture_thread->join();

	capture_thread = NULL;
}

void VideoMonitor::cleanup()
{
	///stop capture thread if it's still running
	if(capture_thread) stop_capture();

	///delete recorder
	if(recorder) delete recorder;
	recorder = NULL;

	///delete buffer
	if(buffer)   delete[] buffer;
	buffer   = NULL;

	///free input frame
	if(in_picture)
		av_free(in_picture);

	///clear SDL stuff
	SDL_Quit();
	screen  = NULL;
	overlay = NULL;

	///signals
	signal  = SIG_QUIT;
	stop_monitor = true;
}
