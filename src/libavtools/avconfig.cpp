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
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include "avconfig.h"


void AVConfig::Clear()
{
	Setting &root = avconfig.getRoot();
	for(int i = 0; i < root.getLength(); i++)
		root.remove(i);
}

void AVConfig::New()
{
	Clear();
	getRoot()->add("video", Setting::TypeGroup);
	getRoot()->add("audio", Setting::TypeGroup);
}

bool AVConfig::Load(string fname, bool readonly)
{
	if(fname.empty())
	{
		log_message(1, "AVConfig: No configuration filename was given.");
		return false;
	}

	try { avconfig.readFile(fname.c_str()); }
	catch(FileIOException)
	{
		log_message(1, "AVConfig: Configuration file \"%s\" cannot be read.", fname.c_str());
		return false;
	}
	catch(ParseException e)
	{
		log_message(1, "AVConfig: Parsing of \"%s\" failed on line %d.", fname.c_str(), e.getLine());
		log_message(0, "AVConfig: %s", e.getError());
		return false;
	}

	if(!readonly)
		filename = fname;
	return true;
}

bool AVConfig::Init()
{
	int video_device = NULL;
	const char *device_name;
	struct stat st;

	//check if there're essential settings
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
		log_message(1, "AVConfig: Cannot identify '%s': %d, %s.", device_name, errno, strerror (errno));
		return false;
	}
	if(!S_ISCHR(st.st_mode))
	{
		log_message(1, "AVConfig: %s is no device.", device_name);
		return false;
	}

	video_device = open(device_name, O_RDWR | O_NONBLOCK, 0);
	if(-1 == video_device)
	{
		log_message(1, "AVConfig: Cannot open '%s': %d, %s.", device_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}


	//determine video device capabilities
	v4l2_capability cap;
	if(-1 == xioctl(video_device, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			log_message(1, "AVConfig: %s is no V4L2 device.", device_name);
			exit(1);
		}
		else log_message(1, "AVConfig: VIDIOC_QUERYCAP: %d, %s.", errno, strerror(errno));
	}
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		log_message(1, "AVConfig: %s is no video capture device.", device_name);
		exit(1);
	}
	if(cap.capabilities & V4L2_CAP_READWRITE)
		log_message(0, "AVConfig: %s supports read i/o.", device_name);
	if(cap.capabilities & V4L2_CAP_STREAMING)
		log_message(0, "AVConfig: %s supports streaming i/o.", device_name);


	//enumerate inputs
	vector<v4l2_input> input_list;
	v4l2_input input;
	int current_input;
	CLEAR(input);
	input.index = 0;
	while(-1 != xioctl(video_device, VIDIOC_ENUMINPUT, &input))
	{
		input_list.push_back(input);
		log_message(0, "AVConfig: Input [%d -- %s] is found.", input.index, input.name);
		input.index++;
	}
	if(-1 == xioctl(video_device, VIDIOC_G_INPUT, &current_input))
	{
		log_errno("AVConfig: VIDIOC_G_INPUT");
		exit(1);
	}
	log_message(0, "AVConfig: Currently selected input is: %d -- %s.", current_input, input_list[current_input].name);


	//enumerate standards if applied
	vector<v4l2_standard> standard_list;
	v4l2_standard standard;
	v4l2_std_id current_standard;
	CLEAR(standard);
	standard.index = 0;
	if(-1 != xioctl(video_device, VIDIOC_G_STD, &current_standard))
	{
        while(0 == xioctl(video_device, VIDIOC_ENUMSTD, &standard))
        {
            if(standard.id & input_list[current_input].std)
            {
                standard_list.push_back(standard);
                log_message(0, "AVConfig: Standard %s is supported by current input.", standard.name);
                log_message(0, "AVConfig: The framerate defined by this standard is: %d", standard.frameperiod.denominator/standard.frameperiod.numerator);
                if(standard.id == current_standard)
                    log_message(0, "AVConfig: Standard %s is currently selected on input.", standard.name);
            }
            standard.index++;
        }
        if(errno != EINVAL || standard.index == 0)
        {
            log_errno("AVConfig: VIDIOC_ENUMSTD");
            return false;
        }
	}

	//enumerate controls
	vector<v4l2_queryctrl> queryctrl_list;
	vector<v4l2_control> control_list;
	map<unsigned long, vector<v4l2_querymenu> > querymenu_list;
	v4l2_queryctrl queryctrl;
	v4l2_control control;
	CLEAR(queryctrl);
	CLEAR(control);
	log_message(0, "AVConfig: Quering standard controls");
	queryctrl.id = V4L2_CID_BASE;
	while(queryctrl.id < V4L2_CID_LASTP1)
	{
	    queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	    control.id    = queryctrl.id;
	    int res = query_control(video_device, &queryctrl, &control,
	                            queryctrl_list, control_list, querymenu_list);
        if(-1 == res)
        {
            log_errno("AVConfig: unable to query device control");
            return false;
        }
        else if(0 == res) break;
	}
	log_message(0, "AVConfig: Quering driver specific controls");
	queryctrl.id = V4L2_CID_PRIVATE_BASE;
    while(true)
    {
        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        control.id    = queryctrl.id;
        int res = query_control(video_device, &queryctrl, &control,
                                queryctrl_list, control_list, querymenu_list);
        if(-1 == res)
        {
            log_errno("AVConfig: unable to query device control");
            return false;
        }
        else if(0 == res) break;
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
		inputs.add(fix_str(input_list[i].name), Setting::TypeInt) = i;

    //construct standarts setting
	try{ video->remove("standard"); } catch(...){}
	try{ video->remove("standards"); } catch(...){}
	if(!standard_list.empty())
	{
	    video->add("standard", Setting::TypeInt64) = (long long)current_standard;
        Setting &standards = video->add("standards", Setting::TypeGroup);
        for(int i = 0; i < standard_list.size(); i++)
            standards.add(fix_str(standard_list[i].name), Setting::TypeInt64) = (long long)standard_list[i].id;
	}

	//construct controls setting
	try{ video->remove("controls"); } catch(...){}
	Setting &controls = video->add("controls", Setting::TypeGroup);

	for(int i = 0; i < queryctrl_list.size(); i++)
	{
		Setting &control = controls.add(fix_str(queryctrl_list[i].name), Setting::TypeGroup);
		control.add("id", Setting::TypeInt) = (int)queryctrl_list[i].id;
		control.add("value", Setting::TypeInt) = (int)control_list[i].value;
		control.add("default_value", Setting::TypeInt) = (int)queryctrl_list[i].default_value;
		if(queryctrl_list[i].type == V4L2_CTRL_TYPE_MENU)
		{
			vector<v4l2_querymenu> items = querymenu_list[(int)queryctrl_list[i].id];
			for(int j = 0; j < items.size(); j++)
				control.add(fix_str(items[j].name), Setting::TypeInt64) = (long long)items[j].index;
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
	if(filename.empty()) return false;
	try { avconfig.writeFile(filename.c_str()); }
	catch(FileIOException)
	{
		log_message(1, "AVConfig: Cannot write to \"%s\".", filename.c_str());
		return false;
	}
	return true;
}


bool AVConfig::SaveAs(string fname)
{
	if(fname.empty())
	{
		log_message(1, "AVConfig: No configuration filename was given.");
		return false;
	}
	try { avconfig.writeFile(fname.c_str()); }
	catch(FileIOException)
	{
		log_message(1, "AVConfig: Cannot write to \"%s\".", fname.c_str());
		return false;
	}
	filename = fname;
	return true;
}


Setting * AVConfig::getSetting(const char * path)
{
	Setting *setting = NULL;
	try { setting = &avconfig.lookup(path); }
	catch(SettingNotFoundException)
	{ log_message(1, "AVConfig: Setting \"%s\" not found.", path); }
	return setting;
}


int AVConfig::xioctl(int fd, int request, void * arg)
{
	int r;
	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
	return r;
}


bool AVConfig::enumerate_v4l2_menu(int fd, v4l2_queryctrl * queryctrl,
                                        map<unsigned long, vector<v4l2_querymenu> > & querymenu_list)
{
	v4l2_querymenu querymenu;
	CLEAR(querymenu);
	querymenu_list.clear();
	querymenu.id = queryctrl->id;
	querymenu_list[(unsigned long)querymenu.id] = vector<v4l2_querymenu>(0);

	for(querymenu.index  = queryctrl->minimum;
		querymenu.index <= queryctrl->maximum;
		querymenu.index++)
	{
		if(0 == xioctl(fd, VIDIOC_QUERYMENU, &querymenu))
		{
			querymenu_list[(unsigned long)querymenu.id].push_back(querymenu);
			log_message(0, "AVConfig: Menu intem: %s", querymenu.name);
		}
		else
		{
		    if(errno == EINVAL) continue;
			log_errno("AVConfig: VIDIOC_QUERYMENU");
			return false;
		}
	}
	return true;
}


int  AVConfig::query_control(int fd,
                                v4l2_queryctrl *queryctrl,
                                v4l2_control *control,
                                vector<v4l2_queryctrl> &queryctrl_list,
                                vector<v4l2_control> &control_list,
                                map<unsigned long, vector<v4l2_querymenu> > & querymenu_list)
{
    if(0 == xioctl(fd, VIDIOC_QUERYCTRL, queryctrl))
    {
        if(queryctrl->flags & V4L2_CTRL_FLAG_DISABLED) return 1;
        queryctrl_list.push_back(*queryctrl);
        if(0 == xioctl(fd, VIDIOC_G_CTRL, control))
            control_list.push_back(*control);
        else
        {
            log_errno("AVConfig: VIDIOC_G_CTRL");
            return -1;
        }
        log_message(0, "AVConfig: Control found: %s", queryctrl->name);
        if(queryctrl->type == V4L2_CTRL_TYPE_MENU)
        {
            log_message(0, "AVConfig: Control is a menu");
            enumerate_v4l2_menu(fd, queryctrl, querymenu_list);
        }
    }
    else
    {
        if(errno == EINVAL) return 0;
        log_errno("AVConfig: VIDIOC_QUERYCTRL");
        return -1;
    }
    return 1;
}

string AVConfig::fix_str(void *buf)
{
    string s = string((const char*)buf);
    replace(s.begin(), s.end(), ' ', '_');
    replace(s.begin(), s.end(), ',', '_');
    return s;
}
