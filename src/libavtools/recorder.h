/***************************************************************************
 *   Copyright (C) 2006 by Allis   *
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
/**@author Allis*/

#ifndef RECORDER_H
#define RECORDER_H

#include <time.h>

#include <iostream>
#include <fstream>
#include <string>
#include <list>
using namespace std;

#include <libconfig.h++>
using namespace libconfig;

#include "common.h"
#include "buffer-ring.h"
#include "sndstream.h"
#include "vidstream.h"
#include "avifile.h"
#include "utimer.h"
#include "img_tools.h"


///signals for main loop
enum rec_sig
{
    SIG_RECORDING,
    SIG_QUIT,
    SIG_CHANGE_FILE
};


///Void mutex class provides mutex interface but does nothing
class _void_mutex
{
	public:
		void lock()    {};
		void unlock()  {};
		bool trylock() { return true; };
};


///Like Glib::Mutex::Lock -- helper class for exception safe mutex locking
template<class _mutex>
class MutexLock
{
public:
	MutexLock(_mutex &mutex)
	: mutex(mutex)
	{ lock(); }
	~MutexLock() { mutex.unlock(); }

	void lock() { mutex.lock(); _locked = true; };
	bool trylock() { _locked = mutex.trylock(); return _locked; };
	void unlock() { mutex.unlock(); _locked = false; };
	bool locked() const { return _locked; };
private:
	_mutex &mutex;
	bool _locked;
};

///This class gathers all input/output classes together
///and provides very simple interface for recording or just
///listenning signals. This is a template class wich is determinated
///by a class that provides simple mutex interface:
///_mutex::lock(), _mutex::unlock()., bool _mutex::trylock();
template<class _mutex>
class BaseRecorder
{
public:
	BaseRecorder(bool _idle = false);
	~BaseRecorder() { Close(); };

	///initializes the recorder
	bool Init(Config *_avrecord_config_ptr);

	///cleans all up
	void Close();

	///Main record loop (the 'signal' is used to handle recording
	///process from outside the class)
	bool RecordLoop(uint *signal);

	///lock recorder's mutex
	void lock() { mutex.lock(); };

	///unlocks recorder's mutex
	void unlock() { mutex.unlock(); };

	///number of motion pixels
	uint getMotion() const { return last_diffs; };
	///maximum number of motion pixels
	uint getMotionMax() const { return width*height - motion_threshold; }

	///sound peak value
	uint getPeak() const { return last_peak_value; };
	///maximum peak value
	uint getPeakMax() const { return SND_PEAK_MAX - sound_peak_threshold; };

	///returns last video buffer
	///you should call BaseRecorder::lock() befor using this method (and BaseRecorder::unlock() somewhere afterward)
	const unsigned char *getVBuffer() { return *v_buffer; };

	///returns previous motion buffer (with highlighted diff-pixels)
	///you should call BaseRecorder::lock() befor using this method (and BaseRecorder::unlock() somewhere afterward)
	const unsigned char *getMBuffer(bool do_print_info = true);

	///returns size of video buffer
	uint getVBSize() const { return v_bsize; };

	///returns width of the image
	uint getWidth()  const { return width; };

	///returns width of the image
	uint getHeight() const { return height; };

	uint getPixelFormat() const { return v_source.pixel_format(); }

	double getFrameInterval() const { return v_source.frame_interval(); }

private:
	bool           inited;    ///< true if all is inited

	_mutex         mutex;     ///< mutex, that serializes access to some info and video-audio buffers

	Sndstream       a_source;  ///< audio source (soundcard)
	Vidstream       v_source;  ///< video source (video capture card)
	AVIFile         av_output; ///< output file

	BufferRing     *a_buffer;  ///< sound buffer ring
	unsigned int	a_bsize;   ///< sound buffer size

	BufferRing      *v_buffer;  ///< video buffer ring
	unsigned int    v_bsize;   ///< video buffer size

	BufferRing      *m_buffer;  ///< image buffer ring for motion detection
	unsigned char  *mask;      ///< buffer that contains mask image
	uint             mask_size; ///< mask buffer size

	unsigned char  *erode_tmp; ///< temporary buffer to use with erode function

	double           a_pts;     ///< audio pts of the av_output
	double           v_pts;     ///< video pts of the av_output


