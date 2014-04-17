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
#include <sys/mman.h>
#include <asm/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <assert.h>

#include <iostream>
#include <algorithm>
#include <list>
#include <map>
using namespace std;

#include "vidstream.h"


Vidstream::Vidstream()
{
	device         = -1;
	dev_name       = NULL;
	io_method      = IO_METHOD_MMAP;
	pix_fmt        = 0;
    convert_format = false;
    conv_data      = NULL;
	width          = 0;
	height         = 0;
	_bsize         = 0;
	init           = INIT_NONE;
}


bool Vidstream::open_device(Setting &video_settings)
{
    if(init & INIT_VIDEO_DEV_OPENED) return true;
    struct stat st;
    try{ dev_name = video_settings["device"]; }
    catch(SettingNotFoundException)
    {
        log_message(1, "Vidstream: no <device> setting was found. Using default /dev/video0");
        Close();
        return false;
    }
    if(-1 == stat(dev_name, &st))
    {
        log_message(1, "Vidstream: cannot identify '%s': %d, %s.", dev_name, errno, strerror (errno));
        Close();
        return false;
    }
    if(!S_ISCHR(st.st_mode))
    {
        log_message(1, "Vidstream: %s is not a character device.", dev_name);
        Close();
        return false;
    }
    device = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if(-1 == device)
    {
        log_message(1, "Vidstream: cannot open '%s': %d, %s.", dev_name, errno, strerror (errno));
        Close();
        return false;
    }
    init |= INIT_VIDEO_DEV_OPENED;
    return true;
}


bool Vidstream::check_capabilities(Setting &video_settings)
{
    if(!(init & INIT_VIDEO_DEV_OPENED)) return false;
    CLEAR(cap);
    if(-1 == xioctl(VIDIOC_QUERYCAP, &cap))
    {
        if(EINVAL == errno)
        {
            log_message(1, "Vidstream: %s is not a V4L2 device.", dev_name);
            Close();
            return false;
        }
        else
        {
            log_errno("Vidstream: could not get device capabilities (VIDIOC_QUERYCAP): ");
            Close();
            return false;
        }
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        log_message(1, "Vidstream: %s is not a video capture device.", dev_name);
        Close();
        return false;
    }
    return true;
}


bool Vidstream::set_standard(Setting &video_settings)
{
    if(!(init & INIT_VIDEO_DEV_OPENED)) return false;
    v4l2_std_id current_standard;
    if(-1 != xioctl(VIDIOC_G_STD, &current_standard))
    {
        try { current_standard = (long long)video_settings["standard"]; }
        catch(SettingNotFoundException)
        { log_message(0, "Vidstream: no <standard> setting was found. Using current device configuration."); }
        if(-1 == xioctl(VIDIOC_S_STD, &current_standard))
        {
            log_errno("Vidstream: unable to set video standard (VIDIOC_S_STD): ");
            Close();
            return false;
        }
        CLEAR(standard);
        standard.index = 0;
        while(0 == xioctl(VIDIOC_ENUMSTD, &standard))
        {
            if(standard.id == current_standard) break;
            standard.index++;
        }
        if(standard.id != current_standard)
        {
            log_errno("Vidstream: unable to get video standard info: ");
            Close();
            return false;
        }
    }
    else //set default framerate to 15fps
    {
        uint fps = 15;
        try { fps = video_settings["desired_fps"]; }
        catch(SettingNotFoundException)
        { log_message(0, "Vidstream: no <desired_fps> setting was found. Using default %d.", fps); }
        standard.frameperiod.numerator   = 1;
        standard.frameperiod.denominator = fps;
    }
    return true;
}


