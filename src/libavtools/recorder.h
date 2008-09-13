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
#include <string>
#include <list>
using namespace std;

#include "common.h"
#include "buffer-ring.h"
#include "sndstream.h"
#include "vidstream.h"
#include "avifile.h"
#include "utimer.h"
#include "configfile.h"
#include "img_tools.h"


///signals for main loop
enum
{
    SIG_RECORDING,
    SIG_QUIT,
    SIG_CHANGE_FILE
};


///Void mutex class provides mutex interface but does nothing
class _void_mutex
{
	public:
		void lock()   {};
		void unlock() {};
		bool locked() { return false; };
		bool tryLock() { return true; };
};


///This class gathers all input/output classes together
///and provides very simple interface for recording or just
///listenning signals. This is a template class wich is determinated
///by a class that provides simple mutex interface:
///_mutex::lock(), _mutex::unlock()., bool _mutex::locked() and bool _mutex::tryLock();
template<class _mutex>
class BaseRecorder
{
public:
	BaseRecorder();
	~BaseRecorder() { Close(); };

	///initializes the recorder using given configuration file
	bool Init(string config_file);

	///initializes the recorder using given config object
	bool Init(const ConfigFile &config);

	///cleans all up
	void Close();


	///Main record loop (the 'signal' is used to handle recording
	///process from outside the class, the 'idle' indicates, is writing to file to be made)
	bool RecordLoop(uint *signal, bool idle = false);
	bool IdleLoop(uint *signal) { return RecordLoop(signal, true); };

	///trys to lock recorder's mutex
	void lock() { mutex.lock(); };

	///unlocks recorder's mutex, then sleeps 5 nanoseconds in
	///order to give a chance to lock the mutex to another thread.
	void unlock() { mutex.unlock(); usleep(5); };


	///returns number of diff-pixels (NOTE: you must enclose call
	///of this function with lock()-unlock() pair)
	uint getDiffs() { return last_diffs; };

	///returns sound noise level (NOTE: you must enclose call
	///of this function with lock()-unlock() pair)
	uint getNoise() { return last_noise_level; };

	///returns last video buffer (NOTE: you must enclose call of this
	///function with lock()-unlock() pair)
	const unsigned char *getVBuffer() const { return v_buffer->rbuffer(); };

	///returns previous motion buffer (with highlighted diff-pixels)
	///(NOTE: you must enclose call of this function with lock()-unlock() pair)
	const unsigned char *getMBuffer() const { return (detect_motion)? *m_buffer : *v_buffer; };

	///returns size of video buffer
	uint getVBSize() const { return v_bsize; };

	///returns width of the image
	uint getWidth() const { return width; };

	///returns width of the image
	uint getHeight() const { return height; };

private:
	bool            inited;			///< true if all is inited

	_mutex          mutex;			///< mutex, that serializes access to some info and video-audio buffers

	Sndstream       a_source;		///< audio source (soundcard)
	Vidstream       v_source;		///< video source (video capture card)
	AVIFile         av_output;	///< output file

	BufferRing	*		a_buffer;		///< sound buffer ring
	unsigned int		a_bsize;		///< sound buffer size

	BufferRing	*		v_buffer;		///< video buffer ring
	unsigned int		v_bsize;		///< video buffer size

	BufferRing *		m_buffer;	///< image buffer ring for motion detection

	unsigned char * erode_tmp;	///< temporary buffer to use with erode function


	//configuration//
	string   output_dir;                ///< output directory
	string   fname_format;              ///< template for filename to record to

	//flags
	bool     detect_noise;              ///< if true, do noise detection
	bool     detect_motion;             ///< if true, do motion detection
	bool     record_on_motion;          ///< if true, record only when motion (or noise, if
                                    	///< correspondig flag is set) is detected
	bool     record_on_noise;           ///< if true, record only when noise (or motion, if
                                    	///< correspondig flag is set) is detected
	bool     print_diffs;               ///< if true, print number of motion pixels to the image
	bool     print_level;               ///< if true, print sound noise level
	bool     print_date;                ///< if true, print current date to the image

