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

#include <errno.h>
#include <iostream>
using namespace std;

#include "avifile.h"

AVIFile::AVIFile()
{
	_opened = INIT_NONE;

    ///pointer to the video Settings object (libconfig++)
	video_settings_ptr = NULL;
	///pointer to the audio Settings object (libconfig++)
	audio_settings_ptr = NULL;

	//file streams and codecs
	o_file  = NULL;
	vstream = NULL;
	astream = NULL;
	vcodec  = NULL;
	acodec  = NULL;

	//video Output
	v_bsize = 0;
	vbuffer = NULL;

	//audio Output
	a_bsize = 0;
	abuffer = NULL;

	vbr     = 0;
}


bool AVIFile::Init(Setting *_audio_settings_ptr, Setting *_video_settings_ptr_ptr)
{
	if(!_audio_settings_ptr || !_video_settings_ptr_ptr) return false;
	audio_settings_ptr = _audio_settings_ptr;
	video_settings_ptr = _video_settings_ptr_ptr;

	/* allocation the output media context */
	o_file = (AVFormatContext*)av_mallocz(sizeof(AVFormatContext));
	if(!o_file)
	{
		log_message(1, "AVIFile: Memory error while allocating output media context");
		cleanup();
		return false;
	}

	//guess the avi output format
	o_file->oformat = guess_format("avi", NULL, NULL);
	if(!o_file->oformat)
	{
		cleanup();
		return false;
	}

	return true;
}


bool AVIFile::setVParams(uint numerator, uint denomenator, uint pix_fmt)
{
	if(opened() || !o_file) return false;
	Setting &video_settings = *video_settings_ptr;

	/* Setup video codec id */
	try
	{
		o_file->oformat->video_codec = get_codec_id(
				(const char*)video_settings["codec"],
				CODEC_TYPE_VIDEO);
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "No video <codec> setting was found. Using default msmpeg4");
		o_file->oformat->video_codec = get_codec_id("msmpeg4", CODEC_TYPE_VIDEO);
	}

	/* Create a new video stream and initialize the codecs */
	if(o_file->oformat->video_codec != CODEC_ID_NONE)
	{
		vstream = av_new_stream(o_file, 0);
		if(!vstream)
		{
			log_message(1, "AVIFile: av_new_stream - could not alloc stream");
			cleanup();
			return false;
		}
	}
	else
	{
		/* We did not get a proper video codec. */
		log_message(1, "AVIFile: Failed to obtain a proper video codec");
		cleanup();
		return false;
	}

	//setup video codec
	vcodec             = vstream->codec;
	vcodec->codec_id   = o_file->oformat->video_codec;
	vcodec->codec_type = CODEC_TYPE_VIDEO;

	/* Set default parameters */
	try
	{
		vcodec->bit_rate       = video_settings["bitrate"];
		vcodec->width          = video_settings["width"];
		vcodec->height         = video_settings["height"];

		if(video_settings.exists("var_bitrate"))
		{
			vbr = video_settings["var_bitrate"];
			vcodec->flags |= CODEC_FLAG_QSCALE;
			vcodec->bit_rate_tolerance = vcodec->bit_rate/20;
			vcodec->qmin = (vbr-2 < 2)? 2 : vbr-2;
			vcodec->qmax = (vbr+2 > 31)? 31 : vbr+2;
		}
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "Video <bitrate>, <width> or <height> settings not found");
		cleanup();
		return false;
	}
	vcodec->time_base.num  = numerator;
	vcodec->time_base.den  = denomenator;
	/* set intra frame distance in frames depending on codec */
	vcodec->gop_size       = 12;
	/* Set the picture format */
	vcodec->pix_fmt = av_pixel_formats[pix_fmt];

	/* Set codec specific parameters. */
	/* some formats want stream headers to be separate */
	if(!strcmp(o_file->oformat->name, "mp4") ||
		!strcmp(o_file->oformat->name, "mov") ||
		!strcmp(o_file->oformat->name, "3gp"))
		vcodec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	/* Now that all the parameters are set, we can open the video
	codec and allocate the necessary encode buffers */
	AVCodec *codec = avcodec_find_encoder(vcodec->codec_id);
	if(!codec)
	{
		log_message(1, "AVIFile: Codec not found");
		cleanup();
		return false;
	}

	/* open the codec */
	if(avcodec_open(vcodec, codec) < 0)
	{
		log_message(1, "AVIFile: avcodec_open - could not open codec");
		cleanup();
		return false;
	}
	else _opened |= INIT_VCODEC;

	/// O_o ???
	if(!(o_file->oformat->flags & AVFMT_RAWPICTURE))
	{
		/* allocate output buffer */
		/* XXX: API change will be done */
		v_bsize = 200000; //taked from ffmpeg.c from "motion" program
		vbuffer = (uint8_t*)malloc(v_bsize);
		if(!vbuffer)
		{
			log_message(1, "AVIFile: can't allocate video output buffer");
			cleanup();
			return false;
		}
	}

	return true;
}


