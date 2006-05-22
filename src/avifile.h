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
#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>
}

#include "common.h"
#include "fifo.h"

///encapsulates simple ffmpeg work with avi file
class AVIFile
{
public:
	AVIFile();
	~AVIFile() { if(opened()) Close(); else cleanup(); };

	///allocates output context
	bool Init();

	///sets parameters of the video codec
	bool setVParams(string codec_name,  ///< name of video codec
	                uint  width,
	                uint  height,
	                uint  rate,         ///< frames per second
	                uint  bps,          ///< bits per second
	                uint  _vbr          ///< variable bitrate
	                                    ///< (0 for constant, or 2-31 for variable quality (2 is the best))
	               );

	///sets parameters of the audio codec
	bool setAParams(string codec_name, ///< name of audio codec
	                uint channels,      ///< number of channels
	                uint rate,          ///< samples per second
	                uint bps            ///< bits per second
	               );

	///opens avi file and writeout the header
	bool Open(string filename);

	///true if the file is opened
  bool opened() const { return _opened & INIT_FULL; };

	///returns pts value (in seconds) of the video stream
	double getVpts() const;

	///returns pts value (in seconds) of the audio stream
	double getApts() const;

	///write video frame to the file
	bool writeVFrame(unsigned char *img, ///< rgb image data
	                 uint width,          ///< width of the image
	                 uint height          ///< height of the image
	                );

	///write audio frame to the file
	bool writeAFrame(uint8_t *samples,   ///< audio data
	                 uint size            ///< audio data size in bytes
	                );

	///writes avi trailer and closes the file
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

	//file streams and codecs
	AVFormatContext *o_file;  ///< output file context
	AVStream        *vstream; ///< output video stream
	AVStream        *astream; ///< output audio stream
	AVCodecContext  *vcodec;  ///< video codec
	AVCodecContext  *acodec;  ///< audio codec
	Fifo             afifo;   ///< audio fifo for framesize sensitive codecs (mp2, mp3...)

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

	///returns frame size of an audio codec if
	///one exests and 0 if it doesn't
	uint aframe_size() const;
};

#endif