	//schedule parameters
	typedef  pair<tm,tm>    t_window;   ///< defines a time window
	typedef  list<t_window> schedule;   ///< simple schedule

	time_t   start_time;                ///< defines time, when recording will be started
	time_t   end_time;                  ///< defines time, when recording will be finished
	schedule rec_schedule;              ///< list of timestamp pairs.
	                                    ///< If not empty, record only when current time is between any pair.

	//motion detection parameters
	bool     recording;                 ///< true, if recording now
	uint     min_gap;                   ///< time of no_motion that is required for starting new movie file
	uint     min_record_time;           ///< movie duratioin that is required for starting new movie file
	uint     post_motion_offset;        ///< when ther's no motion, continue recordig during this time
	uint     latency;										///< number of buffers to write befor the one on which motion was detected
	uint     audio_latency;							///< number of audio buffers equal in duration to the latency number of video frames

	UTimer   record_timer;              ///< timer, that measures current file duration
	UTimer   silence_timer;             ///< timer, that measures no_motion time

	uint     noise_level;               ///< when detecting a motion, difference between two pixels is
	                                    ///< compared with this value
	uint     threshold;                 ///< we detect a motion when number of motion pixels is grater
	                                    ///< than this value
	uint     diff_step;                 ///< step in pixels that is used when difference between
                                    	///< frames is measured
	uint     frames_step;								///< step in frames between the two compared ones
	uint     noise_reduction_level;     ///< shows, how many times we need to erode captured image to
	                                    ///< remove a noise

	uint     snd_noise_level_func;      ///< function for sound noise level calculation: 0 - linear (default)
                                    	///< 1 - 2pwr root, 2 - 4pwr root, 3 - 8pwr root.
	uint     snd_noise_threshold;       ///< we detect sound noise when noise level calculated with
                                    	///< "noise_level_function" is grater than this value (0 - 1000)
	uint     last_diffs;                ///< last measured diff-pixels
	uint     last_noise_level;          ///< last measured noise level


	//audio/video parameters
	string   video_device;              ///< video device file
	string   video_codec;               ///< name of video codec
	uint     input_source;              ///< see the Vidstream class for explanations
	uint     input_mode;                ///< see the Vidstream class for explanations
	uint     width;                     ///< width of the image
	uint     height;                    ///< height of the image
	uint     brightness;                ///< brightness correction
	uint     contrast;                  ///< contrast correction
	uint     hue;                       ///< hue correction
	uint     color;                     ///< color correction
	uint     witeness;                  ///< witeness correction
	uint     frame_rate;                ///< frames per second
	uint     vid_bitrate;               ///< bits per second
	uint     var_bitrate;               ///< quantizer value
	bool     auto_frate;                ///< if true, automaticaly measure frame rate during initialization

	string   audio_codec;               ///< name of audio codec
	uint     sample_rate;               ///< samples per second
	uint     aud_bitrate;               ///< bits per second
	uint     channels;                  ///< number of audio channel
	uint     desired_frames;            ///< number of desired frames per capture operation (0 is auto)
	uint     amp_level;                 ///< amplification level
	/////////////////


	//functions//
	///Erodes given image in a '+' shape
	int erode(unsigned char *img);

	///returns a degree of pixels that are differ in given images.
	///This function uses the most simple algorithm: checking every
	///n-th pixel of both images it compares the difference between them
	///with 'noise level' and if the one is lower this pixel is counted.
	uint fast_diff(unsigned char *old_img, unsigned char *new_img);

	///checks if current time is between start and stop time
	int  check_time(time_t now);

	///checks if current time is in any time window of schedule
	bool time_in_window(time_t now);

	///captures video frame and stores it in video buffer ring
	bool capture_frame();

	///captures audio frame and stores it in audio buffer ring
	void capture_sound();

