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

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <libswscale/swscale.h>
}
#include <SDL/SDL.h>

#include <gtkmm.h>
#include <gtksourceviewmm.h>
#include <sigc++/sigc++.h>
using namespace Gtk;
using namespace gtksourceview;

#include <avconfig.h>
#include <recorder.h>

class AVRTunerWindow: public Window
{
public:
	AVRTunerWindow() : Window() {};
	AVRTunerWindow(GtkWindow* window, const Glib::RefPtr<Builder>& _builder);
	virtual ~AVRTunerWindow() { delete ConfigSourceView; };

	///load configuration
	void LoadConfiguration(string _config_fname);

	///function which run in a thread and in which Recorder loop runs
	void CaptureThread();

	///function which run in a thread and in which Monitor loop runs
	void MonitorThread();

	///append a message to the log textview
	void LogMessage(string message);

private:
	///core stuff
	Glib::Mutex mutex;
	AVConfig    config;
	string      config_fname;
	bool        config_parsed;
	BaseRecorder<Glib::Mutex> *recorder;
	uint signal; ///< Recorder's IdleLoop control signal

	///monitor stuff
	SDL_Event      event;   ///< SDL event

	SDL_Rect       screen_rect; ///< screen dimentions
	SDL_Surface   *screen;  ///< output screen
	SDL_Overlay   *overlay; ///< YUV overlay
	AVPicture      overlay_frame; ///< ffmpeg structure representing overlay. Used for pixel format conversions
	SwsContext    *sws;     ///< scaling and converting context
	PixelFormat    in_fmt;  ///< ffmpeg PixelFormat of the input picture

	uint           motion;  ///< number of diff-pixels
	uint           peak;    ///< sound noise level
	unsigned char *buffer;  ///< buffer for captured image
	uint           v_bsize; ///< size of image buffer
	uint           width;   ///< widht of the image
	uint           height;  ///< height of the image
	bool highlight_motion;  ///< if true, get motion buffer from the catcher, otherwise get video buffer

	///signal handlers
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
