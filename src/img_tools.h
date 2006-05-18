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

// C++ Interface: img_tools

#ifndef IMG_TOOLS_H
#define IMG_TOOLS_H

extern "C"
{
#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>
}

#include <string>
using namespace std;

#include "common.h"


///must be called befor any av operations
void AV_Init();

///cleans all static ffmpeg structures
void AV_Free();

///draws a text (possibly multilite) on a raw image.
void DrawText(unsigned char *image, ///< pointer to the image buffer
              string text,          ///< text to draw
              uint x,                ///< x position of the left side of the first character of the "text"
              uint y,                ///< y position of the bottom side of the last character of the "text"
              uint width,            ///< width of the image
              uint height,           ///< height of the image
              bool big = false      ///< flag that shows what font to use to draw
             );

///returns text width in pixels
uint TextWidth(string text, bool big = false);

#endif
