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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "vidstream.h"

Vidstream::Vidstream()
{
	vid_dev    = -1;
	tmp_buffer = NULL;
	map_buffer = NULL;

	width      = 0;
	height     = 0;
}

bool Vidstream::Open(char * device, uint w, uint h, int source, int mode)
{
	width  = w;
	height = h;

	//open the video4linux device
	vid_dev = open(device, O_RDWR);
	if(vid_dev == -1)
	{
		cerr << "Can't open device: " << device << endl;
		return false;
	}

	//get the capabilities
	if(ioctl(vid_dev, VIDIOCGCAP, &vid_cap) == -1)
	{
		cerr << "ioctl: getting capability failed" << endl;
		Close();
		return false;
	}

	//setup grab source and mode
	vid_channel.channel = source;
	vid_channel.norm    = mode;
	if(ioctl(vid_dev, VIDIOCSCHAN, &vid_channel) == -1)
	{
		cout << "ioctl: setting video channel failed" << endl;
		Close();
		return false;
	}

	//set device video buffer using norma 'read'
	tmp_buffer = new unsigned char[width*height*3];
	if(read(vid_dev, tmp_buffer, width*height*3) <= 0)
	{
		cout << "unable to read from device to set video buffer" << endl;
		Close();
		return false;
	}
	if(ioctl(vid_dev, VIDIOCGMBUF, &vid_buffer) == -1)
	{
		cout << "unable to set device video buffer" << endl;
		Close();
		return false;
	}

	//init mmap
	map_buffer = (unsigned char*)mmap(0, vid_buffer.size, PROT_READ|PROT_WRITE, MAP_SHARED, vid_dev, 0);
	if(int(map_buffer) == -1)
	{
		cerr << "mmap() failed" << endl;
		Close();
		return false;
	}

	//setup mmap
	vid_mmap.format = VIDEO_PALETTE_YUV420P;
	vid_mmap.frame  = 0;
	vid_mmap.width  = width;
	vid_mmap.height = height;
	if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
	{
		cerr << "Faild with YUV20P, trying YUV422 palette" << endl;
		vid_mmap.format = VIDEO_PALETTE_YUV422;;
		/* Try again... */
		if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
		{
			cerr << "Failed with YUV422, trying RGB24 palette" << endl;
			vid_mmap.format = VIDEO_PALETTE_RGB24;
			/* Try again... */
			if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
			{
				cerr << "Failed with RGB24, trying GREYSCALE palette" << endl;
				vid_mmap.format = VIDEO_PALETTE_GREY;
				/* Try one last time... */
				if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
				{
					cerr << "Failed with all supported pallets. Unable to use this device" << endl;
					Close();
					return false;
				}
			}
		}
	}
}

void Vidstream::Close()
{
	if(int(map_buffer) > 0)
		munmap(map_buffer, vid_buffer.size);
	map_buffer = NULL;

	if(int(tmp_buffer) > 0)
		delete[] tmp_buffer;
	tmp_buffer = NULL;

	if(vid_dev > 0)
		close(vid_dev);
	vid_dev    = -1;
}

bool Vidstream::Read(unsigned char * buffer, uint bsize)
{
	if(vid_dev <= 0) return false;

	if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
	{
		cerr << "capturing failed" << endl;
		return false;
	}
	if(ioctl(vid_dev, VIDIOCSYNC, &vid_mmap) == -1)
	{
		cerr << "syncing failed" << endl;
		return false;
	}

	//copy captured image an convert it
	memcpy(buffer, map_buffer, bsize);

	return true;
}

