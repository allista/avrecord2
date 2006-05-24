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

#include <iostream>
using namespace std;

#include "recorder.h"
#include "img_tools.h"


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

//simple swap of two pointers
static void _swap(unsigned char * &x, unsigned char * &y)
{
	unsigned char * tmp = x;
	x = y; y = tmp;
}


Recorder::Recorder()
{
	inited           = false;

	a_buffer         = NULL;

	v_buffer0        = NULL;
	v_buffer1        = NULL;

	m_buffer0        = NULL;
	m_buffer1        = NULL;

	erode_tmp        = NULL;

	//configuration//
	//flags
	detect_motion    = false;
	record_on_motion = false;
	print_diffs      = false;
	print_date       = false;

	//schedule parameters
	start_time       = 0;
	end_time         = 0;

	//motion detection parameters
	recording        = false;
}



bool Recorder::Init(string config_file)
{
	ConfigFile config(config_file);
	if(!config)	log_message(1, "Recorder: Init: bad config file. Using default settings.");
	return Init(config);
}

bool Recorder::Init(const ConfigFile &config)
{
	//configuration//
	fname_format = config.getOptionS("filename");
	if(fname_format.empty()) fname_format = "%Y-%m-%d_%H-%M.avi";

	output_dir   = config.getOptionS("output_dir");
	if(output_dir.empty()) output_dir = "./";
	else if(output_dir[output_dir.size()-1] != '/')
		output_dir += '/';

	//flags
	detect_motion    = config.getOptionB("detect_motion");
	record_on_motion = config.getOptionB("record_on_motion");
	print_diffs      = config.getOptionB("print_diffs");
	print_date       = config.getOptionB("print_date");

	if(record_on_motion) detect_motion = true;
	if(!detect_motion)   print_diffs   = false;

	//schedule parameters
	time_t now = time(0);
	string start_date = config.getOptionS("start_time");
	if(start_date.empty()) start_time = 0;
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
	if(end_date.empty()) end_time = 0;
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
		int wpos = 0, pos = 0;
		tm start, end;
		while((pos = win_list.find(';', pos)) != string::npos)
		{
			win  = win_list.substr(0,pos);
			wpos = win.find('-');
			strptime(win.substr(0,wpos).c_str(), "%H:%M:%S", &start);
			strptime(win.substr(wpos+1).c_str(), "%H:%M:%S", &end);
			if(start < end)	rec_schedule.push_front(t_window(start, end));
			else log_message(1, "Recorder: Init: schedule: time window %s is discarded, because end point is earlyer the the start one.", win);
			win_list = win_list.substr(pos+1);
		}
	}

	//motion detection parameters
	min_gap               = config.getOptionI("min_gap")            * 1000000;
	min_record_time       = config.getOptionI("min_record_time")    * 1000000 * 60;
	post_motion_offset    = config.getOptionI("post_motion_offset") * 1000000;

	noise_level           = config.getOptionI("noise_level");
	threshold             = config.getOptionI("threshold");
	noise_reduction_level = config.getOptionI("noise_reduction_level");

	//audio/video parameters
	video_codec           = config.getOptionS("video_codec");
	input_source          = config.getOptionI("input_source");
	input_mode            = config.getOptionI("input_mode");
	width                 = config.getOptionI("width");
	height                = config.getOptionI("height");
	frame_rate            = config.getOptionI("frame_rate");
	vid_bitrate           = config.getOptionI("video_bitrate");
	var_bitrate           = config.getOptionI("variable_bitrate");
	auto_frate            = config.getOptionI("auto_frame_rate");

	audio_codec           = config.getOptionS("audio_codec");
	sample_rate           = config.getOptionI("sample_rate");
	aud_bitrate           = config.getOptionI("audio_bitrate");
	channels              = config.getOptionI("channels");
	amp_level             = config.getOptionI("amplification_level");

	//picture parameters (used only during initialization)
	int brightness = (config.getOptionS("brightness").size())?
			int(65535 * config.getOptionI("brightness")/100.0) : -1;

	int contrast   = (config.getOptionS("contrast").size())?
			int(65535 * config.getOptionI("contrast")/100.0)   : -1;

	int hue        = (config.getOptionS("hue").size())?
			int(65535 * config.getOptionI("hue")/100.0)        : -1;

	int color      = (config.getOptionS("color").size())?
			int(65535 * config.getOptionI("color")/100.0)      : -1;

	int witeness   = (config.getOptionS("whiteness").size())?
			int(65535 * config.getOptionI("witeness")/100.0)   : -1;

	//audio/video DEFAULT parameters
	if(video_codec.empty()) video_codec = "msmpeg4";
	if(!width)              width       = 640;
	if(!height)             height      = 480;
	if(!vid_bitrate)        vid_bitrate = 650000;
	if(!frame_rate)         auto_frate  = true;

	if(audio_codec.empty()) audio_codec = "mp3";
	if(!sample_rate)        sample_rate = 44100;
	if(!aud_bitrate)        aud_bitrate = 96000;
	if(!channels)           channels    = 1;
	/////////////////


	if(!a_source.Open(SND_R, SND_16BIT, channels, sample_rate))
	{
		log_message(1, "Recorder: Init: unable to open soundstream");
		Close();
		return false;
	}
	a_source.setAmp(amp_level);

	if(!v_source.Open("/dev/video0", width, height, input_source, input_mode))
	{
		log_message(1, "Recorder: Init: unable to open videostream");
		Close();
		return false;
	}
	v_source.setPicParams(brightness, contrast, hue, color, witeness);
	if(auto_frate) frame_rate = uint(1e6/v_source.ptime());
	v_source.Prepare();

	if(print_diffs || print_date)
		InitBitmaps();

	av_register_all();
	if(!av_output.Init() ||
	        !av_output.setAParams(audio_codec, channels, sample_rate, aud_bitrate) ||
	        !av_output.setVParams(video_codec, width, height, frame_rate, vid_bitrate, var_bitrate) ||
	        !av_output.Open(generate_fname()))
	{
		log_message(1, "Recorder: Init: unable to open output file");
		Close();
		return false;
	}

	a_bsize   = a_source.bsize();
	a_buffer  = new unsigned char[a_bsize];

	v_bsize   = width*height*3;
	v_buffer0 = new unsigned char[v_bsize];
	v_buffer1 = new unsigned char[v_bsize];

	if(detect_motion)
	{
		m_buffer0 = new unsigned char[v_bsize];
		m_buffer1 = new unsigned char[v_bsize];
	}

	if(noise_reduction_level)
		erode_tmp = new unsigned char[width*3];

	//////////////
	inited = true;
	return inited;
}

