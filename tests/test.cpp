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

#include <qthread.h>
#include <qmutex.h>

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

void log_message(int level, const char *fmt, ...);
static void setup_signals(struct sigaction *sig_handler_action);
static void sig_handler(int signo);

class Thread1 : public QThread
{
	public:
		void stop()
		{
			_mutex.unlock();
			terminate();
		};

		void lock() { _mutex.lock(); };
		void unlock() { _mutex.unlock(); };

	protected:
		void run()
		{
			while(true)
			{

				log_message(0, "Thread1: locking...");
				_mutex.lock();
				log_message(0, "Thread1: locked");
				int i = 0;
				i++;
				msleep(3);
				log_message(0, "Thread1: unlocking...\n");
				_mutex.unlock();
				usleep(5);
			}
		};

	private:
		QMutex _mutex;
};

class Thread2 : public QThread
{
	public:
		void stop()
		{
			thread.unlock();
			thread.stop();
			thread.wait();
			terminate();
		};

	protected:
		void run()
		{
			thread.start();
			int total = 100;
			while(total--)
			{
				log_message(0, "Thread2: locking...");
				thread.lock();
				log_message(0, "Thread2: locked");
				int i = 0;
				i++;
				log_message(0, "Thread2: unlocking...\n");
				thread.unlock();
				msleep(7);
			}
			stop();
		};

	private:
		Thread1 thread;
};

int main(int argc, char *argv[])
{
	//testing section//
	UTimer test;
	test.start();
	sleep(1);
	test.pause();
	cout << test.elapsed() << endl;
	sleep(1);
	cout << test.elapsed() << endl;
	///////////////////

	Thread2 thread;

	thread.start();
	thread.wait();

// 	struct sigaction sig_handler_action;
// 	setup_signals(&sig_handler_action);
//
// 	Recorder rec;
// 	do
// 	{
// 		if(avrestart)
// 		{
// 			cout << "Restarting..." << endl;
// 			avsignal  = SIG_RECORDING;
// 			avrestart = 0;
// 			sleep(5);
// 		}
//
// 		rec.Init("../../avrecord.conf.sample");
// 		rec.RecordLoop(&avsignal);
// 		rec.Close();
//
// 	}
// 	while(avrestart);

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
