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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

extern "C"
{
#  define INT64_C(c)	c ## LL
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>

#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libconfig.h++>
using namespace libconfig;

#include <map>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

#include <utimer.h>
#include <common.h>
#include <avconfig.h>
#include <img_tools.h>
#include <vidstream.h>
#include <sndstream.h>
#include <recorder.h>


int main(int argc, char *argv[])
{
	string cfg_file;
	string new_cfg;
	bool init = false;

	if(argc > 1)
	{
		int c;
		while((c = getopt(argc, argv, "c:i"))!=EOF)
		switch(c)
		{
			case 'c':
				cfg_file = string(optarg);
				break;
			case 'i':
				new_cfg = "new_";
				init = true;
				break;
			default: break;
		}
	}
	if(cfg_file.empty())
	{
		log_message(1, "No -c <cfg_filename> option was supplied.");
		exit(1);
	}
	if(init) new_cfg += cfg_file;

	AVConfig cfg;
	cfg.Load(cfg_file);

	//load mask test//
	/* must be called before using avcodec lib */
	avcodec_init();

	/* register all the codecs */
	avcodec_register_all();

	fstream mask_file;
	char *mask = NULL;
	uint mask_len;
	char *buffer = NULL;
	uint buf_length;

	AVCodec        *bmp_codec = NULL;
	AVCodecContext *bmp_context = NULL;
	AVFrame        *mask_in = NULL; ///< ffmpeg structure representing a picture. Used for pixel format conversions
	AVFrame        *mask_out = NULL;
	AVPacket       av_packet;
	SwsContext    *sws = NULL;   ///< scaling and converting context
	PixelFormat    in_fmt; ///< ffmpeg PixelFormat of the input picture
	PixelFormat    out_fmt = PIX_FMT_GRAY8; ///< ffmpeg PixelFormat of the output picture

	uint iwidth;
	uint iheight;
	uint owidth  = cfg.lookup("video.width");
	uint oheight = cfg.lookup("video.height");
	mask_len = avpicture_get_size(out_fmt, owidth, oheight);
	mask = (char*)av_mallocz(mask_len);

	av_init_packet(&av_packet);

	/* find the bmp encoder */
	bmp_codec = avcodec_find_decoder_by_name("bmp");
	if(!bmp_codec)
	{
		log_message(1, "BMP codec not found");
		exit(1);
	}

	bmp_context = avcodec_alloc_context();
	mask_in  = avcodec_alloc_frame();
	mask_out  = avcodec_alloc_frame();
	/* open it */
	if(avcodec_open(bmp_context, bmp_codec) < 0)
	{
		log_message(1, "Could not open BMP codec");
		exit(1);
	}

	string mask_filename = "tests/test-mask.bmp";
	mask_file.open(mask_filename.c_str(), ios::in|ios::binary);
	if(!mask_file.is_open())
	{
		log_message(1, "Could not open mask file: %s", mask_filename);
		exit(1);
	}
	//get length of file:
	mask_file.seekg(0, ios::end);
	buf_length = mask_file.tellg();
	log_message(0, "File size is: %d", buf_length);
	mask_file.seekg(0, ios::beg);

	buffer = (char*)av_mallocz(buf_length+FF_INPUT_BUFFER_PADDING_SIZE);

	if(!mask_file.read(buffer, buf_length))
	{
		log_message(1, "Read error");
		exit(1);
	}
	mask_file.close();

	av_packet.size = buf_length;
	av_packet.data = (uint8_t*)buffer;
	int got_picture, len;
	while(av_packet.size > 0)
	{
		//we should use avcodec_decode_video2 instead for the first version is documented as deprecated. Yet, in Ubuntu repos (and even in Medibuntu) libavcodec still lacks declaration of the second version
		len = avcodec_decode_video(bmp_context, mask_in, &got_picture, av_packet.data, av_packet.size);
		if(len < 0)
		{
			log_message(1, "Error while decoding file");
			exit(1);
		}
		if(got_picture)
		{
			log_message(0, "Got it!");
			iwidth = bmp_context->width;
			iheight = bmp_context->height;
			in_fmt = bmp_context->pix_fmt;
			log_message(0, "Mask properties:\n width: %d\n height: %d\n pix_fmt: %d", iwidth, iheight, in_fmt);
			break;
		}

		/* the picture is allocated by the decoder. no need to
		free it */
		av_packet.size -= len;
		av_packet.data += len;
	}

	sws = sws_getContext(iwidth, iheight, in_fmt,
						 owidth, oheight, out_fmt,
					     SWS_LANCZOS, NULL, NULL, NULL);
	if(!sws)
	{
		log_message(1, "MonitorThread: couldn't get SwsContext for pixel format conversion");
		//return false;
	}

	avpicture_fill((AVPicture*)mask_out, (uint8_t*)mask, out_fmt, owidth, oheight);

	if(0 > sws_scale(sws, mask_in->data, mask_in->linesize, 0,
			  iheight, mask_out->data, mask_out->linesize))
	{
		log_message(1,"Unable to convert mask image");
		exit(1);
	}

	av_free(mask_in);
	av_free(mask_out);
	av_free(buffer);

	//do something with mask, then

	av_free(mask);
	//end of load mask test//

	exit(0);
}

void log_message(int level, const char *fmt, ...)
{
	int     n;
	char    buf[1024];
	va_list ap;

	//Next add timstamp and level prefix
	time_t now = time(0);
	n  = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S ", localtime(&now));
	n += snprintf(buf + n, sizeof(buf), "[%s] ", level? "ERROR" : "INFO");

	//Next add the user's message
	va_start(ap, fmt);
	n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

	//newline for printing to the file
	strcat(buf, "\n");

	//output...
	if(level)	cerr << buf << flush; //log to stderr
	else cout << buf << flush; //log to stdout

	//Clean up the argument list routine
	va_end(ap);
}
