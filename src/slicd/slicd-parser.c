/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * slicd-parser.c
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

#define _BSD_SOURCE

#include "slicd/config.h"

#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/direntry.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <skalibs/uint.h>
#include <skalibs/strerr2.h>
#include <slicd/slicd.h>
#include <slicd/parser.h>
#include <slicd/err.h>
#include <slicd/die.h>

enum
{
    RC_OK = 0,
    RC_SYNTAX,
    RC_IO,
    RC_MEMORY,
    RC_NO_CRONTABS,
    RC_NO_OUTPUT,
    RC_ERRORS
};

enum type
{
    TYPE_SYSTEM = 0,
    TYPE_USER,
    TYPE_USER_DIR
};

struct crontab
{
    enum type type;
    const char *path;
};

static int rc = RC_OK;

typedef void (*scandir_cb) (const char *path, const char *name, void *data);
static void scan_dir (const char *path, int files, scandir_cb callback, void *data);

static const char *userfile = "";
static stralloc sa = STRALLOC_ZERO;

static slicd_t slicd = { 0, };

static void
do_parse (const char *user, const char *file, unsigned int *line, int eof)
{
    char *s = sa.s;
    int len = sa.len;

    for (;;)
    {
        int l;
        int r;

        if (len <= 0 || eof == 2)
            break;

        l = byte_chr (s, len, '\n');
        if (l == len)
        {
            if (!eof)
                break;
            eof = 2;
        }
        else
            s[l] = '\0';

        r = slicd_add_job_from_cronline (&slicd, user, s);
        if (r < 0)
        {
            char buf[UINT_FMT];

            buf[uint_fmt (buf, *line)] = '\0';
            if (-r == SLICD_ERR_MEMORY)
                strerr_diefu6sys (RC_MEMORY, "parse '", file, "' line ", buf, ": ",
                        slicd_errmsg[-r]);
            else
            {
                rc = RC_ERRORS;
                strerr_warnwu6x ("parse '", file, "' line ", buf, ": ", slicd_errmsg[-r]);
            }
        }

        ++*line;
        s += l + 1;
        len -= l + 1;
    }

    if (s > sa.s)
    {
        if (len > 0)
            byte_copy (sa.s, len, s);
        else
            len = 0;
        sa.len = len;
    }
}

static void
parse_file (const char *path, const char *name, void *data)
{
    const char *username = data;
    int l_path = strlen (path);
    int l_name = strlen (name);
    char buf[l_path + 1 + l_name + 1];
    int fd;
    int r;
    unsigned int line = 1;

    if (username == userfile)
        username = name;

    byte_copy (buf, l_path, path);
    buf[l_path] = '/';
    byte_copy (buf + l_path + 1, l_name + 1, name);

    fd = open_read (buf);
    if (fd < 0)
        strerr_diefu3sys (RC_IO, "open file '", buf, "'");

    for (;;)
    {
        if (!stralloc_readyplus (&sa, 1024))
        {
            int e = errno;

            fd_close (fd);
            errno = e;
            strerr_diefu3sys (RC_MEMORY, "allocate memory while reading '", buf, "'");
        }

        r = fd_read (fd, sa.s + sa.len, 1024);
        if (r < 0)
        {
            int e = errno;

            fd_close (fd);
            errno = e;
            strerr_diefu3sys (RC_IO, "read '", buf, "'");
        }
        else if (r == 0)
        {
            fd_close (fd);
            do_parse (username, buf, &line, 1);
            return;
        }
        sa.len += r;

        do_parse (username, buf, &line, 0);
    }
}

static void
scan_userdirs (const char *path, const char *name, void *data)
{
    int l_path = strlen (path);
    int l_name = strlen (name);
    char buf[l_path + 1 + l_name + 1];

    byte_copy (buf, l_path, path);
    buf[l_path] = '/';
    byte_copy (buf + l_path + 1, l_name + 1, name);

    scan_dir (buf, 1, parse_file, (void *) name);
}

static void
scan_dir (const char *path, int files, scandir_cb callback, void *data)
{
    DIR *dir;
    int e = 0;

    dir = opendir (path);
    if (!dir)
        strerr_diefu3sys (RC_IO, "open directory '", path, "'");

    for (;;)
    {
        direntry *d;

        errno = 0;
        d = readdir (dir);
        if (!d)
        {
            e = errno;
            break;
        }
        if (d->d_name[0] == '.'
                && (d->d_name[1] == '\0' || (d->d_name[1] == '.' && d->d_name[2] == '\0')))
            continue;
        if ((files && d->d_type != DT_REG) || (!files && d->d_type != DT_DIR))
            continue;

        callback (path, d->d_name, data);
    }
    dir_close (dir);

    if (e > 0)
    {
        errno = e;
        strerr_diefu3sys (RC_IO, "read directory '", path, "'");
    }
}

