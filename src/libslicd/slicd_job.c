/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd_job.c
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

#include <assert.h>
#include <errno.h>
#include <skalibs/bitarray.h>
#include <slicd/job.h>
#include <slicd/fields.h>
#include <slicd/err.h>


#define ensure_in_range(field,n) \
    if (n < 0 || n > _slicd_fields[field].max)  \
        return -SLICD_ERR_NOT_IN_RANGE

int
slicd_job_has (slicd_job_t    *job,
               slicd_field_t   field,
               int             which)
{
    assert (job != NULL);
    assert (field <= _SLICD_NB_FIELD);

    if (_slicd_fields[field].adjust)
        --which;
    ensure_in_range (field, which);

    return bitarray_isset (job->bits, _slicd_fields[field].offset + which);
}

int
slicd_job_has_days_combo (slicd_job_t    *job)
{
    assert (job != NULL);
    return bitarray_isset (job->bits, _SLICD_BIT_DAYS_COMBO);
}

int
slicd_job_set_days_combo (slicd_job_t    *job,
                          int             what)
{
    assert (job != NULL);

    bitarray_clearsetn (job->bits, _SLICD_BIT_DAYS_COMBO, 1, what);
    return 0;
}

int
slicd_job_has_dst_special (slicd_job_t    *job)
{
    assert (job != NULL);
    return bitarray_isset (job->bits, _SLICD_BIT_DST_SPECIAL);
}

int
slicd_job_set_dst_special (slicd_job_t    *job,
                           int             what)
{
    assert (job != NULL);

    bitarray_clearsetn (job->bits, _SLICD_BIT_DST_SPECIAL, 1, what);
    return 0;
}

int
slicd_job_clearset (slicd_job_t    *job,
                    slicd_field_t   field,
                    int             from,
                    int             to,
                    int             what)
{
    assert (job != NULL);
    assert (field <= _SLICD_NB_FIELD);

    if (_slicd_fields[field].adjust)
    {
        --from;
        --to;
    }
    ensure_in_range (field, from);
    ensure_in_range (field, to);
    if (to < from)
        return -SLICD_ERR_INVALID_RANGE;

    bitarray_clearsetn (job->bits, _slicd_fields[field].offset + from, to - from + 1, what);
    return 0;
}

int
slicd_job_first (slicd_job_t    *job,
                 slicd_field_t   field,
                 int             from,
                 int             to,
                 int             what)
{
    int r;

    assert (job != NULL);
    assert (field <= _SLICD_NB_FIELD);

    if (_slicd_fields[field].adjust)
    {
        --from;
        --to;
    }
    ensure_in_range (field, from);
    ensure_in_range (field, to);
    if (to < from)
        return -SLICD_ERR_INVALID_RANGE;

    from += _slicd_fields[field].offset;
    to += _slicd_fields[field].offset;
    if (what)
        r = bitarray_firstset_skip (job->bits, to + 1, from);
    else
        r = bitarray_firstclear_skip (job->bits, to + 1, from);

    return _slicd_fields[field].adjust + r - _slicd_fields[field].offset;
}
