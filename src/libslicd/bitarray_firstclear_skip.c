/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * bitarray_firstclear_skip.c
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
/* Based on bitarray_firstclear.c from skalibs; ISC license
 * Copyright (c) 2011-2015 Laurent Bercot <ska-skaware@skarnet.org> */

#include <skalibs/bitarray.h>

unsigned int
_bitarray_firstclear_skip (register unsigned char const *s, unsigned int max, unsigned int skip)
{
    unsigned int n = bitarray_div8 (max);
    register unsigned int i = bitarray_div8 (skip);

    if (i && s[i - 1] != 0xffU)
    {
        register unsigned int j = skip;

        skip = i << 3;
        if (skip > max)
            skip = max;
        while ((j < skip) && bitarray_peek (s, j))
            ++j;
        if (j < skip)
            return j;
    }

    for ( ; i < n ; ++i)
        if (s[i] != 0xffU)
            break;

    if (i == n)
        return max;

    i <<= 3;
    while ((i < max) && bitarray_peek (s, i))
        ++i;
    return i;
}
