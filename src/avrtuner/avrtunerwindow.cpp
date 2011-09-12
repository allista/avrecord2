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

#include <fstream>
#include <stdio.h>

#include <common.h>
#include "avrtunerwindow.h"

AVRTunerWindow::AVRTunerWindow(GtkWindow * window, const Glib::RefPtr<Builder> &builder)
: Window(window),
  monitor_thread(NULL),
  builder(builder)
{
	//get all nesessary widgets:
	//statusbar
	builder->get_widget("FilenameStatusbar", FilenameStatusbar);

	//toolbar
	builder->get_widget("ConfigToolbar", ConfigToolbar);

	//Main Stack
	builder->get_widget("MainStack", MainStack);

	//all the buttons
	builder->get_widget("ShowLogButton", ShowLogButton);
	builder->get_widget("TestConfigButton", TestConfigButton);
	builder->get_widget("ClearLogButton", ClearLogButton);

	builder->get_widget("ConfigChooserButton", ConfigChooserButton);

	builder->get_widget("RevertButton", RevertButton);
	builder->get_widget("SaveButton", SaveButton);
	builder->get_widget("SaveAsButton", SaveAsButton);
	builder->get_widget("UndoButton", UndoButton);
	builder->get_widget("RedoButton", RedoButton);
	builder->get_widget("InitButton", InitButton);

	builder->get_widget("ShowMotionCheckbox", ShowMotionCheckbox);
	ShowMotionCheckbox->set_active(true);

	//text views: log view
	builder->get_widget("LogTextView", LogTextView);

	//and config view, which is the sourceview. It couldn't be loaded by gtkmm::builder, therefore it has to be created...
	ConfigSourceView   = new SourceView();
	ConfigSourceView->set_show_line_numbers();
	ConfigSourceView->set_auto_indent();
	ConfigSourceView->set_highlight_current_line();
	ConfigSourceView->set_show_line_marks();
	ConfigSourceView->set_indent_on_tab();
	ConfigSourceView->set_auto_indent();
	ConfigSourceView->show();

	//source buffer setup
	gtksourceview::init(); ///< needed for SourceLanguageManager to work properly
	Glib::RefPtr<SourceLanguageManager> LanguageManager = SourceLanguageManager::create();
	ConfigSourceBuffer = SourceBuffer::create(LanguageManager->get_language("cpp"));
	ConfigSourceView->set_source_buffer(ConfigSourceBuffer);

	//...and placed manualy into the scrolled window
	ScrolledWindow *ConfigScrolledWindow = NULL;
	builder->get_widget("ConfigScrolledWindow", ConfigScrolledWindow);
	ConfigScrolledWindow->add(*ConfigSourceView);

	//meters
	builder->get_widget("MotionProgressBar", MotionProgressBar);
	builder->get_widget("NoiseProgressBar", NoiseProgressBar);
	builder->get_widget("MotionValueLabel", MotionValueLabel);
	builder->get_widget("NoiseValueLabel", NoiseValueLabel);

	//connect all signals
	ConfigChooserButton->signal_file_set().connect(sigc::mem_fun(*this, &AVRTunerWindow::open));
	ConfigSourceBuffer->signal_changed().connect(sigc::mem_fun(*this, &AVRTunerWindow::config_changed));
	ConfigSourceBuffer->signal_modified_changed().connect(sigc::mem_fun(*this, &AVRTunerWindow::modified_changed));

	ShowLogButton->signal_toggled().connect(sigc::mem_fun(*this, &AVRTunerWindow::show_log_toggle));
	TestConfigButton->signal_toggled().connect(sigc::mem_fun(*this, &AVRTunerWindow::test_config_toggle));
	ClearLogButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::clear_log_clicked));

	RevertButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::revert));
	UndoButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::undo));
	RedoButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::redo));
	SaveButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::save));
	SaveAsButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::save_as));
	InitButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::init));

	//connect dispatcher
	signal_update_meters.connect(sigc::mem_fun(*this, &AVRTunerWindow::update_meters));
}