	//configuration//
	///pointer to the root Setting object (libconfig)
	Config  *avrecord_config;
	///pointer to the audio Setting object (libconfig)
	Setting *audio_settings_ptr;
	///pointer to the video Setting object (libconfig)
	Setting *video_settings_ptr;

	string   output_dir;       ///< output directory
	string   fname_format;     ///< template for filename to record to

	//flags
	bool     idle;             ///< if true, do not output to the file
	bool     detect_noise;     ///< if true, do noise detection
	bool     detect_motion;    ///< if true, do motion detection
	bool     record_on_motion; ///< if true, record only when motion (or noise, if
                               ///< correspondig flag is set) is detected
	bool     record_on_noise;  ///< if true, record only when noise (or motion, if
                               ///< correspondig flag is set) is detected
	bool     print_motion;     ///< if true, print number of motion pixels to the image
	bool     print_peak;       ///< if true, print sound peak value
	bool     print_date;       ///< if true, print current date to the image
	bool     log_events;       ///< if ture, start and stop record events will be logged to a log file

	//schedule parameters
	typedef  pair<tm,tm>    t_window; ///< defines a time window
	typedef  list<t_window> schedule; ///< simple schedule

	time_t   start_time;              ///< defines time, when recording will be started
	time_t   end_time;                ///< defines time, when recording will be finished
	schedule rec_schedule;            ///< list of timestamp pairs.
	                                  ///< If not empty, record only when current time is between any pair.

	//motion detection parameters
	bool     recording;               ///< true, if recording now
	uint     min_gap;                 ///< time of no_motion that is required for starting new movie file
	uint     min_record_time;         ///< movie duration that is required for starting new movie file
	uint     post_motion_offset;      ///< when ther's no motion, continue recording during this time
	uint     latency;                 ///< number of buffers to write before the one on which motion was detected
	uint     audio_latency;           ///< number of audio buffers equal in duration to the latency number of video frames

	UTimer   record_timer;            ///< timer, that measures current file duration
	UTimer   silence_timer;           ///< timer, that measures no_motion time

	uint     video_noise_level;             ///< when detecting a motion, difference between two pixels is
	                                  ///< compared with this value
	uint     motion_threshold;               ///< we detect a motion when number of motion pixels is grater
	                                  ///< than this value
	uint     pixel_step;               ///< step in pixels that is used when difference between
                                      ///< frames is measured
	uint     frame_step;             ///< step in frames between the two compared ones
	uint     video_noise_reduction_level;   ///< shows, how many times we need to erode captured image to
	                                  ///< remove a noise

	uint     sound_peak_value_function;    ///< function for sound noise level calculation: 0 - linear (default)
                                      ///< 1 - 2pwr root, 2 - 4pwr root, 3 - 8pwr root.
	uint     sound_peak_threshold;     ///< we detect sound noise when noise level calculated with
                                      ///< "noise_level_function" is grater than this value (0 - 1000)
	uint     last_diffs;              ///< last measured diff-pixels
	uint     last_peak_value;         ///< last measured noise level
	time_t   now;                     ///< current timestamp


	//audio/video parameters
	uint     width;                   ///< width of the image
	uint     height;                  ///< height of the image

	uint     amp_level;               ///< amplification level
	/////////////////


	//functions//
	///erode given image in a '+' shape
	int erode(unsigned char *img);

	///return a degree of pixels that are differ in given images.
	///This function uses the most simple algorithm: checking every
	///n-th pixel of both images it compares the difference between them
	///with 'noise level' and if the one is lower this pixel is counted.
	uint fast_diff(unsigned char *old_img, unsigned char *new_img, unsigned char *mask_img = NULL);

	///check if current time is between start and stop time
	int  check_time(time_t now);

	///check if current time is in any time window of schedule
	bool time_in_window(time_t now);

	///capture video frame and stores it in video buffer ring
	int  capture_frame();

	///capture audio frame and stores it in audio buffer ring
	int  capture_sound();

	///detect motion using current captured frame and previous one
	uint measure_motion();

	///print timestamp and other information if necessary
	void print_info(unsigned char *frame);

	///write video frame to av_output
	bool write_frame();

	///write sound frame to av_output
	bool write_sound();

	///capture and write frames if needed
	bool record_av_frame();

	///write latency buffers to av_output
	void write_latency_buffers();

	///generates avi filename according fname_format
	string generate_fname();

	///initialize output file
	bool init_av_output();

	///load from bmp file and initialize mask image
	bool init_mask(string fname ///< path to a bmp file with a mask
				    );
};

