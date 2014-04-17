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

/**
Wrapper for libconfig Config object which also uses v4l2 and ffmpeg api to gather system information and provide flexible customizable configuration.

	@author Allis Tauri <allista@gmail.com>
*/

#ifndef AVCONFIG_H
#define AVCONFIG_H

extern "C"
{
#define INT64_C(c) c ## LL
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <linux/videodev2.h>
#include <libconfig.h++>
using namespace libconfig;

#include <map>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

#include "common.h"

class AVConfig
{
public:
	///conversion to the reference to the Config object
	operator Config & () { return avconfig; }
	///forwardig of the original lookup function. May throw exeptions!
	Setting &lookup(const char* path) { return avconfig.lookup(path); }
	///forwardig of the original lookup function. May throw exeptions!
	Setting &lookup(string path)      { return avconfig.lookup(path); }

	//constructors and destructor
	AVConfig() {}
	AVConfig(string fname) { Load(fname); }
	~AVConfig() {}

	void Clear(); ///< remove all settings

	void New(); ///< make new configuration

	bool Load(string fname, bool readonly = false); ///< load configuration from a file

	///initialize configuration by quering devices using loaded configuration file
	bool Init();

	bool Save(); ///< save configuration into the file "filename"

	bool SaveAs(string fname); ///< save configuration into another file

	string getFileName() const { return filename; }

	///pointer to the Config object
	Config  *getConfig() { return &avconfig; }

	///pointer to the root setting group
	Setting *getRoot() { return &avconfig.getRoot(); }

	///pointer to the setting under the root by path or NULL if not found
	Setting *getSetting(const char *path);

	Setting *getAudioSettings() ///< pointer to the audio section of a configuration
	{ return getSetting("audio"); }

	Setting *getVideoSettings() ///< pointer to the video section of a configuration
	{ return getSetting("video"); }

private:
	Config avconfig;  ///< Config object
	string filename;  ///< name of the configuration file loaded

	string fix_str(void *buf);
	int  xioctl(int fd, int request, void *arg);
	bool enumerate_v4l2_menu(int fd, v4l2_queryctrl *queryctrl,
	                            map<unsigned long, vector<v4l2_querymenu> > &querymenu_list);
	int  query_control(int fd,
	                     v4l2_queryctrl *queryctrl,
	                     v4l2_control *control,
	                     vector<v4l2_queryctrl> &queryctrl_list,
	                     vector<v4l2_control> &control_list,
	                     map<unsigned long, vector<v4l2_querymenu> > & querymenu_list);
};

#endif
