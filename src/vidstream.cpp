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
	vid_dev     = -1;
	map_buffer0 = NULL;
	map_buffer1 = NULL;

	p_frame     = -1;

	width       = 0;
	height      = 0;
}

bool Vidstream::Open(char * device, uint w, uint h, int source, int mode)
{
	width  = w;
	height = h;

	//open the video4linux device
	vid_dev = open(device, O_RDWR);
	if(vid_dev == -1)
	{
		log_message(1, "Vidstream: Can't open device: %s", device);
		return false;
	}

	//get the capabilities
	if(ioctl(vid_dev, VIDIOCGCAP, &vid_cap) == -1)
	{
		log_message(1, "ioctl: getting capability failed");
		Close();
		return false;
	}

	//setup grab source and mode
	vid_channel.channel = source;
	vid_channel.norm    = mode;
	if(ioctl(vid_dev, VIDIOCSCHAN, &vid_channel) == -1)
	{
		log_message(1, "ioctl: setting video channel failed");
		Close();
		return false;
	}

	//set device video buffer using norma 'read'
	if(ioctl(vid_dev, VIDIOCGMBUF, &vid_buffer) == -1)
	{
		log_message(1, "Vidstream: unable to set device video buffer");
		Close();
		return false;
	}

	//init mmap buffer and pointers to the frames of doublebuffering
	map_buffer0 = (unsigned char*) mmap(0, vid_buffer.size, PROT_READ|PROT_WRITE, MAP_SHARED, vid_dev, 0);

	//check if the video device supports double buffering
	if(vid_buffer.frames > 1)	map_buffer1 = (unsigned char*) map_buffer0 + vid_buffer.offsets[1];
	//if not, set second frame buffer to point to the first.
	//all will work fine, but without real doublebuffering
	else map_buffer1 = map_buffer0;
	if(int(map_buffer0) == -1)
	{
		log_message(1, "Vidstream: mmap() failed");
		Close();
		return false;
	}

	//setup pallets for each frame of doublebuffering
	for(int i=0; i<2; i++)
	{
		vid_mmap.format = VIDEO_PALETTE_YUV420P;
		vid_mmap.frame  = i;
		vid_mmap.width  = width;
		vid_mmap.height = height;
		if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
		{
			log_message(1, "Vidstream: Faild with YUV20P, trying YUV422 palette");
			vid_mmap.format = VIDEO_PALETTE_YUV422;;
			/* Try again... */
			if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
			{
				log_message(1, "Vidstream: Failed with YUV422, trying RGB24 palette");
				vid_mmap.format = VIDEO_PALETTE_RGB24;
				/* Try again... */
				if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
				{
					log_message(1, "Vidstream: Failed with RGB24, trying GREYSCALE palette");
					vid_mmap.format = VIDEO_PALETTE_GREY;
					/* Try one last time... */
					if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
					{
						log_message(1, "Vidstream: Failed with all supported pallets. Unable to use this device");
						Close();
						return false;
					}
				}
			}
		}
	}

	//wait for completion of all operations with video device
	usleep(500000);
	return true;
}

void Vidstream::setPicParams( int br, int cont, int hue, int col, int wit )
{
	if(vid_dev <= 0) return;

	video_picture pic;

	//get current params
	if(ioctl(vid_dev, VIDIOCGPICT, &pic) == -1)
	{
		log_message(1, "Vidstream: setPicParams: Faild to get current picture params");
		return;
	}

	//change params
	if(br  >=0) pic.brightness = br;   //Picture brightness
	if(hue >=0) pic.hue        = hue;  //Picture hue (colour only)
	if(col >=0) pic.colour     = col;  //Picture colour (colour only)
	if(cont>=0) pic.contrast   = cont; //Picture contrast
	if(wit >=0) pic.whiteness  = wit;  //Picture whiteness (greyscale only)

	//set current params
	if(ioctl(vid_dev, VIDIOCSPICT, &pic) == -1)
	{
		log_message(1, "Vidstream: setPicParams: Faild to set new picture params");
		return;
	}
}

void Vidstream::Close()
{
	if(int(map_buffer0) > 0)
		munmap(map_buffer0, vid_buffer.size);
	map_buffer0 = NULL;
	map_buffer1 = NULL;

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
	unsigned char* map_buffer = (p_frame)? map_buffer0 : map_buffer1;
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
		log_message(1, "Vidstream: capturing failed");
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
		log_message(1, "Vidstream: syncing failed");
		return false;
	}
	return true;
}
