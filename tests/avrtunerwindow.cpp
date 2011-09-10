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
#include "avrtunerwindow.h"

AVRTunerWindow::AVRTunerWindow(GtkWindow * window, const RefPtr< Gtk::Builder > & _builder)
: Window(window)
{
	builder = _builder;
	builder->get_widget("MainStack", MainStack);

	builder->get_widget("ShowLogButton", ShowLogButton);
	builder->get_widget("TestConfigButton", TestConfigButton);
	builder->get_widget("ClearLogButton", ClearLogButton);

	builder->get_widget("LogTextview", LogTextview);

	ShowLogButton->signal_toggled().connect(sigc::mem_fun(*this, &AVRTunerWindow::show_log_toggle));
	TestConfigButton->signal_toggled().connect(sigc::mem_fun(*this, &AVRTunerWindow::test_config_toggle));
	ClearLogButton->signal_clicked().connect(sigc::mem_fun(*this, &AVRTunerWindow::clear_log_clicked));
}

void AVRTunerWindow::show_log_toggle()
{
	if(ShowLogButton->get_active())
	{
		MainStack->set_current_page(1);
		ClearLogButton->show();
	}
	else
	{
		MainStack->set_current_page(0);
		ClearLogButton->hide();
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

		LogTextview->get_buffer()->set_text("Something to test TextView...");
	}
	else
	{
		MainStack->set_current_page(0);
		ShowLogButton->set_active(0);
		ShowLogButton->show();
	}
}

void AVRTunerWindow::clear_log_clicked()
{ LogTextview->get_buffer()->set_text(""); }

