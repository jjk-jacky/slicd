/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * miniexec.c
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

#include <getopt.h>
#include <skalibs/djbunix.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/strerr2.h>
#include <slicd/die.h>

#define is_blank(c) \
    (c == ' ' || c == '\t')

enum
{
    RC_OK = 0,
    RC_SYNTAX = 131,
    RC_IO,
    RC_MEMORY,
    RC_PARSING
};

static void
dieusage (int rc)
{
    slicd_die_usage (rc, "ARGS...",
            " -h, --help                    Show this help screen and exit\n"
            " -V, --version                 Show version information and exit\n"
            );
}

int
main (int argc, char * const argv[])
{
    PROG = "miniexec";
    stralloc sa = STRALLOC_ZERO;
    genalloc ga = GENALLOC_ZERO;

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

    if (argc < 1)
        dieusage (RC_SYNTAX);

    if (!stralloc_ready (&sa, 64))
        strerr_diefu1sys (RC_MEMORY, "allocate memory");
    if (!genalloc_ready (char *, &ga, argc + 1))
        strerr_diefu1sys (RC_MEMORY, "allocate memory");

    {
        genalloc ga_idx = GENALLOC_ZERO;
        char *p = NULL;
        char *start = *argv;
        int i;

        while (*argv)
        {
            char *s;
            int in_single = 0;

            while (is_blank (*start))
                ++start;

            for (s = start; ; ++s)
            {
                if (*s == '\0')
                {
                    if (in_single)
                        strerr_diefu3x (RC_PARSING, "parse arg '", *argv,
                                "': unclosed single quote");

                    if (!p)
                    {
                        if (s > start)
                        {
                            p = start;
                            genalloc_append (char *, &ga, &p);
                        }
                    }
                    else
                    {
                        stralloc_catb (&sa, start, s - start);
                        stralloc_0 (&sa);
                    }

                    p = NULL;
                    start = *++argv;
                    break;
                }
                else if (*s == '\'')
                {
                    if (!in_single)
                    {
                        if (!p)
                        {
                            i = genalloc_len (char *, &ga);
                            genalloc_append (int, &ga_idx, &i);
                            p = (char *) (intptr_t) sa.len;
                            genalloc_append (char *, &ga, &p);
                        }
                    }
                    stralloc_catb (&sa, start, s - start);

                    start = s + 1;
                    in_single = !in_single;
                }
                else if (in_single)
                    continue;
                else if (is_blank (*s))
                {
                    if (!p)
                    {
                        i = genalloc_len (char *, &ga);
                        genalloc_append (int, &ga_idx, &i);
                        p = (char *) (intptr_t) sa.len;
                        genalloc_append (char *, &ga, &p);
                    }
                    stralloc_catb (&sa, start, s - start);
                    stralloc_0 (&sa);

                    p = NULL;
                    start = s + 1;
                    break;
                }
                else if (*s == '\\' && (s[1] == '\\' || s[1] == '\''))
                {
                    if (!p)
                    {
                        i = genalloc_len (char *, &ga);
                        genalloc_append (int, &ga_idx, &i);
                        p = (char *) (intptr_t) sa.len;
                        genalloc_append (char *, &ga, &p);
                    }
                    stralloc_catb (&sa, start, s - start);

                    start = ++s;
                }
            }
        }
        for (i = 0; i < genalloc_len (int, &ga_idx); ++i)
        {
            int idx = genalloc_s (int, &ga_idx)[i];
            unsigned int offset = (unsigned int) (intptr_t) genalloc_s (char *, &ga)[idx];
            p = sa.s + offset;
            byte_copy (ga.s + idx * sizeof (char *), sizeof (char *), &p);
        }
        genalloc_append (char *, &ga, argv);

        genalloc_free (int, &ga_idx);
        genalloc_shrink (char *, &ga);
        stralloc_shrink (&sa);
    }

    pathexec ((char const * const *) ga.s);
    strerr_dieexec (RC_IO, genalloc_s (char *, &ga)[0]);
}
