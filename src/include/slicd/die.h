/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * die.h
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

#ifndef SLICD_DIE_H
#define SLICD_DIE_H

extern char const *PROG;

extern void slicd_die_usage (int rc, const char *usage, const char *details);
extern void slicd_die_version (void);

#endif /* SLICD_DIE_H */
