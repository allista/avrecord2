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

// C++ Interface: vidstream

#ifndef VIDSTREAM_H
#define VIDSTREAM_H

typedef unsigned long long __u64;
typedef long long __s64;

#include <time.h>
#include <linux/videodev2.h>

#include <vector>
using namespace std;

#include <libconfig.h++>
using namespace libconfig;

#include "common.h"
#include "utimer.h"

///supported pixel formats
///note, that all these formats are planar, I don't use packed ones because of the text rendering function: for all the planar formats one single function is enough, while each packed format requires it's own function.
static const uint pixel_formats[] =
{
	V4L2_PIX_FMT_YUV422P,
	V4L2_PIX_FMT_YUV420,
	V4L2_PIX_FMT_YUV411P,
	V4L2_PIX_FMT_YUV410,
	V4L2_PIX_FMT_GREY
};

///names of the supported formats
static const char *pixel_format_names[] =
{
	"V4L2_PIX_FMT_YUV422P",
	"V4L2_PIX_FMT_YUV420",
	"V4L2_PIX_FMT_YUV411P"
	"V4L2_PIX_FMT_YUV410",
	"V4L2_PIX_FMT_GREY"
};

enum io
{
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR
};

///encapsulates simple grabbing work with v4l device
class Vidstream
{
public:
	Vidstream();
	~Vidstream() { Close(); };

	///open and init video device
	bool Open(Setting *video_settings_ptr ///<pointer to the video settings object (libconfig++)
			 );

	///closes video device
	void Close();

	///initialize capture sequence
	bool StartCapture();

	///uninitialize capture sequence
	bool StopCapture();

	///grabs an image from the device and stores it in the 'buffer'
	int  Read(image_buffer &buffer);

	///pixel format currently in use
	uint pixel_format() const { return pix_fmt; }

	///used for frame rate/interval calculations
	uint frameperiod_numerator() const { return standard.frameperiod.numerator; }

	///used for frame rate/interval calculations
	uint frameperiod_demoninator() const { return standard.frameperiod.denominator; }

	///is it needed at all?
	uint frame_rate() const
	{ return standard.frameperiod.denominator/standard.frameperiod.numerator ;}

	///timespan between frames (sec)
	double frame_interval() const
	{ return (double)standard.frameperiod.numerator/standard.frameperiod.denominator; }

private:
	int  device;    ///< pointer to opened video device
	io   io_method; ///<input/output method used
	uint pix_fmt;   ///<chosen pixel format
	uint width;     ///< width of an image
	uint height;    ///< height of an image

	v4l2_capability cap;     ///< video capabilities
	v4l2_input input;        ///< grab source
	v4l2_standard standard; ///< video standard
	v4l2_format format;      ///< video format

	static const uint NUM_BUFFERS = 4; ///<number of image buffers to request from driver. Hardcoded for now.
	vector<image_buffer> buffers;      ///< image buffers

	//functions
	///make an ioctl call to device safe from signal interuption
	int xioctl(int request, void *arg);

	///prepairs to capture a frame 'n'
	bool fill_buffer(image_buffer &buffer, void *start, uint length, timeval timestamp);

	///converts yuv422 image to yuv420p
	//static void yuv422_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h);

	///converts rgb24 image to yuv420p
	//static void rgb24_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h);
};

#endif