///This Recorder is for a singlethread applications
typedef BaseRecorder<_void_mutex> Recorder;




/////////////////////////////////////////////////////////
//                 implementation                      //

//comparsion operators for tm structures
//only time is compared, not date!!!
static bool operator< (const tm &x, const tm &y)
{
	return
	    x.tm_hour*3600 + x.tm_min*60 + x.tm_sec <
	    y.tm_hour*3600 + y.tm_min*60 + y.tm_sec;
}

static bool operator<=(const tm &x, const tm &y)
{
	return
	    x.tm_hour*3600 + x.tm_min*60 + x.tm_sec <=
	    y.tm_hour*3600 + y.tm_min*60 + y.tm_sec;
}


template<class _mutex>
BaseRecorder<_mutex>::BaseRecorder(bool _idle)
{
	inited           = false;
	a_buffer         = NULL;
	v_buffer         = NULL;
	m_buffer         = NULL;
	mask             = NULL;
	erode_tmp        = NULL;
	v_pts            = 0;
	a_pts            = 0;
	last_diffs       = 0;
	last_peak_value  = 0;

	//configuration//
	//flags
	idle             = _idle;
	detect_motion    = false;
	record_on_motion = false;
	print_motion     = false;
	print_peak       = false;
	print_date       = false;

	//schedule parameters
	start_time       = 0;
	end_time         = 0;

	//motion detection parameters
	recording        = false;
}


