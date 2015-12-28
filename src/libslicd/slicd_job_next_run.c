/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * slicd_job_next_run.c
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

#include <skalibs/bytestr.h>
#include <slicd/sched.h>

/* source: http://stackoverflow.com/a/11595914 */
#define is_leap(year) \
    ((year & 3) == 0 && ((year % 25) != 0 || (year & 15) == 0))

time_t
slicd_job_next_run (slicd_job_t *job, struct tm *next)
{
    int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    time_t time;
    int n;

    days[1] = is_leap (next->tm_year) ? 29 : 28;

month:
    n = slicd_job_first (job, SLICD_MONTHS, next->tm_mon + 1, 12, 1);
    if (n > 12)
    {
bump_year:
        ++next->tm_year;
        days[1] = is_leap (next->tm_year) ? 29 : 28;
        next->tm_mon = 0;
        next->tm_mday = 1;
        next->tm_hour = 0;
        next->tm_min = 0;
        goto month;
    }
    else if (n > next->tm_mon + 1)
    {
        next->tm_mon = n - 1;
        next->tm_mday = 1;
        next->tm_hour = 0;
        next->tm_min = 0;
    }

day:
    if (slicd_job_has_days_combo (job))
        goto hour;

    n = slicd_job_first (job, SLICD_DAYS, next->tm_mday, days[next->tm_mon], 1);
    if (n > days[next->tm_mon])
    {
bump_month:
        if (++next->tm_mon > 11)
            goto bump_year;
        next->tm_mday = 1;
        next->tm_hour = 0;
        next->tm_min = 0;
        goto month;
    }
    else if (n > next->tm_mday)
    {
        next->tm_mday = n;
        next->tm_hour = 0;
        next->tm_min = 0;
    }

hour:
    n = slicd_job_first (job, SLICD_HOURS, next->tm_hour, 23, 1);
    if (n > 23)
    {
bump_day:
        if (++next->tm_mday > days[next->tm_mon])
            goto bump_month;
        next->tm_hour = 0;
        next->tm_min = 0;
        goto day;
    }
    else if (n > next->tm_hour)
    {
        next->tm_hour = n;
        next->tm_min = 0;
    }

/* minute: */
    n = slicd_job_first (job, SLICD_MINUTES, next->tm_min, 59, 1);
    if (n > 59)
    {
        if (++next->tm_hour > 23)
            goto bump_day;
        next->tm_min = 0;
        goto hour;
    }
    else if (n > next->tm_min)
        next->tm_min = n;

    /* get tm_wday, plus return value on success */
    time = mktime (next);
    if (time == (time_t) -1)
        return time;

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
