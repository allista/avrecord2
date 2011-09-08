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


#ifndef _MONITORTHREAD_H_
#define _MONITORTHREAD_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qobject.h>
#include <qthread.h>
#include <qmutex.h>
#include <qimage.h>

#include <recorder.h>


///This class incapsulates Recorder's IdleLoop in a separate thread
class CaptureThread : public QThread
{
public:
	CaptureThread()	{	recorder = NULL;};
	~CaptureThread() {	if(recorder) delete recorder;	};

	///initializes recorder from "cfg" config file
	void Init(QString cfg);

	///gently stops thread execution
	void stop();

  ///returns true if recorder was inited successfully
	bool inited() const	{ return recorder != NULL; }

	///trys to lock recorder's mutex
	void lock()   { if(recorder) recorder->lock();   };

	///unlocks recorder's mutex
	void unlock() { if(recorder) recorder->unlock(); };

	///return last measured diff-pixels of the recorder (NOTE: you must enclose call
	///of this function with lock()-unlock() pair)
	uint getDiffs() const
	{
		if(recorder)
			return recorder->getMotion();
		else return 0;
	};

	///return last measured noise level of the recorder (NOTE: you must enclose call
	///of this function with lock()-unlock() pair)
	uint getNoise() const
	{
		if(recorder)
			return recorder->getPeak();
		else return 0;
	};

	///initializes given pointer (it's better for it to be a NULL pointer)
	///according to the size of recorder's video buffer.
	void initImageBuffer(unsigned char *&buffer, uint &size, uint &w, uint &h) const;

	///copies recorder's video buffer to the given pointer (wich must be
	///initialized with initImageBuffer). (NOTE: you must enclose call
	///of this function with lock()-unlock() pair)
	void getImage(unsigned char *buffer, bool with_motion) const;

protected:
	///thread's run() method
	virtual void run();

private:
	///BaseRecorder template determinated by QMutex mutex class
	BaseRecorder<QMutex> *recorder;

	///Recorder's IdleLoop control signal
	uint signal;
};


///This thread periodically checks slave CaptureThread for
///last measured diff-pixels and noise level and emits these values
///to the main window.
class MonitorThread : public QThread
{
public:
	MonitorThread() : buffer(NULL) {};
	virtual ~MonitorThread()
	{
		catcher.stop();
		if(buffer) delete[] buffer;
	};

	///initializes slave CaptureThread by "cfg" config file
	void Init(QObject* bask, QString cfg, bool with_motion);

	///gently stops thread execution
	void stop();

	///using this structure both diffs and noise values are sent in a single event
	struct TunerData
	{
		uint diffs;
		uint noise;
		QImage *image;
	};

	///event types that is used for data transmition
	///note: user's event id must be in range [1000,65535] (from qevent.h)
	enum {	DATA_EVENT_TYPE  = 1001	};

protected:
	///thread's run() method
	virtual void run();

private:
	CaptureThread  catcher; ///< slave CaptureThread
	QObject       *basket;  ///< basket for event sending

	uint           diffs;   ///< number of diff-pixels
	uint           noise;   ///< sound noise level
	QImage         image;   ///< QImage structure that holds the captuerd one
	unsigned char *buffer;  ///< buffer for captured image
	uint           v_bsize; ///< size of image buffer
	uint           width;   ///< widht of the image
	uint           height;  ///< height of the image
	bool highlight_motion;  ///< if true, get motion buffer from the catcher, otherwise get video buffer

	///sends last measured diff-pixels and noise level to the main window through event queue
	///sends last image to the main window through event queue
	void updateTuner();
};

#endif // _MONITORTHREAD_H_
