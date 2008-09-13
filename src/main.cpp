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
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <getopt.h>

using namespace std;

#include <configfile.h>
#include <recorder.h>


///default config file
char *CONFIG_FILE   = "avrecord.conf";

///default log file
char *LOG_FILE      = "avrecord.log";
bool  nolog         = false; ///< if true, log to stderr

///ofstream object of opened logfile
ofstream log_stream;

///signal that handles recording process
uint avsignal       = SIG_RECORDING;
uint avrestart      = 0; ///< restart flag
bool daemonize      = false;


///a signal handler that takes care about proper record ending
static void sig_handler(int signo);

///sets up our signal handler
static void setup_signals(struct sigaction *sig_handler_action);

/// Turns Motion into a daemon through forking.
/// The parent process (i.e. the one initially calling this function) will
/// exit inside this function, while control will be returned to the child
/// process. Standard input/output are released properly, and the current
/// directory is set to / in order to not lock up any file system.
static void become_daemon(void);

///prints the Usage text
static void print_usage();




///the main program =)
int main(int argc, char *argv[])
{
	//parse commandline args
	string log_fname;
	string conf_fname;
	if(argc > 1)
	{
		int c;
		while((c = getopt(argc, argv, "c:l:dnh?"))!=EOF)
			switch(c)
			{
				case 'c':
					conf_fname = string(optarg);
					break;
				case 'l':
					log_fname  = string(optarg);
					break;
				case 'd':
					daemonize  = true;
					break;
				case 'n':
					nolog      = true;
					break;
				case 'h':
				case '?':
					print_usage();
					exit(1);
				default: break;
			}
	}

	//if -d option is passed, fork the process
	if(daemonize && fork())
	{
		log_message(0, "AVRecord going to daemon mode");
		exit(0);
	}

	//compile filenames
	ifstream tester;
	string homedir(getenv("HOME"));
	if(homedir.size()) homedir += "/.avrecord/.";
	string workdir(getenv("PWD"));
	if(workdir.size()) workdir += '/';

	//config file
	while(conf_fname.empty())
	{
		//in current dir?
		tester.open(CONFIG_FILE, ios::in);
		if(tester.is_open())
		{
			conf_fname = string(CONFIG_FILE);
			tester.close();
			break;
		}
		tester.clear();

		//in home dir?
		tester.open((homedir + CONFIG_FILE).c_str(), ios::in);
		if(tester.is_open())
		{
			conf_fname = (homedir + CONFIG_FILE);
			tester.close();
			break;
		}
		tester.clear();

		//in /etc?
		string etc("/etc/");
		etc = etc + PACKAGE + '/' + CONFIG_FILE;
		tester.open(etc.c_str(), ios::in);
		if(tester.is_open())
		{
			conf_fname = etc;
			tester.close();
			break;
		}
		tester.clear();

		//no config file =(
		break;
	}

	//make sure, that we work with absolute path only
	//this will be needed in daemon mode
	if(conf_fname.size() && conf_fname[0] != '/')
		conf_fname = workdir + conf_fname;

	//load config file
	ConfigFile config;
	if(conf_fname.size())
		config = ConfigFile(conf_fname);
	else log_message(0, "No config file was found. Using default settings.");

	//setup output directory
	string output_dir = config.getOptionS("output_dir");
	if(output_dir.size() && output_dir[0] != '/')
		output_dir = workdir + output_dir;
	config.setOption("output_dir", output_dir);

	//log file
	while(!nolog && log_fname.empty())
	{
		//no config file? using current dir for log file
		if(conf_fname.empty())
		{
			log_fname = string(LOG_FILE);
			break;
		}

		//loading from config file
		log_fname = config.getOptionS("log_file");

		//no log_file option in config file? using wordir for log file
		if(log_fname.empty())
		{
			log_fname = string(LOG_FILE);
			break;
		}

		//have logfile =)
		break;
	}

	//open logfile
	if(!nolog)
	{
		//make sure, that we work with absolute paths only
		//this will be needed in daemon mode
		if(log_fname.size() && log_fname[0] != '/')
			log_fname = workdir + log_fname;

		log_stream.open(log_fname.c_str(), ios::out|ios::app);
		if(!log_stream.is_open())
			log_message(1, "Can't open log file %s Logging to stderr...", log_fname.c_str());
	}

	struct sigaction sig_handler_action;
	setup_signals(&sig_handler_action);
	Recorder rec;

	//if daemon mode is used, we are in the child proces now, so
	//after all initialization is done, turn this process into daemon
	if(daemonize) become_daemon();
	do
	{
		if(avrestart)
		{
			log_message(0, "Restarting...");
			avsignal  = SIG_RECORDING;
			avrestart = 0;
			sleep(5);
		}
		else log_message(0, "Starting...");

		rec.Init(config);
		rec.RecordLoop(&avsignal);
		rec.Close();

	}
	while(avrestart);

	log_message(0, "Quiting...");
	log_stream.close();
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


static void become_daemon()
{
	struct sigaction sig_ign_action;

	//Setup sig_ign_action
	sig_ign_action.sa_flags = SA_RESTART;
	sig_ign_action.sa_handler = SIG_IGN;
	sigemptyset(&sig_ign_action.sa_mask);

	//changing dir to root enables people to unmount a disk
	//without having to stop avrecord
	if(chdir("/")) cerr << "Could not change directory to '/'" << endl;
	setpgrp();

	int i;
	if((i = open("/dev/tty", O_RDWR)) >= 0)
	{
		ioctl(i, TIOCNOTTY, NULL);
		close(i);
	}

	setsid();
	i = open("/dev/null", O_RDONLY);

	if(i != -1)
	{
		dup2(i, STDIN_FILENO);
		close(i);
	}

	i = open("/dev/null", O_WRONLY);

	if(i != -1)
	{
		dup2(i, STDOUT_FILENO);
		dup2(i, STDERR_FILENO);
		close(i);
	}

	sigaction(SIGTTOU, &sig_ign_action, NULL);
	sigaction(SIGTTIN, &sig_ign_action, NULL);
	sigaction(SIGTSTP, &sig_ign_action, NULL);
}


static void print_usage()
{
	cout << "Usage:\n"
	<< "\tavrecord [-c <config_file>] "
	<< "[-l <log_file>] [-ndh?]\n"
	<< "\t-n\tno log file. Log to stderr instead.\n"
	<< "\t-d\tstart in the daemon mode.\n"
	<< "\t-h -?\tprint this message.\n";
}


void log_message(int level, const char *fmt, ...)
{
	int     n = 0;
	char    buf[1024];
	va_list ap;

	//Next add timstamp and level prefix
	time_t now = time(0);
	n += strftime(buf+n, sizeof(buf)-n, "%Y-%m-%d %H:%M:%S ", localtime(&now));
	n += snprintf(buf+n, sizeof(buf)-n, "[%s] ", level? "ERROR" : "INFO");

	//Next add the user's message
	va_start(ap, fmt);
	n += vsnprintf(buf+n, sizeof(buf)-n, fmt, ap);

	//newline for printing to the file
	strcat(buf, "\n");

	//output...
	if(nolog || !log_stream.is_open())
		if(level)	cerr << buf << flush; //log to stderr
		else cout << buf << flush; //log to stdout
	else log_stream << buf << flush;

	//Clean up the argument list routine
	va_end(ap);
}
