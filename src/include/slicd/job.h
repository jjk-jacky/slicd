/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * job.h
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

#ifndef SLICD_JOB_H
#define SLICD_JOB_H

#include <slicd/slicd.h>

typedef struct
{
    unsigned int offset;
    unsigned char bits[17];
} slicd_job_t;

typedef enum
{
    SLICD_MINUTES = 0,
    SLICD_HOURS,
    SLICD_DAYS,
    SLICD_MONTHS,
    SLICD_DAYS_OF_WEEK,
    _SLICD_NB_FIELD
} slicd_field_t;
#define SLICD_DOW   SLICD_DAYS_OF_WEEK

extern int slicd_job_has            (slicd_job_t    *job,
                                     slicd_field_t   field,
                                     int             which);

extern int slicd_job_has_days_combo (slicd_job_t    *job);

extern int slicd_job_set_days_combo (slicd_job_t    *job,
                                     int             what);

extern int slicd_job_has_dst_special(slicd_job_t    *job);

extern int slicd_job_set_dst_special(slicd_job_t    *job,
                                     int             what);

extern int slicd_job_clearset       (slicd_job_t    *job,
                                     slicd_field_t   field,
                                     int             from,
                                     int             to,
                                     int             what);
#define slicd_job_clear(job,field,from,to) \
    slicd_job_clearset (job, field, from, to, 0)
#define slicd_job_set(job,field,from,to) \
    slicd_job_clearset (job, field, from, to, 1)

extern int slicd_job_first          (slicd_job_t    *job,
                                     slicd_field_t   field,
                                     int             from,
                                     int             to,
                                     int             what);


extern int slicd_job_ensure_valid   (slicd_job_t    *job);

#endif /* SLICD_JOB_H */
