/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd-dump.c
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
#include <slicd/slicd.h>
#include <slicd/job.h>
#include <slicd/fields.h>
#include <slicd/die.h>
#include <skalibs/buffer.h>
#include <skalibs/uint.h>
#include <skalibs/genalloc.h>
#include <skalibs/strerr2.h>

static slicd_t slicd = { 0, };

#define job_str(job)        (slicd.str.s + job->offset)

static void
dieusage (int rc)
{
    slicd_die_usage (rc, "FILE",
            " -h, --help                    Show this help screen and exit\n"
            " -V, --version                 Show version information and exit\n"
            );
}

static void
put_uint (unsigned int u, slicd_field_t field)
{
    if (field == SLICD_MONTHS || field == SLICD_DAYS_OF_WEEK)
    {
        const char *names[] = {
            "JanFebMarAprMayJunJulAugSepOctNovDec",
            "SunMonTueWedThuFriSat"
        };

        buffer_put (buffer_1small, names[field - 3] + (u - _slicd_fields[field].adjust) * 3, 3);
    }
    else
    {
        char buf[UINT_FMT];

        buf[uint_fmt (buf, u)] = '\0';
        buffer_puts (buffer_1small, buf);
    }
}

int
main (int argc, char * const argv[])
{
    PROG = "slicd-dump";
    unsigned int i, nb;
    int r;

    for (;;)
    {
        struct option longopts[] = {
            { "help",               no_argument,        NULL,   'h' },
            { "version",            no_argument,        NULL,   'V' },
            { NULL, 0, 0, 0 }
        };
        int c;

        c = getopt_long (argc, argv, "hV", longopts, NULL);
        if (c == -1)
            break;
        switch (c)
        {
            case 'h':
                dieusage (0);

            case 'V':
                slicd_die_version ();

            default:
                dieusage (1);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 1)
        dieusage (1);

    r = slicd_load (&slicd, argv[0]);
    if (r < 0)
        strerr_diefu3sys (2, "load compiled crontabs from '", argv[0], "'");

    nb = genalloc_len (slicd_job_t, &slicd.jobs);
    for (i = 0; i < nb; ++i)
    {
        slicd_job_t *job = &genalloc_s (slicd_job_t, &slicd.jobs)[i];
        const char *name[] = { "Minutes", "Hours", "Days", "Months", "Days of the week" };
        int field;

        buffer_puts (buffer_1small, "Job ");
        put_uint (i + 1, 0);
        buffer_puts (buffer_1small, "/");
        put_uint (nb, 0);
        buffer_puts (buffer_1small, ": ");
        buffer_puts (buffer_1small, job_str (job));
        buffer_puts (buffer_1small, "\n");

        for (field = 0; field < 5; ++field)
        {
            int first = _slicd_fields[field].adjust;
            int max = _slicd_fields[field].adjust + _slicd_fields[field].max;

            buffer_puts (buffer_1small, name[field]);
            buffer_puts (buffer_1small, ": ");
            r = slicd_job_first (job, field, first, max, 0);
            if (r > max)
                buffer_puts (buffer_1small, "*");
            else
            {
                int last = -1;
                int interval = 0;

                for (;;)
                {
                    r = slicd_job_first (job, field, first, max, 1);
                    if (r > max)
                    {
out:
                        if (interval)
                        {
                            buffer_puts (buffer_1small, "-");
                            put_uint (last, field);
                        }
                        break;
                    }

                    if (last < 0)
                        put_uint (r, field);
                    else if (r > last + 1)
                    {
                        if (interval)
                        {
                            buffer_puts (buffer_1small, "-");
                            put_uint (last, field);
                        }
                        interval = 0;
                        buffer_puts (buffer_1small, ",");
                        put_uint (r, field);
                    }
                    else
                        interval = 1;

                    last = r;
                    first = r + 1;
                    if (first > max)
                        goto out;
                }
            }
            buffer_puts (buffer_1small, "\n");
        }

        {
            slicd_dst_special_t dst = slicd_job_get_dst_special (job);

            if (dst)
            {
                buffer_puts (buffer_1small, "Special DST mode enabled");
                if (dst == SLICD_DST_ON_DEACTIVATION)
                    buffer_puts (buffer_1small, " on deactivation only");
                else if (dst == SLICD_DST_ON_ACTIVATION)
                    buffer_puts (buffer_1small, " on activation only");
                buffer_puts (buffer_1small, ".\n");
            }
        }
        if (slicd_job_has_days_combo (job))
            buffer_puts (buffer_1small, "Note: Days Combo activated.\n");
        buffer_putsflush (buffer_1small, "\n");
    }

    slicd_free (&slicd);
    return 0;
}