bool Vidstream::set_cropping(Setting &video_settings)
{
    if(!(init & INIT_VIDEO_DEV_OPENED)) return false;
    v4l2_cropcap cropcap;
    v4l2_crop crop;
    CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(0 == xioctl(VIDIOC_CROPCAP, &cropcap))
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        if(-1 == xioctl(VIDIOC_S_CROP, &crop))
            if(errno == EINVAL)
                log_errno("Vidstream: cropping is not supported (VIDIOC_S_CROP): ");
            else log_errno("Vidstream: unable to set cropping (VIDIOC_S_CROP): ");
    }
    else
    {
        log_errno("Vidstream: unable query cropping capabilities (VIDIOC_CROPCAP): ");
        Close();
        return false;
    }
    return true;
}


bool Vidstream::set_format(Setting &video_settings)
{
    if(!(init & INIT_VIDEO_DEV_OPENED)) return false;
    //get width-height parametrs from settings
    try
    {
         width  = video_settings["width"];
         height = video_settings["height"];
    }
    catch(SettingNotFoundException)
    {
        log_message(1, "Vidstream: no <width> or <height> setting was found.");
        Close();
        return false;
    }
    //list available and supported formats
    int fmt_idx = -1;
    list<uint> *formats = NULL;
    list<uint>  device_formats, supported_formats;
    map<uint, string> format_names;
    map<int, uint> sorted_formats;
    v4l2_fmtdesc fmt; CLEAR(fmt);
    for(int i = 0 ; ; i++)
    {
        CLEAR(fmt);
        fmt.index = i;
        fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(-1 == xioctl(VIDIOC_ENUM_FMT, &fmt)) break;
        device_formats.push_back(fmt.pixelformat);
        format_names[fmt.pixelformat] = string((const char*)fmt.description);
        fmt_idx = supported_format_index(fmt.pixelformat);
        if(fmt_idx >= 0) sorted_formats[fmt_idx] = fmt.pixelformat;
    }
    if(sorted_formats.empty())
    {
        convert_format = true;
        formats = &device_formats;
    }
    else //fill the list of supported formats in the same order as in v4l2_pixel_formats
    {
        for(map<int, uint>::iterator it = sorted_formats.begin(), end = sorted_formats.end();
            it != end; it++) supported_formats.push_back(it->second);
        formats = &supported_formats;
    }
    //use first format which we're able to set; ordered by support priority
    CLEAR(format);
    for(std::list<uint>::iterator it = formats->begin(), end = formats->end();
        it != end; it++)
    {
        pix_fmt = *it;
        format.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width       = width;
        format.fmt.pix.height      = height;
        format.fmt.pix.field       = V4L2_FIELD_ANY;
        format.fmt.pix.pixelformat = pix_fmt;
        if(-1 == xioctl(VIDIOC_S_FMT, &format))
        {
            log_message(0, "Vidstream: unable set format %s", format_names[pix_fmt].c_str());
            CLEAR(format);
        }
        else
        {
            log_message(0, "Vidstream: using %s pixel format", format_names[pix_fmt].c_str());
            break;
        }
    }
    if(format.fmt.pix.height == 0)
    {
        log_message(1, "Vidstream: unable to set any of the supported pixel formats.");
        Close();
        return false;
    }
    else ///<driver may change image dimensions
    {
        width  = format.fmt.pix.width;
        height = format.fmt.pix.height;
        video_settings["width"]  = (int)width;
        video_settings["height"] = (int)height;
    }
    if(convert_format)
    {
        pix_fmt   = v4l2_pixel_formats[0]; //use the first format for conversion
        conv_data = v4lconvert_create(device);
        out_format.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        out_format.fmt.pix.width       = width;
        out_format.fmt.pix.height      = height;
        out_format.fmt.pix.field       = V4L2_FIELD_ANY;
        out_format.fmt.pix.pixelformat = pix_fmt;
    }
    return true;
}


