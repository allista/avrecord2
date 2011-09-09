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

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

///logs given message to our logfile
void log_message(int level, const char *fmt, ...);

///logs given message as error to our logfile, appendig errno and it's description
static void log_errno(const char *message = NULL)
{ log_message(1, "%s errno: %d, %s.", message, errno, strerror(errno)); }

///supported pixel formats
///note, that all these formats are planar, I don't use packed ones because of the text rendering function: for all the planar formats one single function is enough, while each packed format requires it's own function.
static const uint v4l2_pixel_formats[] =
{
	V4L2_PIX_FMT_YUV420,
	V4L2_PIX_FMT_YUV411P,
	V4L2_PIX_FMT_YUV410,
	V4L2_PIX_FMT_GREY
};

///names of the supported formats
static const char *v4l2_pixel_format_names[] =
{
	"V4L2_PIX_FMT_YUV420",
	"V4L2_PIX_FMT_YUV411P"
	"V4L2_PIX_FMT_YUV410",
	"V4L2_PIX_FMT_GREY"
};

static const PixelFormat av_pixel_formats[] =
{
	PIX_FMT_YUV420P,
	PIX_FMT_YUV411P,
	PIX_FMT_YUV410P,
	PIX_FMT_GRAY8
};

#endif
