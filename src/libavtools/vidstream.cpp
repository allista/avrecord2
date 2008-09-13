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
#include <signal.h>

#include <iostream>
using namespace std;

#include "vidstream.h"

Vidstream::Vidstream()
{
	vid_dev     = -1;

	p_frame     = 0;

	width       = 0;
	height      = 0;
}

bool Vidstream::Open(const char * device, uint w, uint h, int source, int mode)
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
	vid_channel.type		= VIDEO_TYPE_CAMERA;
	if(ioctl(vid_dev, VIDIOCSCHAN, &vid_channel) == -1)
	{
		log_message(1, "ioctl: setting video channel failed");
		Close();
		return false;
	}

	//set device video buffer using normal 'read'
	if(ioctl(vid_dev, VIDIOCGMBUF, &vid_buffer) == -1)
	{
		log_message(1, "Vidstream: unable to set device video buffer");
		Close();
		return false;
	}

	//init mmap buffer and pointers to the frames of multibuffering
	map_buffers = vector<unsigned char*>(vid_buffer.frames);
	map_buffers[0] = (unsigned char*) mmap(0, vid_buffer.size, PROT_READ|PROT_WRITE, MAP_SHARED, vid_dev, 0);
	if(int(map_buffers[0]) == -1)
	{
		log_message(1, "Vidstream: mmap() failed");
		Close();
		return false;
	}
	for(int i=1; i<map_buffers.size(); i++)
		map_buffers[i] = (unsigned char*) map_buffers[0] + vid_buffer.offsets[i];

	//setup pallet
	vid_mmap.frame  = 0;
	vid_mmap.width  = width;
	vid_mmap.height = height;
	int pallet			= 0;
	if(mode == NORM_PAL_NC && used_pallets[VIDEO_PALETTE_GREY])
	{
		pallet = VIDEO_PALETTE_GREY;
		vid_mmap.format = used_pallets[VIDEO_PALETTE_GREY];
		if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
		{
			log_message(1, "Vidstream: Faild setting %s palette...", pallets[vid_mmap.format]);
			pallet = 0;
		}
	}
	if(!pallet) for(pallet = 16; pallet > 0; pallet--)
	{
		vid_mmap.format = used_pallets[pallet];
		if(!vid_mmap.format) continue;
		if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
		{
			log_message(1, "Vidstream: Faild setting %s palette...", pallets[vid_mmap.format]);
			continue;
		}
		else break;
	}
	if(!pallet) { Close(); return false; }

	switch (vid_mmap.format)
	{
		case VIDEO_PALETTE_YUV420P:
			b_size = (width*height*3)/2;
			break;
		case VIDEO_PALETTE_YUV422:
			b_size = (width*height*2);
			break;
		case VIDEO_PALETTE_RGB24:
			b_size = (width*height*3);
			break;
		case VIDEO_PALETTE_RGB32:
			b_size = (width*height*4);
			break;

		case VIDEO_PALETTE_HI240:
		case VIDEO_PALETTE_RGB565:
		case VIDEO_PALETTE_RGB555:
		case VIDEO_PALETTE_YUYV:
		case VIDEO_PALETTE_UYVY:
		case VIDEO_PALETTE_YUV420:
		case VIDEO_PALETTE_YUV411:
		case VIDEO_PALETTE_RAW:
		case VIDEO_PALETTE_YUV422P:
		case VIDEO_PALETTE_YUV411P:
		case VIDEO_PALETTE_YUV410P:
			b_size = (width*height*3);
			break;

		case VIDEO_PALETTE_GREY:
			b_size = width*height;
			break;

		default: b_size = (width*height*3);
	}

	//wait for completion of all operations with video device
	usleep(500000);

	//make test capture in order to "heat up" capturing interface =)
	ptime(map_buffers.size());

	//all is done
	return true;
}

void Vidstream::setPicParams( uint br, uint cont, uint hue, uint col, uint wit )
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
	pic.brightness = br;   //Picture brightness
	pic.hue        = hue;  //Picture hue (colour only)
	pic.colour     = col;  //Picture colour (colour only)
	pic.contrast   = cont; //Picture contrast
	pic.whiteness  = wit;  //Picture whiteness (greyscale only)

	//set current params
	if(ioctl(vid_dev, VIDIOCSPICT, &pic) == -1)
	{
		log_message(1, "Vidstream: setPicParams: Faild to set new picture params");
		return;
	}
}

void Vidstream::Close()
{
	if(int(map_buffers[0])  != -1)
		munmap(map_buffers[0], vid_buffer.size);
	map_buffers.clear();

	if(vid_dev > 0)
		close(vid_dev);
	vid_dev    = -1;
}

bool Vidstream::Read(unsigned char * buffer, uint bsize)
{
	if(vid_dev <= 0) return false;

	/* Block signals during IOCTL */
	sigset_t  set, old;
	sigemptyset (&set);
	sigaddset (&set, SIGCHLD);
	sigaddset (&set, SIGALRM);
	sigaddset (&set, SIGUSR1);
	sigaddset (&set, SIGTERM);
	sigaddset (&set, SIGHUP);
	pthread_sigmask (SIG_BLOCK, &set, &old);

	//prepare the next one
	if(!prepare_frame(p_frame))
	{
		pthread_sigmask (SIG_UNBLOCK, &old, NULL);
		return false;
	}

	//capture prepared frame
	uint c_frame = p_frame;
	if(!capture_frame(c_frame))
	{
		pthread_sigmask (SIG_UNBLOCK, &old, NULL);
		return false;
	}

	/*undo the signal blocking*/
	pthread_sigmask (SIG_UNBLOCK, &old, NULL);

	//copy captured frame and convert it if needed
	unsigned char* map_buffer = map_buffers[c_frame];
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

bool Vidstream::prepare_frame(int fnum)
{
	if(--fnum < 0) fnum = map_buffers.size()-1;
	vid_mmap.frame = fnum;
	if(ioctl(vid_dev, VIDIOCMCAPTURE, &vid_mmap) == -1)
	{
		log_message(1, "Vidstream: capturing failed");
		return false;
	}
	return true;
}

bool Vidstream::capture_frame(int fnum)
{
	p_frame = (fnum+1 >= map_buffers.size())? 0 : fnum+1;
	if(ioctl(vid_dev, VIDIOCSYNC, &fnum) == -1)
	{
		log_message(1, "Vidstream: syncing failed");
		return false;
	}
	return true;
}

uint Vidstream::ptime(uint frames)
{
	if(vid_dev <= 0) return 0;

	UTimer timer;
	unsigned char* buffer = new unsigned char[b_size];

	timer.start();
	for(int i=0; i<frames; i++)
		Read(buffer, b_size);
	timer.stop();

	delete[] buffer;

	return timer.elapsed()/frames;
}


//private functions
void Vidstream::yuv422_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h)
{
	const unsigned char *src1, *src2;
	unsigned char       *dest1, *dest2;
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

void Vidstream::rgb24_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h)
{
	unsigned char       *y, *u, *v;
	const unsigned char *r, *g, *b;
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

