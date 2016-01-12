/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd-sched.c
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
#include <errno.h>
#include <sys/timerfd.h>
#include <skalibs/uint.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/selfpipe.h>
#include <skalibs/tai.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <slicd/slicd.h>
#include <slicd/sched.h>
#include <slicd/err.h>
#include <slicd/die.h>

enum
{
    RC_OK = 0,
    RC_SYNTAX,
    RC_IO,
    RC_MEMORY,
    RC_NO_CRONTABS,
    RC_OTHER
};

enum
{
    DO_NOTHING = 0,
    DO_EXIT,
    DO_RELOAD,
    DO_RESET
};

static slicd_t slicd = { 0, };

#define job_str(job)        (slicd.str.s + job->offset)

static int
handle_signals (const char *file)
{
    /* signals */
    for (;;)
    {
        char c;

        c = selfpipe_read ();
        switch (c)
        {
            case -1:
                strerr_diefu1sys (RC_IO, "selfpipe_read");

            case 0:
                break;

            case SIGHUP:
                {
                    slicd_t new = { 0, };
                    int r;

                    r = slicd_load (&new, file);
                    if (r < 0)
                        strerr_warnwu3sys ("reload compiled crontabs from '", file, "'");
                    else
                    {
                        slicd_free (&slicd);
                        byte_copy (&slicd, sizeof (slicd), &new);
                        strerr_warn3x ("Compiled crontabs reloaded from '", file, "'");
                        return DO_RELOAD;
                    }
                }
                break;

            case SIGUSR1:
                return DO_RESET;

            case SIGINT:
            case SIGTERM:
                return DO_EXIT;

            default:
                strerr_dief1x (RC_IO, "internal error: invalid selfpipe_read value");
        }
    }
    return DO_NOTHING;
}

static void
dieusage (int rc)
{
    slicd_die_usage (rc, "[OPTION...] FILE",
            " -R, --ready-fd FD             Prints LF ('\\n') on FD once ready\n"
            " -h, --help                    Show this help screen and exit\n"
            " -V, --version                 Show version information and exit\n"
            );
}

