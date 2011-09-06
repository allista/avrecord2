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

Vidstream::Vidstream()
{
	device    = -1;
	io_method = IO_METHOD_USERPTR;
	pix_fmt   = 0
	buffers.resize(NUM_BUFFERS);

	p_frame     = 0;
}


bool Vidstream::Open(Setting *video_settings_ptr)
{
	if(!video_settings_ptr) return false;
	Setting &video_settings = *video_settings_ptr;

	//stat and open the device
	stat st;
	const char *dev_name = NULL;
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
		CLEAR(standard);
		standard.index = (long long)video_settings["standard"];
		if(-1 == xioctl(VIDIOC_S_STD, &standard.index))
		{
			log_errno("Unable to set video standard (VIDIOC_G_STD): ");
			Close();
			return false;
		}
		else if(-1 == xioctl(VIDIOC_ENUMSTD, &standard))
		{
			log_errno("Unable to get video standard info (VIDIOC_ENUMSTD): ");
			Close();
			return false;
		}
	}
	catch(SettingNotFoundException)
	{ log_message(0, "No <standard> setting was found. Using current device configuration."); }


	//resetting crop boundaries and setting video format
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


	try
	{
		 width  = video_settings["width"];
		 height = video_settings["height"];
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "No <width> or <height> setting was found.");
		Close();
		return false;
	}
	CLEAR(format);
	for(pix_fmt = 0; pix_fmt < sizeof(pixel_formats)/sizeof(pixel_formats[0]); pix_fmt++)
	{
		format.pix.height      = width;
		format.pix.width       = height;
		format.type            = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.pix.field       = V4L2_FIELD_INTERLACED;
		format.pix.pixelformat = pixel_formats[pix_fmt];
		if(-1 == xioctl(VIDIOC_S_FMT, &format))
		{
			log_message(0, "Unable set format $s. Errno: %d %s.", pixel_format_names[fmt], errno, strerror(errno));
			CLEAR(format);
		}
		else break;
	}
	if(format.pix.height == 0)
	{
		log_message(1, "Unable to set any of the supported pixel formats.");
		Close();
		return false;
	}
	else ///<driver may change image dimensions
	{
		video_settings["width"]  = width  = format.pix.height;
		video_settings["height"] = height = format.pix.width;
	}

	//setup input/output mechanism
	if(cap.capabilities & V4L2_CAP_STREAMING)
	{
		v4l2_requestbuffers req;

		///trying memory mapping first
		CLEAR (req);
		req.count               = NUM_BUFFERS;
		req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory              = V4L2_MEMORY_MMAP;
		if(0 == xioctl(VIDIOC_REQBUFS, &req))
		{
			if(req.count < 2)
			{
				log_message(1, "Insufficient buffer memory on %s.", dev_name);
				Close();
				return false;
			}

			for (int b = 0; b < NUM_BUFFERS; b++)
			{
				v4l2_buffer buf;
				CLEAR(buf);

				buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory      = V4L2_MEMORY_MMAP;
				buf.index       = NUM_BUFFERS;
				if(-1 == xioctl(VIDIOC_QUERYBUF, &buf))
				{
					log_errno("Failed to query mmap buffers (VIDIOC_QUERYBUF): ");
					Close();
					return false;
				}

				buffers[b].length = buf.length;
				buffers[b].start  = mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */, device, buf.m.offset);

				if(MAP_FAILED == buffers[b].start)
				{
					log_errno("mmap failed: ");
					Close();
					return false;
				}
			}

			io_method = IO_METHOD_MMAP;
		}
		else
		{
			if(EINVAL != errno)
			{
				log_errno("VIDIOC_REQBUFS");
				Close();
				return false;
			}
			else
			{
				log_message(0, "Device %s does not support memory mapping.", dev_name);

				///now trying user pointers
				CLEAR (req);
				req.count               = NUM_BUFFERS;
				req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				req.memory              = V4L2_MEMORY_USERPTR;
				if(0 == xioctl(VIDIOC_REQBUFS, &req))
				{
					uint page_size = getpagesize();
					uint buffer_size = (format.fmt.pix.sizeimage + page_size - 1) & ~(page_size - 1);

					for (int b = 0; b < NUM_BUFFERS; b++)
					{
						buffers[b].length = buffer_size;
						buffers[b].start  = memalign(/* boundary */ page_size, buffer_size);

						if(!buffers[b].start)
						{
							log_message(1, "Out of memory trying to allocate image buffers (USERPTR METHOD).");
							Close();
							return false;
						}
					}

					io_method = IO_METHOD_USERPTR;
				}
				else
				{
					if(EINVAL == errno)
						log_message(1, "Device %s does not support user pointers.", dev_name);
					else
						log_errno("VIDIOC_REQBUFS");
					Close();
					return false;
				}
			}
		}
	}
	else if(cap.capabilities & V4L2_CAP_READWRITE)
	{
		buffers.resize(1);
		buffers[0].length = format.fmt.pix.imagesize;
		buffers[0].start  = malloc(format.fmt.pix.imagesize);

		if(!buffers[0].start)
		{
			log_message(1, "Out of memory trying to allocate image buffer (READ METHOD).");
			Close();
			return false;
		}

		io_method = IO_METHOD_READ;
	}
	else
	{
		log_message(1, "Device has no I/O capability.");
		Close();
		return false;
	}

	//set values for all the controls defined in configuration
	try
	{
		Setting &controls = video_settings["controls"]
		v4l2_control control;
		int control_idx = 0;
		for(control_idx = 0; control_idx < controls.getLength(); control_idx++)
		{
			try
			{
				Setting &control_s = controls[control_idx];
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
			catch(...)
			{ log_message(1, "Video control %d has no <id> or <value> setting.", control_idx); }
		}
	}
	catch(SettingNotFoundException)
	{ log_message(0, "No <controls> setting group was found."); }

	return true;
}

