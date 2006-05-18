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

using namespace std;

#include <sndstream.h>
#include <avifile.h>
#include <img_tools.h>

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		cerr << "Usage: test <file>" << endl;
		exit(1);
	}

	Sndstream snd;
	snd.Open(SND_R, SND_16BIT, 1, 44100);
	snd.setAmp(30);

	uint frate = uint(1e6/snd.ptime());
	AV_Init();
	AVIFile avif;
	avif.Init();
	//avif.setAParams("pcm_s16le", 1, 44100, 64000);
	avif.setAParams("mp2", 1, 44100, 128000);
	avif.setVParams("msmpeg4", 640, 480, frate, 650000, 2);
	avif.Open(argv[1]);

	uint8_t* buffer;
	uint     size;
	size   = snd.bsize();
	buffer = new uint8_t[size];
	unsigned char *img = new unsigned char[640*480*3];

	for(int i = 0; i<500; i++)
	{
		memset(img, i, 640*480*3);
		DrawText(img, "This is a test text\n wich low left corner is located\n  in the center of the screen",
		         320, 240, 640, 480);
		avif.writeVFrame(img, 640, 480);
		snd.Read(buffer, &size);
		avif.writeAFrame(buffer, size);
	}

	delete[] img;
	delete[] buffer;
	avif.Close();
	snd.Close();
	AV_Free();

	exit(EXIT_SUCCESS);
}
