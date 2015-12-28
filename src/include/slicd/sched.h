/*
 * slicd - Copyright (C) 2015 Olivier Brunel
 *
 * sched.h
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

#ifndef SLICD_SCHED_H
#define SLICD_SCHED_H

#include <time.h>
#include <slicd/job.h>

extern time_t slicd_job_next_run (slicd_job_t   *job,
                                  struct tm     *next);

#endif /* SLICD_SCHED_H */