bool Vidstream::set_IO(Setting &video_settings)
{
    if(!(init & INIT_VIDEO_DEV_OPENED)) return false;
    if(cap.capabilities & V4L2_CAP_STREAMING)
    {
        buffers.resize(NUM_BUFFERS);
        v4l2_requestbuffers req; CLEAR (req);
        ///trying memory mapping first
        req.count  = NUM_BUFFERS;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if(0 == xioctl(VIDIOC_REQBUFS, &req))
        {
            if(req.count < 2)
            {
                log_message(1, "Vidstream: insufficient buffer memory on %s.", dev_name);
                Close();
                return false;
            }
            for(int b = 0; b < NUM_BUFFERS; b++)
            {
                v4l2_buffer buf; CLEAR(buf);
                buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index  = b;
                if(-1 == xioctl(VIDIOC_QUERYBUF, &buf))
                {
                    log_errno("Vidstream: failed to query mmap buffers (VIDIOC_QUERYBUF): ");
                    Close();
                    return false;
                }
                buffers[b].length = buf.length;
                buffers[b].start  = mmap(NULL /* start anywhere */,
                                         buf.length, PROT_READ | PROT_WRITE /* required */,
                                         MAP_SHARED /* recommended */,
                                         device, buf.m.offset);
                if(MAP_FAILED == buffers[b].start)
                {
                    log_errno("Vidstream: mmap failed: ");
                    Close();
                    return false;
                }
                if(_bsize < buf.length) _bsize = buf.length;
                init |= INIT_BUFFERS_ALLOCATED;
            }
            io_method = IO_METHOD_MMAP;
        }
        else
        {
            if(EINVAL != errno)
            {
                log_errno("Vidstream: VIDIOC_REQBUFS");
                Close();
                return false;
            }
            else
            {
                log_message(0, "Vidstream: device %s does not support memory mapping.", dev_name);
                ///now trying user pointers
                CLEAR (req);
                req.count  = NUM_BUFFERS;
                req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                req.memory = V4L2_MEMORY_USERPTR;
                if(0 == xioctl(VIDIOC_REQBUFS, &req))
                {
                    uint page_size = getpagesize();
                    _bsize = (format.fmt.pix.sizeimage + page_size - 1) & ~(page_size - 1);
                    for (int b = 0; b < NUM_BUFFERS; b++)
                    {
                        buffers[b].length = _bsize;
                        buffers[b].start  = memalign(/* boundary */ page_size, _bsize);
                        if(!buffers[b].start)
                        {
                            log_message(1, "Vidstream: out of memory trying to allocate image buffers (USERPTR METHOD).");
                            Close();
                            return false;
                        }
                        init |= INIT_BUFFERS_ALLOCATED;
                    }
                    io_method = IO_METHOD_USERPTR;
                }
                else
                {
                    if(EINVAL == errno)
                        log_message(1, "Vidstream: device %s does not support user pointers.", dev_name);
                    else
                        log_errno("Vidstream: VIDIOC_REQBUFS");
                    Close();
                    return false;
                }
            }
        }
    }
    else if(cap.capabilities & V4L2_CAP_READWRITE)
    {
        buffers.resize(1);
        buffers[0].length = _bsize = format.fmt.pix.sizeimage;
        buffers[0].start  = malloc(_bsize);
        if(!buffers[0].start)
        {
            log_message(1, "Vidstream: out of memory while trying to allocate image buffers (READ METHOD).");
            Close();
            return false;
        }
        init |= INIT_BUFFERS_ALLOCATED;
        io_method = IO_METHOD_READ;
    }
    else
    {
        log_message(1, "Vidstream: device has no I/O capability.");
        Close();
        return false;
    }
    return true;
}


bool Vidstream::set_controls(Setting &video_settings)
{
    if(!(init & INIT_VIDEO_DEV_OPENED)) return false;
    try
    {
        Setting &controls = video_settings["controls"];
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
                if(-1 == xioctl(VIDIOC_S_CTRL, &control) && errno != ERANGE && errno != EINVAL)
                {
                    log_errno("Vidstream: unable to set control value (VIDIOC_S_CTRL): ");
                    Close();
                    return false;
                }
                control_idx++;
            }
            catch(...)
            { log_message(1, "Vidstream: video control %d has no <id> or <value> setting.", control_idx); }
        }
    }
    catch(SettingNotFoundException)
    { log_message(0, "Vidstream: no <controls> setting group was found."); }
    return true;
}