template<class _mutex>
bool BaseRecorder<_mutex>::Init(Config *_avrecord_config_ptr)
{
	//configuration//
	if(!_avrecord_config_ptr) return false;
	avrecord_config = _avrecord_config_ptr;
	try
	{
		audio_settings_ptr = &avrecord_config->lookup("audio");
		video_settings_ptr = &avrecord_config->lookup("video");
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "No <audio> or <video> setting group was found.");
		return false;
	}

	//paths
	try	{ fname_format = (const char*)avrecord_config->lookup("paths.filename"); }
	catch(SettingNotFoundException) { fname_format = "%Y-%m-%d_%H-%M.avi"; }
	try	{ output_dir   = (const char*)avrecord_config->lookup("paths.output_dir"); }
	catch(SettingNotFoundException) { output_dir   = "./"; }

	if(output_dir[output_dir.size()-1] != '/')
		output_dir += '/';

	//flags
	try	{ detect_motion    = avrecord_config->lookup("switches.detect_motion");	}
	catch(SettingNotFoundException) { detect_motion    = true; }
	try	{ detect_noise     = avrecord_config->lookup("switches.detect_noise"); }
	catch(SettingNotFoundException) { detect_noise     = false; }
	try	{ record_on_motion = avrecord_config->lookup("switches.record_on_motion"); }
	catch(SettingNotFoundException) { record_on_motion = true; }
	try	{ record_on_noise  = avrecord_config->lookup("switches.record_on_noise"); }
	catch(SettingNotFoundException) { record_on_noise  = false; }
	try	{ print_motion     = avrecord_config->lookup("switches.print_motion_amount"); }
	catch(SettingNotFoundException) { print_motion      = true; }
	try	{ print_peak       = avrecord_config->lookup("switches.print_sound_peak_value"); }
	catch(SettingNotFoundException) { print_peak      = false; }
	try	{ print_date       = avrecord_config->lookup("switches.print_date"); }
	catch(SettingNotFoundException) { print_date       = true; }
	try	{ log_events       = avrecord_config->lookup("switches.log_events"); }
	catch(SettingNotFoundException) { log_events       = false; }

	if(record_on_motion)
		detect_motion = true;
	if(record_on_noise)
		detect_noise  = true;
	if(!detect_motion)
		print_motion   = false;
	if(!detect_noise)
		print_peak   = false;

	//schedule parameters
	string start_date, end_date, win_list;
	try	{ start_date = (const char*)avrecord_config->lookup("schedule.start_time"); }
	catch(SettingNotFoundException) { start_time = 0; }
	try	{ end_date   = (const char*)avrecord_config->lookup("schedule.end_time"); }
	catch(SettingNotFoundException) { end_time   = 0; }
	try	{ win_list   = (const char*)avrecord_config->lookup("schedule.schedule"); }
	catch(SettingNotFoundException) { ; }

	time(&now);
	if(!start_date.empty())
	{
		tm *s_date = localtime(&now);
		if(!strptime(start_date.c_str(), "%d.%m.%Y-%H:%M:%S", s_date))
		{
			log_message(1, "Recorder: Init: unable to parse start time string: %s", start_date.c_str());
			return false;
		}
		start_time = mktime(s_date);
	}
	if(!end_date.empty())
	{
		tm *e_date = localtime(&now);
		if(!strptime(end_date.c_str(), "%d.%m.%Y-%H:%M:%S", e_date))
		{
			log_message(1, "Recorder: Init: unable to parse end time string: %s", end_date.c_str());
			return false;
		}
		end_time   = mktime(e_date);
	}
	if(!win_list.empty())
	{
		string win;
		uint wpos = 0, pos = 0;
		tm start, end;
		while((pos = win_list.find(';', pos)) != string::npos)
		{
			win  = win_list.substr(0,pos);
			wpos = win.find('-');
			strptime(win.substr(0,wpos).c_str(), "%H:%M:%S", &start);
			strptime(win.substr(wpos+1).c_str(), "%H:%M:%S", &end);
			if(start < end)
				rec_schedule.push_front(t_window(start, end));
			else
				log_message(1, "Recorder: Init: schedule: time window %s is discarded, because end point is earlyer then the start one.", win.c_str());
			win_list = win_list.substr(pos+1);
		}
	}

	//motion detection parameters
	try	{ min_gap
		= (int)avrecord_config->lookup("detection.min_gap") * 1000000; }
	catch(SettingNotFoundException)
	{ min_gap              = 300*1000000; } //300 sec

	try	{ min_record_time
		= (int)avrecord_config->lookup("detection.min_record_time") * 1000000 * 60; }
	catch(SettingNotFoundException)
	{ min_record_time      = 30 * 1000000 * 60; } //30 min

	try	{ post_motion_offset
		= (int)avrecord_config->lookup("detection.post_motion_offset") * 1000000; }
	catch(SettingNotFoundException)
	{ post_motion_offset   = 30 * 1000000; } //30 sec

	try	{ latency
		= (int)avrecord_config->lookup("detection.latency"); }
	catch(SettingNotFoundException)
	{ latency              = 2; }

	try	{ video_noise_level
		= (int)avrecord_config->lookup("detection.video_noise_level"); }
	catch(SettingNotFoundException)
	{ video_noise_level    = 2; }

	try	{ motion_threshold
		= (int)avrecord_config->lookup("detection.motion_threshold"); }
	catch(SettingNotFoundException)
	{ motion_threshold     = 25; }

	try	{ pixel_step
		= (int)avrecord_config->lookup("detection.pixel_step"); }
	catch(SettingNotFoundException)
	{ pixel_step           = 1; }

	try	{ frame_step
		= (int)avrecord_config->lookup("detection.frame_step"); }
	catch(SettingNotFoundException)
	{ frame_step           = 5; }

	try	{ video_noise_reduction_level
		= (int)avrecord_config->lookup("detection.video_noise_reduction_level"); }
	catch(SettingNotFoundException)
	{ video_noise_reduction_level = 1; }

	try	{ sound_peak_value_function
		= (int)avrecord_config->lookup("detection.sound_peak_value_function"); }
	catch(SettingNotFoundException)
	{ sound_peak_value_function  = SND_LIN; }

	try	{ sound_peak_threshold
		= (int)avrecord_config->lookup("detection.sound_peak_threshold"); }
	catch(SettingNotFoundException)
	{ sound_peak_threshold = 25000; }
	/////////////////

	if(!a_source.Open(audio_settings_ptr))
	{
		log_message(1, "Recorder: Init: unable to open soundstream");
		Close();
		return false;
	}
	if(!v_source.Open(video_settings_ptr))
	{
		log_message(1, "Recorder: Init: unable to open videostream");
		Close();
		return false;
	}

	if(print_motion || print_peak || print_date) InitBitmaps();

	av_register_all();

	lock();
	if(latency < 2) latency = 2;
	audio_latency	= uint(latency*v_source.frame_interval()/a_source.ptime()+1);
	a_bsize   		= a_source.bsize();
	a_buffer  		= new BufferRing(a_bsize, audio_latency);

	v_bsize   		= v_source.bsize();
	v_buffer		= new BufferRing(v_bsize, latency);

	if(frame_step < 1) frame_step = 1;
	if(detect_motion)
		m_buffer = new BufferRing(v_bsize, frame_step+2);
	unlock();

	try
	{
		width  = (*video_settings_ptr)["width"];
		height = (*video_settings_ptr)["height"];
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "Recorder: no <width> or <height> setting was found.");
		Close();
		return false;
	}

	if(video_noise_reduction_level)
		erode_tmp = new unsigned char[width*3];

	string mask_filename;
	try { mask_filename = (const char*)avrecord_config->lookup("paths.mask_file"); }
	catch(SettingNotFoundException) {}
	if(detect_motion && mask_filename.size()) init_mask(mask_filename);
	//////////////

	inited = true;
	return inited;
}


