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

#include <string>
#include <list>
using namespace std;

#include "common.h"
#include "sndstream.h"
#include "vidstream.h"
#include "avifile.h"
#include "utimer.h"
#include "configfile.h"


///signals for main loop
enum
{
    SIG_RECORDING,
    SIG_QUIT,
    SIG_CHANGE_FILE
};

class Recorder
{
public:
	Recorder();
	~Recorder() { Close(); };

	///initializes the recorder using given configuration file
	bool Init(string config_file);

	///initializes the recorder using given config object
	bool Init(const ConfigFile &config);

	///cleans all up
	void Close();

	///main record loop (the 'signal' is used to handle recording
	///process from outside the class
	bool RecordLoop(uint *signal);

private:
	bool            inited;    ///< true if all is inited

	Sndstream       a_source;  ///< audio source (soundcard)
	Vidstream       v_source;  ///< video source (video capture card)
	AVIFile         av_output; ///< output file

	uint            a_bsize;   ///< size of audio buffer
	unsigned char * a_buffer;  ///< sound buffer

	uint            v_bsize;   ///< size of video buffers
	unsigned char * v_buffer0; ///< image buffer 0
	unsigned char * v_buffer1; ///< image buffer 1

	unsigned char * m_buffer0; ///< image buffer 0 for motion detection
	unsigned char * m_buffer1; ///< image buffer 1 for motion detection

	unsigned char * erode_tmp; ///< temporary buffer to use with erode function


	//configuration//
	string   output_dir;                ///<
	string   fname_format;              ///<

	//flags
	bool     detect_motion;             ///<
	bool     record_on_motion;          ///<
	bool     print_diffs;               ///<
	bool     print_date;                ///<

	//schedule parameters
	typedef  pair<tm,tm>    t_window;   ///< defines a time window
	typedef  list<t_window> schedule;   ///< simple schedule

	time_t   start_time;                ///<
	time_t   end_time;                  ///<
	schedule rec_schedule;              ///<

	//motion detection parameters
	bool     recording;                 ///<
	uint     min_gap;                   ///<
	uint     min_record_time;           ///<
	uint     post_motion_offset;        ///<

	UTimer   record_timer;              ///<
	UTimer   silence_timer;             ///<

	uint     noise_level;               ///<
	uint     threshold;                 ///<
	uint     noise_reduction_level;     ///<

	//audio/video parameters
	string   video_codec;               ///<
	uint     input_source;              ///<
	uint     input_mode;                ///<
	uint     width;                     ///<
	uint     height;                    ///<
	uint     frame_rate;                ///<
	uint     vid_bitrate;               ///<
	uint     var_bitrate;               ///<
	bool     auto_frate;                ///<

	string   audio_codec;               ///<
	uint     sample_rate;               ///<
	uint     aud_bitrate;               ///<
	uint     channels;                  ///<
	uint     amp_level;                 ///<
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

	///captures video frame and stores it in current video buffer
	bool capture_frame();

	///detects motion using current captured frame and previous one
	uint measure_motion();

	///write video frame to av_output
	bool write_frame(unsigned char *frame, time_t now, uint diffs);

	///generates avi filename according fname_format
	string generate_fname();
};

#endif
