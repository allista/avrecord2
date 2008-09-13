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
#include "buffer-ring.h"
#include <cstdlib>


BufferRing::BufferRing(unsigned int _b_size, unsigned int _r_size)
{
	b_size = _b_size;
	r_size = (_r_size < 2)? 2 : _r_size;
	c_size = 0;
	iw = 0;
	ir = 0;

	written = new unsigned int[r_size];
	buffers = new unsigned char*[r_size];
	for(unsigned int i = 0; i < r_size; i++)
	{
		buffers[i] = new unsigned char[b_size];
		written[i] = 0;
	}
}


BufferRing::~BufferRing()
{
	for(unsigned int i = 0; i < r_size; i++)
		delete[] buffers[i];
	delete[] buffers;
}


void BufferRing::push(unsigned int written_size)
{
	written[iw] = written_size;
	iw = (iw+1 >= r_size)? 0 : iw+1;
	if(c_size >= r_size-1)
		ir = (ir+1 >= r_size)? 0 : ir+1;
	c_size = (iw >= ir)? (iw - ir) : (iw + r_size - ir);
}


void BufferRing::pop()
{
	if(iw == ir) return;
	written[ir] = 0;
	if(iw > ir) ir++;
	else ir = (ir >= r_size-1)? 0 : ir+1;
	c_size = (iw >= ir)? (iw - ir) : (iw + r_size - ir);
}

unsigned char * BufferRing::operator [ ](int i)
{
	if(abs(i) > c_size) return NULL;	unsigned int n;
	if(i >= 0)
		return buffers[(iw+i >= r_size)? iw+i-r_size : iw+i];
	else
		return buffers[(ir-i >= r_size)? ir-i-r_size : ir-i];
}

unsigned int BufferRing::written_size(int i)
{
	if(abs(i) > c_size) return 0;
	if(i >= 0)
		return written[(iw+i >= r_size)? iw+i-r_size : iw+i];
	else
		return written[(ir-i >= r_size)? ir-i-r_size : ir-i];
}

