/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd_job_ensure_valid.c
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

#include <slicd/job.h>
#include <slicd/fields.h>
#include <slicd/err.h>


int
slicd_job_ensure_valid (slicd_job_t *job)
{
    int i;

    for (i = 0; i < 5; ++i)
        if (slicd_job_first (job, i, _slicd_fields[i].adjust,
                    _slicd_fields[i].max + _slicd_fields[i].adjust, 1)
                > _slicd_fields[i].max + _slicd_fields[i].adjust)
            return -SLICD_ERR_NO_MATCH;

    /* special case: days combo */
    if (slicd_job_first (job, SLICD_DAYS_OF_WEEK, 0, 6, 0) <= 6)
    {
        /* there is a restriction on DOW, now let's check how the first 6 days
         * are set:
         * - all set    = same as '*', no restriction on DAYS
         * - none set   = invalid
         * - a mix      = DAYS_COMBO
         */

        if (slicd_job_first (job, SLICD_DAYS, 1, 6, 0) == 7)
        {
            slicd_job_set (job, SLICD_DAYS, 1, 31);
            slicd_job_set_days_combo (job, 0);
        }
        else if (slicd_job_first (job, SLICD_DAYS, 1, 6, 1) == 7)
            return -SLICD_ERR_DAYS_COMBO;
        else
            slicd_job_set_days_combo (job, 1);
    }
    else
        slicd_job_set_days_combo (job, 0);

    /* ensure date validity/coherence */
    if (slicd_job_first (job, SLICD_DAYS, 1, 29, 1) == 30)
    {
        /* none of the first 29 days are set, so we need to do some checking:
         * if day 30 is set, that there's a month other than Feb,
         * else (i.e. day 31 is set) that there's at least one matching month
         */
        if (slicd_job_has (job, SLICD_DAYS, 30))
        {
            if (!(slicd_job_has (job, SLICD_MONTHS, 1)
                        || slicd_job_first (job, SLICD_MONTHS, 3, 12, 1) <= 12))
                return -SLICD_ERR_IMPOSSIBLE_DATE;
        }
        else
        {
            if (!(slicd_job_has (job, SLICD_MONTHS, 1)
                        || slicd_job_has (job, SLICD_MONTHS, 3)
                        || slicd_job_has (job, SLICD_MONTHS, 5)
                        || slicd_job_has (job, SLICD_MONTHS, 7)
                        || slicd_job_has (job, SLICD_MONTHS, 8)
                        || slicd_job_has (job, SLICD_MONTHS, 10)
                        || slicd_job_has (job, SLICD_MONTHS, 12)))
                return -SLICD_ERR_IMPOSSIBLE_DATE;
        }
    }

    return 0;
}
