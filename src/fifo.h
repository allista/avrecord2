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
class Fifo
{
public:
	Fifo(uint size = 0);
	~Fifo() { delete[] buffer; };

	uint data_size() const;               ///< returns size of stored data
	bool read(uint8_t* buf, uint bsize);  ///< reads data from fifo
	                                      ///  if data_size is less than bsize, return false
	void write(uint8_t* buf, uint bsize); ///< writes data to fifo

private:
	uint8_t *buffer;  ///< fifo buffer
	uint     size;    ///< buffer size
	uint8_t *r_point; ///< read pointer
	uint8_t *w_point; ///< write pointer
	uint8_t *end;     ///< end pointer
};

#endif