int
main (int argc, char * const argv[])
{
    PROG = "slicd-sched";
    struct itimerspec its = { 0, };
    iopause_fd iop[2];
    sigset_t set;
    unsigned int fd_ready = 0;
    int r;

    for (;;)
    {
        struct option longopts[] = {
            { "help",               no_argument,        NULL,   'h' },
            { "ready-fd",           required_argument,  NULL,   'R' },
            { "version",            no_argument,        NULL,   'V' },
            { NULL, 0, 0, 0 }
        };
        int c;

        c = getopt_long (argc, argv, "hR:V", longopts, NULL);
        if (c == -1)
            break;
        switch (c)
        {
            case 'h':
                dieusage (RC_OK);

            case 'R':
                if (!uint0_scan (optarg, &fd_ready) || fd_ready <= 2)
                    dieusage (RC_SYNTAX);
                break;

            case 'V':
                slicd_die_version ();

            default:
                dieusage (RC_SYNTAX);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 1)
        dieusage (RC_SYNTAX);

    r = slicd_load (&slicd, argv[0]);
    if (r < 0)
        strerr_warnwu3sys ("load compiled crontabs from '", argv[0], "'");

    if (ndelay_on (1) < 0)
        strerr_diefu1sys (RC_IO, "set stdout non-blocking");

    iop[0].fd = selfpipe_init ();
    if (iop[0].fd < 0)
        strerr_diefu1sys (RC_IO, "init selfpipe");
    iop[0].events = IOPAUSE_READ;

    sigemptyset (&set);
    sigaddset (&set, SIGTERM);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGHUP);
    sigaddset (&set, SIGUSR1);
    if (selfpipe_trapset (&set) < 0)
        strerr_diefu1sys (RC_IO, "trap signals");

    iop[1].fd = timerfd_create (CLOCK_REALTIME, TFD_NONBLOCK);
    if (iop[1].fd < 0)
        strerr_diefu1sys (RC_OTHER, "create timerfd");
    iop[1].events = IOPAUSE_READ;
    /* force processing jobs before the first iopause */
    iop[1].revents = IOPAUSE_READ;

    tain_now_g ();

    if (fd_ready > 0)
    {
        if (fd_write (fd_ready, "\n", 1) < 0)
            strerr_warnwu1sys ("announce readiness");
        fd_close (fd_ready);
    }

    for (;;)
    {
        if (iop[1].revents & IOPAUSE_READ)
        {
            struct tm cur_tm, next_tm, since_tm, *tm;
            time_t cur_time, next_time, since_time;
            int i;

            if (clock_gettime (CLOCK_REALTIME, &its.it_interval) < 0)
                strerr_diefu1sys (RC_OTHER, "get current time");
            cur_time = its.it_interval.tv_sec;
            its.it_interval.tv_sec = 0;
            its.it_interval.tv_nsec = 0;

            tm = localtime (&cur_time);
            if (!tm)
                strerr_diefu1x (RC_OTHER, "break down time");
            byte_copy (&cur_tm, sizeof (cur_tm), tm);
            cur_time -= cur_tm.tm_sec;
            cur_tm.tm_sec = 0;

            since_time = cur_time;
            byte_copy (&since_tm, sizeof (since_tm), &cur_tm);

            /* is there a timerfd set? i.e. did we wait for some time, if so we
             * might need to handle some time changes.
             * Any time change shouldn't affect us, since our timerfd is based
             * on CLOCK_REALTIME and simply "adjust" as needed. But, time might
             * have jumped forwad and we "missed" our deadline...
             * We'll also account for the case of the time going back right
             * after our timerfd expired, just in case.
             */
            if (its.it_value.tv_sec > 0)
            {
                /* post-DO_RELOAD, wait for next minute to avoid rerunning jobs */
                if (its.it_value.tv_nsec > 0)
                {
                    its.it_value.tv_nsec = 0;
                    next_time = cur_time + 60;
                    goto timer;
                }
                else if (cur_time > its.it_value.tv_sec)
                {
                    /* we're ahead of the plan, we need to trigger anything that
                     * we "missed" */
                    since_time = its.it_value.tv_sec;
                    tm = localtime (&since_time);
                    if (!tm)
                        strerr_diefu1x (RC_OTHER, "break down time");
                    byte_copy (&since_tm, sizeof (since_tm), tm);
                }
                else if (cur_time < its.it_value.tv_sec)
                {
                    /* we're early somehow; just wait it out... */
                    goto pause;
                }
            }

            next_time = (time_t) -1;
            for (i = 0; i < genalloc_len (slicd_job_t, &slicd.jobs); ++i)
            {
                slicd_job_t *job = &genalloc_s (slicd_job_t, &slicd.jobs)[i];
                struct tm job_tm;
                time_t job_time;

                byte_copy (&job_tm, sizeof (job_tm), &since_tm);
next_run:
                job_time = slicd_job_next_run (job, &job_tm);
                if (job_time == (time_t) -1)
                {
                    strerr_warnwu3x ("get next run for job '", job_str (job), "'");
                    continue;
                }

                if (job_time <= cur_time)
                {
                    buffer_puts (buffer_1small, job_str (job));
                    if (!buffer_putsflush (buffer_1small, "\n"))
                        strerr_diefu1sys (RC_IO, "write to stdout");

                    /* now calculate the "actual" next run, using cur_tm in case
                     * since_tm was set earlier (time jump) */
                    byte_copy (&job_tm, sizeof (job_tm), &cur_tm);
                    ++job_tm.tm_min;
                    /* call mktime() to update job_tm, as one more minute
                     * might be a new hour, day, etc */
                    if (mktime (&job_tm) == (time_t) -1)
                    {
                        strerr_warnwu3x ("get next run for job '", job_str (job), "'");
                        continue;
                    }
                    goto next_run;
                }
                else if (next_time == (time_t) -1 || job_time < next_time)
                {
                    byte_copy (&next_tm, sizeof (next_tm), &job_tm);
                    next_time = job_time;
                }
            }

timer:
            its.it_value.tv_sec = (next_time == (time_t) -1) ? 0 : next_time;
            r = timerfd_settime (iop[1].fd, TFD_TIMER_ABSTIME, &its, NULL);
            if (r < 0)
                strerr_diefu1sys (RC_OTHER, "set timerfd");
        }
        else if (iop[1].revents & IOPAUSE_EXCEPT)
            strerr_dief1sys (RC_IO, "trouble with timerfd");

pause:
        r = iopause_g (iop, 2, NULL);
        if (r < 0)
            strerr_diefu1sys (RC_IO, "iopause");

        if (iop[0].revents & IOPAUSE_READ)
        {
            r = handle_signals (argv[0]);
            if (r == DO_RELOAD)
            {
                iop[1].revents = IOPAUSE_READ;
                its.it_value.tv_sec = 1;
                its.it_value.tv_nsec = 1;
            }
            else if (r == DO_RESET)
            {
                iop[1].revents = IOPAUSE_READ;
                its.it_value.tv_sec = 0;
            }
            else if (r == DO_EXIT)
                break;
        }
        else if (iop[0].revents & IOPAUSE_EXCEPT)
            strerr_dief1sys (RC_IO, "trouble with selfpipe");
    }

    slicd_free (&slicd);
    return RC_OK;
}