static void
dieusage (int rc)
{
    slicd_die_usage (rc, "OPTION...",
            " -s, --system FILE             Parse FILE for system crontabs\n"
            " -u, --users DIR               Parse DIR for user crontabs\n"
            " -U, --users-dirs DIR          Parse DIR for directories of user crontabs\n"
            " -o, --output FILE             Write compiled crontabs to FILE\n"
            " -h, --help                    Show this help screen and exit\n"
            " -V, --version                 Show version information and exit\n"
            );
}

int
main (int argc, char * const argv[])
{
    PROG = "slicd-parser";
    const char *output = NULL;
    struct crontab crontab[argc / 2];
    int nb = 0;

    for (;;)
    {
        struct option longopts[] = {
            { "help",               no_argument,        NULL,   'h' },
            { "output",             required_argument,  NULL,   'o' },
            { "system",             required_argument,  NULL,   's' },
            { "users-dirs",         required_argument,  NULL,   'U' },
            { "users",              required_argument,  NULL,   'u' },
            { "version",            no_argument,        NULL,   'V' },
            { NULL, 0, 0, 0 }
        };
        int c;

        c = getopt_long (argc, argv, "ho:s:U:u:V", longopts, NULL);
        if (c == -1)
            break;
        switch (c)
        {
            case 'h':
                dieusage (RC_OK);

            case 'o':
                output = optarg;
                break;

            case 's':
                crontab[nb].type = TYPE_SYSTEM;
                crontab[nb].path = optarg;
                ++nb;
                break;

            case 'U':
                crontab[nb].type = TYPE_USER_DIR;
                crontab[nb].path = optarg;
                ++nb;
                break;

            case 'u':
                crontab[nb].type = TYPE_USER;
                crontab[nb].path = optarg;
                ++nb;
                break;

            case 'V':
                slicd_die_version ();

            default:
                dieusage (RC_SYNTAX);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc > 0)
        dieusage (RC_SYNTAX);
    if (nb == 0)
        strerr_dief1x (RC_NO_CRONTABS, "no crontabs specified");
    if (!output)
        strerr_dief1x (RC_NO_OUTPUT, "no output file specified");

    {
        int i;

        for (i = 0; i < nb; ++i)
        {
            struct stat st;

            if (stat (crontab[i].path, &st) < 0)
            {
                rc = RC_ERRORS;
                strerr_warnwu3sys ("stat '", crontab[i].path, "'");
                continue;
            }

            switch (crontab[i].type)
            {
                case TYPE_SYSTEM:
                    if (S_ISREG (st.st_mode))
                    {
                        int len = strlen (crontab[i].path);
                        char buf[len + 1];
                        int l;

                        byte_copy (buf, len + 1, crontab[i].path);

                        l = byte_rchr (buf, len, '/');
                        if (l == len)
                            l = 0;
                        else
                            buf[l++] = '\0';

                        parse_file ((l) ? buf : ".", buf + l, NULL);
                    }
                    else if (S_ISDIR (st.st_mode))
                        scan_dir (crontab[i].path, 1, parse_file, NULL);
                    else
                    {
                        rc = RC_ERRORS;
                        strerr_warnwu3x ("cannot parse system crontab(s) '",
                                crontab[i].path, "': not a file nor directory");
                        continue;
                    }
                    break;

                case TYPE_USER:
                    if (!S_ISDIR (st.st_mode))
                    {
                        rc = RC_ERRORS;
                        strerr_warnwu3x ("cannot parse user crontabs from '",
                                crontab[i].path, "': not a directory");
                        continue;
                    }
                    scan_dir (crontab[i].path, 1, parse_file, (void *) userfile);
                    break;

                case TYPE_USER_DIR:
                    if (!S_ISDIR (st.st_mode))
                    {
                        rc = RC_ERRORS;
                        strerr_warnwu3x ("cannot parse user crontabs from '",
                                crontab[i].path, "': not a directory");
                        continue;
                    }
                    scan_dir (crontab[i].path, 0, scan_userdirs, NULL);
                    break;
            }
        }
    }

    if (rc != RC_OK)
        strerr_dief1x (rc, "Errors occured; Aborting");
    if (slicd_save (&slicd, output) < 0)
        strerr_diefu3sys (RC_IO, "save compiled crontabs to '", output, "'");

    stralloc_free (&sa);
    return rc;
}
