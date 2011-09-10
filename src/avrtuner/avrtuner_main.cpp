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


#include <iostream>
using namespace std;

#include <qapplication.h>
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kmainwindow.h>

#include <common.h>

#include "tunerform.h"

static const char description[] =
    I18N_NOOP("A KDE KPart Application");

static const char version[] = "0.1";

static KCmdLineOptions options[] =
{
    { "+[URL]", I18N_NOOP( "Config file to open" ), 0 },
    KCmdLineLastOption
};


///Main window widget
TunerForm *mainWin = 0;

int main(int argc, char **argv)
{
    KAboutData about("AVRTuner", I18N_NOOP("AVRTuner"), version, description,
                     KAboutData::License_GPL, "(C) 2007 Allis Tauri", 0, 0, "allista@gmail.com");
    about.addAuthor( "Allis Tauri", 0, "allista@gmail.com" );
    KCmdLineArgs::init(argc, argv, &about);
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;


    if (app.isRestored())
    {
        RESTORE(TunerForm);
    }
    else
    {
        // no session.. just start up normally
        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

        mainWin = new TunerForm();
	if(args->count())
		mainWin->fileOpen(args->arg(0));
        app.setMainWidget( mainWin );
        mainWin->show();

        args->clear();
    }

    // mainWin has WDestructiveClose flag by default, so it will delete itself.
    return app.exec();
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
	QString str(buf);
	if(level) cerr << buf << flush; //log to stderr
	else cout << buf << flush; //log to stdout
	mainWin->log_message(str); //to log window

	//Clean up the argument list routine
	va_end(ap);
}
