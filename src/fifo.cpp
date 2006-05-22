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

#include "fifo.h"

Fifo::Fifo(uint size)
		: size(size)
{
	buffer  = (uint8_t*)av_malloc(size);
	r_point = buffer;
	w_point = buffer;
	end     = buffer + size;
	dsize   = 0;
}

bool Fifo::read(uint8_t *buf, uint bsize)
{
	if(data_size() < bsize) return false;
	dsize -= bsize;
	uint len;
	while(bsize > 0)
	{
		len = end - r_point;
		if(len > bsize)
			len = bsize;

		memcpy(buf, r_point, len);
		buf     += len;
		r_point += len;

		if(r_point >= end)
			r_point = buffer;
		bsize -= len;
	}
	return true;
}

void Fifo::write(uint8_t *buf, uint bsize)
{
	dsize += bsize;
	int len;
	while(bsize > 0)
	{
		len = end - w_point;
		if(len > bsize)
			len = bsize;

		memcpy(w_point, buf, len);

		w_point += len;
		if(w_point >= end)
			w_point = buffer;
		buf   += len;
		bsize -= len;
	}
}


