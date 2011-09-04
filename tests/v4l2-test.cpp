/***************************************************************************
 *   Copyright (C) 2007 by Allis Tauri   *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>

#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libconfig.h++>
using namespace libconfig;

#include <map>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

#include <utimer.h>
#include <common.h>
#include <configfile.h>
#include <img_tools.h>

static Config cfg;
int fd = -1;

///tool functions
static int xioctl(int fd, int request, void *arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

static Setting& get_setting(const char* path)
{
	Setting* setting = NULL;
	try
		{ setting = &cfg.lookup(path); }
	catch(SettingNotFoundException)
		{
			log_message(1, "Setting %s not found.", path);
			assert(setting);
		}
	return *setting;
}

static int enumerate_v4l2_menu(v4l2_queryctrl queryctrl, map<unsigned long, vector<v4l2_querymenu> >& querymenu_list)
{
	v4l2_querymenu querymenu;
	CLEAR(querymenu);
	querymenu_list.clear();
	querymenu.id = queryctrl.id;
	querymenu_list[(unsigned long)querymenu.id] = vector<v4l2_querymenu>(0);
	for(querymenu.index = queryctrl.minimum;
		querymenu.index <= queryctrl.maximum;
		querymenu.index++)
	{
		if(0 == xioctl(fd, VIDIOC_QUERYMENU, &querymenu))
		{
			querymenu_list[(unsigned long)querymenu.id].push_back(querymenu);
			log_message(0, "Menue intem: %s", querymenu.name);
		}
		else
		{
			log_errno("VIDIOC_QUERYMENU");
			return false;
		}
	}
	return true;
}

int main(int argc, char *argv[])
{
	string cfg_file;
	string new_cfg;
	bool init = false;

	if(argc > 1)
	{
		int c;
		while((c = getopt(argc, argv, "c:i"))!=EOF)
		switch(c)
		{
			case 'c':
				cfg_file = string(optarg);
				break;
			case 'i':
				new_cfg = "new_";
				init = true;
				break;
			default: break;
		}
	}
	if(cfg_file.empty())
	{
		log_message(1, "No -c <cfg_filename> option was supplied.");
		exit(1);
	}
	if(init) new_cfg += cfg_file;

	try { cfg.readFile(cfg_file.c_str()); }
	catch(FileIOException)
		{
			log_message(1, "Configuration file \"%s\" cannot be read.", cfg_file.c_str());
			exit(1);
		}
	catch(ParseException)
		{
			log_message(1, "Parse error while parsing \"%s\".", cfg_file.c_str());
			exit(1);
		}
	catch(...)
		{ log_message(0, "Unknown exeption while parsing \"%s\".", cfg_file.c_str()); }

	const char* dev_name = NULL;
	uint io_method = 0;
	struct stat st;

	dev_name = get_setting("avrecord.video.device");
	if(-1 == stat(dev_name, &st))
	{
		log_message(1, "Cannot identify '%s': %d, %s.", dev_name, errno, strerror (errno));
		exit(1);
	}

	if(!S_ISCHR(st.st_mode))
	{
		log_message(1, "%s is no device.", dev_name);
		exit (1);
	}

	fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
	if(-1 == fd)
	{
		log_message(1, "Cannot open '%s': %d, %s.", dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	v4l2_capability cap;
	if(-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			log_message(1, "%s is no V4L2 device.", dev_name);
			exit(1);
		}
		else log_message(1, "VIDIOC_QUERYCAP: %d, %s.", errno, strerror(errno));
	}

	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		log_message(1, "%s is no video capture device.", dev_name);
		exit(1);
	}

	if(cap.capabilities & V4L2_CAP_READWRITE)
	{
		log_message(0, "%s supports read i/o.", dev_name);
		io_method |= V4L2_CAP_READWRITE;
	}

	if(cap.capabilities & V4L2_CAP_STREAMING)
	{
		log_message(0, "%s supports streaming i/o.", dev_name);
		io_method |= V4L2_CAP_STREAMING;
	}


	//enumerate inputs
	vector<v4l2_input> input_list;
	v4l2_input input;
	int current_input;
	CLEAR(input);
	input.index = 0;
	while(-1 != xioctl(fd, VIDIOC_ENUMINPUT, &input))
	{
		input_list.push_back(input);
		log_message(0, "Input [%d -- %s] is found.", input.index, input.name);
		input.index++;
	}
	if(-1 == xioctl(fd, VIDIOC_G_INPUT, &current_input))
	{
		log_errno("VIDIOC_G_INPUT");
		exit(1);
	}
	log_message(0, "Currently selected input is: %d -- %s.", current_input, input_list[current_input].name);

	//enum standarts
	vector<v4l2_standard> standard_list;
	v4l2_standard standard;
	v4l2_std_id current_standard;
	CLEAR(standard);
	standard.index = 0;
	if(-1 == xioctl(fd, VIDIOC_G_STD, &current_standard))
	{
		log_errno("VIDIOC_G_STD");
		exit(0);
	}
	while(0 == xioctl(fd, VIDIOC_ENUMSTD, &standard))
	{
		if(standard.id & input_list[current_input].std)
		{
			standard_list.push_back(standard);
			log_message(0, "Standard %s is supported by current input.", standard.name);
			log_message(0, "The framerate defined by this standard is: %d", standard.frameperiod.denominator/standard.frameperiod.numerator);
			if(standard.id & current_standard)
				log_message(0, "Standard %s is currently selected on input.", standard.name);
		}
		standard.index++;
	}
	if(errno != EINVAL || standard.index == 0)
	{
		log_errno("VIDIOC_ENUMSTD");
		exit(0);
	}

	//enumerate controls
	vector<v4l2_queryctrl> queryctrl_list;
	vector<v4l2_control> control_list;
	map<unsigned long, vector<v4l2_querymenu> > querymenu_list;
	v4l2_queryctrl queryctrl;
	v4l2_control control;
	CLEAR(queryctrl);
	CLEAR(control);
	log_message(0, "Quering standard controls");
	for(queryctrl.id = V4L2_CID_BASE;
		queryctrl.id < V4L2_CID_LASTP1 && queryctrl.id >= V4L2_CID_BASE;
		queryctrl.id++)
	{
		if(0 == xioctl(fd, VIDIOC_QUERYCTRL, &queryctrl))
		{
			if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;
			queryctrl_list.push_back(queryctrl);
			control.id = queryctrl.id;
			if(0 == xioctl(fd, VIDIOC_G_CTRL, &control))
				control_list.push_back(control);
			else
			{
				log_errno("VIDIOC_G_CTRL");
				exit(0);
			}
			log_message(0, "Control found: %s", queryctrl.name);

			if(queryctrl.type == V4L2_CTRL_TYPE_MENU)
			{
				log_message(0, "Control is a menu");
				enumerate_v4l2_menu(queryctrl, querymenu_list);
			}
		}
		else
		{
			if(errno == EINVAL) continue;
			log_errno("VIDIOC_QUERYCTRL");
			exit(0);
		}
	}

	log_message(0, "Quering driver specific controls");
	for(queryctrl.id = V4L2_CID_PRIVATE_BASE; queryctrl.id >= V4L2_CID_PRIVATE_BASE; queryctrl.id++)
	{
		if(0 == xioctl(fd, VIDIOC_QUERYCTRL, &queryctrl))
		{
			if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;
			queryctrl_list.push_back(queryctrl);
			log_message(0, "Control found: %s", queryctrl.name);

			if(queryctrl.type == V4L2_CTRL_TYPE_MENU)
			{
				log_message(0, "Control is a menu");
				enumerate_v4l2_menu(queryctrl, querymenu_list);
			}
		}
		else
		{
			if(errno == EINVAL) break;
			log_errno("VIDIOC_QUERYCTRL");
			exit(0);
		}
	}

	//determing croping parameters
	v4l2_cropcap cropcap;
	v4l2_format fmt;
	CLEAR(cropcap);
	CLEAR (fmt);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
	{
		fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width       = cropcap.defrect.width;
		fmt.fmt.pix.height      = cropcap.defrect.height;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
		fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
		if(-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
			log_errno("VIDIOC_TRY_FMT");
	}
	else
	{
		log_errno("VIDIOC_CROPCAP");
		exit(0);
	}

	//sleep(10);
	close(fd);

	//construct inputs setting
	if(init)
	{
		Setting& video = get_setting("avrecord.video");
		try{ video.remove("input"); } catch(...){}
		video.add("input", Setting::TypeInt) = current_input;

		try{ video.remove("inputs"); } catch(...){}
		Setting& inputs = video.add("inputs", Setting::TypeGroup);
		for(int i = 0; i < input_list.size(); i++)
			inputs.add((const char*)input_list[i].name, Setting::TypeInt) = i;
	}

	//construct standarts setting
	if(init)
	{
		Setting& video = get_setting("avrecord.video");
		try{ video.remove("standard"); } catch(...){}
		video.add("standard", Setting::TypeInt64) = (long long)current_standard;

		try{ video.remove("standards"); } catch(...){}
		Setting& inputs = video.add("standards", Setting::TypeGroup);
		for(int i = 0; i < standard_list.size(); i++)
			inputs.add((const char*)standard_list[i].name, Setting::TypeInt64) = (long long)standard_list[i].id;
	}

	//construct controls setting
	if(init)
	{
		Setting& video = get_setting("avrecord.video");
		try{ video.remove("controls"); } catch(...){}
		Setting& controls = video.add("controls", Setting::TypeGroup);
		for(int i = 0; i < queryctrl_list.size(); i++)
		{
			string control_name = (const char*)queryctrl_list[i].name;
			replace(control_name.begin(), control_name.end(), " "[0], "_"[0]);
			Setting& control = controls.add(control_name.c_str(), Setting::TypeGroup);
			control.add("id", Setting::TypeInt) = (int)queryctrl_list[i].id;
			control.add("value", Setting::TypeInt) = (int)control_list[i].value;
			control.add("default_value", Setting::TypeInt) = (int)queryctrl_list[i].default_value;
			if(queryctrl_list[i].type == V4L2_CTRL_TYPE_MENU)
			{
				vector<v4l2_querymenu> items = querymenu_list[(int)queryctrl_list[i].id];
				for(int j = 0; j < items.size(); j++)
					control.add((const char*)items[j].name, Setting::TypeInt64) = (int)items[j].index;
			}
			else
			{
				control.add("minimum", Setting::TypeInt) = (int)queryctrl_list[i].minimum;
				control.add("maximum", Setting::TypeInt) = (int)queryctrl_list[i].maximum;
				control.add("step", Setting::TypeInt) = (int)queryctrl_list[i].step;
			}
		}
	}

	//write configuration
	if(init)
	{
		try { cfg.writeFile(new_cfg.c_str()); }
		catch(FileIOException)
			{
				log_message(1, "Cannot write configuration to file \"%s\".", cfg_file.c_str());
				exit(1);
			}
	}

	exit(0);
}

void log_message(int level, const char *fmt, ...)
{
	int     n;
	char    buf[1024];
	va_list ap;

	//Next add timstamp and level prefix
	time_t now = time(0);
	n  = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S ", localtime(&now));
	n += snprintf(buf + n, sizeof(buf), "[%s] ", level? "ERROR" : "INFO");

	//Next add the user's message
	va_start(ap, fmt);
	n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

	//newline for printing to the file
	strcat(buf, "\n");

	//output...
	if(level)	cerr << buf << flush; //log to stderr
	else cout << buf << flush; //log to stdout

	//Clean up the argument list routine
	va_end(ap);
}
