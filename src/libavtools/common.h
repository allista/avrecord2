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
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
//typedef unsigned char uint8_t;

#include <initializer_list>
#include <algorithm>
#include <map>

using namespace std;

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define LEN(x) ((int)sizeof(x)/(int)sizeof((x)[0]))

///logs given message to the logfile
void log_message(int level, const char *fmt, ...);

///logs given message as error to our logfile, appendig errno and it's description
static void log_errno(const char *message = NULL)
{ log_message(1, "%s errno: %d, %s.", message, errno, strerror(errno)); }

///supported pixel formats
///note, that all these formats are planar, we don't use packed ones because of
///the text rendering function: for all the planar formats one single function
///is enough, while each packed format requires it's own function.
static const uint v4l2_pixel_formats[] =
{
    //1/2 chroma
    V4L2_PIX_FMT_YUV420, //this is the default
    V4L2_PIX_FMT_YUV422P,
    V4L2_PIX_FMT_NV12,
    V4L2_PIX_FMT_NV21,
    //1/4 chroma
	V4L2_PIX_FMT_YUV411P,
	V4L2_PIX_FMT_YUV410,
	//greyscale
	V4L2_PIX_FMT_GREY //8bpp
};

static const map<uint, PixelFormat> av_pixel_formats =
{
    {V4L2_PIX_FMT_YUV420,  PIX_FMT_YUV420P},
    {V4L2_PIX_FMT_YUV422P, PIX_FMT_YUV422P},
    {V4L2_PIX_FMT_NV12,    PIX_FMT_NV12},
    {V4L2_PIX_FMT_NV21,    PIX_FMT_NV21},
    {V4L2_PIX_FMT_YUV411P, PIX_FMT_YUV411P},
    {V4L2_PIX_FMT_YUV410,  PIX_FMT_YUV410P},
    {V4L2_PIX_FMT_GREY,    PIX_FMT_GRAY8}
};

static int supported_format_index(const uint format)
{
    const uint *end = v4l2_pixel_formats+LEN(v4l2_pixel_formats);
    const uint *it = std::find(v4l2_pixel_formats, end, format);
    if(it != end) return it-v4l2_pixel_formats; //index of the format
    return -1;
}

#endif