AVRTunerWindow::~ AVRTunerWindow()
{
	stop_monitor();
	delete ConfigSourceView;
}

void AVRTunerWindow::LoadConfiguration(string _config_fname)
{
	if(_config_fname.empty()) return;
	config_fname = _config_fname;
	revert();
}

void AVRTunerWindow::LogMessage(string message)
{ LogTextView->get_buffer()->insert(LogTextView->get_buffer()->end(), message); }


bool AVRTunerWindow::on_delete_event(GdkEventAny *event)
{
	if(ConfigSourceBuffer->get_modified())
	{
		MessageDialog dialog(*this,
							 "Configuration has not been saved. Are you sure you whant to quit?", false,
							 Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
		dialog.set_secondary_text("All modifications will be lost.");

		if(RESPONSE_OK != dialog.run()) return true;
	}
	return false;
}


void AVRTunerWindow::stop_monitor()
{
	if(!monitor_thread) return;

	monitor.stop();
	if(monitor_thread->joinable())
		monitor_thread->join();

	monitor_thread = NULL;
}

void AVRTunerWindow::update_meters()
{
	if(!monitor_thread) return;

	Glib::Mutex::Lock lock(mutex);
	uint motion = monitor.getMotion();
	double motion_percent = (double)motion/monitor.getMotionMax();
	uint peak   = monitor.getPeak();
	double peak_percent   = (double)peak/monitor.getPeakMax();
	lock.release();

	motion_percent = (motion_percent < 0)? 0 : motion_percent;
	motion_percent = (motion_percent > 1)? 1 : motion_percent;
	peak_percent   = (peak_percent   < 0)? 0 : peak_percent;
	peak_percent   = (peak_percent   > 1)? 1 : peak_percent;

	char motion_string[10] = {0};
	char peak_string[10] = {0};
	sprintf(motion_string, "%d", motion);
	sprintf(peak_string, "%d", peak);

	MotionProgressBar->set_fraction(motion_percent);
	MotionValueLabel->set_text(motion_string);
	NoiseProgressBar->set_fraction(peak_percent);
	NoiseValueLabel->set_text(peak_string);
}

void AVRTunerWindow::config_changed()
{ ConfigSourceBuffer->set_modified(true); }

void AVRTunerWindow::modified_changed()
{
	if(ConfigSourceBuffer->get_modified())
	{
		FilenameStatusbar->pop();
		FilenameStatusbar->push(config_fname+" [modified]");
	}
	else
	{
		FilenameStatusbar->pop();
		if(config_parsed) FilenameStatusbar->push(config_fname);
		else FilenameStatusbar->push(config_fname+" [parse errors]");
	}
}

void AVRTunerWindow::show_log()
{
	ShowLogButton->set_active(true);
	show_log_toggle();
}

void AVRTunerWindow::show_log_toggle()
{
	if(ShowLogButton->get_active())
	{
		MainStack->set_current_page(1);
		ClearLogButton->show();

		ConfigToolbar->hide();
		ConfigChooserButton->hide();
	}
	else
	{
		MainStack->set_current_page(0);
		ClearLogButton->hide();

		ConfigToolbar->show();
		ConfigChooserButton->show();
	}
}

void AVRTunerWindow::test_config_toggle()
{
	if(config_fname.empty())
	{
		if(ConfigSourceBuffer->get_modified())
			log_message(1, "Configuration has to be, saved in a file first.");
		else log_message(1, "No configuration loaded.");
		show_log();
		return;
	}

	if(TestConfigButton->get_active())
	{
		MainStack->set_current_page(1);
		ShowLogButton->set_active(1);
		ShowLogButton->hide();
		ClearLogButton->hide();
		ConfigToolbar->hide();
		ConfigChooserButton->hide();

		if(ConfigSourceBuffer->get_modified())
			save();

		if(!config_parsed)
		{
			TestConfigButton->set_active(false);
			test_config_toggle();
			show_log();
			return;
		}

		clear_log_clicked();

		if(!monitor.Init(config.getConfig(), &signal_update_meters, ShowMotionCheckbox->get_active()))
		{
			log_message(1, "Monitor was not initialized properly");
			TestConfigButton->set_active(false);
			test_config_toggle();
			show_log();
			return;
		}
		monitor_thread = Glib::Thread::create(sigc::mem_fun(monitor, &VideoMonitor::run), true);
	}
	else
	{
		stop_monitor();
		MainStack->set_current_page(0);
		ShowLogButton->set_active(0);
		ShowLogButton->show();
		ConfigToolbar->show();
		ConfigChooserButton->show();
		MotionProgressBar->set_fraction(0);
		MotionValueLabel->set_text("");
		NoiseProgressBar->set_fraction(0);
		NoiseValueLabel->set_text("");
	}
}

void AVRTunerWindow::clear_log_clicked()
{ LogTextView->get_buffer()->set_text(""); }


void AVRTunerWindow::open()
{
	config_fname = ConfigChooserButton->get_filename();
	if(!config_fname.empty())
	{
		ConfigSourceBuffer->set_modified(true);
		revert();
	}
}

void AVRTunerWindow::revert()
{
	if(config_fname.empty()) return;
	clear_log_clicked();

	ifstream cfg;
	cfg.open(config_fname.c_str(), ios_base::in);
	if(!cfg.is_open())
	{
		config_fname.clear();
		log_message(1, "Unable to open %s", config_fname);
		show_log();
		return;
	}
	cfg.seekg(0);

	string config_text;
	char   line[1024];
	while(cfg)
	{
		cfg.getline((char*)line, 1024);
		config_text += line;
		if(cfg) config_text += '\n';
	}
	cfg.close();

	ConfigSourceBuffer->begin_not_undoable_action();
	ConfigSourceBuffer->set_text(config_text);
	ConfigSourceBuffer->end_not_undoable_action();

	if(!config.Load(config_fname))
	{
		config_parsed = false;
		show_log();
	}
	else config_parsed = true;

	ConfigSourceBuffer->set_modified(false);
	ConfigChooserButton->set_filename(config_fname);
}

void AVRTunerWindow::undo()
{
	if(ConfigSourceBuffer->can_undo())
		ConfigSourceBuffer->undo();
}

void AVRTunerWindow::redo()
{
	if(ConfigSourceBuffer->can_redo())
		ConfigSourceBuffer->redo();
}

void AVRTunerWindow::save()
{
	if(config_fname.empty())
	{ save_as(); return; };
	clear_log_clicked();

	ofstream cfg;
	cfg.open(config_fname.c_str(), ios_base::out);
	if(!cfg.is_open())
	{
		log_message(1, "Unable to open %s for writing", config_fname);
		show_log();
		return;
	}

	string config_text = ConfigSourceBuffer->get_text();
	cfg.write(config_text.c_str(), config_text.length());
	cfg.close();

	if(!config.Load(config_fname))
	{
		config_parsed = false;
		show_log();
	}
	else config_parsed = true;

	ConfigSourceBuffer->set_modified(false);
	ConfigChooserButton->set_filename(config_fname);
}

void AVRTunerWindow::save_as()
{
	FileChooserDialog file_chooser("Save as...", FILE_CHOOSER_ACTION_SAVE);
	file_chooser.add_button(Stock::CANCEL, RESPONSE_CANCEL);
	file_chooser.add_button(Stock::SAVE, RESPONSE_OK);
	file_chooser.set_transient_for(*this);

	int result = file_chooser.run();
	switch(result)
	{
		case RESPONSE_OK:
			config_fname = file_chooser.get_filename();
			ConfigSourceBuffer->set_modified(true);
			save();
			break;

		case RESPONSE_CANCEL:
			break;
	}
}

void AVRTunerWindow::init()
{
	if(!config_parsed) return;

	MessageDialog dialog(*this,
						  "Are you sure you whant to initialize configuration by quering video device?", false,
						  Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
	dialog.set_secondary_text("Current video configuration will be lost.");

	if(RESPONSE_OK != dialog.run()) return;

	if(!config.Init() || !config.Save())
	{
		show_log();
		return;
	}
	else revert();
}