template<class _mutex>
void BaseRecorder<_mutex>::Close( )
{
	MutexLock<_mutex> lock(mutex);
    av_output.Close();
    a_source.Close();
    v_source.Close();
    //////////////
	if(a_buffer)
		delete a_buffer;
	a_buffer = NULL;

	if(v_buffer)
		delete v_buffer;
	v_buffer = NULL;

	if(m_buffer)
		delete m_buffer;
	m_buffer = NULL;

	if(mask)
		av_free(mask);
	mask = NULL;

	if(erode_tmp)
		delete[] erode_tmp;
	erode_tmp = NULL;
	//////////////
	inited = false;
}


template<class _mutex>
bool BaseRecorder<_mutex>::init_av_output()
{
    if(!(av_output.Init(audio_settings_ptr, video_settings_ptr) &&
         av_output.setAParams()                                 &&
         av_output.setVParams(v_source.frameperiod_numerator(),
                              v_source.frameperiod_demoninator(),
                              v_source.pixel_format())          &&
         av_output.Open(generate_fname())))
    {
        log_message(1, "Recorder: Init: unable to open output file.");
        return false;
    }
    return true;
}


template<class _mutex>
bool BaseRecorder<_mutex>::RecordLoop( uint * signal )
{
	if(!inited)	return false;
	//initialize output file
	if(idle) log_message(0, "Recorder: IdleLoop: starting recording simulation.");
	else if(!init_av_output())	{ Close(); return false; }
    //capture first audio frame. Without it audio stream would be out of sync
    if(capture_sound() < 0)
    {
        log_message(1, "Recorder: Init: capture first audio frame.");
        Close();
        return false;
    }
	//start video capture
	if(!v_source.StartCapture())
	{
		log_message(1, "Recorder: Init: unable to start video capture.");
		Close();
		return false;
	}

	//reset pts and diffs values
    v_pts           = 0;
    a_pts           = 0;
    last_diffs      = 0;
    last_peak_value = 0;
	//main record loop
	while(*signal != SIG_QUIT)
	{
		time(&now);
		//check if present time between start time and stop time
		switch(check_time(now))
		{
			case  0:
				break;
			case -1:
				sleep(1);
				continue;
			case  1:
				goto exit_loop;
		}
		//check if present time is in schedule
		if(!time_in_window(now))
		{
			if(recording)
			{
				recording = false;
				record_timer.pause();
			}
			sleep(1);
			continue;
		}
		//changing the file
		if(*signal == SIG_CHANGE_FILE)
		{
			//first, write latency buffer to the old file
		    write_latency_buffers();
			//close the old and initialize the new file
			if(!idle)
			{
				av_output.Close();
				if(!init_av_output()) { Close(); return false;	}
			}
			else log_message(0, "Recorder: IdleLoop: switched to next file");
			lock(); *signal = SIG_RECORDING; unlock();
		}
		//record next frame
		if(!record_av_frame()) { Close(); return false; }
		//waiting for motion or noise
		if((record_on_motion || record_on_noise) && !recording) //just listen
		{
			if((record_on_motion && last_diffs) ||
			   (record_on_noise  && last_peak_value))
			{
				recording = true;
				record_timer.start();
				silence_timer.reset();
				if(idle || log_events)
					log_message(0, "Recorder: motion or noise detected. Start recording.");
				continue;
			}
            if(silence_timer.elapsed() > min_gap && record_timer.elapsed() > min_record_time)
            {
                lock(); *signal = SIG_CHANGE_FILE; unlock();
                silence_timer.reset();
                record_timer.reset();
                record_timer.pause();
                continue;
            }
		}
		else //recording frames
		{
			//reset silence timer if needed
			if((record_on_motion && last_diffs) ||
			   (record_on_noise  && last_peak_value))
			    silence_timer.reset();
			//pause recording if no motion or noise were detected long enough
			if((record_on_motion || record_on_noise) &&
			   silence_timer.elapsed() > post_motion_offset)
			{
				recording = false;
				record_timer.pause();
				if(idle || log_events)
					log_message(0, "Recorder: no motion or noise for %d seconds. Pause recording.", post_motion_offset/1000000);
			}
		}
	}
exit_loop:
    write_latency_buffers();
    Close();
	return true;
}


