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

#include <iostream>
#include <fstream>
using namespace std;

#include "configfile.h"


ConfigFile::ConfigFile(string fname)
{
	if(fname.empty()) return;

	ifstream *file = new ifstream(fname.c_str(), ios::in);
	if(!file->is_open())
	{
		log_message(1, "ConfigFile: Can't open file: %s", fname.c_str());
		return;
	}
	file->seekg(0);

	char opt[256], val[256];
	string line;
	while(readstr(file, line))
	{
		if(sscanf(line.c_str(), "%255[a-zA-Z1234567890_] = %255[a-zA-Z1234567890_\\/.:;%-]", opt, val)==2)
			if(options.find(opt) == options.end())
				options[opt] = val;
	}

	file->close();
	delete file;
}

bool ConfigFile::getOptionB( const char * opt ) const
{
	if(options.empty()) return false;

	opt_map::const_iterator i = options.find(opt);
	if(i != options.end())
		if(i->second == "yes") return true;
		else return false;
	return false;
}

int ConfigFile::getOptionI( const char * opt ) const
{
	if(options.empty()) return 0;

	opt_map::const_iterator i = options.find(opt);
	if(i != options.end())
		return strtol(i->second.c_str(),NULL,0);
	return 0;
}

string ConfigFile::getOptionS( const char * opt ) const
{
	if(options.empty()) return string();

	opt_map::const_iterator i = options.find(opt);
	if(i != options.end())
		return i->second;
	return string();
}


int ConfigFile::readstr(ifstream* file, string &line)
{
	if(!file) return 0;
	char ch; bool started;
	line.clear();
	do
	{
		started = false;
		while(file->get(ch) && (ch != '\n' && ch != '#') && !file->eof())
			if(started || (ch != ' ' && ch != '\t'))
			{
				line += ch;
				if(!started) started = true;
			}
		if(ch == '#')
			while(file->get(ch) && ch != '\n' && !file->eof());
	}
	while(line[0] == '\0' && !file->eof());
	if(file->eof() && line[0] == '\0') return 0;
	else return 1;
}
