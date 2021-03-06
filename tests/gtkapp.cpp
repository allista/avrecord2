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

#include <gtkmm.h>
#include <gtksourceviewmm.h>
#include <sigc++/sigc++.h>
#include <iostream>
using namespace Gtk;
using namespace Glib;

#include "avrtunerwindow.h"

const char *avrtuner_glade_file = "gtkapp.glade";

int main (int argc, char *argv[])
{
	Main gtk_main(argc, argv);

	RefPtr<Builder> builder;
	try { builder = Builder::create_from_file(avrtuner_glade_file); }
	catch(const Error& ex)
	{
		std::cerr << "Exception from Gtk::Builder::create_from_file() from file " << avrtuner_glade_file << std::endl;
		std::cerr << "  Error: " << ex.what() << std::endl;
		return -1;
	}

	AVRTunerWindow *MainWindow = NULL;
	builder->get_widget_derived("MainWindow", MainWindow);

	gtk_main.run(*MainWindow);

	return 0;
}