template<class _mutex>
const unsigned char * BaseRecorder<_mutex>::getMBuffer(bool do_print_info)
{
	if(detect_motion)
	{
		if(do_print_info) print_info(*m_buffer);
		return *m_buffer;
	}
	else return *v_buffer;
}


template<class _mutex>
string BaseRecorder<_mutex>::generate_fname()
{
	char name[256];
	time_t now = time(0);
	strftime(name, 256, fname_format.c_str(), localtime(&now));
	return string(output_dir + name);
}


template<class _mutex>
void BaseRecorder<_mutex>::print_info(unsigned char *frame)
{
	if(print_date)
	{
		const char* time = ctime(&now);
		string date = (time)? time : "\n";
		date.erase(date.size()-1);
		PrintText(frame, date, width-5-TextWidth(date), height-5, width, height);
	}
	if(print_motion)
	{
		char diffs_str[10] = {'\0'};
		sprintf(diffs_str, "%d", last_diffs);
		PrintText(frame, diffs_str,	width-5-TextWidth(diffs_str), 10, width, height);
	}
	if(print_peak)
	{
		char level_str[10] = {'\0'};
		sprintf(level_str, "%d", last_peak_value);
		PrintText(frame, level_str,	width-5-TextWidth(level_str), 20, width, height);
	}
}


template<class _mutex>
bool BaseRecorder<_mutex>::write_frame()
{
	if(!inited || v_buffer->empty())	return false;
	bool err = av_output.writeVFrame(*v_buffer);
    a_pts = av_output.getApts();
    v_pts = av_output.getVpts();
	lock(); v_buffer->pop(); unlock();
	return err;
}


template<class _mutex>
bool BaseRecorder<_mutex>::write_sound()
{
	if(!inited || a_buffer->empty()) return false;
	bool err = av_output.writeAFrame(*a_buffer, a_buffer->rsize());
    a_pts = av_output.getApts();
    v_pts = av_output.getVpts();
    a_buffer->pop();
	return err;
}


template<class _mutex>
bool BaseRecorder<_mutex>::record_av_frame()
{
    if(a_pts < v_pts)
    {
        //capture sound frame
        if(capture_sound() < 0)
        {
            log_message(1, "Recorder: unable to capture sound");
            return false;
        }
        if(recording and !idle) write_sound();
        else
        {
            a_pts += a_source.ptime();
            if(recording) a_buffer->pop();
        }
    }
    else
    {
        //capture video frame if needed
        if(capture_frame() < 0)
        {
            log_message(1, "Recorder: unable capture video frame.");
            return false;
        }
        if(recording and !idle) write_frame();
        else
        {
            v_pts += v_source.frame_interval();
            if(recording) { lock(); v_buffer->pop(); unlock(); }
        }
    }
    return true;
}


template<class _mutex>
void BaseRecorder<_mutex>::write_latency_buffers()
{
    while(!v_buffer->empty() and !a_buffer->empty())
    {
        if(a_pts < v_pts)
        {
            if(!idle) write_sound();
            else
            {
                a_pts += a_source.ptime();
                a_buffer->pop();
            }
        }
        else
        {
            if(!idle) write_frame();
            else
            {
                v_pts += v_source.frame_interval();
                { lock(); v_buffer->pop(); unlock(); }
            }
        }
    }
}


template<class _mutex>
int BaseRecorder<_mutex>::capture_frame()
{
	if(!inited) return -1;

	MutexLock<_mutex> lock(mutex);
	int read = v_source.Read(v_buffer->wbuffer(), v_bsize);
	if(read == -1) { return -1; }
	if(read ==  0) { return  0; }
	v_buffer->push(v_bsize);

	if(detect_motion) last_diffs = measure_motion();
	print_info((*v_buffer)[1]);

	if(detect_motion) return last_diffs;
	else return 0;
}