bool AVIFile::setAParams()
{
	if(opened() || !o_file) return false;
	Setting &audio_settings = *audio_settings_ptr;

	/* Setup audio codec id */
	o_file->oformat->audio_codec = get_codec_id(codec_name, CODEC_TYPE_AUDIO);
	try
	{
		o_file->oformat->audio_codec = get_codec_id(
				(const char*)audio_settings["codec"],
				 CODEC_TYPE_AUDIO);
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "No audio <codec> setting was found. Using default mp2");
		o_file->oformat->audio_codec = get_codec_id("mp2", CODEC_TYPE_AUDIO);
	}

	/* Create a new audio stream and initialize the codecs */
	if(o_file->oformat->audio_codec != CODEC_ID_NONE)
	{
		astream = av_new_stream(o_file, 0);
		if(!astream)
		{
			log_message(1, "AVIFile: av_new_stream - could not alloc stream");
			cleanup();
			return false;
		}
	}
	else
	{
		/* We did not get a proper audio codec. */
		log_message(1, "AVIFile: Failed to obtain a proper audio codec");
		cleanup();
		return false;
	}

	//setup audio codec
	acodec             = astream->codec;
	acodec->codec_id   = o_file->oformat->audio_codec;
	acodec->codec_type = CODEC_TYPE_AUDIO;

	/* Set default parameters */
	try
	{
		acodec->bit_rate    = audio_settings["bitrate"];
		acodec->sample_rate = audio_settings["sample_rate"];
		acodec->channels    = audio_settings["channels"];
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "Audio <bitrate>, <sample_rate> or <channels> setting(s) not found");
		cleanup();
		return false;
	}
	acodec->frame_size    = 1; //it's initial value. it will be changed during codec initialization
	acodec->strict_std_compliance = 0;

	/* Now that all the parameters are set, we can open the video
	codec and allocate the necessary encode buffers */
	AVCodec *codec = avcodec_find_encoder(acodec->codec_id);
	if (!codec)
	{
		log_message(1, "AVIFile: Codec not found");
		cleanup();
		return false;
	}

	/* open the codec */
	if(avcodec_open(acodec, codec) < 0)
	{
		log_message(1, "AVIFile: avcodec_open - could not open codec");
		cleanup();
		return false;
	}
	else _opened |= INIT_ACODEC;

	/* allocate output buffer and fifo */
	if(acodec->frame_size > 1)
	{
		a_fsize = acodec->frame_size * 2 * acodec->channels;
		afifo   = Fifo<uint8_t>(2 * MAX_AUDIO_PACKET_SIZE);
	}

	a_bsize = 4 * MAX_AUDIO_PACKET_SIZE; //taked from "ffmpeg" program (ffmpeg library)
	abuffer = (uint8_t*)malloc(a_bsize);
	if(!abuffer)
	{
		log_message(1, "AVIFile: can't allocate video output buffer");
		cleanup();
		return false;
	}

	//all is fine
	return true;
}


