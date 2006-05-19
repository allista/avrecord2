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

using namespace std;

#include <sndstream.h>
#include <vidstream.h>
#include <avifile.h>
#include <img_tools.h>
#include <utimer.h>

void save_frame()
{}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		cerr << "Usage: test <file>" << endl;
		exit(1);
	}

	UTimer timer, total;

	Sndstream snd;
	snd.Open(SND_R, SND_16BIT, 1, 44100);
	snd.setAmp(20);

	Vidstream vid;
	vid.Open("/dev/video0", 640, 480, IN_COMPOSITE1, NORM_PAL_NC);


	uint vperiod  = (uint) vid.ptime();
	uint speriod  = (uint) snd.ptime();
	uint frate    = (uint) 1e6/speriod;
	uint duration = int(15*1e6);

	cout << "fps: " << frate << endl;

	AV_Init();
	AVIFile avif;
	avif.Init();
	avif.setAParams("mp3", 1, 44100, 128000);
	avif.setVParams("msmpeg4", 640, 480, frate, 650000, 2);
	avif.Open(argv[1]);

	uint8_t* buffer;
	uint     size = snd.bsize();
	uint     vsize = 640*480*3;
	buffer = new uint8_t[size];
	unsigned char *img = new unsigned char[vsize];

	time_t now;
	timer.start();
	total.start();
	vid.Prepare();
	while(total.elapsed() < duration)
	{
		if(timer.elapsed() >= vperiod)
		{
			timer.reset();
			now = time(0);
			string date = ctime(&now);
			date.erase(date.size()-1);
			vid.Read(img, vsize);
			DrawText(img, date, 635-TextWidth(date), 475, 640, 480);
		}
		avif.writeVFrame(img, 640, 480);
		snd.Read(buffer, &size);
		avif.writeAFrame(buffer, size);
	}

	delete[] img;
	delete[] buffer;
	avif.Close();
	snd.Close();
	vid.Close();
	AV_Free();

	exit(EXIT_SUCCESS);
}