template<class _mutex>
int BaseRecorder<_mutex>::capture_sound()
{
	if(!inited) return -1;

	int read = a_source.Read(a_buffer->wbuffer(), a_bsize);
	if(read == -1) return -1;
	if(read ==  0) return  0;
	a_buffer->push(read);

	if(detect_noise)
	{
		MutexLock<_mutex> lock(mutex);
		last_peak_value = a_source.Peak();
		last_peak_value = (last_peak_value > sound_peak_threshold)?
				           last_peak_value - sound_peak_threshold : 0;
		return last_peak_value;
	}
	else return 0;
}


template<class _mutex>
uint BaseRecorder<_mutex>::measure_motion( )
{
	if(!inited || !detect_motion) return 0;

	memcpy(m_buffer->wbuffer(), (*v_buffer)[1], v_bsize);
	for(int i = video_noise_reduction_level; i > 0; i--)
		erode(m_buffer->wbuffer());
	m_buffer->push(v_bsize);

	if(!m_buffer->full()) return 0;

	return fast_diff(*m_buffer, (*m_buffer)[1], mask);
}


template<class _mutex>
bool BaseRecorder<_mutex>::time_in_window(time_t now)
{
	if(rec_schedule.empty())
		return true;
	tm *tm_now = localtime(&now);
	bool ret = false;

	schedule::iterator i;
	for(i = rec_schedule.begin(); !ret && i != rec_schedule.end(); i++)
		ret = i->first <= *tm_now && *tm_now <= i->second;

	return ret;
}


template<class _mutex>
int  BaseRecorder<_mutex>::check_time(time_t now)
{
	if(start_time)
		if(now < start_time)
			return -1;
	if(end_time)
		if(now > end_time)
			return  1;
	return 0;
}


template<class _mutex>
uint BaseRecorder<_mutex>::fast_diff( unsigned char * old_img, unsigned char * new_img, unsigned char * mask_img )
{
	int diffs  = 0;
	int pixels = width*height;
	int step   = pixel_step;
	if(!step%2)  step++;
	unsigned char dummy_mask = 255;
	bool real_mask = true;
	if(!mask_img)
	{
		mask_img = &dummy_mask;
		real_mask = false;
	}

	for (int i = pixels; i > 0; i -= step)
	{
		/* using a temp variable is 12% faster */
		register unsigned char curdiff= abs(int(*old_img)-int(*new_img));
		//130560 = 512*255; the formula is: diff > noise_leve*512/(1+old+new)*(255/mask)
		if(curdiff > video_noise_level*130560/(1 + *old_img + *new_img)/(*mask_img))
		{ diffs++; *old_img = 255; }
		old_img  +=step;
		new_img  +=step;
		if(real_mask)
			mask_img +=step;
	}
	diffs -= motion_threshold;
	return (uint) (diffs<0)? 0 : diffs;
}


template<class _mutex>
int BaseRecorder<_mutex>::erode(unsigned char * img)
{
	unsigned char flag = 127;
	uint y, i, sum = 0;
	char *Row1,*Row2,*Row3;

	Row1 = (char*) erode_tmp;
	Row2 = Row1 + width;
	Row3 = Row1 + 2*width;

	memset(Row2, flag, width);
	memcpy(Row3, img, width);

	for (y = 0; y < height; y++)
	{
		memcpy(Row1, Row2, width);
		memcpy(Row2, Row3, width);

		if (y == height-1)
			memset(Row3, flag, width);
		else
			memcpy(Row3, img+(y+1)*width, width);

		for (i = width-2; i >= 1; i--)
		{
			if (Row1[i]   == 0 ||
					Row2[i-1] == 0 ||
			    Row2[i]   == 0 ||
			    Row2[i+1] == 0 ||
			    Row3[i]   == 0)
				img[y*width+i] = flag;
			else
				sum++;
		}
		img[y*width] = img[y*width+width-1] = flag;
	}
	return sum;
}