void Vidstream::Close()
{
	StopCapture();
	switch(io_method)
	{
		case IO_METHOD_READ:
			free(buffers[0].start);
			break;

		case IO_METHOD_MMAP:
			for(uint i = 0; i < NUM_BUFFERS; i++)
				if(-1 == munmap(buffers[i].start, buffers[i].length))
				{
					log_errno("munmap failed: ");
					Close();
					return false;
				}
			break;

		case IO_METHOD_USERPTR:
			for(uint i = 0; i < NUM_BUFFERS; i++)
				free(buffers[i].start);
			break;
	}
	buffers.clear();

	if(device > 0) close(device);
	device = -1;
}


bool Vidstream::StartCapture()
{
	v4l2_buf_type type;
	switch (io_method)
	{
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;

		case IO_METHOD_MMAP:
			for(uint i = 0; i < NUM_BUFFERS; i++)
			{
				v4l2_buffer buf;
				CLEAR (buf);

				buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory      = V4L2_MEMORY_MMAP;
				buf.index       = i;
				if(-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					log_errno("VIDIOC_QBUF failed: ");
					Close();
					return false;
				}
			}

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(VIDIOC_STREAMON, &type))
			{
				log_errno("VIDIOC_STREAMON failed: ");
				Close();
				return false;
			}
			break;

		case IO_METHOD_USERPTR:
			for(uint i = 0; i < NUM_BUFFERS; i++)
			{
				v4l2_buffer buf;
				CLEAR (buf);

				buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory      = V4L2_MEMORY_USERPTR;
				buf.index       = i;
				buf.m.userptr   = (unsigned long)buffers[i].start;
				buf.length      = buffers[i].length;
				if(-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					log_errno("VIDIOC_QBUF failed: ");
					Close();
					return false;
				}
			}

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(VIDIOC_STREAMON, &type))
			{
				log_errno("VIDIOC_STREAMON failed: ");
				Close();
				return false;
			}
			break;
	}
}


