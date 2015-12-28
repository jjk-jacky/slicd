/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * slicd_die_usage.c
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

#include <unistd.h>
#include <skalibs/buffer.h>
#include <slicd/die.h>

void
slicd_die_usage (int rc, const char *usage, const char *details)
{
    buffer_puts (buffer_1small, "Usage: ");
    buffer_puts (buffer_1small, PROG);
    buffer_puts (buffer_1small, " ");
    buffer_puts (buffer_1small, usage);
    buffer_puts (buffer_1small, "\n\n");
    buffer_putsflush (buffer_1small, details);
    _exit (rc);
}