bool AVIFile::Open(string filename)
{
	if(!filename.size())    return false;
	if(opened() || !o_file) return false;
	if(!(vcodec || acodec)) return false;

	//setup file streams//
	o_file->nb_streams = 0;
	if(vstream)
	{
		o_file->streams[o_file->nb_streams] = vstream;
		o_file->nb_streams += 1;
		vstream->index = 0;
	}
	if(astream)
	{
		o_file->streams[o_file->nb_streams] = astream;
		o_file->nb_streams += 1;
		astream->index = (vstream?1:0);
	}

	//store filename
	snprintf(o_file->filename, sizeof(o_file->filename), "%s", filename.c_str());

	/* set the output parameters (must be done even if no parameters). */
	if(av_set_parameters(o_file, NULL) < 0)
	{
		log_message(1, "AVIFile: ffmpeg av_set_parameters error: Invalid output format parameters");
		cleanup();
		return false;
	}

#ifdef DEBUG_VERSION
	/* Dump the format settings.  This shows how the various streams relate to each other */
	dump_format(o_file, 0, filename.c_str(), 1);
#endif

	if(url_fopen(&o_file->pb, filename.c_str(), URL_WRONLY) < 0)
	{
		/* path did not exist? */
		if(errno == ENOENT)
		{
			log_message(1, "AVIFile: cannot create output file -- directory not found");
			cleanup();
			return false;
		}
		/* Permission denied */
		else if (errno ==  EACCES)
		{
			log_message(1, "AVIFile: url_fopen - error opening file %s Check access rights to target directory", filename.c_str());
			cleanup();
			return false;
		}
		else
		{
			log_message(1, "AVIFile: Error opening file %s", filename.c_str());
			cleanup();
			return false;
		}
	}
	else _opened |= INIT_FOPEN;

	/* write the stream header, if any */
	if(av_write_header(o_file) < 0)
	{
		log_message(1, "AVIFile: cannot write stream header.");
		cleanup();
		return false;
	}

	//if all is done...//
	_opened |= INIT_FULL;
	return true;
}

double AVIFile::getVpts( ) const
{
	if(!vstream) return 0;
	return (double)vstream->pts.val * vstream->time_base.num / vstream->time_base.den;
}

double AVIFile::getApts( ) const
{
	if(!astream) return 0;
	return (double)astream->pts.val * astream->time_base.num / astream->time_base.den;
}

bool AVIFile::writeVFrame(image_buffer &buffer)
{
	if(!opened()) return false;

	/* allocate the encoded raw picture */
	AVFrame *picture = avcodec_alloc_frame();
	if(!picture)
	{
		log_message(1, "AVIFile: avcodec_alloc_frame - could not allocate frame");
		return false;
	}

	//fill in the picture
	avpicture_fill((AVPicture *)picture, buffer.start, vcodec->pix_fmt, vcodec->width, vcodec->height);

	/* set variable bitrate if requested */
	if(vbr) picture->quality = vbr;

	//encode and write a video frame using the av_write_frame API.
	int out_size, ret;
	AVPacket pkt;
	av_init_packet(&pkt); /* init static structure */
	pkt.stream_index = vstream->index;
	if(o_file->oformat->flags & AVFMT_RAWPICTURE)
	{
		/* raw video case. The API will change slightly in the near future for that */
		pkt.flags |= PKT_FLAG_KEY;
		pkt.data   = (uint8_t *)picture;
		pkt.size   = sizeof(AVPicture);
		ret = av_interleaved_write_frame(o_file, &pkt);
	}
	else
	{
		/* encode the image */
		out_size = avcodec_encode_video(vcodec, vbuffer, v_bsize, picture);

		/* zero size means the image was buffered */
		if(out_size != 0)
		{
			/* write the compressed frame to the media file */
			/* XXX: in case of B frames, the pts is not yet valid */
			if(vcodec->coded_frame)
				pkt.pts = vcodec->coded_frame->pts;
			if(vcodec->coded_frame->key_frame)
				pkt.flags |= PKT_FLAG_KEY;
			pkt.data = vbuffer;
			pkt.size = out_size;
			ret = av_interleaved_write_frame(o_file, &pkt);
		}
		else ret = 0;
	}
	av_free(picture);

	if(ret != 0)
	{
		log_message(1, "AVIFile: Error while writing video frame");
		return false;
	}

	return true;
}

