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


///This class gathers all input/output classes together
///and provides very simple interface for recording
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

	UTimer   record_timer;              ///< timer, that measures current file duration
	UTimer   silence_timer;             ///< timer, that measures no_motion time

	uint     noise_level;               ///< when detecting a motion, difference between two pixels is
	                                    ///< compared with this value
	uint     threshold;                 ///< we detect a motion when number of motion pixels is grater
	                                    ///< than this value
	uint     noise_reduction_level;     ///< shows, how many times we need to erode captured image to
	                                    ///< remove a noise

	uint     snd_noise_level_func;      ///< function for sound noise level calculation: 0 - linear (default)
                                    	///< 1 - 2pwr root, 2 - 4pwr root, 3 - 8pwr root.
	uint     snd_noise_threshold;       ///< we detect sound noise when noise level calculated with
                                    	///< "noise_level_function" is grater than this value (0 - 1000)


	//audio/video parameters
	string   video_codec;               ///< name of video codec
	uint     input_source;              ///< see the Vidstream class for explanations
	uint     input_mode;                ///< see the Vidstream class for explanations
	uint     width;                     ///< width of the image
	uint     height;                    ///< haight of the image
	uint     frame_rate;                ///< frames per second
	uint     vid_bitrate;               ///< bits per second
	uint     var_bitrate;               ///< quantizer value
	bool     auto_frate;                ///< if true, automaticaly measure frame rate during initialization

	string   audio_codec;               ///< name of audio codec
	uint     sample_rate;               ///< samples per second
	uint     aud_bitrate;               ///< bits per second
	uint     channels;                  ///< number of audio channel
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

	///captures video frame and stores it in current video buffer
	bool capture_frame();

	///detects motion using current captured frame and previous one
	uint measure_motion();

	///write video frame to av_output
	bool write_frame(unsigned char *frame, time_t now, uint diffs, uint level);

	///generates avi filename according fname_format
	string generate_fname();
};

#endif
