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

#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include "avconfig.h"


bool AVConfig::Load(string fname)
{
	if(fname.empty())
	{
		log_message(1, "No configuration filename was given.");
		return false;
	}

	avconfig = new Config();

	try { avconfig->readFile(fname.c_str()); }
	catch(FileIOException)
	{
		log_message(1, "Configuration file \"%s\" cannot be read.", fname.c_str());
		delete avconfig;
		avconfig = NULL;
		return false;
	}
	catch(ParseException e)
	{
		log_message(1, "Parsing of \"%s\" failed on line %d.", fname.c_str(), e.getLine());
		log_message(0, "%s", e.getError());
		delete avconfig;
		avconfig = NULL;
		return false;
	}

	filename = fname;
	return true;
}

bool AVConfig::Init()
{
	if(!avconfig) return false;

	int video_device = NULL;
	const char *device_name;
	struct stat st;


	//check if there're essential setting groups
	if(!getRoot()->exists("audio"))
		getRoot()->add("audio", Setting::TypeGroup);
	if(!getRoot()->exists("video"))
		getRoot()->add("video", Setting::TypeGroup);

	//open video device
	if(!getVideoSettings()->exists("device"))
		getVideoSettings()->add("device", Setting::TypeString) = "/dev/video0";
	device_name = (*getVideoSettings())["device"];
	if(-1 == stat(device_name, &st))
	{
		log_message(1, "Cannot identify '%s': %d, %s.", device_name, errno, strerror (errno));
		return false;
	}
	if(!S_ISCHR(st.st_mode))
	{
		log_message(1, "%s is no device.", device_name);
		return false;
	}

	video_device = open(device_name, O_RDWR | O_NONBLOCK, 0);
	if(-1 == video_device)
	{
		log_message(1, "Cannot open '%s': %d, %s.", device_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}


	//determine video device capabilities
	v4l2_capability cap;
	if(-1 == xioctl(video_device, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			log_message(1, "%s is no V4L2 device.", device_name);
			exit(1);
		}
		else log_message(1, "VIDIOC_QUERYCAP: %d, %s.", errno, strerror(errno));
	}
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		log_message(1, "%s is no video capture device.", device_name);
		exit(1);
	}
	if(cap.capabilities & V4L2_CAP_READWRITE)
		log_message(0, "%s supports read i/o.", device_name);
	if(cap.capabilities & V4L2_CAP_STREAMING)
		log_message(0, "%s supports streaming i/o.", device_name);


	//enumerate inputs
	vector<v4l2_input> input_list;
	v4l2_input input;
	int current_input;
	CLEAR(input);
	input.index = 0;
	while(-1 != xioctl(video_device, VIDIOC_ENUMINPUT, &input))
	{
		input_list.push_back(input);
		log_message(0, "Input [%d -- %s] is found.", input.index, input.name);
		input.index++;
	}
	if(-1 == xioctl(video_device, VIDIOC_G_INPUT, &current_input))
	{
		log_errno("VIDIOC_G_INPUT");
		exit(1);
	}
	log_message(0, "Currently selected input is: %d -- %s.", current_input, input_list[current_input].name);


	//enumerate standards
	vector<v4l2_standard> standard_list;
	v4l2_standard standard;
	v4l2_std_id current_standard;
	CLEAR(standard);
	standard.index = 0;
	if(-1 == xioctl(video_device, VIDIOC_G_STD, &current_standard))
	{
		log_errno("VIDIOC_G_STD");
		return false;
	}
	while(0 == xioctl(video_device, VIDIOC_ENUMSTD, &standard))
	{
		if(standard.id & input_list[current_input].std)
		{
			standard_list.push_back(standard);
			log_message(0, "Standard %s is supported by current input.", standard.name);
			log_message(0, "The framerate defined by this standard is: %d", standard.frameperiod.denominator/standard.frameperiod.numerator);
			if(standard.id == current_standard)
				log_message(0, "Standard %s is currently selected on input.", standard.name);
		}
		standard.index++;
	}
	if(errno != EINVAL || standard.index == 0)
	{
		log_errno("VIDIOC_ENUMSTD");
		return false;
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
		if(0 == xioctl(video_device, VIDIOC_QUERYCTRL, &queryctrl))
		{
			if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;
			queryctrl_list.push_back(queryctrl);
			control.id = queryctrl.id;
			if(0 == xioctl(video_device, VIDIOC_G_CTRL, &control))
				control_list.push_back(control);
			else
			{
				log_errno("VIDIOC_G_CTRL");
				return false;
			}
			log_message(0, "Control found: %s", queryctrl.name);

			if(queryctrl.type == V4L2_CTRL_TYPE_MENU)
			{
				log_message(0, "Control is a menu");
				enumerate_v4l2_menu(video_device, queryctrl, querymenu_list);
			}
		}
		else
		{
			if(errno == EINVAL) continue;
			log_errno("VIDIOC_QUERYCTRL");
			return false;
		}
	}

	log_message(0, "Quering driver specific controls");
	for(queryctrl.id = V4L2_CID_PRIVATE_BASE; queryctrl.id >= V4L2_CID_PRIVATE_BASE; queryctrl.id++)
	{
		if(0 == xioctl(video_device, VIDIOC_QUERYCTRL, &queryctrl))
		{
			if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;
			queryctrl_list.push_back(queryctrl);
			log_message(0, "Control found: %s", queryctrl.name);

			if(queryctrl.type == V4L2_CTRL_TYPE_MENU)
			{
				log_message(0, "Control is a menu");
				enumerate_v4l2_menu(video_device, queryctrl, querymenu_list);
			}
		}
		else
		{
			if(errno == EINVAL) break;
			log_errno("VIDIOC_QUERYCTRL");
			return false;
		}
	}


	//close the device
	close(video_device);


	//construct inputs setting
	Setting *video = getVideoSettings();

	try{ video->remove("input"); } catch(...){}
	video->add("input", Setting::TypeInt) = current_input;

	try{ video->remove("inputs"); } catch(...){}
	Setting &inputs = video->add("inputs", Setting::TypeGroup);
	for(int i = 0; i < input_list.size(); i++)
		inputs.add((const char*)input_list[i].name, Setting::TypeInt) = i;

	//construct standarts setting
	try{ video->remove("standard"); } catch(...){}
	video->add("standard", Setting::TypeInt64) = (long long)current_standard;

	try{ video->remove("standards"); } catch(...){}
	Setting &standards = video->add("standards", Setting::TypeGroup);
	for(int i = 0; i < standard_list.size(); i++)
		standards.add((const char*)standard_list[i].name, Setting::TypeInt64) = (long long)standard_list[i].id;

	//construct controls setting
	try{ video->remove("controls"); } catch(...){}
	Setting &controls = video->add("controls", Setting::TypeGroup);

	for(int i = 0; i < queryctrl_list.size(); i++)
	{
		string control_name = (const char*)queryctrl_list[i].name;
		replace(control_name.begin(), control_name.end(), " "[0], "_"[0]);
		Setting &control = controls.add(control_name.c_str(), Setting::TypeGroup);
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

	return true;
}

bool AVConfig::Save()
{
	if(!avconfig) return false;

	try { avconfig->writeFile(filename.c_str()); }
	catch(FileIOException)
	{
		log_message(1, "Cannot write to \"%s\".", filename.c_str());
		return false;
	}

	return true;
}

bool AVConfig::SaveAs(string fname)
{
	if(!avconfig) return false;

	if(fname.empty())
	{
		log_message(1, "No configuration filename was given.");
		return false;
	}

	try { avconfig->writeFile(fname.c_str()); }
	catch(FileIOException)
	{
		log_message(1, "Cannot write to \"%s\".", fname.c_str());
		return false;
	}

	filename = fname;
	return true;
}


Setting * AVConfig::getRoot()
{
	if(!avconfig) return NULL;
	return &avconfig->getRoot();
}

Setting * AVConfig::getSetting(const char * path)
{
	if(!avconfig) return NULL;

	Setting *setting = NULL;
	try { setting = &avconfig->lookup(path); }
	catch(SettingNotFoundException)
	{ log_message(1, "Setting \"%s\" not found.", path); }
	return setting;
}

int AVConfig::xioctl(int fd, int request, void * arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

bool AVConfig::enumerate_v4l2_menu(int fd, v4l2_queryctrl queryctrl, map< unsigned long, vector < v4l2_querymenu > > & querymenu_list)
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


