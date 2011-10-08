/***************************************************************************
 *   Copyright (C) 2007 by Allis Tauri   *
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
#ifndef JPEGCONVERTER_H
#define JPEGCONVERTER_H

/**
This utility class encapsulates libjpeg conversion routine.

	@author Allis Tauri <allista@gmail.com>
*/

extern "C"
{
#  define INT64_C(c)	c ## LL
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <jpeglib.h>

#include <libconfig.h++>
using namespace libconfig;

#include <setjmp.h>

#include <vector>
using namespace std;

#include <common.h>

class JpegConverter
{
private:
	enum
	{
		JPEG_INITED = 0x01,
  		INITED = 0x80
	};

	struct buffer_dest
	{
		jpeg_destination_mgr    dest_mgr;
		uint8_t*                work_buffer;
		vector<uint8_t>*        out_buffer;
	};
	static void init_destination(j_compress_ptr _cinfo);
	static boolean empty_output_buffer(j_compress_ptr _cinfo);
	static void term_destination(j_compress_ptr _cinfo);

	struct log_err
	{
		jpeg_error_mgr err_mgr;
		jmp_buf jump_buffer;
	};
	static void error_exit(j_common_ptr _cinfo);
	static void output_message(j_common_ptr _cinfo);

public:
    JpegConverter();
    ~JpegConverter();

	bool Init(uint _in_fmt, Setting* video_settings_ptr, Setting* webcam_settings_ptr);
	void Close();

	int  Convert(uint8_t* &out_frame, uint8_t* in_frame, uint in_len);

private:
	static const J_COLOR_SPACE jcs   = JCS_RGB;
	static const PixelFormat out_fmt = PIX_FMT_RGB24;
	static const uint JPEG_WORK_BUFFER_SIZE = 8192;

	int inited;

	uint        width;
	uint        height;
	uint8_t*    tmp_buffer;
	uint        tmp_buffer_len;
	AVFrame*    in_frame;
	AVFrame*    out_frame;
	SwsContext* sws;
	PixelFormat in_fmt;

	uint            quality;
	vector<uint8_t> out_buffer;
	buffer_dest*    work_buffer;
	log_err         error_logger;
	jpeg_compress_struct cinfo;
};

#endif
