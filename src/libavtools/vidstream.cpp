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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <assert.h>

#include <iostream>
using namespace std;

#include "vidstream.h"

Vidstream::Vidstream(Setting& _video_settings)
	: video_settings(_video_settings)
{
	device = -1;
	io_method = 0;

	p_frame     = 0;
}


bool Vidstream::Init()
{
	uint io_method = 0;
	struct stat st;

	//stat and open the device
	const char* dev_name = NULL;
	try{ dev_name = video_settings["device"]; }
	catch(SettingNotFoundException)
	{
		log_message(1, "No <device> setting was found. Using default /dev/video0");
		dev_name = "/dev/video0";
	}
	if(-1 == stat(dev_name, &st))
	{
		log_message(1, "Cannot identify '%s': %d, %s.", dev_name, errno, strerror (errno));
		Close();
		return false;
	}

	if(!S_ISCHR(st.st_mode))
	{
		log_message(1, "%s is no device.", dev_name);
		Close();
		return false;
	}

	device = open(dev_name, O_RDWR | O_NONBLOCK, 0);
	if(-1 == device)
	{
		log_message(1, "Cannot open '%s': %d, %s.", dev_name, errno, strerror (errno));
		Close();
		return false;
	}

	//query device capabilities and check the necessary ones
	if(-1 == xioctl(VIDIOC_QUERYCAP, &cap))
	{
		if(EINVAL == errno)
		{
			log_message(1, "%s is no V4L2 device.", dev_name);
			Close();
			return false;
		}
		else
		{
			log_errno("Could not get device capabilities (VIDIOC_QUERYCAP): ");
			Close();
			return false;
		}
	}

	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		log_message(1, "%s is no video capture device.", dev_name);
		Close();
		return false;
	}

	//set desired video source
	try
	{
		input.index = (int)video_settings["input"];
		if(-1 == xioctl(VIDIOC_S_INPUT, &input))
		{
			log_errno("Unable to set video input (VIDIOC_S_INPUT): ");
			Close();
			return false;
		}
	}
	catch(SettingNotFoundException)
	{ log_message(0, "No <input> setting was found. Using current device configuration."); }

	//set desired video standard
	try
	{
		standard = (long long)video_settings["standard"];
		if(-1 == xioctl(VIDIOC_S_STD, &standard))
		{
			log_errno("Unable to set video standard (VIDIOC_G_STD): ");
			Close();
			return false;
		}
	}
	catch(SettingNotFoundException)
	{ log_message(0, "No <standard> setting was found. Using current device configuration."); }


	//resetting crop and setting video format
	v4l2_cropcap cropcap;
	CLEAR(cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(0 == xioctl(VIDIOC_CROPCAP, &cropcap))
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */
		if(-1 == xioctl(VIDIOC_S_CROP, &crop))
		{
			switch (errno)
			{
				case EINVAL:
					log_errno("Cropping not supported (VIDIOC_G_CROP): ");
					break;
				default:
					log_errno("Unable to set cropping (VIDIOC_G_CROP): ");
					break;
			}
		}
	}
	else
	{
		log_errno("Unable query cropping capabilities (VIDIOC_CROPCAP): ");
		Close();
		return false;
	}

	CLEAR(format);
	try
	{
		format.fmt.pix.width   = video_settings["width"];
		format.fmt.pix.height  = video_settings["height"];
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "No <width> or <height> setting was found.");
		Close();
		return false;
	}
	format.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.field       = V4L2_FIELD_INTERLACED;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	if(-1 == xioctl(VIDIOC_S_FMT, &format))
	{
		log_errno("Unable set format (VIDIOC_S_FMT): ");
		Close();
		return false;
	}

	//setup input/output mechanism
	if(cap.capabilities & V4L2_CAP_STREAMING)
	{
	}
	else if(cap.capabilities & V4L2_CAP_READWRITE)
	{
	}

	//set values for all the controls defined in configuration
	try
	{
		Setting& controls = video_settings["controls"]
		v4l2_control control;
		int control_idx = 0;
		while(true)
		{
			try
			{
				Setting& control_s = controls[control_idx];
				CLEAR(control);
				control.id = (int)control_s["id"];
				control.value = (int)control_s["value"];
				if(-1 == xioctl(fd, VIDIOC_S_CTRL, &control) && errno != ERANGE && errno != EINVAL)
				{
					log_errno("Unable to set control value (VIDIOC_S_CTRL): ");
					Close();
					return false;
				}
				control_idx++;
			}
			catch(...) { break; }
		}
	}
	catch(SettingNotFoundException)
	{ log_message(0, "No <controls> setting group was found."); }

	return true;
}

bool Vidstream::Open(const char * device, uint w, uint h, intput_source source, intput_mode mode)
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
	vid_channel.type    = VIDEO_TYPE_CAMERA;
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
	if(long(map_buffers[0]) == -1)
	{
		log_message(1, "Vidstream: mmap() failed");
		Close();
		return false;
	}
	for(int i=1; i<map_buffers.size(); i++)
		map_buffers[i] = (unsigned char*) map_buffers[0] + vid_buffer.offsets[i];


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

void Vidstream::Close()
{
	if(long(map_buffers[0])  != -1)
		munmap(map_buffers[0], vid_buffer.size);
	map_buffers.clear();

	if(device > 0) close(device);
	device = -1;
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

/////////////////////////////////private functions////////////////////////////////

int Vidstream::xioctl(int request, void * arg)
{
	int r;
	do r = ioctl(device, request, arg);
	while(-1 == r && EINTR == errno);
	return r;
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


