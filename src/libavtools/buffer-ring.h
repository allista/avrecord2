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
#ifndef BUFFER_RING_H
#define BUFFER_RING_H

/**
Ring of byte buffers. Works as queue with constant number of nods.

	@author Allis Tauri <allista@gmail.com>
*/


class BufferRing
{
	public:
		///Constructor
		BufferRing(unsigned int _b_size,    ///< size of a buffer
				   unsigned int _r_size = 2 ///< number of buffers in the ring
				  );
		///Destructor
		~BufferRing();

		unsigned int buffer_size() const { return b_size; }; ///< Returns size of a buffer
		unsigned int ring_size()   const { return r_size; }; ///< Returns number of buffers in the ring
		unsigned int filled_size() const { return c_size; }; ///< Returns number of buffers currently used

		bool empty() const { return iw == ir; }; ///< Returns true, if there're no buffers to read
		bool full()  const { return c_size >= r_size-1; }; ///< true if the buffer is full

		///Returns current write buffer
		unsigned char* wbuffer() { return buffers[iw]; };

		///Returns current read buffer
		unsigned char* rbuffer() { return buffers[ir]; };
		operator unsigned char*() { return buffers[ir]; }; ///< Same as rbuffer()
		///Returns number of bytes written in current read buffer
		unsigned int rsize() const { return written[ir]; };

		///Returns N'th buffer after the write one, or -N'th befor the read one, unless |N| is not grater than csize
		unsigned char* operator[](int i);

		///Returns N'th buffer size after the write one, or -N'th befor the read one, unless |N| is not grater than csize
		unsigned int written_size(int i);

		///Pushes current write buffer down the ring
		void push(unsigned int written_size = 0);

		///Pops current read buffer out of the ring
		void pop();

		///Resets the ring
		void reset() { iw = ir = c_size = 0; };

	private:
		unsigned int    b_size;  ///< size of a buffer
		unsigned int    r_size;  ///< number of buffers in the ring
		unsigned int    c_size;  ///< number of buffers currently used
		unsigned char **buffers; ///< the ring itself
		unsigned int   *written; ///< how many bytes was written in i'th buffer

		unsigned int    ir;      ///< read index
		unsigned int    iw;      ///< write index
};

#endif