bool Vidstream::Open(Setting *video_settings_ptr)
{
	if(!video_settings_ptr) return false;
	Setting &video_settings = *video_settings_ptr;
	//check device file and open the device
	if(!open_device(video_settings)) return false;
	//query device capabilities and check the necessary ones
	if(!check_capabilities(video_settings)) return false;
	//set desired video source
	try
	{
		input.index = (int)video_settings["input"];
		if(-1 == xioctl(VIDIOC_S_INPUT, &input))
		{
			log_errno("Vidstream: unable to set video input (VIDIOC_S_INPUT): ");
			Close();
			return false;
		}
	}
	catch(SettingNotFoundException)
	{ log_message(0, "Vidstream: no <input> setting was found. Using current device configuration."); }
	//set desired video standard if applied
	if(!set_standard(video_settings)) return false;
	//reset crop boundaries
	if(!set_cropping(video_settings)) return false;
	//setup video format
    if(!set_format(video_settings))   return false;
	//setup input/output mechanism
    if(!set_IO(video_settings))       return false;
	//set values for all the controls defined in configuration
	if(!set_controls(video_settings)) return false;
	return true;
}


void Vidstream::Close()
{
	StopCapture();
	if(conv_data) { v4lconvert_destroy(conv_data); conv_data = NULL; }
	if(init & INIT_BUFFERS_ALLOCATED)
	{
		switch(io_method)
		{
			case IO_METHOD_READ:
				free(buffers[0].start);
				break;
			case IO_METHOD_MMAP:
				for(uint i = 0; i < NUM_BUFFERS; i++)
					if(-1 == munmap(buffers[i].start, buffers[i].length))
						log_errno("Vidstream: munmap failed: ");
				break;
			case IO_METHOD_USERPTR:
				for(uint i = 0; i < NUM_BUFFERS; i++)
					free(buffers[i].start);
				break;
		}
		buffers.clear();
		init ^= INIT_BUFFERS_ALLOCATED;
	}
	if(device > 0) { close(device); device = -1; }
	init ^= INIT_VIDEO_DEV_OPENED;
}


bool Vidstream::StartCapture()
{
	v4l2_buffer buf;
	switch (io_method)
	{
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
		case IO_METHOD_MMAP:
			for(uint i = 0; i < NUM_BUFFERS; i++)
			{
				CLEAR (buf);
				buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index  = i;
				if(-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					log_errno("Vidstream: VIDIOC_QBUF failed: ");
					Close();
					return false;
				}
			}
			break;
		case IO_METHOD_USERPTR:
			for(uint i = 0; i < NUM_BUFFERS; i++)
			{
				CLEAR(buf);
				buf.type      = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory    = V4L2_MEMORY_USERPTR;
				buf.index     = i;
				buf.m.userptr = (unsigned long)buffers[i].start;
				buf.length    = buffers[i].length;
				if(-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					log_errno("VIDIOC_QBUF failed: ");
					Close();
					return false;
				}
			}
			break;
	}
    if(io_method != IO_METHOD_READ)
    {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(VIDIOC_STREAMON, &type))
        {
            log_errno("VIDIOC_STREAMON failed: ");
            Close();
            return false;
        }
    }
	init |= INIT_CAPTURE_STARTED;
	return true;
}


bool Vidstream::StopCapture()
{
	if(!(init & INIT_CAPTURE_STARTED)) return true;
	switch(io_method)
	{
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
		    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if(-1 == xioctl(VIDIOC_STREAMOFF, &type))
			{
				log_errno("Vidstream: VIDIOC_STREAMOFF failed: ");
				return false;
			}
			break;
	}
	init ^= INIT_CAPTURE_STARTED;
	return true;
}