bool Vidstream::StopCapture()
{
	v4l2_buf_type type;
	switch(io_method)
	{
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;

		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if(-1 == xioctl(VIDIOC_STREAMOFF, &type))
			{
				log_errno("VIDIOC_STREAMOFF failed: ");
				Close();
				return false;
			}
			break;
	}
}


int Vidstream::Read(image_buffer& buffer)
{
	if(device <= 0) return false;

	/* Block signals during IOCTL */
	sigset_t  set, old;
	sigemptyset (&set);
	sigaddset (&set, SIGCHLD);
	sigaddset (&set, SIGALRM);
	sigaddset (&set, SIGUSR1);
	sigaddset (&set, SIGTERM);
	sigaddset (&set, SIGHUP);
	pthread_sigmask (SIG_BLOCK, &set, &old);

	v4l2_buffer buf;
	switch(io_method)
	{
		case IO_METHOD_READ:
			if(-1 == read(device, buffers[0].start, buffers[0].length))
			{
				switch (errno)
				{
					case EAGAIN:
						pthread_sigmask(SIG_UNBLOCK, &old, NULL);
						return 0;
					default:
						log_errno("Read error while trying to grab image with READ method: ");
						Close();
						pthread_sigmask(SIG_UNBLOCK, &old, NULL);
						return -1;
				}
			}

			//copy captured frame
			timeval t;
			CLEAR(t);
			if(!fill_buffer(buffer, buffers[0].start, buffers[0].length, t))
			{
				Close();
				pthread_sigmask(SIG_UNBLOCK, &old, NULL);
				return -1;
			}
			break;

		case IO_METHOD_MMAP:
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			if(-1 == xioctl(VIDIOC_DQBUF, &buf))
			{
				switch (errno) {
					case EAGAIN:
						pthread_sigmask(SIG_UNBLOCK, &old, NULL);
						return 0;
					default:
						log_errno("(mmap method) VIDIOC_DQBUF failed: ");
						Close();
						pthread_sigmask(SIG_UNBLOCK, &old, NULL);
						return -1;
				}
			}
			if(!buf.index < NUM_BUFFERS)
			{
				log_message(1, "mmap buffer index is out of range");
				Close();
				pthread_sigmask(SIG_UNBLOCK, &old, NULL);
				return -1;
			}

			//copy captured frame
			if(!fill_buffer(buffer, buffers[buf.index].start, buffers[buf.index].length, buf.timestamp))
			{
				Close();
				pthread_sigmask(SIG_UNBLOCK, &old, NULL);
				return -1;
			}

			if(-1 == xioctl(VIDIOC_QBUF, &buf))
			{
				log_errno("VIDIOC_QBUF failed: ");
				Close();
				pthread_sigmask(SIG_UNBLOCK, &old, NULL);
				return -1;
			}
			break;

		case IO_METHOD_USERPTR:
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			if(-1 == xioctl(VIDIOC_DQBUF, &buf))
			{
				switch (errno) {
					case EAGAIN:
						pthread_sigmask(SIG_UNBLOCK, &old, NULL);
						return 0;
					default:
						log_errno("(user pointer method) VIDIOC_DQBUF failed: ");
						Close();
						pthread_sigmask(SIG_UNBLOCK, &old, NULL);
						return -1;
				}
			}

			//find current buffer index
			for(uint buf_idx = 0; i < NUM_BUFFERS; buf_idx++)
				if(buf.m.userptr == (unsigned long)buffers[buf_idx].start
					&& buf.length == buffers[buf_idx].length)
					break;

			if(!buf_idx < NUM_BUFFERS)
			{
				log_message(1, "user pointer buffer index is out of range");
				Close();
				pthread_sigmask(SIG_UNBLOCK, &old, NULL);
				return -1;
			}

			//copy captured frame
			if(!fill_buffer(buffer, (void*)buf.m.userptr, buf.length, buf.timestamp))
			{
				Close();
				pthread_sigmask(SIG_UNBLOCK, &old, NULL);
				return -1;
			}

			if(-1 == xioctl(VIDIOC_QBUF, &buf))
			{
				log_errno("VIDIOC_QBUF failed: ");
				Close();
				pthread_sigmask(SIG_UNBLOCK, &old, NULL);
				return -1;
			}
			break;
	}

	/*undo the signal blocking*/
	pthread_sigmask(SIG_UNBLOCK, &old, NULL);
	return 1;
}

