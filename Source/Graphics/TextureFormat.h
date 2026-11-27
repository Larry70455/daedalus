/*
Copyright (C) 2005 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef GRAPHICS_TEXTUREFORMAT_H_
#define GRAPHICS_TEXTUREFORMAT_H_

#include <stdlib.h>

#include "Base/Types.h"
#include <unordered_map>

enum	ETextureFormat
{
	TexFmt_5650,
	TexFmt_5551,
	TexFmt_4444,
	TexFmt_8888,

	// Palletised
	TexFmt_CI4_8888,
	TexFmt_CI8_8888,
};

inline bool IsTextureFormatPalettised( ETextureFormat texture_format )
{
	return texture_format >= TexFmt_CI4_8888;
}


// A switch, not a table built on first use: no allocation or thread safe
// initialisation (kernel calls), so the display list front end can use it
// on the Media Engine
inline uint32_t GetBitsPerPixel(ETextureFormat texture_format)
{
    switch (texture_format)
    {
    case ETextureFormat::TexFmt_5650:		return 16;
    case ETextureFormat::TexFmt_5551:		return 16;
    case ETextureFormat::TexFmt_4444:		return 16;
    case ETextureFormat::TexFmt_8888:		return 32;
    case ETextureFormat::TexFmt_CI4_8888:	return 4;
    case ETextureFormat::TexFmt_CI8_8888:	return 8;
    default:								return 8;	// Unhandled texture format
    }
}
inline u32		CalcBytesRequired( u32 pixels, ETextureFormat texture_format )
{
	return ( pixels * GetBitsPerPixel( texture_format ) + 4 ) / 8;
}

#endif // GRAPHICS_TEXTUREFORMAT_H_