int Vidstream::Read(void* buffer, uint length)
{
	if(!(init & INIT_CAPTURE_STARTED) || !buffer) return -1;

	/* Block signals during IOCTL */
	sigset_t  set, old;
	sigemptyset (&set);
	sigaddset (&set, SIGCHLD);
	sigaddset (&set, SIGALRM);
	sigaddset (&set, SIGUSR1);
	sigaddset (&set, SIGTERM);
	sigaddset (&set, SIGHUP);
	pthread_sigmask (SIG_BLOCK, &set, &old);

	//wait for the device
	fd_set fd_set;
	timeval timeout;
	FD_ZERO(&fd_set);
	FD_SET(device, &fd_set);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	int ret = select(FD_SETSIZE, &fd_set, NULL, NULL, &timeout);
	if(-1 == ret && EINTR != errno)
	{
		log_errno("Vidstream: error on select");
		return -1;
	}
	else if(0 == ret)
	{
		log_errno("Vidstream: select timeout");
		return 0;
	}

	//capture
	ret = -1;
	if(io_method == IO_METHOD_READ)
    {
	    if(-1 == read(device, buffers[0].start, buffers[0].length))
        {
            switch(errno)
            {
                case EAGAIN:
                    pthread_sigmask(SIG_UNBLOCK, &old, NULL);
                    return 0;
                default:
                    log_errno("Vidstream: Read error while trying to grab image with READ method: ");
                    Close();
                    pthread_sigmask(SIG_UNBLOCK, &old, NULL);
                    return -1;
            }
        }
	    ret = copy_captured(buffer, length, buffers[0].start, buffers[0].length);
    }
	else
    {
	    v4l2_buffer buf; CLEAR(buf);
        switch(io_method)
        {
            case IO_METHOD_MMAP:
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                break;
            case IO_METHOD_USERPTR:
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                break;
            default: break;
        } //try to dequeue buffer
        if(-1 == xioctl(VIDIOC_DQBUF, &buf))
        {
            switch(errno)
            {
                case EAGAIN: //buffer not ready
                    pthread_sigmask(SIG_UNBLOCK, &old, NULL);
                    return 0;
                default:
                    log_errno("Vidstream: VIDIOC_DQBUF failed: ");
                    Close();
                    pthread_sigmask(SIG_UNBLOCK, &old, NULL);
                    return -1;
            }
        } //find buffer index for userptr buffer
        if(io_method == IO_METHOD_USERPTR)
        {
            for(buf.index = 0; buf.index < NUM_BUFFERS; buf.index++)
                if(buf.m.userptr == (unsigned long)buffers[buf.index].start
                   && buf.length == buffers[buf.index].length) break;
        }
        if(!(buf.index < NUM_BUFFERS))
        {
            log_message(1, "Vidstream: buffer index is out of range");
            Close();
            pthread_sigmask(SIG_UNBLOCK, &old, NULL);
            return -1;
        }
        //copy captured frame
        ret = copy_captured(buffer, length, buffers[buf.index].start, buffers[buf.index].length);
        //enqueue next buffer
        if(-1 == xioctl(VIDIOC_QBUF, &buf))
        {
            log_errno("Vidstream: VIDIOC_QBUF failed: ");
            Close();
            pthread_sigmask(SIG_UNBLOCK, &old, NULL);
            return -1;
        }
    }
	/*undo the signal blocking*/
	pthread_sigmask(SIG_UNBLOCK, &old, NULL);
	return ret;
}

/////////////////////////////////private functions////////////////////////////////

int Vidstream::xioctl(int request, void *arg)
{
	int r;
	do r = ioctl(device, request, arg);
	while(-1 == r && EINTR == errno);
	return r;
}


int Vidstream::copy_captured(void * to, uint to_len, void * from, uint from_len)
{
	to_len = (to_len < from_len)? to_len : from_len;
	if(!convert_format)
	{
	    memcpy(to, from, to_len);
	    return to_len;
	}
	//convert to the default pixel format
	else return v4lconvert_convert(conv_data, &format, &out_format,
                                       (unsigned char*)from, from_len,
                                       (unsigned char*)to, to_len);

}
