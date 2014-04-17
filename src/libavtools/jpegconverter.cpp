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
#include "jpegconverter.h"

JpegConverter::JpegConverter()
{
	inited         = false;

	tmp_buffer     = NULL;
	tmp_buffer_len = NULL;
	in_frame       = NULL;
	out_frame      = NULL;
	sws            = NULL;
	in_fmt         = PIX_FMT_NONE;

	quality        = 75;
	work_buffer    = NULL;
}


JpegConverter::~JpegConverter()
{
	Close();
}

void JpegConverter::Close()
{
	if(tmp_buffer) av_free(tmp_buffer);
	tmp_buffer = NULL;

	if(in_frame) av_free(in_frame);
	in_frame = NULL;

	if(out_frame) av_free(out_frame);
	out_frame = NULL;

	if(sws) av_free(sws);
	sws = NULL;

	if(inited & JPEG_INITED) jpeg_destroy_compress(&cinfo);
	work_buffer = NULL;

	inited = 0;
}

bool JpegConverter::Init(uint _in_fmt, Setting* video_settings_ptr, Setting* webcam_settings_ptr)
{
	if(inited) return false;

	//get settings: frame width and height from video group, quality from webcam group
	Setting &video_settings  = *video_settings_ptr;
	Setting &webcam_settings = *webcam_settings_ptr;
	try
	{
		width          = video_settings["width"];
		height         = video_settings["height"];
		quality        = webcam_settings["quality"];
	}
	catch(SettingNotFoundException)
	{
		log_message(1, "JpegConverter: <video.width>, <video.height> or <webcam.quality> setting not found.");
		Close();
		return false;
	}
	quality = (quality > 100)? 100 : quality;

	//initialize software scaler for pixel format conversion if needed
	in_fmt = av_pixel_formats.find(_in_fmt)->second;
	if(in_fmt != out_fmt)
	{
		tmp_buffer_len = avpicture_get_size(out_fmt, width, height);
		tmp_buffer = (uint8_t*)av_mallocz(tmp_buffer_len);
		if(!tmp_buffer)
		{
			log_message(1, "JpegConverter: av_mallocz - could not allocate memory for tmp_buffer");
			Close();
			return false;
		}

		in_frame = avcodec_alloc_frame();
		if(!in_frame)
		{
			log_message(1, "JpegConverter: avcodec_alloc_frame - could not allocate in_frame");
			Close();
			return -1;
		}

		out_frame = avcodec_alloc_frame();
		if(!out_frame)
		{
			log_message(1, "JpegConverter: avcodec_alloc_frame - could not allocate out_frame");
			Close();
			return false;
		}

		sws = sws_getContext(width, height, in_fmt,
							 width, height, out_fmt,
							 SWS_POINT, NULL, NULL, NULL);
		if(!sws)
		{
			log_message(1, "JpegConverter: couldn't get SwsContext for pixel format conversion");
			Close();
			return false;
		}
		avpicture_fill((AVPicture*)out_frame, tmp_buffer, out_fmt, width, height);
	}

	//initialize jpeg encoder
	cinfo.err = jpeg_std_error(&error_logger.err_mgr);
	error_logger.err_mgr.error_exit     = error_exit;
	error_logger.err_mgr.output_message = output_message;

	if(setjmp(error_logger.jump_buffer))
	{
    	// If we get here, the JPEG code has signaled an error.
		// We need to clean up the JPEG object, close the input file, and return.
		Close();
		return false;
	}

	jpeg_create_compress(&cinfo);
	inited |= JPEG_INITED;
	cinfo.dest = (jpeg_destination_mgr*)(cinfo.mem->alloc_small) ((j_common_ptr) &cinfo, JPOOL_PERMANENT, sizeof(buffer_dest));

	work_buffer = (buffer_dest*)(cinfo.dest);
	work_buffer->dest_mgr.init_destination     = init_destination;
	work_buffer->dest_mgr.empty_output_buffer  = empty_output_buffer;
	work_buffer->dest_mgr.term_destination     = term_destination;
	work_buffer->out_buffer                    = &out_buffer;

	inited |= INITED;
	return true;
}

