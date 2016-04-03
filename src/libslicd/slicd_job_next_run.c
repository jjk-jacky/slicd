/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd_job_next_run.c
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

#include <skalibs/bytestr.h>
#include <slicd/sched.h>

#define ONE_MINUTE      60
#define ONE_HOUR        60 * ONE_MINUTE
#define ONE_DAY         24 * ONE_HOUR

/* source: http://stackoverflow.com/a/11595914 */
#define is_leap(year) \
    ((year & 3) == 0 && ((year % 25) != 0 || (year & 15) == 0))

static time_t
first_minute_of_dst (time_t time, struct tm *time_tm)
{
    int interval = 10 * ONE_DAY;
    int back = 1;

    for (;;)
    {
        struct tm *tm;

        if (back)
            time -= interval;
        else
            time += interval;

        tm = localtime (&time);
        if (!tm)
            return (time_t) -1;
        if (back)
        {
            if (tm->tm_isdst != time_tm->tm_isdst)
            {
                back = 0;
                if (interval == 10 * ONE_DAY)
                    interval = ONE_DAY;
                else
                    interval = ONE_MINUTE;
            }
        }
        else if (tm->tm_isdst == time_tm->tm_isdst)
        {
            back = 1;
            if (interval == ONE_DAY)
                interval = ONE_HOUR;
            else
            {
                memcpy (time_tm, tm, sizeof (struct tm));
                return time;
            }
        }
    }
}

time_t
slicd_job_next_run (slicd_job_t *job, struct tm *next)
{
    int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    time_t time;
    int dst = -1;
    int n;

again:
    /* set tm_wday & tm_isdst, plus return value on success */
    time = mktime (next);
    if (time == (time_t) -1)
        return time;
    else if (dst == -1)
        dst = next->tm_isdst;
    else if (next->tm_isdst != dst)
    {
        /* we changed DST state, set next to the first minute with this new DST
         * state, and move on from here (to properly handle DST changes, i.e.
         * skip the "unexisting" interval when jumping forward, or find both
         * times (w/ & w/out DST) when jumping backward, since that's what
         * actually happens */
        time = first_minute_of_dst (time, next);
        if (time == (time_t) -1)
            return time;
        dst = next->tm_isdst;
    }

    days[1] = is_leap (next->tm_year) ? 29 : 28;

month:
    n = slicd_job_first (job, SLICD_MONTHS, next->tm_mon + 1, 12, 1);
    if (n > 12)
    {
        ++next->tm_year;
        next->tm_mon = 0;
        next->tm_mday = 1;
        next->tm_hour = 0;
        next->tm_min = 0;
        /* we know nothing possibly matches in the remaining months of the year,
         * so a DST change doesn't matter and can be safely ignored */
        dst = -1;
        goto again;
    }
    else if (n > next->tm_mon + 1 || dst == -1)
    {
        next->tm_mon = n - 1;
        next->tm_mday = 1;
        next->tm_hour = 0;
        next->tm_min = 0;
        dst = -1;
        goto again;
    }

    if (!slicd_job_has_days_combo (job))
    {
day:
        n = slicd_job_first (job, SLICD_DAYS, next->tm_mday, days[next->tm_mon], 1);
        if (n > days[next->tm_mon])
        {
            ++next->tm_mon;
            dst = -1;
            if (next->tm_mon <= 11)
                goto month;
            else
                goto again;
        }
        else if (n > next->tm_mday || dst == -1)
        {
            next->tm_mday = n;
            next->tm_hour = 0;
            next->tm_min = 0;
            dst = -1;
            goto again;
        }
    }

    n = slicd_job_first (job, SLICD_HOURS, next->tm_hour, 23, 1);
    if (n > 23)
    {
bump_day:
        ++next->tm_mday;
        dst = -1;
        if (!slicd_job_has_days_combo (job) && next->tm_mday <= days[next->tm_mon])
            goto day;
        next->tm_hour = 0;
        next->tm_min = 0;
        goto again;
    }
    else if (n > next->tm_hour)
    {
        /* from here on, a DST change matters & must be accounted for. */
        next->tm_hour = n;
        next->tm_min = 0;
        goto again;
    }

    n = slicd_job_first (job, SLICD_MINUTES, next->tm_min, 59, 1);
    if (n > 59)
    {
        ++next->tm_hour;
        next->tm_min = 0;
        goto again;
    }
    else if (n > next->tm_min)
    {
        next->tm_min = n;
        goto again;
    }


    if (!slicd_job_has (job, SLICD_DOW, next->tm_wday))
        goto bump_day;

    if (slicd_job_has_days_combo (job))
    {
        struct tm first;
        int i;

        byte_copy (&first, sizeof (first), next);
        first.tm_mday = 1;
        /* get tm_wday */
        if (mktime (&first) == (time_t) -1)
            return (time_t) -1;

        /* let's get first.tm_mday to be the first next->tm_wday of the month */
        first.tm_mday = next->tm_wday;
        if (first.tm_mday < first.tm_wday)
            first.tm_mday += 7;
        first.tm_mday += 1 - first.tm_wday;

        for (i = 1; i < 6; ++i, first.tm_mday += 7)
            if (first.tm_mday == next->tm_mday)
            {
                if (slicd_job_has (job, SLICD_DAYS, i)
                        || (first.tm_mday + 7 > days[next->tm_mon]
                            && slicd_job_has (job, SLICD_DAYS, 6)))
                    break;
                else
                    goto bump_day;
            }
    }

    return time;
}
