//this file is part of MimeTools (plugin for Notepad++)
//Copyright (C)2019 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <string.h>
#include <ctype.h>
#include "PluginInterface.h"

#include "url.h"

// These characters must be encoded in a URL, as per RFC1738
static const char gReservedAscii[] = "<>\"#%{}|\\^~[]`;/?:@=& ";
static const char gHexChar[] = "0123456789ABCDEF";

int AsciiToUrl (char* dest, const char* src, int destSize, bool encodeAll)
{
  int i;

  memset (dest, 0, destSize);

  for (i = 0; (i < (destSize - 2)) && *src; ++i, ++src)
  {
    // Encode source if it is a reserved or non-printable character.
    //
    if (encodeAll || (strchr (gReservedAscii, *src) != 0) || !isprint(*src))
    {
      *dest++ = '%';
      *dest++ = gHexChar [((*src >> 4) & 0x0f)];
      *dest++ = gHexChar [(*src & 0x0f)];
      i += 2;
    }
    else  // don't encode character
    {
      *dest++ = *src;
    }
  }

  return i;  // return characters stored to destination
}


int UrlToAscii (char* dest, const char* src, int destSize)
{
    int len;
    int c1, c2;
    for (len = 0; len < destSize && (c1 = *src++) != '\0'; )
    {
        if (c1 == '%' && isxdigit(src[0]) && isxdigit(src[1]))
        {
            c1 = (UCHAR)*src++;
            c2 = (UCHAR)*src++;
            c1 = ((isdigit(c1) ? c1 - '0' : tolower(c1) - 'a' + 10) << 4) +
                  (isdigit(c2) ? c2 - '0' : tolower(c2) - 'a' + 10);
        }
        dest[len++] = (char) c1;
    }
    return len;
}