template<class _mutex>
bool BaseRecorder<_mutex>::init_mask(string fname)
{
	if(!video_settings_ptr) return false;
	Setting &video_settings = *video_settings_ptr;
	bool ret = false;

	fstream         mask_file;
	char           *buffer      = NULL;
	uint            buf_length;

	AVCodec        *bmp_codec   = NULL;
	AVCodecContext *bmp_context = NULL;
	AVFrame        *mask_in     = NULL;
	AVFrame        *mask_out    = NULL;
	PixelFormat     in_fmt;
	PixelFormat     out_fmt     = PIX_FMT_GRAY8;
	AVPacket        av_packet; ///< this will be needed when libavcodec api will change. For now it is used as a temp variable
	SwsContext     *sws_cntx    = NULL;

	uint iwidth;
	uint iheight;
	uint owidth;
	uint oheight;
	try
	{
		owidth  = video_settings["width"];
		oheight = video_settings["height"];
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "Recorder: Video <width> or <height> settings not found.");
		goto cleanup;
	}
	mask_size = avpicture_get_size(out_fmt, owidth, oheight);
	mask = (unsigned char*)av_mallocz(mask_size);
	if(!mask)
	{
		log_message(1, "Recorder: failed to allocate %d bytes for mask buffer", mask_size);
		goto cleanup;
	}

	/* find the bmp encoder */
	bmp_codec = avcodec_find_decoder(CODEC_ID_BMP);
	if(!bmp_codec)
	{
		log_message(1, "Recorder: BMP codec not found");
		goto cleanup;
	}

	/* open it */
	bmp_context = avcodec_alloc_context3(bmp_codec);
	if(avcodec_open2(bmp_context, bmp_codec, NULL) < 0)
	{
		log_message(1, "Recorder: Could not open BMP codec");
		goto cleanup;
	}

	mask_file.open(fname.c_str(), ios::in|ios::binary);
	if(!mask_file.is_open())
	{
		log_message(1, "Recorder: Could not open mask file: %s", fname.c_str());
		goto cleanup;
	}

	//get length of the file
	mask_file.seekg(0, ios::end);
	buf_length = mask_file.tellg();
	mask_file.seekg(0, ios::beg);
	buffer = (char*)av_mallocz(buf_length+FF_INPUT_BUFFER_PADDING_SIZE);
	if(!buffer)
	{
		log_message(1, "Recorder: failed to allocate %d bytes for input buffer", buf_length+FF_INPUT_BUFFER_PADDING_SIZE);
		goto cleanup;
	}
	if(!mask_file.read(buffer, buf_length))
	{
		log_message(1, "Recorder: Read error");
		goto cleanup;
	}

	mask_in = avcodec_alloc_frame();
	if(!mask_in)
	{
		log_message(1, "Recorder: Faild to allocate mask_in frame");
		goto cleanup;
	}
	av_init_packet(&av_packet);
	av_packet.size = buf_length;
	av_packet.data = (uint8_t*)buffer;
	int got_picture, len;
	while(av_packet.size > 0)
	{
		//one should use avcodec_decode_video2 instead for the first version is documented as deprecated. Yet, in Ubuntu repos (and even in Medibuntu) libavcodec still lacks declaration of the second version
		len = avcodec_decode_video2(bmp_context, mask_in, &got_picture, &av_packet);
		if(len < 0)
		{
			log_message(1, "Recorder: Error while decoding file");
			goto cleanup;
		}
		if(got_picture)
		{
			iwidth  = bmp_context->width;
			iheight = bmp_context->height;
			in_fmt  = bmp_context->pix_fmt;
			break;
		}

		/* the picture is allocated by the decoder. no need to
		free it */
		av_packet.size -= len;
		av_packet.data += len;
	}

	sws_cntx = sws_getContext(iwidth, iheight, in_fmt,
							  owidth, oheight, out_fmt,
							  SWS_LANCZOS, NULL, NULL, NULL);
	if(!sws_cntx)
	{
		log_message(1, "Recorder: couldn't get SwsContext for pixel format conversion");
		goto cleanup;
	}

	mask_out = avcodec_alloc_frame();
	if(!mask_out)
	{
		log_message(1, "Recorder: Faild to allocate mask_out frame");
		goto cleanup;
	}
	avpicture_fill((AVPicture*)mask_out, (uint8_t*)mask, out_fmt, owidth, oheight);

	if(0 > sws_scale(sws_cntx, mask_in->data, mask_in->linesize, 0,
	   iheight, mask_out->data, mask_out->linesize))
	{
		log_message(1,"Recorder: Unable to convert mask image");
		goto cleanup;
	}

	///if all whent well, return true
	ret = true;

cleanup:
	///free allocated structures
	mask_file.close();
	if(bmp_context) av_free(bmp_context);
	if(mask_in)     av_free(mask_in);
	if(mask_out)    av_free(mask_out);
	if(buffer)      av_free(buffer);
	if(!ret && mask)
	{
		av_free(mask);
		mask = NULL;
	}
	return ret;
}

#endif
