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


#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))
#define NORM               100
#define ABS(x)             ((x)<0 ? -(x) : (x))
#define DIFF(x, y)         (ABS((x)-(y)))
#define NDIFF(x, y)        (ABS(x)*NORM/(ABS(x)+2*DIFF(x,y)))


/* Erodes in a + shape */
static int erode5(unsigned char *img, int width, int height, void *buffer)
{
	int y, i, sum = 0;
	char *Row1,*Row2,*Row3;
	Row1 = (char*) buffer;
	Row2 = Row1 + width;
	Row3 = Row1 + 2*width;
	memset(Row2, 0, width);
	memcpy(Row3, img, width);
	for (y = 0; y < height; y++)
	{
		memcpy(Row1, Row2, width);
		memcpy(Row2, Row3, width);
		if (y == height-1)
			memset(Row3, 0, width);
		else
			memcpy(Row3, img+(y+1)*width, width);

		for (i = width-2; i >= 1; i--)
		{
			if (Row1[i]   == 0 ||
			        Row2[i-1] == 0 ||
			        Row2[i]   == 0 ||
			        Row2[i+1] == 0 ||
			        Row3[i]   == 0)
				img[y*width+i] = 0;
			else
				sum++;
		}
		img[y*width] = img[y*width+width-1] = 0;
	}
	return sum;
}

static int alg_diff_fast(unsigned char *old_img, unsigned char *new_img)
{
	int diffs=0, step=640*480/10000;
	int noise = 3;

	if(!step%2) step++;

	//cout << "cdiff: start" << endl;
	for (int i = 640*480; i>0; i-=step)
	{
		/* using a temp variable is 12% faster */
		register unsigned char curdiff= abs(*old_img-*new_img);
		if(curdiff > noise)	diffs++;
		old_img+=step;
		new_img+=step;
	}
	if(diffs < 14) diffs=0;
	return diffs;
}

int main(int argc, char *argv[])
{
	//testing section//
	/*	UTimer tmr;
		cout << "tmr: " << tmr.elapsed() << endl;*/
	///////////////////

	if(argc < 2)
	{
		cerr << "Usage: test <file>" << endl;
		exit(1);
	}

	Sndstream snd;
	snd.Open(SND_R, SND_16BIT, 1, 44100);
	snd.setAmp(20);

	Vidstream vid;
	vid.Open("/dev/video0", 640, 480, IN_COMPOSITE1, NORM_PAL_NC);


	uint vperiod  = (uint) vid.ptime();
	uint speriod  = (uint) snd.ptime();
	uint frate    = (uint) 1e6/vperiod;
	uint duration = int(20*1e6);
	double a_pts = 0;
	double v_pts = 0;

	cout << "fps: " << frate << endl;

	AV_Init();
	AVIFile avif;
	avif.Init();
	avif.setAParams("mp3", 1, 44100, 96000);
	avif.setVParams("msmpeg4", 640, 480, frate, 650000, 2);
	avif.Open(argv[1]);

	uint8_t* buffer;
	uint     size = snd.bsize();
	uint     vsize = 640*480*3;
	buffer = new uint8_t[size];
	unsigned char *img  = new unsigned char[vsize];
	unsigned char *img_new = new unsigned char[vsize];
	unsigned char *img_old = new unsigned char[vsize];
	unsigned char *tmp = new unsigned char[640*3];

	UTimer total;
	time_t now;

	total.start();
	vid.Prepare();
	while(total.elapsed() < duration)
	{
		/* compute current audio and video time */
		v_pts = avif.getVpts();
		a_pts = avif.getApts();

		/* write interleaved audio and video frames */
		if(a_pts < v_pts)
		{
			snd.Read(buffer, &size);
			avif.writeAFrame(buffer, size);
		}
		else
		{
			now = time(0);
			string date = ctime(&now);
			date.erase(date.size()-1);
			vid.Read(img, vsize);
			memcpy(img_old, img_new, vsize);
			memcpy(img_new, img, vsize);
			erode5(img_new, 640, 480, tmp);
			char diffs[255];
			sprintf(diffs, "%d", alg_diff_fast(img_old, img_new));
			DrawText(img, diffs, 600, 10, 640, 480);
			DrawText(img, date, 635-TextWidth(date), 475, 640, 480);
			avif.writeVFrame(img, 640, 480);
		}
	}

	delete[] img;
	delete[] img_old;
	delete[] img_new;
	delete[] tmp;
	delete[] buffer;
	avif.Close();
	snd.Close();
	vid.Close();
	AV_Free();

	exit(EXIT_SUCCESS);
}
