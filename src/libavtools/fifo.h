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
/**
@author Allis
*/

#ifndef FIFO_H
#define FIFO_H

#include "common.h"

///implements standart fifo operations
template<class _type>
class Fifo
{
public:
	Fifo(uint _size = 0);
	~Fifo() { if(buffer) av_free(buffer); };

	uint data_size() const { return dsize; }; ///< return size of stored data
	bool read(_type* buf, uint bsize);        ///< read data from fifo
	                                          ///  if data_size is less than bsize, return false
	bool write(_type* buf, uint bsize);       ///< write data to fifo, if size is
	                                          ///  less then bsize, return false

private:
	_type   *buffer;  ///< fifo buffer
	uint     size;    ///< buffer size
	uint    dsize;    ///< size of the stored data
	_type   *r_point; ///< read pointer
	_type   *w_point; ///< write pointer
	_type   *end;     ///< end pointer
};



template<class _type>
Fifo<_type>::Fifo(uint _size)
{
    size = _size;
    if(!size) buffer = r_point = w_point = end = NULL;
	else
	{
		buffer  = (_type*)av_malloc(size);
		r_point = buffer;
		w_point = buffer;
		end     = buffer + size;
	}
	dsize   = 0;
}

template<class _type>
bool Fifo<_type>::read(_type *buf, uint bsize)
{
	if(dsize < bsize) return false;
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
		dsize -= len;
	}
	return true;
}

template<class _type>
bool Fifo<_type>::write(_type *buf, uint bsize)
{
	if(bsize > size) return false;
	uint len;
	while(bsize > 0)
	{
		len = end - w_point;
		if(len > bsize)
			len = bsize;

		memcpy(w_point, buf, len);

		if(w_point <= r_point && dsize)
		{
			if(w_point+len > r_point)
			{
				dsize  -= w_point+len - r_point;
				r_point = w_point;
			}
			if(r_point >= end)
				r_point = buffer;
		}
		w_point += len;
		if(w_point >= end)
			w_point = buffer;
		buf   += len;
		bsize -= len;
		dsize += len;
	}
	return true;
}

#endif
