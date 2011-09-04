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

/** @author Allis */

#ifndef AVIFILE_H
#define AVIFILE_H

extern "C"
{
#  define INT64_C(c)	c ## LL
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include<libconfig++.h>

#include "common.h"
#include "fifo.h"

static const uint *av_pixel_formats[] =
{
	PIX_FMT_YUV422P,
	PIX_FMT_YUV420P,
	PIX_FMT_YUV411P
	PIX_FMT_YUV410P,
	PIX_FMT_GREY8
};

///encapsulates simple ffmpeg work with avi file
class AVIFile
{
public:
	AVIFile(Setting& _video_settings, Setting& _audio_settings);
	~AVIFile() { if(opened()) Close(); else cleanup(); };

	///allocate output context, set audio and video parameters
	bool Init();

	///sets parameters of the video codec
	bool setVParams(uint numerator,   ///<frame interval numerator (determined by video standard)
					uint denomenator, ///<frame interval denomenator (determined by video standard)
	 				uint pix_fmt      ///<pixel format used in video capture
				   );

	///sets parameters of the audio codec
	bool setAParams();

	///open avi file and write out the header
	bool Open(string filename);

	///true if the file is opened
	bool opened() const { return _opened & INIT_FULL; };

	///returns pts value (in seconds) of the video stream
	double getVpts() const;

	///returns pts value (in seconds) of the audio stream
	double getApts() const;

	///write video frame to the file
	bool writeVFrame(image_buffer& buffer);

	///write audio frame to the file
	bool writeAFrame(uint8_t *samples,    ///< audio data
	                 uint size            ///< audio data size in bytes
	                );

	///write avi trailer and close the file
	void Close();

private:
	//predefinitions
	///taked from "ffmpeg" program (ffmpeg library)
	static const uint MAX_AUDIO_PACKET_SIZE = (128 * 1024);

	///init stages
	enum
	{
	    INIT_NONE   = 0x000,
	    INIT_VCODEC = 0x001,
	    INIT_ACODEC = 0x002,
	    INIT_FOPEN  = 0x004,
	    INIT_FULL   = 0x008
	};
	uint _opened; ///< bitflag that shows what was inited already

	Setting& video_settings; ///<reference to the video settings object (libconfig++)
	Setting& audio_settings; ///<reference to the audio settings object (libconfig++)

	//file streams and codecs
	AVFormatContext *o_file;  ///< output file context
	AVStream        *vstream; ///< output video stream
	AVStream        *astream; ///< output audio stream
	AVCodecContext  *vcodec;  ///< video codec
	AVCodecContext  *acodec;  ///< audio codec
	Fifo<uint8_t>    afifo;   ///< audio fifo for writing with framesize sensitive codecs (mp2, mp3...)
	uint             a_fsize; ///< audio codec frame size

	//video Output
	uint      v_bsize; ///< video output buffer size
	uint8_t  *vbuffer; ///< video output buffer

	//audio Output
	uint      a_bsize; ///< audio output buffer size
	uint8_t  *abuffer; ///< audio output buffer

	uint   vbr;        ///< variable bitrate setting


	//functions
	void cleanup();    ///< cleans up all structures

	///gets output codec ID according to given name and type
	CodecID get_codec_id(string name, int codec_type) const;
};

#endif
