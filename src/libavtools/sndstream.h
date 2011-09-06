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

#include <libconfig.h++>
using namespace libconfig;

#include "common.h"

///soundstream i/o modes
typedef enum snd_io_mode
{
	SND_R,
	SND_W
};

///soundstream pcm formats
typedef enum pcm_fmt
{
	SND_8BIT,
	SND_16BIT
};

///functions for weighting peak values
typedef enum weight_func
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

	///open soundstream in given mode and format
	bool Open(Setting *audio_settings_ptr,       ///< pointer to the audio Setting object (libconfig++)
			  snd_io_mode  mode     = SND_R,     ///< i/o mode
	          pcm_fmt      fmt      = SND_16BIT, ///< pcm format
	          weight_func  weight_f = SND_LIN   ///< weighting function
			 );

	void Close(); ///< close soundstream

	/// read data from soundstram.
	/// Number of readed bytes is returned
	uint Read(void *buffer, uint size);

	/// write data to soundstream.
	/// Number of written bytes is returned
	uint Write(void *buffer, uint size);

	/// it's a peak value after amplification and weighting by the weight function updated on every Read
	uint Peak() const { return peak_level; };

	/// true if i/o operations is allowed
	bool opened() const { return snd_dev != NULL; };

	/// minimum size in bytes of buffer for read operation.
	/// This is the size of the single read period
	uint bsize()  const { return _bsize; };

	/// returns time width of a read period in microseconds
	uint ptime()  const { return _ptime; };

private:
	void   amplify(void *buffer, uint bsize); ///< amplifys data stored in the 'buffer'
	double weight(double sample, double max); ///< weights a value of the samle

	snd_pcm_t *snd_dev;          ///< soundcard structure
	snd_pcm_hw_params_t *params; ///< hardware parameters
	uint   dev_mode;             ///< i/o mode of the sndstream

	uint   rate;                 ///< samples per second
	uint   format;               ///< pcm format
	uint   channels;             ///< number of channels
	double amp_level;            ///< ampliffication level
	double sig_offset;           ///< vertical signal offset
	uint   wieght_func;          ///< math function for weight calculations
	uint   peak_value;           ///< current peak value

	snd_pcm_uframes_t frames;    ///< number of frames in a single read period
	uint  framesize;             ///< size of one frame in bytes
	snd_pcm_uframes_t _bsize;    ///< size of a single read period in bytes
	uint _ptime;                 ///< time width of a read period in microseconds
};

#endif
