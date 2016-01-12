/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * fields.h
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

#ifndef SLICD_FIELDS_H
#define SLICD_FIELDS_H

/* slicd_job_t contains a bitarray where bits are stored as follow:
 * 60   minutes (0-59)
 * 24   hours (0-23)
 * 31   days (0-30)
 * 12   months (0-11)
 *  7   days of week (0-6; 0=sunday, 1=monday, etc)
 *  1   days combo: set when both days & dow are set, i.e. the Nth dow
 *
 * 135 bits total; hence an array of 17 char-s (leaving 1 bit unused)
 *
 * Both DAYS & DOW set means that DAYS is limited to 0-6 which stands for the
 * 1st, 2nd, ... till the 5th & then last DOW of the month.
 */
#define _SLICD_BITS_OFFSET_MINUTES      0
#define _SLICD_BITS_OFFSET_HOURS        _SLICD_BITS_OFFSET_MINUTES + 60
#define _SLICD_BITS_OFFSET_DAYS         _SLICD_BITS_OFFSET_HOURS + 24
#define _SLICD_BITS_OFFSET_MONTHS       _SLICD_BITS_OFFSET_DAYS + 31
#define _SLICD_BITS_OFFSET_DOW          _SLICD_BITS_OFFSET_MONTHS + 12
#define _SLICD_BIT_DAYS_COMBO           _SLICD_BITS_OFFSET_DOW + 7

static struct
{
    unsigned int adjust : 1; /* i.e. -1 to what is given, e.g. days are 1-31 but stored as 0-30 */
    unsigned int max    : 8;
    unsigned int offset : 23;
} _slicd_fields[] = {
    { 0, 59, _SLICD_BITS_OFFSET_MINUTES },
    { 0, 23, _SLICD_BITS_OFFSET_HOURS },
    { 1, 30, _SLICD_BITS_OFFSET_DAYS },
    { 1, 11, _SLICD_BITS_OFFSET_MONTHS },
    { 0, 6,  _SLICD_BITS_OFFSET_DOW }
};

#endif /* SLICD_FIELDS_H */