void Recorder::Close( )
{
	if(!inited) return;

	if(a_buffer)
		delete[] a_buffer;
	a_buffer  = NULL;

	if(v_buffer0)
		delete[] v_buffer0;
	if(v_buffer1)
		delete[] v_buffer1;
	v_buffer0 = NULL;
	v_buffer1 = NULL;

	if(m_buffer0)
		delete[] m_buffer0;
	if(m_buffer1)
		delete[] m_buffer1;
	m_buffer0 = NULL;
	m_buffer1 = NULL;

	if(erode_tmp)
		delete[] erode_tmp;
	erode_tmp = NULL;

	av_output.Close();
	a_source.Close();
	v_source.Close();

	av_free_static();

	//////////////
	inited = false;
}


bool Recorder::RecordLoop( uint * signal )
{
	if(!inited) return false;

	uint    diffs = 0;
	double  v_pts = 0;
	double  a_pts = 0;
	uint a_readed = 0;
	time_t now;

	//capture the first frame for motion detection
	if(detect_motion)
	{ capture_frame(); measure_motion(); }

	//main record loop
	while(*signal != SIG_QUIT)
	{
		now = time(0);

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

		if(!time_in_window(now))
		{ sleep(1); continue; }

		if(*signal == SIG_CHANGE_FILE)
		{
			av_output.Close();
			av_output.Init();
			av_output.setAParams(audio_codec, channels, sample_rate, aud_bitrate);
			av_output.setVParams(video_codec, width, height, frame_rate, vid_bitrate, var_bitrate);
			av_output.Open(generate_fname());
			*signal = SIG_RECORDING;
		}

		if(record_on_motion && !recording)
		{
			if(silence_timer.elapsed() > min_gap && record_timer.elapsed() > min_record_time)
			{
				*signal = SIG_CHANGE_FILE;
				silence_timer.reset();
				continue;
			}

			capture_frame();
			diffs = measure_motion();

			if(diffs) //record previous and current frames
			{
				recording = true;
				if(record_timer.elapsed() > min_record_time)
					record_timer.reset();
				else record_timer.start();
				silence_timer.reset();

				write_frame(v_buffer1, time(0), diffs);
				write_frame(v_buffer0, time(0), diffs);
			}
		}
		else //record frames
		{
			v_pts = av_output.getVpts();
			a_pts = av_output.getApts();
			if(a_pts < v_pts)
			{
				a_readed = a_source.Read(a_buffer, a_bsize);
				av_output.writeAFrame(a_buffer, a_readed);
			}
			else
			{
				capture_frame();
				if(detect_motion)
					if(diffs = measure_motion())
						silence_timer.reset();
				write_frame(v_buffer0, now, diffs);

				if(silence_timer.elapsed() > post_motion_offset)
					recording = false;
			}
		}

	next_loop: continue;
	}

exit_loop: return true;
}

