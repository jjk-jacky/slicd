/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * errmsg.c
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

#include <slicd/err.h>

const char const *slicd_errmsg[_SLICD_NB_ERR] = {
    "",
    "I/O error",
    "Memory error",
    "Not empty",
    "Not in allowed range",
    "Invalid range",
    "Unknown name in field",
    "Syntax error",
    "Out of range days alongside days of the week",
    "Impossible date matching (e.g. Feb 31)",
    "No username specified",
    "No possible match (field without any selection)",
    "Invalid job data"
};
