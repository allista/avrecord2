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
#ifndef AVRTUNERWINDOW_H
#define AVRTUNERWINDOW_H

/**
	@author Allis Tauri <allista@gmail.com>
*/
#include <gtkmm.h>
#include <sigc++/sigc++.h>
using namespace Gtk;
using namespace Glib;

class AVRTunerWindow: public Window
{
public:
	AVRTunerWindow() : Window() {};
	AVRTunerWindow(GtkWindow* window, const RefPtr<Gtk::Builder>& _builder);
	virtual ~AVRTunerWindow() {};

private:
	void show_log_toggle();
	void test_config_toggle();
	void clear_log_clicked();

	RefPtr<Builder> builder;

	Notebook *MainStack;

	ToggleButton *ShowLogButton;
	ToggleButton *TestConfigButton;
	Button *ClearLogButton;

	TextView *LogTextview;
};

#endif