	///detects motion using current captured frame and previous one
	uint measure_motion();

	///writes video frame to av_output
	bool write_frame(time_t now, uint diffs, uint level);

	///writes sound frame to av_output
	bool write_sound();

	///generates avi filename according fname_format
	string generate_fname();
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
BaseRecorder<_mutex>::BaseRecorder()
{
	inited           = false;

	a_buffer         = NULL;

	v_buffer         = NULL;

	m_buffer         = NULL;

	erode_tmp        = NULL;

	//configuration//
	//flags
	detect_motion    = false;
	record_on_motion = false;
	print_diffs      = false;
	print_level      = false;
	print_date       = false;

	//schedule parameters
	start_time       = 0;
	end_time         = 0;

	//motion detection parameters
	recording        = false;
}


template<class _mutex>
bool BaseRecorder<_mutex>::Init(string config_file)
{
	ConfigFile config(config_file);
	if(!config)
		log_message(1, "Recorder: Init: bad config file. Using default settings.");
	return Init(config);
}


template<class _mutex>
bool BaseRecorder<_mutex>::Init(const ConfigFile &config)
{
	//configuration//
	fname_format = config.getOptionS("filename");
	if(fname_format.empty())
		fname_format = "%Y-%m-%d_%H-%M.avi";

	output_dir   = config.getOptionS("output_dir");
	if(output_dir.empty())
		output_dir = "./";
	else if(output_dir[output_dir.size()-1] != '/')
		output_dir += '/';

	//flags
	detect_motion    = config.getOptionB("detect_motion");
	detect_noise     = config.getOptionB("detect_noise");
	record_on_motion = config.getOptionB("record_on_motion");
	record_on_noise  = config.getOptionB("record_on_noise");
	print_diffs      = config.getOptionB("print_diffs");
	print_level      = config.getOptionB("print_level");
	print_date       = config.getOptionB("print_date");

	if(record_on_motion)
		detect_motion = true;
	if(record_on_noise)
		detect_noise  = true;
	if(!detect_motion)
		print_diffs   = false;
	if(!detect_noise)
		print_level   = false;


	//schedule parameters
	time_t now = time(0);
	string start_date = config.getOptionS("start_time");
	if(start_date.empty())
		start_time = 0;
	else
	{
		tm *s_date = localtime(&now);
		if(!strptime(start_date.c_str(), "%d.%m.%Y-%H:%M:%S", s_date))
		{
			log_message(1, "Recorder: Init: unable to parse start time string: %s", start_date.c_str());
			return false;
		}
		start_time = mktime(s_date);
	}

	string end_date = config.getOptionS("end_time");
	if(end_date.empty())
		end_time = 0;
	else
	{
		tm *e_date = localtime(&now);
		if(!strptime(end_date.c_str(), "%d.%m.%Y-%H:%M:%S", e_date))
		{
			log_message(1, "Recorder: Init: unable to parse end time string: %s", end_date.c_str());
			return false;
		}
		end_time   = mktime(e_date);
	}

	string win_list = config.getOptionS("schedule");
	if(win_list.size())
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
	min_gap               = config.getOptionI("min_gap")            * 1000000;
	min_record_time       = config.getOptionI("min_record_time")    * 1000000 * 60;
	post_motion_offset    = config.getOptionI("post_motion_offset") * 1000000;
	latency								= config.getOptionI("latency");

	noise_level           = config.getOptionI("noise_level");
	threshold             = config.getOptionI("threshold");
	diff_step             = config.getOptionI("pixels_step");
	frames_step           = config.getOptionI("frames_step");
	noise_reduction_level = config.getOptionI("noise_reduction_level");

	snd_noise_level_func  = config.getOptionI("snd_noise_level_func");
	snd_noise_threshold   = config.getOptionI("snd_noise_threshold");

	//audio/video parameters
	video_device          = config.getOptionS("video_device");
	video_codec           = config.getOptionS("video_codec");
	input_source          = config.getOptionI("input_source");
	input_mode            = config.getOptionI("input_mode");
	width                 = config.getOptionI("width");
	height                = config.getOptionI("height");
	frame_rate            = config.getOptionI("frame_rate");
	vid_bitrate           = config.getOptionI("video_bitrate");
	var_bitrate           = config.getOptionI("variable_bitrate");
	auto_frate            = config.getOptionB("auto_frate");

	audio_codec           = config.getOptionS("audio_codec");
	sample_rate           = config.getOptionI("sample_rate");
	aud_bitrate           = config.getOptionI("audio_bitrate");
	channels              = config.getOptionI("channels");
	desired_frames        = config.getOptionI("audio_buffer");
	amp_level             = config.getOptionI("amplification_level");

	//picture parameters (used only during initialization)
	brightness = (config.getOptionS("brightness").size())?
	                 int(65535 * config.getOptionI("brightness")/100.0) : -1;

	contrast   = (config.getOptionS("contrast").size())?
	                 int(65535 * config.getOptionI("contrast")/100.0)   : -1;

	hue        = (config.getOptionS("hue").size())?
	                 int(65535 * config.getOptionI("hue")/100.0)        : -1;

	color      = (config.getOptionS("color").size())?
	                 int(65535 * config.getOptionI("color")/100.0)      : -1;

	witeness   = (config.getOptionS("whiteness").size())?
	                 int(65535 * config.getOptionI("witeness")/100.0)   : -1;

	//audio/video DEFAULT parameters
	if(video_codec.empty())
		video_codec = "msmpeg4";
	if(!width)
		width       = 640;
	if(!height)
		height      = 480;
	if(!vid_bitrate)
		vid_bitrate = 650000;
	if(!frame_rate)
		auto_frate  = true;

	if(audio_codec.empty())
		audio_codec = "mp3";
	if(!sample_rate)
		sample_rate = 44100;
	if(!aud_bitrate)
		aud_bitrate = 96000;
	if(!channels)
		channels    = 1;
	/////////////////


	if(!a_source.Open(SND_R, SND_16BIT, channels, sample_rate, desired_frames))
	{
		log_message(1, "Recorder: Init: unable to open soundstream");
		Close();
		return false;
	}
	a_source.setAmp(amp_level);
	if(detect_noise)
	{
		a_source.setFun(snd_noise_level_func);
		a_source.setLev(snd_noise_threshold);
	}

	if(!v_source.Open(video_device.c_str(), width, height, input_source, input_mode))
	{
		log_message(1, "Recorder: Init: unable to open videostream");
		Close();
		return false;
	}
	v_source.setPicParams(brightness, contrast, hue, color, witeness);
	if(auto_frate)
		frame_rate = 1000000/(v_source.ptime()+5000);

	if(print_diffs || print_level || print_date)
		InitBitmaps();

	av_register_all();

	lock();
	if(latency < 2) latency = 2;
	audio_latency	= uint(latency*(1e6/frame_rate)/a_source.ptime()+1);
	a_bsize   		= a_source.bsize();
	a_buffer  		= new BufferRing(a_bsize, audio_latency);

	v_bsize   		= v_source.bsize();
	v_buffer			= new BufferRing(v_bsize, latency);

	if(frames_step < 1) frames_step = 1;
	if(detect_motion)
		m_buffer = new BufferRing(v_bsize, frames_step+2);
	unlock();

	if(noise_reduction_level)
		erode_tmp = new unsigned char[width*3];

	//////////////
	inited = true;
	return inited;
}


template<class _mutex>
void BaseRecorder<_mutex>::Close( )
{
	if(!inited)
		return;

	mutex.tryLock();
	if(a_buffer)
		delete a_buffer;
	a_buffer = NULL;

	if(v_buffer)
		delete v_buffer;
	v_buffer = NULL;

	if(m_buffer)
		delete m_buffer;
	m_buffer = NULL;

	if(erode_tmp)
		delete[] erode_tmp;
	erode_tmp = NULL;

	av_output.Close();
	a_source.Close();
	v_source.Close();

	unlock();

	//////////////
	inited = false;
}


template<class _mutex>
bool BaseRecorder<_mutex>::RecordLoop( uint * signal, bool idle )
{
	if(!inited)	return false;

	//initialize output file
	if(idle)
		log_message(0, "Recorder: IdleLoop: starting recording simulation.");
	else if(!av_output.Init() ||
	   !av_output.setAParams(audio_codec, channels, sample_rate, aud_bitrate) ||
	   !av_output.setVParams(video_codec, width, height, frame_rate, vid_bitrate, var_bitrate) ||
	   !av_output.Open(generate_fname()))
	{
		log_message(1, "Recorder: Init: unable to open output file");
		Close();
		return false;
	}

	double  frame_time	= 1.0/frame_rate;
	double  v_pts = 0;
	double  a_pts = 0;
	time_t  now;

	//capture the first video frame for motion detection
	lock();
	if(detect_motion)
	{
		capture_frame();
		measure_motion();
	}

	//capture the first sound block for noise detection
	if(detect_noise)
		capture_sound();
	unlock();

	//main record loop
	while(*signal != SIG_QUIT)
	{
		now = time(0);


		//check if present time between start time and stop time
		switch(check_time(now))
		{
			case  0:
				break;
			case -1:
				sleep(1);
				goto next_loop;
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
			//but first, write latency buffer to the old one
			while(!v_buffer->empty())
			{
				if(detect_noise)
					last_noise_level = a_source.Noise();
				if(!idle)
				{
					write_sound();
					a_pts = av_output.getApts();
					v_pts = av_output.getVpts();
				}
				else
				{
					a_pts += a_source.ptime()*(a_buffer->rsize()/a_bsize)/1e6;
					a_buffer->pop();
				}

				if(v_pts < a_pts || a_buffer->empty())
				{
					if(detect_motion)	last_diffs = measure_motion();
					if(!idle) write_frame(now, last_diffs, last_noise_level);
					else { v_pts += frame_time; v_buffer->pop(); }
				}
			}


			//close the old and initialize the new one
			if(!idle)
			{
				av_output.Close();
				if(!av_output.Init() ||
						!av_output.setAParams(audio_codec, channels, sample_rate, aud_bitrate) ||
						!av_output.setVParams(video_codec, width, height, frame_rate, vid_bitrate, var_bitrate) ||
						!av_output.Open(generate_fname()))
				{
					log_message(1, "Recorder: Init: unable to open output file");
					Close();
					return false;
				}
			}
			else log_message(0, "Recorder: IdleLoop: switched to next file");
			*signal = SIG_RECORDING;
		}


		//recording or waiting for motion or noise
		lock();
		if((record_on_motion || record_on_noise) && !recording) //just listen
		{
			if(silence_timer.elapsed() > min_gap && record_timer.elapsed() > min_record_time)
			{
				*signal = SIG_CHANGE_FILE;
				silence_timer.reset();
				record_timer.reset();
				record_timer.pause();
				unlock();
				continue;
			}

			capture_sound();
			if(record_on_noise)
				last_noise_level = a_source.Noise();
			a_pts += a_source.ptime()*(a_buffer->rsize()/a_bsize)/1e6;

			if(v_pts < a_pts)
			{
				capture_frame();
				if(record_on_motion)
					last_diffs = measure_motion();
				v_pts += frame_time;
			}

			if((record_on_motion && last_diffs) ||
			   (record_on_noise  && last_noise_level))
			{
				recording = true;
				record_timer.start();
				silence_timer.reset();
				if(idle) log_message(0, "Recorder: IdleLoop: motion or noise detected. Start recording.");
			}
		}
		else //record frames
		{
			capture_sound();
			if(detect_noise)
			{
				last_noise_level = a_source.Noise();
				if(record_on_noise && last_noise_level)
					silence_timer.reset();
			}
			if(!idle)
			{
				write_sound();
				a_pts = av_output.getApts();
				v_pts = av_output.getVpts();
			}
			else
			{
				a_pts += a_source.ptime()*(a_buffer->rsize()/a_bsize)/1e6;
				a_buffer->pop();
			}


			if(v_pts < a_pts)
			{
				capture_frame();
				if(detect_motion)
				{
					last_diffs = measure_motion();
					if(record_on_motion && last_diffs)
						silence_timer.reset();
				}
				if(!idle) write_frame(now, last_diffs, last_noise_level);
				else { v_pts += frame_time; v_buffer->pop(); }
			}


			if((record_on_motion || record_on_noise) &&
					silence_timer.elapsed() > post_motion_offset)
			{
				recording = false;
				record_timer.pause();
				if(idle) log_message(0, "Recorder: IdleLoop: no motion or noise. Pause recording.");
			}
		}
		unlock();

next_loop:
		continue;
	}

exit_loop:
	return true;
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
bool BaseRecorder<_mutex>::write_frame(time_t now, uint diffs, uint level)
{
	if(!inited || v_buffer->empty())	return false;

	if(print_date)
	{
		string date = ctime(&now);
		date.erase(date.size()-1);
		PrintText(*v_buffer, date, width-5-TextWidth(date), height-5, width, height);
	}
	if(print_diffs)
	{
		char diffs_str[10] = {'\0'};
		sprintf(diffs_str, "%d", diffs);
		PrintText(*v_buffer, diffs_str,	width-5-TextWidth(diffs_str), 10, width, height);
	}
	if(print_level)
	{
		char level_str[10] = {'\0'};
		sprintf(level_str, "%d", level);
		PrintText(*v_buffer, level_str,	width-5-TextWidth(level_str), 20, width, height);
	}

	bool err = av_output.writeVFrame(*v_buffer, width, height);
	v_buffer->pop();
	return err;
}


template<class _mutex>
bool BaseRecorder<_mutex>::write_sound()
{
	if(!inited || a_buffer->empty())	return false;

	bool err = av_output.writeAFrame(*a_buffer, a_buffer->rsize());
	a_buffer->pop();
	return err;
}


template<class _mutex>
bool BaseRecorder<_mutex>::capture_frame()
{
	if(!inited)	return false;

	bool err = v_source.Read(v_buffer->wbuffer(), v_bsize);
	v_buffer->push(v_bsize);
	return err;
}


template<class _mutex>
void BaseRecorder<_mutex>::capture_sound()
{
	if(!inited) return;

	unsigned int a_readed = a_source.Read(a_buffer->wbuffer(), a_bsize);
	a_buffer->push(a_readed);
}


template<class _mutex>
uint BaseRecorder<_mutex>::measure_motion( )
{
	if(!inited || !detect_motion || v_buffer->empty()) return 0;

	memcpy(m_buffer->wbuffer(), *v_buffer, v_bsize);
	for(int i = noise_reduction_level; i > 0; i--)
		erode(m_buffer->wbuffer());
	m_buffer->push();

	if(m_buffer->filled_size() <= frames_step) return 1000;

	return fast_diff(*m_buffer, (*m_buffer)[-frames_step]);
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
uint BaseRecorder<_mutex>::fast_diff( unsigned char * old_img, unsigned char * new_img )
{
	int diffs  = 0;
	int pixels = width*height;
	int step   = diff_step;
	if(!step%2)  step++;

	for (int i = pixels; i > 0; i -= step)
	{
		/* using a temp variable is 12% faster */
		register unsigned char curdiff= abs(int(*old_img)-int(*new_img));
		if(curdiff > noise_level*512/(1 + *old_img + *new_img))
		{ diffs++; *old_img = 255; }
		old_img +=step;
		new_img +=step;
	}
	diffs -= threshold;
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

#endif
