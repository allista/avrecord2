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
#include <limits.h>
#include <math.h>
#include <iostream>
using namespace std;

#include "sndstream.h"

Sndstream::Sndstream()
{
	snd_dev   = NULL;
	params    = NULL;

	frames    = 0;
	format    = SND_PCM_FORMAT_UNKNOWN;
	framesize = 0;
	rate      = 0;
	channels  = 0;
	dev_mode  = 0;

	weight_function = SND_LIN;
	amp_level   = 1;
	sig_offset  = 0;
	peak_value  = 0;

	_bsize    = 0;
	_ptime    = 0;
}

bool Sndstream::Open(Setting *audio_settings_ptr,
					 snd_io_mode  mode, pcm_fmt fmt, weight_func weight_f)
{
	if(!audio_settings_ptr) return false;
	Setting &audio_settings = *audio_settings_ptr;

	int ret     = false;
	int dir     = 0;
	sig_offset  = 0;
	dev_mode    = mode;
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
		log_message(0, "Sndstream: no <amplification_level> setting was found. Using default: 1.");
		amp_level = 1;
	}
	amp_level = (amp_level <= 0)? 1 : amp_level;

	/* Open PCM device. */
	switch(dev_mode)
	{
		case SND_R:
			ret = snd_pcm_open(&snd_dev, "default", SND_PCM_STREAM_CAPTURE, 0);
			break;

		case SND_W:
			ret = snd_pcm_open(&snd_dev, "default", SND_PCM_STREAM_PLAYBACK, 0);
			break;

		default:
			log_message(1, "Sndstream: unknown open mode");
			return false;
	}

	if(ret < 0)
	{
		log_message(1, "Sndstream: unable to open pcm device: %s", snd_strerror(ret));
		return false;
	}

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(snd_dev, params);

	/* Set the desired hardware parameters. */
	/* Interleaved mode */
	snd_pcm_hw_params_set_access(snd_dev, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	/* Set sample format */
	snd_pcm_format_t pcm_format;
	switch(format)
	{
		case SND_8BIT:
			pcm_format = SND_PCM_FORMAT_S8;
			snd_pcm_hw_params_set_format(snd_dev, params, pcm_format);
			break;

		case SND_16BIT:
			pcm_format = SND_PCM_FORMAT_S16;
			snd_pcm_hw_params_set_format(snd_dev, params, pcm_format);
			break;

		default:
			log_message(1, "Sndstream: unknown format");
			return false;
	}

	/* Set channels number */
	ret = snd_pcm_hw_params_set_channels(snd_dev, params, channels);
	if(ret < 0)
	{
		log_message(1, "Sndstream: unable to set channels number: %s", snd_strerror(ret));
		return false;
	}

	/* Set sampling rate */
	ret = snd_pcm_hw_params_set_rate_near(snd_dev, params, &rate, &dir);
	if(ret < 0)
	{
		log_message(1, "Sndstream: unable to set sample rate near: %s", snd_strerror(ret));
		return false;
	}

	/* Set period size to 0 for default. */
	try { frames = audio_settings["buffer_lenght"]; }
	catch(...)
	{
		log_message(1, "Sndstream: no buffer_longht setting. Setting to auto.");
		frames = 0;
	}
	ret = snd_pcm_hw_params_set_period_size_near(snd_dev, params, &frames, &dir);
	if(ret < 0)
	{
		log_message(1, "Sndstream: unable to set period size: %s.",	snd_strerror(ret));
		return false;
	}

	/* Calculate one period buffer size and period time */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	snd_pcm_hw_params_get_period_time(params, &_ptime, &dir);
	framesize = snd_pcm_format_size(pcm_format, channels);
	_bsize = frames * framesize;

	/* Write the parameters to the driver */
	ret = snd_pcm_hw_params(snd_dev, params);
	if(ret < 0)
	{
		log_message(1, "Sndstream: unable to set hw parameters: %s", snd_strerror(ret));
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
	//snd_pcm_hw_params_free(params);

	snd_dev = NULL;
	params  = NULL;
	sig_offset = 0;
}

uint Sndstream::Read(void* buffer, uint size)
{
	if(!opened() || dev_mode != SND_R) return false;
	if(size < _bsize)
	{
		log_message(1, "Sndstream: read buffer too small - needed %d bytes buffer", _bsize);
		return 0;
	}

	int ret = false;
	ret = snd_pcm_readi(snd_dev, buffer, frames);
	if(ret == -EAGAIN) return 0;
	else if(ret == -EPIPE)
	{
		/* EPIPE means overrun */
		if(snd_pcm_prepare(snd_dev) < 0)
			log_message(1, "Sndstream: cant recover from overrun");
		return 0;
	}
	else if(ret < 0)
	{
		log_message(1, "Sndstream: error from read: %s", snd_strerror(ret));
		return 0;
	}
	else if(ret < (int)frames)
	{
		log_message(1, "Sndstream: short read, read %d frames", ret);
		size = ret*framesize;
	}

	amplify(buffer, size);

	return size;
}

uint Sndstream::Write(void* buffer, uint size)
{
	if(!opened() || dev_mode != SND_W) return false;

	int ret = snd_pcm_writei(snd_dev, buffer, size/framesize);
	if(ret == -EAGAIN) return 0;
	else if(ret == -EPIPE)
	{
		/* EPIPE means underrun */
		if(snd_pcm_prepare(snd_dev) < 0)
			log_message(1, "Sndstream: cant recover from underrun");
		return 0;
	}
	else if(ret < 0)
	{
		log_message(1, "Sndstream: error from writei: %s", snd_strerror(ret));
		return false;
	}

	return ret*framesize;
}

void Sndstream::amplify(void *buffer, uint bsize)
{
	char  *cb = NULL;
	short *sb = NULL;
	uint   size = 0;
	long   max  = 0;
	long   cur  = 0;

	switch(format)
	{
		case SND_8BIT:
			cb		= (char*)buffer;
			size	= bsize/sizeof(char);

			if(!sig_offset)
			{
				for(int i = 0; i < size; i++)
					sig_offset += cb[i]; sig_offset /= size;
			}

			for(int i = 0; i < size; i++)
			{
				cur		= (cb[i] - sig_offset) * amp_level;
				if(labs(cur) > CHAR_MAX*0.9)
					cur = (cur > 0)? CHAR_MAX*0.9 : CHAR_MAX*(-0.9);
				cb[i]	= char(cur);
				if(abs(cb[i]) > max) max = abs(cb[i]);
			}
			max = weight(max, CHAR_MAX);

			break;


		case SND_16BIT:
			sb  = (short*)buffer;
			size = bsize/sizeof(short);

			if(!sig_offset)
			{
				for(int i = 0; i < size; i++)
					sig_offset += sb[i]; sig_offset /= size;
			}

			for(int i = 0; i < size; i++)
			{
				cur		= (sb[i] - sig_offset) * amp_level;
				if(labs(cur) > SHRT_MAX*0.9)
					cur = (cur > 0)? SHRT_MAX*0.9 : SHRT_MAX*(-0.9);
				sb[i]   = short(cur);
				if(abs(sb[i]) > max) max = abs(sb[i]);
			}
			max = weight(max, SHRT_MAX);

			break;
	}

	peak_value = uint(max);
}

double Sndstream::weight( double sample, double max )
{
	switch(weight_function)
	{
		case SND_LIN:
			return sample * 1000 / max;
		case SND_RT2:
			return sqrt(sample / max)*1000;
		case SND_RT4:
			return sqrt(sqrt(sample / max))*1000;
		case SND_RT8:
			return sqrt(sqrt(sqrt(sample / max)))*1000;
		case SND_PWR2:
			return pow(sample / max, 2)*1000;
		case SND_PWR4:
			return pow(sample / max, 4)*1000;
		case SND_PWR8:
			return pow(sample / max, 8)*1000;
		default:
			return sample * 1000 / max;
	}
}
