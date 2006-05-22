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
#include <configfile.h>
#include <recorder.h>

int main(int argc, char *argv[])
{
	//testing section//
	///////////////////

	Recorder rec;
	rec.Init("../../avrecord.conf.sample");

	uint signal = 1;
	rec.RecordLoop(&signal);

	rec.Close();

	exit(EXIT_SUCCESS);
}
