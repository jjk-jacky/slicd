/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * setuid.c
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

#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <skalibs/djbunix.h>
#include <skalibs/strerr2.h>
#include <slicd/die.h>

enum
{
    RC_OK = 0,
    RC_SYNTAX = 131,
    RC_IO,
    RC_MEMORY,
    RC_UNKNOWN,
    RC_OTHER
};

static void
dieusage (int rc)
{
    slicd_die_usage (rc, "USERNAME PROG...",
            " -h, --help                    Show this help screen and exit\n"
            " -V, --version                 Show version information and exit\n"
            );
}

int
main (int argc, char * const argv[])
{
    PROG = "setuid";
    struct passwd *pw;

    for (;;)
    {
        struct option longopts[] = {
            { "help",               no_argument,        NULL,   'h' },
            { "version",            no_argument,        NULL,   'V' },
            { NULL, 0, 0, 0 }
        };
        int c;

        c = getopt_long (argc, argv, "+hV", longopts, NULL);
        if (c == -1)
            break;
        switch (c)
        {
            case 'h':
                dieusage (RC_OK);

            case 'V':
                slicd_die_version ();

            default:
                dieusage (RC_SYNTAX);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 2)
        dieusage (RC_SYNTAX);

    pw = getpwnam (argv[0]);
    if (!pw)
        strerr_dief2x (RC_UNKNOWN, "unknown user: ", argv[0]);

    {
        gid_t gids[NGROUPS_MAX];
        int n = 0;

        errno = 0;
        for (;;)
        {
            struct group *gr;
            register char **member;

            gr = getgrent ();
            if (!gr)
                break;
            for (member = gr->gr_mem; *member; ++member)
                if (str_equal (*member, argv[0]))
                    break;
            if (*member)
                gids[n++] = gr->gr_gid;
        }
        endgrent ();
        if (errno != 0)
            strerr_diefu2sys (RC_OTHER, "get supplementary groups for ", argv[0]);
        if (setgroups (n, gids) < 0)
            strerr_diefu2sys (RC_OTHER, "set supplementary groups for ", argv[0]);
    }
    if (setgid (pw->pw_gid) < 0)
        strerr_diefu1sys (RC_OTHER, "setgid");
    if (setuid (pw->pw_uid) < 0)
        strerr_diefu1sys (RC_OTHER, "setuid");

    pathexec ((char const * const *) ++argv);
    strerr_dieexec (RC_IO, argv[0]);
}