int JpegConverter::Convert(uint8_t *& out_image, uint8_t * in_image, uint in_len)
{
	if(!(inited & INITED)) return -1;

	uint8_t* buffer = NULL;
	//convert pixel format, if needed
	if(sws)
	{
		avpicture_fill((AVPicture*)in_frame, in_image, in_fmt, width, height);
		if(0 > sws_scale(sws, in_frame->data, in_frame->linesize, 0,
		   height, out_frame->data, out_frame->linesize))
		{
			log_message(1,"JpegConverter: sws_scale - Unable to convert image");
			Close();
			return -1;
		}
		buffer = tmp_buffer;
	}
	else buffer = in_image;

	cinfo.image_width      = width;
	cinfo.image_height     = height;
	cinfo.input_components = 3;
	cinfo.in_color_space   = jcs;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, true);

	//convert frame to jpeg
	jpeg_start_compress(&cinfo, true);
	JSAMPROW row_pointer[1];
	while(cinfo.next_scanline < cinfo.image_height)
	{
		row_pointer[0] = (JSAMPLE*)&buffer[cinfo.next_scanline * cinfo.image_width * cinfo.input_components];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);

	uint out_image_size = out_buffer.size();
	out_image = (uint8_t*)malloc(out_image_size);
	memcpy(out_image, &out_buffer.front(), out_image_size);
	out_buffer.clear();
	return out_image_size;
}

void JpegConverter::init_destination(j_compress_ptr _cinfo)
{
	buffer_dest* buf_dest = (buffer_dest*)(_cinfo->dest);

	buf_dest->work_buffer = (uint8_t*)(*_cinfo->mem->alloc_small) ((j_common_ptr)_cinfo, JPOOL_IMAGE, JPEG_WORK_BUFFER_SIZE);
	buf_dest->dest_mgr.next_output_byte = buf_dest->work_buffer;
	buf_dest->dest_mgr.free_in_buffer = JPEG_WORK_BUFFER_SIZE;
}

boolean JpegConverter::empty_output_buffer(j_compress_ptr _cinfo)
{
	buffer_dest* buf_dest = (buffer_dest*)(_cinfo->dest);

	size_t out_buf_size = buf_dest->out_buffer->size();
	buf_dest->out_buffer->resize(out_buf_size + JPEG_WORK_BUFFER_SIZE);
	memcpy((&buf_dest->out_buffer->front()) + out_buf_size, buf_dest->work_buffer, JPEG_WORK_BUFFER_SIZE);

	buf_dest->dest_mgr.next_output_byte = buf_dest->work_buffer;
	buf_dest->dest_mgr.free_in_buffer = JPEG_WORK_BUFFER_SIZE;

	return true;
}

void JpegConverter::term_destination(j_compress_ptr _cinfo)
{
	buffer_dest* buf_dest = (buffer_dest*)(_cinfo->dest);
	size_t datacount = JPEG_WORK_BUFFER_SIZE - buf_dest->dest_mgr.free_in_buffer;

	size_t out_buf_size = buf_dest->out_buffer->size();
	buf_dest->out_buffer->resize(out_buf_size + datacount);
	memcpy((&buf_dest->out_buffer->front()) + out_buf_size, buf_dest->work_buffer, datacount);
}

void JpegConverter::error_exit(j_common_ptr _cinfo)
{
	log_err* lerr = (log_err*)_cinfo->err;
	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	(*_cinfo->err->output_message)(_cinfo);

	/* Return control to the setjmp point */
	longjmp(lerr->jump_buffer, 1);
}

void JpegConverter::output_message(j_common_ptr _cinfo)
{
	char message[JMSG_LENGTH_MAX];
	(*_cinfo->err->format_message)(_cinfo, message);
	log_message(1, message);
}