string Recorder::generate_fname()
{
	char name[256];
	time_t now = time(0);
	strftime(name, 256, fname_format.c_str(), localtime(&now));
	return string(output_dir + name);
}

bool Recorder::write_frame(unsigned char * frame, time_t now, uint diffs)
{
	if(!inited) return false;

	if(print_date)
	{
		string date = ctime(&now);
		date.erase(date.size()-1);
		PrintText(frame, date, width-5-TextWidth(date), height-5, width, height);
	}
	if(print_diffs)
	{
		char diffs_str[10] = {'\0'};
		sprintf(diffs_str, "%d", diffs);
		string str = diffs_str;
		PrintText(frame, diffs_str,	width-5-TextWidth(diffs_str), 10, width, height);
	}

	return av_output.writeVFrame(frame, width, height);
}

uint Recorder::measure_motion( )
{
	if(!inited || !detect_motion) return false;
	_swap(m_buffer0, m_buffer1);
	memcpy(m_buffer0, v_buffer0, v_bsize);

	for(int i = noise_reduction_level; i > 0; i--)
		erode(m_buffer0);

	return fast_diff(m_buffer1, m_buffer0);
}

bool Recorder::capture_frame()
{
	if(!inited) return false;

	_swap(v_buffer0, v_buffer1);
	return v_source.Read(v_buffer0, v_bsize);
}

bool Recorder::time_in_window(time_t now)
{
	if(rec_schedule.empty()) return true;
	tm *tm_now = localtime(&now);
	bool ret = false;

	schedule::iterator i;
	for(i = rec_schedule.begin(); !ret && i != rec_schedule.end(); i++)
		ret = (*i).first <= *tm_now && *tm_now <= (*i).second;

	return ret;
}

int  Recorder::check_time(time_t now)
{
	if(start_time) if(now < start_time) return -1;
	if(end_time)   if(now > end_time)   return  1;
	return 0;
}

uint Recorder::fast_diff( unsigned char * old_img, unsigned char * new_img )
{
	int diffs  = 0;
	int pixels = width*height;
	int step   = pixels/10000;
	if(!step%2) step++;

	for (int i = pixels; i > 0; i -= step)
	{
		/* using a temp variable is 12% faster */
		register unsigned char curdiff= abs(int(*old_img)-int(*new_img));
		if(curdiff > noise_level*512/(1 + *old_img + *new_img))	diffs++;
		old_img+=step;
		new_img+=step;
	}
	diffs -= threshold;
	return (uint) (diffs<0)? 0 : diffs;
}

int Recorder::erode(unsigned char * img)
{
	int y, i, sum = 0;
	char *Row1,*Row2,*Row3;
	Row1 = (char*) erode_tmp;
	Row2 = Row1 + width;
	Row3 = Row1 + 2*width;
	memset(Row2, 0, width);
	memcpy(Row3, img, width);
	for (y = 0; y < height; y++)
	{
		memcpy(Row1, Row2, width);
		memcpy(Row2, Row3, width);
		if (y == height-1)
			memset(Row3, 0, width);
		else
			memcpy(Row3, img+(y+1)*width, width);

		for (i = width-2; i >= 1; i--)
		{
			if (Row1[i]   == 0 ||
			        Row2[i-1] == 0 ||
			        Row2[i]   == 0 ||
			        Row2[i+1] == 0 ||
			        Row3[i]   == 0)
				img[y*width+i] = 0;
			else
				sum++;
		}
		img[y*width] = img[y*width+width-1] = 0;
	}
	return sum;
}
