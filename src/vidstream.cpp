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

	p_frame    = -1;

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

	//wait for completion of all operations with video device
	usleep(500000);
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

	//prepare the next one
	if(!prepare_frame(!p_frame))
		return false;

	//capture prepared frame
	if(!capture_frame(!p_frame))
		return false;

	//copy captured frame and convert it if needed
	switch(vid_mmap.format)
	{
		case VIDEO_PALETTE_RGB24:
			rgb24_to_yuv420p(buffer, map_buffer, width, height);
			break;
		case VIDEO_PALETTE_YUV422:
			yuv422_to_yuv420p(buffer, map_buffer, width, height);
			break;
		default:
			memcpy(buffer, map_buffer, bsize);
			break;
	}
	return true;
}

double Vidstream::ptime(uint frames)
{
	if(vid_dev <= 0) return 0;
	prepare_frame(0);

	UTimer timer;
	uint bsize = width*height*3;
	unsigned char* buffer = new unsigned char[bsize];

	timer.start();
	for(int i=0; i<frames; i++)
		Read(buffer, bsize);
	timer.stop();

	delete[] buffer;

	return timer.elapsed()/frames;
}


//private functions
void Vidstream::yuv422_to_yuv420p(unsigned char *dest, unsigned char *src, int w, int h) const
{
	unsigned char *src1, *dest1, *src2, *dest2;
	int i, j;

	/* Create the Y plane */
	src1  = src;
	dest1 = dest;
	for(i = w*h; i; i--)
	{
		*dest++ = *src;
		src += 2;
	}

	/* Create U and V planes */
	src1  = src + 1;
	src2  = src + w*2 + 1;
	dest  = dest + w*h;
	dest2 = dest1 + (w*h)/4;
	for(i = h/2; i; i--)
	{
		for(j = w/2; j; j--)
		{
			*dest1 = ((int)*src1+(int)*src2)/2;
			src1  += 2;
			src2  += 2;
			dest1++;
			*dest2 = ((int)*src1+(int)*src2)/2;
			src1  += 2;
			src2  += 2;
			dest2++;
		}
		src1 += w*2;
		src2 += w*2;
	}
}

void Vidstream::rgb24_to_yuv420p(unsigned char *dest, unsigned char *src, int w, int h) const
{
	unsigned char *y, *u, *v;
	unsigned char *r, *g, *b;
	int i, loop;

	b = src;
	g = b+1;
	r = g+1;
	y = dest;
	u = y + w*h;
	v = u+(w*h)/4;
	memset(u, 0, w*h/4);
	memset(v, 0, w*h/4);

	for(loop=0; loop<h; loop++)
	{
		for(i=0; i<w; i+=2)
		{
			*y++ = (9796**r+19235**g+3736**b)>>15;
			*u  += ((-4784**r-9437**g+14221**b)>>17)+32;
			*v  += ((20218**r-16941**g-3277**b)>>17)+32;
			r  += 3;
			g  += 3;
			b  += 3;
			*y++ = (9796**r+19235**g+3736**b)>>15;
			*u  += ((-4784**r-9437**g+14221**b)>>17)+32;
			*v  += ((20218**r-16941**g-3277**b)>>17)+32;
			r  += 3;
			g  += 3;
			b  += 3;
			u++;
			v++;
		}

		if((loop & 1) == 0)
		{
			u -= w/2;
			v -= w/2;
		}
	}
}

bool Vidstream::prepare_frame(uint fnum)
{
	vid_mmap.frame  = fnum;
	if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
	{
		cerr << "capturing failed" << endl;
		return false;
	}
	p_frame = fnum;
	return true;
}

bool Vidstream::capture_frame(uint fnum)
{
	vid_mmap.frame  = fnum;
	if(ioctl(vid_dev, VIDIOCSYNC, &vid_mmap) == -1)
	{
		cerr << "syncing failed" << endl;
		return false;
	}
	return true;
}
