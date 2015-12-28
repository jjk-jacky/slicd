/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * slicd_save.c
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
#include <fcntl.h>
#include <skalibs/skamisc.h>
#include <skalibs/uint32.h>
#include <skalibs/djbunix.h>
#include <slicd/slicd.h>
#include <slicd/err.h>

int
slicd_save (slicd_t *slicd, const char *file)
{
    char buf[4];
    int fd;
    struct iovec iov[3];

    fd = open3 (file, O_WRONLY | O_NONBLOCK | O_TRUNC | O_CREAT, 0600);
    if (fd < 0)
        return -SLICD_ERR_IO;

    uint32_pack (buf, (uint32) slicd->str.len);
    iov[0].iov_base = buf;
    iov[0].iov_len = 4;
    iov[1].iov_base = slicd->str.s;
    iov[1].iov_len = slicd->str.len;
    iov[2].iov_base = slicd->jobs.s;
    iov[2].iov_len = slicd->jobs.len;

    if (fd_writev (fd, iov, 3) < 0)
    {
        int e = errno;

        fd_close (fd);
        errno = e;
        return -SLICD_ERR_IO;
    }

    fd_close (fd);
    return 0;
}