/////////////////////////////////private functions////////////////////////////////

int Vidstream::xioctl(int request, void *arg)
{
	int r;
	do r = ioctl(device, request, arg);
	while(-1 == r && EINTR == errno);
	return r;
}

bool Vidstream::fill_buffer(image_buffer &buffer, void *start, uint length, timeval timestamp)
{
	if(buffer.start) free(buffer.start);
	buffer.start = malloc(length);
	if(!buffer.start)
	{
		log_message(1, "Out of memory trying to allocate image buffer (fill_buffer).");
		return false;
	}
	memcpy(buffer, start, length);
	buffer.timestamp.tv_sec = timestamp.tv_sec;
	buffer.timestamp.tv_usec = timestamp.tv_usec;
	return true;
}

// uint Vidstream::ptime(uint frames)
// {
// 	if(device <= 0) return 0;
//
// 	UTimer timer;
// 	unsigned char *buffer = new unsigned char[b_size];
//
// 	timer.start();
// 	for(int i=0; i<frames; i++)
// 		Read(buffer, b_size);
// 	timer.stop();
//
// 	delete[] buffer;
//
// 	return timer.elapsed()/frames;
// }
//
//
// //private functions
// void Vidstream::yuv422_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h)
// {
// 	const unsigned char *src1, *src2;
// 	unsigned char       *dest1, *dest2;
// 	int i, j;
//
// 	/* Create the Y plane */
// 	src1  = src;
// 	dest1 = dest;
// 	for(i = w*h; i; i--)
// 	{
// 		*dest++ = *src;
// 		src += 2;
// 	}
//
// 	/* Create U and V planes */
// 	src1  = src + 1;
// 	src2  = src + w*2 + 1;
// 	dest  = dest + w*h;
// 	dest2 = dest1 + (w*h)/4;
// 	for(i = h/2; i; i--)
// 	{
// 		for(j = w/2; j; j--)
// 		{
// 			*dest1 = ((int)*src1+(int)*src2)/2;
// 			src1  += 2;
// 			src2  += 2;
// 			dest1++;
// 			*dest2 = ((int)*src1+(int)*src2)/2;
// 			src1  += 2;
// 			src2  += 2;
// 			dest2++;
// 		}
// 		src1 += w*2;
// 		src2 += w*2;
// 	}
// }
//
// void Vidstream::rgb24_to_yuv420p(unsigned char *dest, const unsigned char *src, int w, int h)
// {
// 	unsigned char       *y, *u, *v;
// 	const unsigned char *r, *g, *b;
// 	int i, loop;
//
// 	b = src;
// 	g = b+1;
// 	r = g+1;
// 	y = dest;
// 	u = y + w*h;
// 	v = u+(w*h)/4;
// 	memset(u, 0, w*h/4);
// 	memset(v, 0, w*h/4);
//
// 	for(loop=0; loop<h; loop++)
// 	{
// 		for(i=0; i<w; i+=2)
// 		{
// 			*y++ = (9796**r+19235**g+3736**b)>>15;
// 			*u  += ((-4784**r-9437**g+14221**b)>>17)+32;
// 			*v  += ((20218**r-16941**g-3277**b)>>17)+32;
// 			r  += 3;
// 			g  += 3;
// 			b  += 3;
// 			*y++ = (9796**r+19235**g+3736**b)>>15;
// 			*u  += ((-4784**r-9437**g+14221**b)>>17)+32;
// 			*v  += ((20218**r-16941**g-3277**b)>>17)+32;
// 			r  += 3;
// 			g  += 3;
// 			b  += 3;
// 			u++;
// 			v++;
// 		}
//
// 		if((loop & 1) == 0)
// 		{
// 			u -= w/2;
// 			v -= w/2;
// 		}
// 	}
// }


