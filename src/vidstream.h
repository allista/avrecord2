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

#include <sys/mman.h>
#include <linux/videodev.h>

#include "common.h"
#include "utimer.h"

///input sources
enum
{
    IN_TV         = 0,
    IN_COMPOSITE1 = 1,
    IN_COMPOSITE2 = 2,
    IN_SVIDEO     = 3,
    IN_DEFAULT    = 1
};

///input modes
enum
{
    NORM_PAL      = 0,
    NORM_NTSC     = 1,
    NORM_SECAM    = 2,
		NORM_PAL_NC   = 3,
    NORM_DEFAULT  = 0
};

///encapsulates simple grabbing work with v4l device
class Vidstream
{
public:
	Vidstream();
	~Vidstream() { Close(); };

	///opens a video device
	bool Open(char *device, ///< name of the video device to open
						uint w = 640, ///< width of captured images
						uint h = 480, ///< height of captured images
						int source = IN_DEFAULT,  ///< grab source
						int mode   = NORM_DEFAULT ///< grab mode
					 );

	///closes video device
	void Close();

	///grabs an image from the device and stores it in the 'buffer'
	bool Read(unsigned char* buffer, uint bsize);

	///make a simple grab benchmark to determine an average fps value
	double measureFPS(uint frames = 10);

private:
	int  vid_dev;  ///< pointer to opened video device

	unsigned char*   tmp_buffer;  ///< temporary video buffer
	unsigned char*   map_buffer;  ///< temporary mmap buffer
	video_capability vid_cap;     ///< video capabilities
	video_channel    vid_channel; ///< grab source and mode
	video_mbuf       vid_buffer;  ///< device video buffer
	video_mmap       vid_mmap;    ///< video mmap

	uint width;    ///< width of a grabbing image
	uint height;   ///< height of a grabbing image

	//functions
	///converts yuv422 image to yuv420p
	void yuv422_to_yuv420p(unsigned char *dest, unsigned char *src, int w, int h) const;

	///converts rgb24 image to yuv420p
	void rgb24_to_yuv420p(unsigned char *dest, unsigned char *src, int w, int h) const;
};

#endif
