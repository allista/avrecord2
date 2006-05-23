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
#include <signal.h>

using namespace std;

#include <sndstream.h>
#include <vidstream.h>
#include <avifile.h>
#include <img_tools.h>
#include <utimer.h>
#include <configfile.h>
#include <recorder.h>

uint avsignal  = SIG_RECORDING;
uint avrestart = 0;

static void setup_signals(struct sigaction *sig_handler_action);
static void sig_handler(int signo);

int main(int argc, char *argv[])
{
	//testing section//
	///////////////////

	struct sigaction sig_handler_action;
	setup_signals(&sig_handler_action);

	Recorder rec;
	do
	{
		if(avrestart)
		{
			cout << "Restarting..." << endl;
			avsignal  = SIG_RECORDING;
			avrestart = 0;
			sleep(5);
		}

		rec.Init("../../avrecord.conf.sample");
		rec.RecordLoop(&avsignal);
		rec.Close();

	} while(avrestart);

	cout << "Quiting..." << endl;
	exit(EXIT_SUCCESS);
}

static void setup_signals(struct sigaction *sig_handler_action)
{
	sig_handler_action->sa_flags = SA_RESTART;
	sig_handler_action->sa_handler = sig_handler;
	sigemptyset(&sig_handler_action->sa_mask);

	//Enable automatic zombie reaping//
	sigaction(SIGALRM, sig_handler_action, NULL);
	sigaction(SIGHUP,  sig_handler_action, NULL);
	sigaction(SIGINT,  sig_handler_action, NULL);
	sigaction(SIGQUIT, sig_handler_action, NULL);
	sigaction(SIGTERM, sig_handler_action, NULL);
	sigaction(SIGUSR1, sig_handler_action, NULL);
}

static void sig_handler(int signo)
{
	switch(signo)
	{
		case SIGALRM:
			//avsignal  = SIG_; //what else can we do? =)
			break;
		case SIGUSR1:
			avsignal  = SIG_CHANGE_FILE;
			break;
		case SIGHUP:
			avrestart = 1;
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
			avsignal  = SIG_QUIT;
			break;
		case SIGSEGV:
			exit(0);
	}
}
