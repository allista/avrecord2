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
#icnlude <strstream>
#include <iostream>

#include "avrtunerwindow.h"

AVRTunerWindow::AVRTunerWindow(GtkWindow * window, const Glib::RefPtr<Builder> & _builder)
: Window(window)
{
	builder = _builder;

	//get all nesessary widgets:
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
	ShowLogButton->signal_toggled().connect(sigc::mem_fun(*this, &AVRTunerWindow::show_log_toggle));
	TestConfigButton->signal_toggled().connect(sigc::mem_fun(*this, &AVRTunerWindow::test_config_toggle));
	ClearLogButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::clear_log_clicked));

	UndoButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::undo));
	RedoButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::redo));
	SaveAsButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::save_as));
}

void AVRTunerWindow::Init(string _config_fname)
{
	if(_config_fname.empty()) return;
	config_fname = _config_fname;
	revert();

	return true;
}

void AVRTunerWindow::CaptureThread()
{
	if(recorder)
	{
		delete recorder;
		recorder = NULL;
	}

	recorder = new BaseRecorder<Glib::Mutex>();
	if(!recorder->Init(config.getConfig()))
	{
			::log_message(1, "CaptureThread: Init: unable to initialize recorder");
			delete recorder;
			recorder = NULL;
			return;
	}


}

void AVRTunerWindow::MonitorThread()
{
}

void AVRTunerWindow::log_message(string message)
{ LogTextView->get_buffer()->insert(LogTextView->get_buffer()->end(), message); }


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
	if(TestConfigButton->get_active())
	{
		MainStack->set_current_page(1);
		ShowLogButton->set_active(1);
		ShowLogButton->hide();
		ClearLogButton->hide();
		ConfigToolbar->hide();
		ConfigChooserButton->hide();

		LogTextView->get_buffer()->set_text("Something to test TextView...");
	}
	else
	{
		MainStack->set_current_page(0);
		ShowLogButton->set_active(0);
		ShowLogButton->show();
		ConfigToolbar->show();
		ConfigChooserButton->show();
	}
}

void AVRTunerWindow::clear_log_clicked()
{ LogTextView->get_buffer()->set_text(""); }


void AVRTunerWindow::revert()
{
	if(!config.Load(config_fname))
		show_log_toggle();
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

			break;
		case RESPONSE_CANCEL:
			break;
	}
}
