/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * parser.h
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

#ifndef SLICD_PARSER_H
#define SLICD_PARSER_H

#include <slicd/slicd.h>

extern int slicd_add_job_from_cronline (slicd_t     *slicd,
                                        const char  *username,
                                        const char  *cronline);

#endif /* SLICD_PARSER_H */
