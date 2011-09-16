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

#include <sigc++/sigc++.h>
#include <gtkmm.h>
using namespace Gtk;

#include <iostream>
#include <string>
using namespace std;

#include "avrtunerwindow.h"

static string avrtuner_glade_file = string(DATA_PATH) + "/avrtunerwindow.glade";

AVRTunerWindow *MainWindow = NULL;

int main (int argc, char *argv[])
{
#ifdef DEBUG_VERSION
	avrtuner_glade_file = "./avrtunerwindow.glade";
#endif

	string config_file;
	if(argc > 1) config_file = string(argv[1]);
	if(config_file.empty())
		log_message(0, "No configuration filename supplied.");

	if(!Glib::thread_supported()) Glib::thread_init();
	if(!Glib::thread_supported())
		std::cerr << "Thread system is not initialized.\n" << flush;

	Main gtk_main(argc, argv);

	Glib::RefPtr<Builder> builder;
	try { builder = Builder::create_from_file(avrtuner_glade_file); }
	catch(const Glib::Error& ex)
	{
		std::cerr << "Exception from Gtk::Builder::create_from_file() from file " << avrtuner_glade_file << std::endl;
		std::cerr << "  Error: " << ex.what() << std::endl;
		return -1;
	}

	builder->get_widget_derived("MainWindow", MainWindow);
	if(config_file.size())
		MainWindow->LoadConfiguration(config_file);
	gtk_main.run(*MainWindow);

	return 0;
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
	if(level) cerr << buf << flush; //log to stderr
	else cout << buf << flush; //log to stdout
	if(MainWindow) MainWindow->LogMessage(buf); //to log window

	//Clean up the argument list routine
	va_end(ap);
}
