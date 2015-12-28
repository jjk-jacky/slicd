/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * bitarray_clearsetn.c
 * Copyright (C) 2015 Olivier Brunel <jjk@jjacky.com>
 *
 * This file is part of slicd.
 *
 * slicd is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * slicd is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * slicd. If not, see http://www.gnu.org/licenses/
 */
/* Based on bitarray_clearsetn.c from skalibs; ISC license
 * Copyright (c) 2011-2015 Laurent Bercot <ska-skaware@skarnet.org> */

#include <skalibs/bitarray.h>

void _bitarray_clearsetn (register unsigned char *s,
                          register unsigned int a,
                          register unsigned int b,
                          register unsigned int h)
{
 if (!b) return ;
  b += a ;
  if ((a >> 3) == ((b-1) >> 3))
  {
    register unsigned char mask = ((1 << (a & 7)) - 1) ^ ((1 << ((b & 7) ? b & 7 : 8)) - 1) ;
    if (h) s[a>>3] |= mask ; else s[a>>3] &= ~mask ;
  }
  else
  {
    register unsigned char mask = ~((1 << (a & 7)) - 1) ;
    register unsigned int i = (a>>3) + 1 ;
    if (h) s[a>>3] |= mask ; else s[a>>3] &= ~mask ;
    mask = h ? 0xff : 0x00 ;
    for (; i < b>>3 ; i++) s[i] = mask ;
    mask = (1 << (b & 7)) - 1 ;
    if (h) s[b>>3] |= mask ; else s[b>>3] &= ~mask ;
  }
}
