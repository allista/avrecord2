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

#include <math.h>
#include <iostream>
using namespace std;

#include "sndstream.h"

Sndstream::Sndstream()
{
	snd_dev   = NULL;
	hw_params = NULL;
	sw_params = NULL;

	frames    = 0;
	format    = SND_PCM_FORMAT_UNKNOWN;
	framesize = 0;
	rate      = 0;
	channels  = 0;

	weight_function = SND_LIN;
	amp_level   = 1;
	peak_value  = 0;

	_bsize    = 0;
	_ptime    = 0;
}

bool Sndstream::Open(Setting *audio_settings_ptr, pcm_fmt fmt, weight_func weight_f)
{
	if(!audio_settings_ptr) return false;
	Setting &audio_settings = *audio_settings_ptr;

	int ret     = 0;
	int dir     = 0;
	format      = fmt;
	weight_function = weight_f;

	try
	{
		rate       = audio_settings["sample_rate"];
		channels   = audio_settings["channels"];
	}
	catch(SettingNotFoundException)
	{
		log_message(0, "Sndstream: no <sample_rate>, <channels> or setting was found.");
		return false;
	}

	try { amp_level  = audio_settings["amplification_level"]; }
	catch(SettingNotFoundException)
	{
		log_message(0, "Sndstream: no <amplification_level> setting was found. Using default: 1.0");
		amp_level = 1;
	}
	catch(SettingTypeException)
	{
		log_message(1, "Sndstream: <amplification_level> setting must be a floating point value. Using default: 1.0");
		amp_level = 1;
	}
	amp_level = (amp_level <= 0)? 1 : amp_level;

	/* Open PCM device. */
	if((ret = snd_pcm_open(&snd_dev, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0)
	{
		log_message(1, "Sndstream: unable to open pcm device: %s", snd_strerror(ret));
		return false;
	}

	/* Allocate a hardware parameters object. */
	if((ret = snd_pcm_hw_params_malloc(&hw_params)) < 0)
	{
		log_message(1, "Sndstream: unable to allocate hw_params: %s", snd_strerror(ret));
		return false;
	}

	/* Fill it in with default values. */
	if((ret = snd_pcm_hw_params_any(snd_dev, hw_params)) < 0)
	{
		log_message(1, "Sndstream: unable to initialize hw_params: %s", snd_strerror(ret));
		return false;
	}

	/* set interleaved mode */
	if((ret = snd_pcm_hw_params_set_access(snd_dev, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		log_message(1, "Sndstream: unable to set access mode type: %s", snd_strerror(ret));
		return false;
	}

	/* Set sample format */
	snd_pcm_format_t pcm_format;
	switch(format)
	{
		case SND_8BIT:
			pcm_format = SND_PCM_FORMAT_S8;
			snd_pcm_hw_params_set_format(snd_dev, hw_params, pcm_format);
			break;

		case SND_16BIT:
			pcm_format = SND_PCM_FORMAT_S16;
			snd_pcm_hw_params_set_format(snd_dev, hw_params, pcm_format);
			break;

		default:
			log_message(1, "Sndstream: unknown format");
			return false;
	}

	/* Set channels number */
	if((ret = snd_pcm_hw_params_set_channels(snd_dev, hw_params, channels)) < 0)
	{
		log_message(1, "Sndstream: unable to set channels number: %s", snd_strerror(ret));
		return false;
	}

	/* Set sampling rate */
	if((ret = snd_pcm_hw_params_set_rate_near(snd_dev, hw_params, &rate, &dir)) < 0)
	{
		log_message(1, "Sndstream: unable to set sample rate near: %s", snd_strerror(ret));
		return false;
	}

	/* Set period size to 0 for default. */
	try { frames = audio_settings["buffer_length"]; }
	catch(...)
	{
		log_message(1, "Sndstream: no buffer_length setting. Setting to auto.");
		frames = 0;
	}
	if((ret = snd_pcm_hw_params_set_period_size_near(snd_dev, hw_params, &frames, &dir)) < 0)
	{
		log_message(1, "Sndstream: unable to set period size: %s.",	snd_strerror(ret));
		return false;
	}

	/* Calculate one period buffer size and period time */
	snd_pcm_hw_params_get_period_size(hw_params, &frames, &dir);
	snd_pcm_hw_params_get_period_time(hw_params, &_ptime, &dir);
	framesize = snd_pcm_format_size(pcm_format, channels);
	_bsize = frames * framesize;
	cur_ptime = _ptime;

	/* Write the parameters to the driver */
	if((ret = snd_pcm_hw_params(snd_dev, hw_params)) < 0)
	{
		log_message(1, "Sndstream: unable to set hw parameters: %s", snd_strerror(ret));
		return false;
	}

	//free snd_params structure
	//snd_pcm_hw_params_free(hw_params);
	/*
	strangly this call triggers the message:
	*** glibc detected *** avrtuner: free(): invalid pointer: 0x00007fffe058fc00 ***
	*/

	//set software parameters
	if((ret = snd_pcm_sw_params_malloc(&sw_params)) < 0)
	{
		log_message(1, "Sndstream: cannot allocate sw parameters: %s", snd_strerror(ret));
		return false;
	}
	if((ret = snd_pcm_sw_params_current (snd_dev, sw_params)) < 0)
	{
		log_message(1, "Sndstream: cannot initialize sw parameters: %s", snd_strerror(ret));
		return false;
	}
	if((ret = snd_pcm_sw_params_set_avail_min (snd_dev, sw_params, frames)) < 0)
	{
		log_message(1, "Sndstream: cannot set minimum available count: %s", snd_strerror(ret));
		return false;
	}
	if((ret = snd_pcm_sw_params_set_start_threshold(snd_dev, sw_params, frames)) < 0)
	{
		log_message(1, "Sndstream: cannot set start mode: %s", snd_strerror(ret));
		return false;
	}
	if((ret = snd_pcm_sw_params(snd_dev, sw_params)) < 0)
	{
		log_message(1, "Sndstream: cannot set software parameters: %s", snd_strerror(ret));
		exit (1);
	}

	//prepare the device for capture
	ret = snd_pcm_prepare(snd_dev);
	if (ret < 0)
	{
		log_message(1, "Sndstream: cannot prepare audio interface for use: %s", snd_strerror(ret));
		return false;
	}

	//all is done
	return true;
}

void Sndstream::Close()
{
	if(!opened()) return;

	snd_pcm_drain(snd_dev);
	snd_pcm_close(snd_dev);

	snd_dev    = NULL;
	hw_params  = NULL;
	sw_params  = NULL;
}

int Sndstream::Read(void* buffer, uint size)
{
	if(!opened()) return 0;
	if(size < _bsize)
	{
		log_message(1, "Sndstream: read buffer too small - needed at least %d bytes buffer", _bsize);
		return -1;
	}

	cur_ptime = _ptime;
	frames_available = snd_pcm_readi(snd_dev, buffer, frames);
	if(frames_available == -EAGAIN) return 0;
	else if(frames_available== -EPIPE)
	{
		/* EPIPE means overrun */
		if(snd_pcm_prepare(snd_dev) < 0)
			log_message(1, "Sndstream: cant recover from overrun");
		else log_message(0, "Sndstream: recovered from overrun");
		return 0;
	}
	else if(frames_available < 0)
	{
		log_message(1, "Sndstream: error from read: %s", snd_strerror(frames_available));
		return 0;
	}
	else if(frames_available < frames)
	{
		log_message(0, "Sndstream: short read, read %d frames", frames_available);
		size = frames_available*framesize;
		cur_ptime = (uint)(_ptime*((double)frames_available/frames));
	}
	amplify(buffer, frames_available);

	return size;
}


void Sndstream::amplify(void *buffer, uint num_frames)
{
	char  *cb = NULL;
	short *sb = NULL;
	long   max  = 0;
	long   cur  = 0;

	switch(format)
	{
		case SND_8BIT:
			cb		= (char*)buffer;
			for(int i = 0; i < num_frames; i++)
			{
				cur		= cb[i] * amp_level;
				if(labs(cur) > SND_8BIT_MAX)
					cur = (cur > 0)? SND_8BIT_MAX : -1*SND_8BIT_MAX;
				cb[i]	= char(cur);
				if(abs(cb[i]) > max) max = abs(cb[i]);
			}
			max = weight(max, SND_8BIT_MAX);
			break;


		case SND_16BIT:
			sb  = (short*)buffer;
			for(int i = 0; i < num_frames; i++)
			{
				cur		= sb[i] * amp_level;
				if(labs(cur) > SND_16BIT_MAX)
					cur = (cur > 0)? SND_16BIT_MAX : -1*SND_16BIT_MAX;
				sb[i]   = short(cur);
				if(abs(sb[i]) > max) max = abs(sb[i]);
			}
			max = weight(max, SND_16BIT_MAX);
			break;
	}

	peak_value = uint(max);
}

double Sndstream::weight( double sample, double max )
{
	switch(weight_function)
	{
		case SND_LIN:
			return (sample / max)*SND_PEAK_MAX;
		case SND_RT2:
			return sqrt(sample / max)*SND_PEAK_MAX;
		case SND_RT4:
			return sqrt(sqrt(sample / max))*SND_PEAK_MAX;
		case SND_RT8:
			return sqrt(sqrt(sqrt(sample / max)))*SND_PEAK_MAX;
		case SND_PWR2:
			return pow(sample / max, 2)*SND_PEAK_MAX;
		case SND_PWR4:
			return pow(sample / max, 4)*SND_PEAK_MAX;
		case SND_PWR8:
			return pow(sample / max, 8)*SND_PEAK_MAX;
		default:
			return (sample / max)*SND_PEAK_MAX;
	}
}
