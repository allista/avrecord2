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
/**@author Allis*/

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <fstream>
#include <string>
#include <map>
using namespace std;

#include "common.h"

///provides loading of simple config file.
///Format of the file is 'option = value'.
class ConfigFile
{
public:
	ConfigFile(string fname = string(""));
	~ConfigFile() {};

	///sets an option
	void   setOption(const char* opt, string val)
	{ options[opt] = val; };

	///returns value of the option 'opt' as bool
	///(if there's no such option false is returned)
	bool   getOptionB(const char* opt) const;

	///returns value of the option 'opt' as integer
	///(if there's no such option 0 is returned)
	int    getOptionI(const char* opt) const;

	///returns value of the option 'opt' as string
	///(if there's no such option string("") is returned)
	string getOptionS(const char* opt) const;

	operator bool() const { return !options.empty(); };

private:
	typedef map<string,string> opt_map;
	opt_map options; ///< map of loaded options with thier values

	///reads a single line from ifstream.
	///this function takes care about empty lines, comments and whitespaces
	int readstr(ifstream* file, string &line);
};

#endif
