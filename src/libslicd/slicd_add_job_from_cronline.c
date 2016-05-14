/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd_add_job_from_cronline.c
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

#include <errno.h>
#include <slicd/slicd.h>
#include <slicd/parser.h>
#include <slicd/job.h>
#include <slicd/fields.h>
#include <slicd/err.h>


static const char *names[_SLICD_NB_FIELD] = {
    NULL,
    NULL,
    NULL,
    "JanFebMarAprMayJunJulAugSepOctNovDec",
    "SunMonTueWedThuFriSat",
};

#define is_blank(c) \
    (c == ' ' || c == '\t')
#define is_num(c) \
    (c >= '0' && c <= '9')
#define skip_blanks(s) \
    for ( ; *s != '\0' && is_blank (*s); ++s) ;
#define read_int(s,n) \
    do {                            \
        n = 0;                      \
        for (;;)                    \
        {                           \
            if (!is_num (*s))       \
                break;              \
            n *= 10;                \
            n += *s - '0';          \
            ++s;                    \
        }                           \
    } while (0)
#define in_interval(n,f,t) \
    (n >= f && n <= t)

static int
parse_field_names (slicd_field_t field, const char **s)
{
    const char *n = names[field];
    int max = _slicd_fields[field].max * 3;
    int i = 'A' - 'a';
    int p = 0;

    for (p = 0; p <= max; p += 3)
    {
        if ( ((*s)[0] == n[p] || (*s)[0] == n[p] - i)
                && ((*s)[1] == n[p + 1] || (*s)[1] == n[p + 1] + i)
                && ((*s)[2] == n[p + 2] || (*s)[2] == n[p + 2] + i) )
        {
            *s += 3;
            return (p / 3) + _slicd_fields[field].adjust;
        }
    }

    return -1;
}

static int
parse_interval (slicd_job_t *job, slicd_field_t field, const char **s)
{
    int min = _slicd_fields[field].adjust;
    int max = min + _slicd_fields[field].max;
    int from, to, step;

    if (field == SLICD_HOURS && **s == '$')
    {
        slicd_job_set_dst_special (job, 1);
        ++*s;
    }

again:
    if (**s == '*')
    {
        from = min;
        to = max;
        ++*s;
    }
    else
    {
        if (is_num (**s))
            read_int (*s, from);
        else if (names[field])
        {
            from = parse_field_names (field, s);
            if (from < 0)
                return -SLICD_ERR_UNKNOWN_NAME_FIELD;
        }
        else
            return -SLICD_ERR_SYNTAX;

        if (!in_interval (from, min, max))
            return -SLICD_ERR_NOT_IN_RANGE;

        if (**s == '-')
        {
            ++*s;
            if (is_num (**s))
                read_int (*s, to);
            else if (names[field])
            {
                to = parse_field_names (field, s);
                if (to < 0)
                    return -SLICD_ERR_UNKNOWN_NAME_FIELD;
            }
            else
                return -SLICD_ERR_SYNTAX;

            if (!in_interval (to, from, max))
                return -SLICD_ERR_NOT_IN_RANGE;
        }
        else
            to = from;

        if (!is_blank (**s) && **s != '/' && **s != ',')
            return -SLICD_ERR_SYNTAX;
    }
    if (**s == '/')
    {
        ++*s;
        if (!is_num (**s))
            return -SLICD_ERR_SYNTAX;

        read_int (*s, step);
    }
    else
        step = 1;

    if (!is_blank (**s) && **s != ',')
        return -SLICD_ERR_SYNTAX;

    if (from == min && to == max && step == 1)
        slicd_job_set (job, field, from, to);
    else
        for ( ; from <= to; from += step)
            slicd_job_set (job, field, from, from);

    if (**s == ',')
    {
        ++*s;
        goto again;
    }
    skip_blanks (*s);

    return 0;
}

int
slicd_add_job_from_cronline (slicd_t     *slicd,
                             const char  *username,
                             const char  *cronline)
{
    slicd_job_t j = { 0, };
    const char *s = cronline;
    int len;
    int r;
    int i;

    if (!s)
        return -SLICD_ERR_SYNTAX;

    skip_blanks (s);
    if (*s == '\0' || *s == '#')
        return 0;

    for (i = 0; i < 5; ++i)
    {
        r = parse_interval (&j, i, &s);
        if (r < 0)
            return r;
    }

    r = slicd_job_ensure_valid (&j);
    if (r < 0)
        return r;

    if (username)
        i = strlen (username);
    else
    {
        username = s;
        for (i = 0; *s != '\0' && !is_blank (*s); ++s, ++i)
            ;
        skip_blanks (s);
    }
    if (i == 0)
        return -SLICD_ERR_NO_USERNAME;
    if (*s == '\0')
        return -SLICD_ERR_SYNTAX;

    for (len = strlen (s); is_blank (s[len - 1]); --len)
        ;

    if (!stralloc_readyplus (&slicd->str, i + 1 + len + 1)
            || !genalloc_readyplus (slicd_job_t, &slicd->jobs, 1))
        return -SLICD_ERR_MEMORY;

    j.offset = slicd->str.len;
    stralloc_catb (&slicd->str, username, i);
    stralloc_catb (&slicd->str, ":", 1);
    stralloc_catb (&slicd->str, s, len);
    stralloc_0 (&slicd->str);
    genalloc_append (slicd_job_t, &slicd->jobs, &j);

    return 1;
}
