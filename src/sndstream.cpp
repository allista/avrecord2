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
	params    = NULL;

	frames    = 0;
	format    = SND_PCM_FORMAT_UNKNOWN;
	framesize = 0;
	rate      = 0;
	channels  = 0;
	dev_mode  = 0;

	amp_level = 1;

	_bsize    = 0;
	_ptime    = 0;
}

bool Sndstream::Open(int mode, uint fmt, uint chans, uint rte)
{
	int ret  = false;
	format   = fmt;
	rate     = rte;
	channels = chans;
	dev_mode = mode;

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

	/* Sen channels number */
	snd_pcm_hw_params_set_channels(snd_dev, params, channels);

	/* Set sampling rate */
	snd_pcm_hw_params_set_rate_near(snd_dev, params, &rate, NULL);

	/* Set period size to 0 for default. */
	frames = 0;
	snd_pcm_hw_params_set_period_size_near(snd_dev, params, &frames, NULL);

	/* Write the parameters to the driver */
	ret = snd_pcm_hw_params(snd_dev, params);
	if(ret < 0)
	{
		log_message(1, "Sndstream: unable to set hw parameters: %s", snd_strerror(ret));
		exit(1);
	}

	/* Calculate one period buffer size and period time */
	snd_pcm_hw_params_get_period_size(params, &frames, NULL);
	snd_pcm_hw_params_get_period_time(params, &_ptime, NULL);
	framesize = snd_pcm_format_size(pcm_format, channels);
	_bsize = frames * framesize;

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
}

uint Sndstream::Read(void* buffer, uint* size)
{
	if(!opened() || dev_mode != SND_R) return false;
	if(*size < _bsize)
	{
		fprintf(stderr, "read buffer too small - needed %d bytes buffer\n", _bsize);
		return false;
	}

	int ret = false;
	ret = snd_pcm_readi(snd_dev, buffer, frames);
	if (ret == -EPIPE)
	{
		/* EPIPE means overrun */
		fprintf(stderr, "overrun occurred\n");
		snd_pcm_prepare(snd_dev);
		return false;
	}
	else if(ret < 0)
	{
		fprintf(stderr, "error from read: %s\n", snd_strerror(ret));
		return false;
	}
	else if(ret < (int)frames)
	{
		fprintf(stderr, "short read, read %d frames\n", ret);
		*size = ret*framesize;
	}

	amplify(buffer, *size);

	return ret*channels;
}

uint Sndstream::Write(void* buffer, uint* size)
{
	if(!opened() || dev_mode != SND_W) return false;

	int ret = snd_pcm_writei(snd_dev, buffer, *size/framesize);
	if(ret == -EPIPE)
	{
		/* EPIPE means underrun */
		fprintf(stderr, "underrun occurred\n");
		snd_pcm_prepare(snd_dev);
		return false;
	}
	else if(ret < 0)
	{
		fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
		return false;
	}

	return ret*channels;
}

void Sndstream::amplify(void* buffer, uint bsize)
{
	if(amp_level == 1) return;

	uint size = 0;
	switch(format)
	{
		case SND_8BIT:
			char* cb  = (char*)buffer;
			size = bsize/sizeof(char);
			for(int i = 0; i < size; i++)
				cb[i] = char(cb[i] * amp_level);
			break;
		case SND_16BIT:
			short* sb = (short*)buffer;
			size = bsize/sizeof(short);
			for(int i = 0; i < size; i++)
				sb[i] = short(sb[i] * amp_level);
			break;
	}
}
