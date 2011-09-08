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
#include <avconfig.h>
#include <img_tools.h>
#include <vidstream.h>
#include <sndstream.h>
#include <recorder.h>


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

	AVConfig cfg;
	cfg.Load(cfg_file);

	if(init)
	{
		cfg.Init();

		Vidstream v_source;
		v_source.Open(cfg.getVideoSettings());
		v_source.Close();

		Sndstream a_source;
		a_source.Open(cfg.getAudioSettings());
		a_source.Close();

		Recorder recorder;
		recorder.Init(cfg.getConfig());

		cfg.SaveAs(new_cfg);
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