bool AVIFile::writeAFrame(uint8_t * samples, uint size)
{
	if(!opened() || !size) return false;
	int ret      = 0;
	int out_size = 0;
	AVPacket pkt;

	if(acodec->frame_size > 1)
	{
		uint8_t *tmp = new uint8_t[a_fsize];
		afifo.write(samples, size);
		while(afifo.read(tmp, a_fsize))
		{
			av_init_packet(&pkt); // init static structure
			out_size = avcodec_encode_audio(acodec, abuffer, a_bsize, (short*)tmp);
			if(out_size != 0)
			{
				//write the compressed frame in the media file
				pkt.stream_index = astream->index;
				if(acodec->coded_frame && acodec->coded_frame->pts != AV_NOPTS_VALUE)
					pkt.pts = av_rescale_q(acodec->coded_frame->pts, acodec->time_base, astream->time_base);
				pkt.flags |= PKT_FLAG_KEY;
				pkt.data = abuffer;
				pkt.size = out_size;
				ret |= av_interleaved_write_frame(o_file, &pkt);
			}
			av_free_packet(&pkt);
		}
		delete[] tmp;
	}
	else
	{
		//output a pcm frame
		//XXX: change encoding codec API to avoid this ?
		switch(acodec->codec->id)
		{
			case CODEC_ID_PCM_S32LE:
			case CODEC_ID_PCM_S32BE:
			case CODEC_ID_PCM_U32LE:
			case CODEC_ID_PCM_U32BE:
				size = size << 1;
				break;
			case CODEC_ID_PCM_S24LE:
			case CODEC_ID_PCM_S24BE:
			case CODEC_ID_PCM_U24LE:
			case CODEC_ID_PCM_U24BE:
			case CODEC_ID_PCM_S24DAUD:
				size = size / 2 * 3;
				break;
			case CODEC_ID_PCM_S16LE:
			case CODEC_ID_PCM_S16BE:
			case CODEC_ID_PCM_U16LE:
			case CODEC_ID_PCM_U16BE:
				break;
			default:
				size = size >> 1;
				break;
		}

		av_init_packet(&pkt); // init static structure
		out_size = avcodec_encode_audio(acodec, abuffer, size, (short*)(samples));
		if(out_size != 0)
		{
			//write the compressed frame in the media file
			pkt.stream_index = astream->index;
			if(acodec->coded_frame && acodec->coded_frame->pts != AV_NOPTS_VALUE)
				pkt.pts= av_rescale_q(acodec->coded_frame->pts, acodec->time_base, astream->time_base);
			pkt.flags |= PKT_FLAG_KEY;
			pkt.data = abuffer;
			pkt.size = out_size;
			ret |= av_interleaved_write_frame(o_file, &pkt);
		}
		av_free_packet(&pkt);
	}

	if(ret != 0)
	{
		log_message(1, "AVIFile: Error while writing audio frame");
		return false;
	}

	return true;
}

void AVIFile::Close()
{
	if(!opened()) return;
	log_message(0, "AVIFile: Closing avi file...");
	av_write_trailer(o_file);
	cleanup();
}


//private functions
void AVIFile::cleanup()
{
	///pointer to the video Settings object (libconfig++)
	video_settings_ptr = NULL;
	///pointer to the audio Settings object (libconfig++)
	audio_settings_ptr = NULL;

	//encoders//
	if(_opened & INIT_VCODEC)
		avcodec_close(vcodec);
	if(_opened & INIT_ACODEC)
		avcodec_close(acodec);
	vcodec  = NULL;
	acodec  = NULL;

	//output streams//
	if(vstream)
		av_free(vstream);
	if(astream)
		av_free(astream);
	vstream = NULL;
	astream = NULL;

	//file//
	if(o_file)
	{
		if(_opened & INIT_FOPEN)
			url_fclose(o_file->pb);
		av_free(o_file);
	}
	o_file  = NULL;


	//Video Output
	if(vbuffer) av_free(vbuffer);
	vbuffer = NULL;

	//Audio Output
	if(abuffer) av_free(abuffer);
	abuffer = NULL;

	_opened = INIT_NONE;
}

CodecID AVIFile::get_codec_id(string name, int codec_type) const
{
	AVCodec *codec = avcodec_find_encoder_by_name(name.c_str());//first_avcodec;
	/*while(codec)
	{
		if((name == codec->name) && (codec->type == codec_type))
			break;
		codec = codec->next;
	}*/
	return (codec)? codec->id : CODEC_ID_NONE;
}
