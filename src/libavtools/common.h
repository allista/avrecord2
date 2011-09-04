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

#ifndef AVRECORD_COMMON_H
#define AVRECORD_COMMON_H

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
//typedef unsigned char uint8_t;

#define CLEAR(x) memset (&(x), 0, sizeof (x))

///logs given message to our logfile
void log_message(int level, const char *fmt, ...);

///logs given message as error to our logfile, appendig errno and it's description
void log_errno(const char *message = NULL)
{ log_message(1, "%s errno: %d, %s.", message, errno, strerror(errno)); }

struct image_buffer    ///< image buffer structure
{
	void* start;       ///< the beginning of a buffer
	size_t length;     ///< buffer length
	timeval timestamp; ///< image timestamp as returnd by VIDIOC_DQBUF in v4l2_buffer structure
};

#endif
