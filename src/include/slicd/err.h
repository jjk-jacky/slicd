/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * err.h
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

#ifndef SLICD_ERR_H
#define SLICD_ERR_H

enum
{
    SLICD_ERR_SUCCESS = 0,
    SLICD_ERR_IO,
    SLICD_ERR_MEMORY,
    SLICD_ERR_NOT_EMPTY,
    SLICD_ERR_NOT_IN_RANGE,
    SLICD_ERR_INVALID_RANGE,
    SLICD_ERR_UNKNOWN_NAME_FIELD,
    SLICD_ERR_SYNTAX,
    SLICD_ERR_DAYS_COMBO,
    SLICD_ERR_IMPOSSIBLE_DATE,
    SLICD_ERR_NO_USERNAME,
    SLICD_ERR_NO_MATCH,
    SLICD_ERR_INVALID_JOB,
    _SLICD_NB_ERR
};

extern const char const *slicd_errmsg[_SLICD_NB_ERR];

#endif /* SLICD_ERR_H */
