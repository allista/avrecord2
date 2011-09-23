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
#include <gtksourceviewmm.h>
#include <sigc++/sigc++.h>
using namespace Gtk;
using namespace gtksourceview;

#include <avconfig.h>
#include <videomonitor.h>

class AVRTunerWindow: public Window
{
public:
	AVRTunerWindow(GtkWindow* window, const Glib::RefPtr<Builder> &builder);
	virtual ~AVRTunerWindow();

	///load configuration
	void LoadConfiguration(string _config_fname);

	///append a message to the log textview
	void LogMessage(string message);

protected:
	virtual bool on_delete_event(GdkEventAny *event);

private:
	///video monitor
	VideoMonitor monitor;
	Glib::Dispatcher signal_update_meters;
	Glib::Thread *monitor_thread;

	void stop_monitor();

	///configuration
	AVConfig    config;
	string      config_fname;
	string      work_dir;
	bool        config_parsed;

	///signal handlers
	void update_meters();

	void config_changed();
	void modified_changed();

	void show_log();
	void show_log_toggle();
	void test_config_toggle();
	void clear_log_clicked();

	void open();
	void revert();
	void save();
	void save_as();
	void undo();
	void redo();
	void init();

	///all the gtkmm stuff
	///builder
	Glib::RefPtr<Builder> builder;

	///toolbar
	Toolbar *ConfigToolbar;

	///statusbar
	Statusbar *FilenameStatusbar;

	///area stack
	Notebook *MainStack;

	///buttons
	ToggleButton *ShowLogButton;
	ToggleButton *TestConfigButton;
	Button *ClearLogButton;

	FileChooserButton* ConfigChooserButton;

	ToolButton *RevertButton;
	ToolButton *SaveButton;
	ToolButton *SaveAsButton;
	ToolButton *UndoButton;
	ToolButton *RedoButton;
	ToolButton *InitButton;

	CheckButton *ShowMotionCheckbox;

	///text areas
	TextView *LogTextView;
	SourceView *ConfigSourceView;
	Glib::RefPtr<SourceBuffer> ConfigSourceBuffer;

	///meters
	ProgressBar *MotionProgressBar;
	ProgressBar *NoiseProgressBar;
	Label *MotionValueLabel;
	Label *NoiseValueLabel;
};

#endif
