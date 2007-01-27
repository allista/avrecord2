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
/**
@author Allis
*/

#ifndef SNDSTREAM_H
#define SNDSTREAM_H

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#include "common.h"

///sounstream i/o modes
enum
{
	SND_R,
	SND_W
};

///sounstream pcm formats
enum
{
	SND_8BIT,
	SND_16BIT
};


///functions for signal level calculation
enum
{
	 SND_LIN,
	 SND_RT2,
	 SND_RT4,
	 SND_RT8,
	 SND_PWR2,
	 SND_PWR4,
	 SND_PWR8
};

///encapsulates alsa api to soundcard.
class Sndstream
{
public:
	Sndstream();
	~Sndstream() { Close(); };

	///opens soundstream in given mode and format
	bool Open(int  mode  = SND_R,     ///< i/o mode
	          uint fmt   = SND_16BIT, ///< pcm format
	          uint chans = 2,         ///< number of channels
	          uint rte   = 44100      ///< samples per second
	         );

	void Close(); ///< closes soundstream


	/// reads data from soundstram.
	/// number of readed bytes is returned
	uint Read(void* buffer, uint size);

	/// writes data to soundstream.
	/// number of written bytes is returned
	uint Write(void* buffer, uint size);

	/// returns "noise_detected flag"
	/// this flag is set by amplify function on every Read
	/// signal level is grater than threshold
	uint Noise() const { return noise_level; };


	/// sets ampliffication level of READED data
	void setAmp(double amp) { amp_level = (amp <= 0)? 1 : amp; };

	/// sets threshold level of READED data
	void setLev(uint level) { threshold = level; };

	/// sets function for signal level calculation
	void setFun(uint func) { level_func = func; };


	/// returns true if i/o operations is allowed
	bool opened() const { return snd_dev != NULL; };

	/// returns minimum size in bytes of buffer for read operation.
	/// this is the size of the single read period
	uint bsize()  const { return _bsize; };

	/// returns time width of a read period in microseconds
	uint ptime()  const { return _ptime; };

private:
	void   amplify(void* buffer, uint bsize); ///< amplifys data stored in the 'buffer'
	double level(double sample, double max);  ///< calculates signal level using selected math function

	snd_pcm_t* snd_dev;          ///< soundcard structure
	snd_pcm_hw_params_t* params; ///< hardware parameters
	uint   dev_mode;             ///< i/o mode of the sndstream

	uint   rate;                 ///< samples per second
	uint   format;               ///< pcm format
	uint   channels;             ///< number of channels
	double amp_level;            ///< ampliffication level
	double sig_offset;           ///< vertical signal offset
	uint   threshold;            ///< signal threshold for "record on noise"
	int    level_func;           ///< math function for level calculations
	uint   noise_level;          ///< level of detected noise

	snd_pcm_uframes_t frames;    ///< number of frames in a single read period
	uint  framesize;             ///< size of one frame in bytes
	uint _bsize;                 ///< size of a single read period in bytes
	uint _ptime;                 ///< time width of a read period in microseconds
};

#endif
