/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd_die_version.c
 * Copyright (C) 2016 Olivier Brunel <jjk@jjacky.com>
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

#include "slicd/config.h"

#include <unistd.h>
#include <skalibs/buffer.h>
#include <slicd/die.h>

void
slicd_die_version (void)
{
    buffer_puts (buffer_1small, PROG);
    buffer_puts (buffer_1small, " v" SLICD_VERSION "\n");
    buffer_putsflush (buffer_1small,
            "Copyright (C) 2016 Olivier Brunel - http://jjacky.com/slicd\n"
            "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
            "This is free software: you are free to change and redistribute it.\n"
            "There is NO WARRANTY, to the extent permitted by law.\n"
            );
    _exit (0);
}
