/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * slicd_load.c
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

#include <errno.h>
#include <skalibs/skamisc.h>
#include <skalibs/uint32.h>
#include <skalibs/djbunix.h>
#include <slicd/slicd.h>
#include <slicd/err.h>

int
slicd_load (slicd_t *slicd, const char *file)
{
    char buf[4];
    int fd;
    int r;
    uint32 len, left;

    if (slicd->str.len > 0 || slicd->jobs.len > 0)
        return -SLICD_ERR_NOT_EMPTY;

    fd = open_read (file);
    if (fd < 0)
        return -SLICD_ERR_IO;

    len = 0;
    left = 4;
    while (left > 0)
    {
        r = fd_read (fd, buf + len, left);
        if (r <= 0)
        {
            int e = (r == 0) ? EPROTO : errno;

            fd_close (fd);
            errno = e;
            return -SLICD_ERR_IO;
        }
        left -= r;
        len += r;
    }

    uint32_unpack (buf, &left);
    if (!stralloc_ready_tuned (&slicd->str, left, 0, 0, 1))
    {
        r = -SLICD_ERR_MEMORY;
        goto err;
    }
    while (left > 0)
    {
        r = fd_read (fd, slicd->str.s + slicd->str.len, left);
        if (r <= 0)
        {
            if (r == 0)
                errno = EPROTO;
            r = -SLICD_ERR_IO;
            goto err;
        }
        slicd->str.len += r;
        left -= r;
    }

    for (;;)
    {
        if (!stralloc_readyplus (&slicd->jobs, 1024))
        {
            r = -SLICD_ERR_MEMORY;
            goto err;
        }
        r = fd_read (fd, slicd->jobs.s + slicd->jobs.len, 1024);
        if (r < 0)
        {
            r = -SLICD_ERR_IO;
            goto err;
        }
        else if (r == 0)
        {
            stralloc_shrink (&slicd->jobs);
            fd_close (fd);
            return 0;
        }
        slicd->jobs.len += r;
    }

err:
    {
        int e = errno;

        slicd->str.len = 0;
        stralloc_shrink (&slicd->str);
        slicd->jobs.len = 0;
        stralloc_shrink (&slicd->jobs);

        fd_close (fd);
        errno = e;
        return r;
    }
}
