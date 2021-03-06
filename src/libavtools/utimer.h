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
/** @author Allis*/

#ifndef UTIMER_H
#define UTIMER_H

#include <sys/time.h>
#include <time.h>


///implemets a standart microsecond timer
class UTimer
{
public:
	UTimer() { reset(); };
	~UTimer() {};

	///sets start point to current time and stop time to 0
	void start() { gettimeofday(&_start, NULL); _stop.tv_sec = 0; };

	///sets stop point to current time
	void pause() { if(_stop.tv_sec) return; stop(); _elapsed = elapsed(); _start = _stop; };

	///sets stop point to current time
	void stop()  { if(_stop.tv_sec) return; gettimeofday(&_stop, NULL); };

	///sets start time to the current time and stop and elapsed time to 0
	void reset() { gettimeofday(&_start, NULL); _stop.tv_sec = 0; _elapsed = 0; };

	///returns differens in microseconds between start and stop or current time, if stop isn't set yet
	uint elapsed() const
	{
		timeval cur;
		if(!_stop.tv_sec)
			gettimeofday(&cur, NULL);
		else cur = _stop;

		cur.tv_sec  = cur.tv_sec  - _start.tv_sec;
		cur.tv_usec = cur.tv_usec - _start.tv_usec;

		return _elapsed + cur.tv_sec*1000000 + cur.tv_usec;
	};

private:
	uint    _elapsed; ///< elapsed time in microseconds from the start to the last pause
	timeval _start;   ///< start time
	timeval _stop;    ///< stop time
};

#endif
