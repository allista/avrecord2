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

#ifndef VIDEOMONITOR_H
#define VIDEOMONITOR_H

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <libswscale/swscale.h>
}
#include <SDL/SDL.h>
#include <gtkmm.h>

#include <libconfig.h++>
#include <recorder.h>

/**
	@author Allis Tauri <allista@gmail.com>
*/
class VideoMonitor
{
public:
    VideoMonitor();
    ~VideoMonitor();

	///initialize recorder and SDL stuff
	bool Init(Config *_config, Glib::Dispatcher *signal, bool with_motion = false);

	///function which should be run in a thread and in which Monitor loop runs
	void run();

	///make the run function to return
	void stop()
	{ Glib::Mutex::Lock lock(mutex); stop_monitor = true; };

	///lock Monitor mutex
	void lock() { mutex.lock(); }
	///unlock Monitor mutex
	void unlock() { mutex.unlock(); }

	///motion and noise info
	uint getMotion() const { return motion; }
	uint getMotionMax() const { return (recorder)? recorder->getMotionMax() : 1; }
	uint getPeak() const { return peak; }
	uint getPeakMax() const { return (recorder)? recorder->getPeakMax() : 1; }

private:
	///function which run in a thread and in which Recorder loop runs
	void capture();
	void stop_capture(); ///make the capture thread return
	void cleanup(); ///< clean up initialization

	///threading stuff
	Glib::Dispatcher *signal_update_meters;
	Glib::Thread *capture_thread;
	Glib::Mutex mutex;
	bool stop_monitor; ///< signal for the run function to return

	///capture stuff
	Config *config;
	BaseRecorder<Glib::Mutex> *recorder;
	uint signal; ///< Recorder's IdleLoop control signal


	///monitor stuff
	SDL_Event      event;   ///< SDL event

	SDL_Rect       screen_rect; ///< screen dimentions
	SDL_Surface   *screen;  ///< output screen
	SDL_Overlay   *overlay; ///< YUV overlay
	AVPicture      overlay_frame; ///< ffmpeg structure representing overlay. Used for pixel format conversions
	AVFrame       *in_picture; ///< ffmpeg structure representing input frame
	SwsContext    *sws;     ///< scaling and converting context
	PixelFormat    in_fmt;  ///< ffmpeg PixelFormat of the input picture

	uint           motion;  ///< number of diff-pixels
	uint           peak;    ///< sound noise level
	unsigned char *buffer;  ///< buffer for captured image
	uint           v_bsize; ///< size of image buffer
	uint           width;   ///< widht of the image
	uint           height;  ///< height of the image
	bool highlight_motion;  ///< if true, get motion buffer from the catcher, otherwise get video buffer
};

#endif
