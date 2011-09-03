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

#include <sys/mman.h>
#include <linux/videodev2.h>

#include <vector>
using namespace std;

#include <libconfig.h++>
using namespace libconfig;

#include "common.h"
#include "utimer.h"

///tool functions
#define CLEAR(x) memset (&(x), 0, sizeof (x))
static int xioctl(int fd, int request, void *arg);

///io method
typedef enum
{
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
} io_method;

static const char *pallets[] =
{
	"PALLET UNDEFINED",
	"VIDEO_PALETTE_GREY",		/* 1 Linear greyscale */
	"VIDEO_PALETTE_HI240",		/* 2 High 240 cube (BT848) */
	"VIDEO_PALETTE_RGB565",		/* 3 565 16 bit RGB */
	"VIDEO_PALETTE_RGB24", 		/* 4 24bit RGB */
	"VIDEO_PALETTE_RGB32", 		/* 5 32bit RGB */
	"VIDEO_PALETTE_RGB555", 	/* 6 555 15bit RGB */
	"VIDEO_PALETTE_YUV422", 	/* 7 YUV422 capture */
	"VIDEO_PALETTE_YUYV", 		/* 8 */
	"VIDEO_PALETTE_UYVY", 		/* 9 The great thing about standards is ... */
	"VIDEO_PALETTE_YUV420", 	/* 10 */
	"VIDEO_PALETTE_YUV411", 	/* 11 YUV411 capture */
	"VIDEO_PALETTE_RAW", 		/* 12 RAW capture (BT848) */
	"VIDEO_PALETTE_YUV422P",	/* 13 YUV 4:2:2 Planar */
	"VIDEO_PALETTE_YUV411P",	/* 14 YUV 4:1:1 Planar */
	"VIDEO_PALETTE_YUV420P",	/* 15 YUV 4:2:0 Planar */
	"VIDEO_PALETTE_YUV410P",	/* 16 YUV 4:1:0 Planar */
};

static const uint used_pallets[] =
{
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15,
	16
};

///encapsulates simple grabbing work with v4l device
class Vidstream
{
public:
	Vidstream(Setting& _video_settings);
	~Vidstream() { Close(); };

	///open and init video device
	bool Init();

	///closes video device
	void Close();

	///grabs an image from the device and stores it in the 'buffer'
	bool Read(unsigned char* buffer, uint bsize);

	///make a simple grab benchmark to determine an average period duration
	uint ptime(uint frames = 25);

	///returns required video buffer size according to used pallet
	uint bsize() const { return b_size; };

private:
	Setting& video_settings; ///<reference to the video settings object (libconfig++)

	int device;  ///< pointer to opened video device

	v4l2_capability cap;   ///< video capabilities
	v4l2_input input;      ///< grab source
	v4l2_std_id standard;  ///< video standard
	v4l2_format format;    ///< video format

	video_mbuf       mbuf;  ///< device video buffer
	video_mmap       mmap;    ///< video mmap

	v4l2_buffer buffer;

	vector<unsigned char*> map_buffers; ///< mmap buffers
	int                    p_frame;     ///< last prepaired frame

	uint width;    ///< width of a grabbing image
	uint height;   ///< height of a grabbing image
	uint b_size;   ///< pallet speciffic size of video buffer

	//functions
	///make an ioctl call to device safe from signal interuption
	int xioctl(int request, void *arg);

	///prepairs to capture a frame 'n'
	bool prepare_frame(int fnum);

	///captures a frame 'n'
	bool capture_frame(int fnum);

	///converts yuv422 image to yuv420p
	static void yuv422_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h);

	///converts rgb24 image to yuv420p
	static void rgb24_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h);
};

#endif
