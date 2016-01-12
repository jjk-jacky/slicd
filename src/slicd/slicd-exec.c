/*
 * slicd - Copyright (C) 2016 Olivier Brunel
 *
 * slicd-exec.c
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

#include "slicd/config.h"

#include <getopt.h>
#include <errno.h>
#include <skalibs/uint.h>
#include <skalibs/uint32.h>
#include <skalibs/sig.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/iopause.h>
#include <skalibs/selfpipe.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/skamisc.h>
#include <skalibs/strerr2.h>
#include <slicd/die.h>

enum
{
    RC_OK = 0,
    RC_SYNTAX,
    RC_IO,
    RC_MEMORY
};

enum
{
    TYPE_DIRECT,
    TYPE_USERNAME,
    TYPE_COMBINE,
    TYPE_COMBINE_LAST,
    TYPE_PARTIAL,
    TYPE_PARTIAL_LAST
};

struct arg
{
    unsigned char type;
    char *str;
    int len;
};

struct child
{
    pid_t pid;
    int fd_out;
    int fd_err;
    stralloc sa_out;
    stralloc sa_err;
};

typedef void (*process_line_fn) (int fd, char *line);

static genalloc ga_iop = GENALLOC_ZERO;
static genalloc ga_child = GENALLOC_ZERO;
static int exiting = 0;
static int looping = 1;
static stralloc sa_in = STRALLOC_ZERO;
static struct arg *args;
static int len_args;
static int n_args;

#define ga_remove(type, ga, i)     do {         \
    int len = (ga)->len / sizeof (type);        \
    int c = len - (i) - 1;                      \
    if (c > 0)                                  \
        memmove (genalloc_s (type, (ga)) + (i), genalloc_s (type, (ga)) + (i) + 1, c * sizeof (type)); \
    genalloc_setlen (type, (ga), len - 1);    \
} while (0)

static void close_fd (int fd);
static void process_line (int i_c, int fd, char *line, int len);

static inline void
start_exiting (void)
{
    if (exiting)
        return;
    exiting = 1;
    close_fd (0);
    if (genalloc_len (struct child, &ga_child) == 0)
        looping = 0;
}

static void
iop_remove_fd (int fd)
{
    int i;

    for (i = 0; i < genalloc_len (iopause_fd, &ga_iop); ++i)
    {
        if (genalloc_s (iopause_fd, &ga_iop)[i].fd == fd)
        {
            ga_remove (iopause_fd, &ga_iop, i);
            break;
        }
    }
}

static inline int
get_child_from_fd (int fd)
{
    int i;

    for (i = 0; i < genalloc_len (struct child, &ga_child); ++i)
        if (genalloc_s (struct child, &ga_child)[i].fd_out == fd
                || genalloc_s (struct child, &ga_child)[i].fd_err == fd)
            return i;
    return -1;
}

static void
close_fd (int fd)
{

    fd_close (fd);
    iop_remove_fd (fd);

    if (fd == 0)
        start_exiting ();
    else
    {
        struct child *child;
        int i;

        i = get_child_from_fd (fd);
        if (i < 0)
        {
            char buf[UINT32_FMT];
            uint32 u;

            u = (uint32) fd;
            buf[uint32_fmt (buf, u)] = '\0';
            strerr_warnwu2x ("to find child for fd#", buf);
            return;
        }
        child = &genalloc_s (struct child, &ga_child)[i];

        if (fd == child->fd_out)
        {
            if (child->sa_out.len > 0)
                process_line (i, fd, child->sa_out.s, child->sa_out.len);
            child->fd_out = -1;
        }
        else
        {
            if (child->sa_err.len > 0)
                process_line (i, fd, child->sa_err.s, child->sa_err.len);
            child->fd_err = -1;
        }
    }
}

static void
new_child (char *line, int len)
{
    char buf[UINT32_FMT];
    uint32 u;
    int p_int[2];
    int p_out[2];
    int p_err[2];
    char c;
    pid_t pid;
    int n;

    n = byte_chr (line, len, ':');
    if (n == 0 || n >= len - 1)
    {
        strerr_warnw3x ("invalid data on stdin: '", line, "'");
        return;
    }

    if (pipecoe (p_int) < 0)
    {
        strerr_warnwu3sys ("fork for '", line, "': failed to set up pipes");
        return;
    }
    if (pipe (p_out) < 0)
    {
        strerr_warnwu3sys ("fork for '", line, "': failed to set up pipes");

        fd_close (p_int[0]);
        fd_close (p_int[1]);
        return;
    }
    else if (ndelay_on (p_out[0]) < 0 || coe (p_out[0]) < 0)
    {
        strerr_warnwu3sys ("fork for '", line, "': failed to set up pipes");

        fd_close (p_int[0]);
        fd_close (p_int[1]);
        fd_close (p_out[0]);
        fd_close (p_out[1]);
        return;
    }
    if (pipe (p_err) < 0)
    {
        strerr_warnwu3sys ("fork for '", line, "': failed to set up pipes");

        fd_close (p_int[0]);
        fd_close (p_int[1]);
        fd_close (p_out[0]);
        fd_close (p_out[1]);
        return;
    }
    else if (ndelay_on (p_err[0]) < 0 || coe (p_err[0]) < 0)
    {
        strerr_warnwu3sys ("fork for '", line, "': failed to set up pipes");

        fd_close (p_int[0]);
        fd_close (p_int[1]);
        fd_close (p_out[0]);
        fd_close (p_out[1]);
        fd_close (p_err[0]);
        fd_close (p_err[1]);
        return;
    }

    pid = fork ();
    if (pid < 0)
    {
        strerr_warnwu3sys ("fork for '", line, "'");

        fd_close (p_int[0]);
        fd_close (p_int[1]);
        fd_close (p_out[0]);
        fd_close (p_out[1]);
        fd_close (p_err[0]);
        fd_close (p_err[1]);
        return;
    }
    else if (pid == 0)
    {
        char * argv[n_args + 2];
        stralloc sa = STRALLOC_ZERO;
        int i;

        PROG = "slicd-exec (child)";

        argv[n_args] = line + n + 1;
        line[n] = '\0';
        n = 0;
        for (i = 0; i < len_args; ++i)
        {
            switch (args[i].type)
            {
                case TYPE_DIRECT:
                    argv[n] = args[i].str;
                    ++n;
                    break;

                case TYPE_USERNAME:
                    argv[n] = line;
                    ++n;
                    break;

                case TYPE_COMBINE:
                case TYPE_COMBINE_LAST:
                    stralloc_catb (&sa, args[i].str, args[i].len);
                    stralloc_cats (&sa, line);
                    if (args[i].type == TYPE_COMBINE_LAST)
                        ++n;
                    break;

                case TYPE_PARTIAL:
                case TYPE_PARTIAL_LAST:
                    stralloc_catb (&sa, args[i].str, args[i].len);
                    if (args[i].type == TYPE_PARTIAL_LAST)
                        ++n;
                    break;
            }
        }
        argv[++n] = NULL;

        selfpipe_finish ();
        fd_close (p_int[0]);
        fd_close (p_out[0]);
        fd_close (p_err[0]);

        fd_close (1);
        fd_close (2);
        if (fd_move2 (1, p_out[1], 2, p_err[1]) < 0)
        {
            u = (uint32) errno;
            fd_write (p_int[1], "p", 1);
            uint32_pack (buf, u);
            fd_write (p_int[1], buf, 4);
            errno = (int) u;
            strerr_diefu1sys (RC_IO, "set up pipes");
        }

        pathexec ((char const * const *) argv);
        /* if it fails... */
        u = (uint32) errno;
        fd_write (p_int[1], "e", 1);
        uint32_pack (buf, u);
        fd_write (p_int[1], buf, 4);
        errno = (int) u;
        strerr_dieexec (RC_IO, argv[0]);
    }

    {
        struct child child = { 0, };
        iopause_fd iop;

        child.pid = pid;
        child.fd_out = p_out[0];
        child.fd_err = p_err[0];
        genalloc_append (struct child, &ga_child, &child);

        iop.fd = child.fd_out;
        iop.events = IOPAUSE_READ;
        genalloc_append (iopause_fd, &ga_iop, &iop);

        iop.fd = child.fd_err;
        iop.events = IOPAUSE_READ;
        genalloc_append (iopause_fd, &ga_iop, &iop);
    }

    buffer_putsnoflush (buffer_1small, "Forked PID#");
    u = (uint32) pid;
    buf[uint32_fmt (buf, u)] = '\0';
    buffer_putsnoflush (buffer_1small, buf);
    buffer_putsnoflush (buffer_1small, " for job '");
    buffer_putsnoflush (buffer_1small, line);
    buffer_putsflush (buffer_1small, "'\n");

    fd_close (p_int[1]);
    fd_close (p_out[1]);
    fd_close (p_err[1]);
    switch (fd_read (p_int[0], &c, 1))
    {
        case 0:     /* it worked */
            break;

        case 1:     /* child failed to exec */
            {
                char b[4];
                uint32 e = 0;

                if (fd_read (p_int[0], b, 4) == 4)
                    uint32_unpack (b, &e);

                if (e > 0)
                {
                    errno = e;
                    strerr_warnw4sys ("PID#", buf, ": failed to ",
                            (c == 'p') ? "set up pipes" : "exec");
                }
                else
                    strerr_warnw4x ("PID#", buf, ": failed to ",
                            (c == 'p') ? "set up pipes" : "exec");
            }
            break;

        case -1:    /* internal failure */
            strerr_warnw3sys ("PID#", buf, ": failed to read internal pipe");
            break;
    }
    fd_close (p_int[0]);
}

static void
process_line (int i_c, int fd, char *line, int len)
{
    struct child *child = &genalloc_s (struct child, &ga_child)[i_c];
    char buf[UINT32_FMT];
    uint32 u;

    u = (uint32) child->pid;
    buf[uint32_fmt (buf, u)] = '\0';

    buffer_putsnoflush (buffer_1small, "PID#");
    buffer_putsnoflush (buffer_1small, buf);
    buffer_putsnoflush (buffer_1small, " ");
    buffer_putsnoflush (buffer_1small, (fd == child->fd_out) ? "stdout" : "stderr");
    buffer_putsnoflush (buffer_1small, ": ");
    buffer_putnoflush (buffer_1small, line, len);
    buffer_putsflush (buffer_1small, "\n");
}

static void
handle_fd (int fd, int all)
{
    stralloc *sa;
    int i_c = 0; /* to silence warning */
    int r;
    int close = 0;

    if (fd == 0)
        sa = &sa_in;
    else
    {
        i_c = get_child_from_fd (fd);
        if (i_c < 0)
        {
            char buf[UINT32_FMT];
            uint32 u;

            u = (uint32) fd;
            buf[uint32_fmt (buf, u)] = '\0';
            strerr_warnwu3x ("to find child for fd#", buf, "; closing");

            fd_close (fd);
            iop_remove_fd (fd);
            return;
        }
        if (fd == genalloc_s (struct child, &ga_child)[i_c].fd_out)
            sa = &genalloc_s (struct child, &ga_child)[i_c].sa_out;
        else
            sa = &genalloc_s (struct child, &ga_child)[i_c].sa_err;
    }

    for (;;)
    {
        if (!stralloc_readyplus (sa, 1024))
            strerr_diefu1sys (RC_MEMORY, "allocate memory");

        r = sanitize_read (fd_read (fd, sa->s + sa->len, 1024));
        if (r < 0)
        {
            if (errno != EPIPE)
            {
                char buf[UINT32_FMT];
                uint32 u;
                int e = errno;

                close_fd (fd);
                errno = e;

                u = (uint32) fd;
                buf[uint32_fmt (buf, u)] = '\0';
                if (fd == 0)
                    strerr_warnwu1sys ("read from stdin");
                else
                {
                    struct child *child = &genalloc_s (struct child, &ga_child)[i_c];
                    char buf[UINT32_FMT];
                    uint32 u;

                    u = (uint32) child->pid;
                    buf[uint32_fmt (buf, u)] = '\0';
                    strerr_warnwu4sys ("read from ",
                            (fd == child->fd_out) ? "stdout" : "stderr",
                            " of PID#", buf);
                }
                return;
            }
            close = 1;
        }
        else
            sa->len += r;

        while (sa->len > 0)
        {
            int l;

            r = byte_chr (sa->s, sa->len, '\n');
            if (r == sa->len)
                break;

            sa->s[r] = '\0';
            if (fd == 0)
                new_child (sa->s, r);
            else
                process_line (i_c, fd, sa->s, r);
            l = sa->len - r - 1;
            if (l > 0)
                byte_copy (sa->s, l, sa->s + r + 1);
            sa->len = l;
        }

        if (!all || close)
            break;
    }

    if (close)
        close_fd (fd);
}

static inline void
handle_sigchld (void)
{
    for (;;)
    {
        struct child *child;
        char buf[20 + UINT32_FMT];
        uint32 u;
        int wstat;
        int r;
        int i;

        r = wait_nohang (&wstat);
        if (r < 0)
        {
            if (errno != ECHILD)
                strerr_warnwu1sys ("reap zombies (waitpid)");
            break;
        }
        else if (r == 0)
            break;

        u = (uint32) r;
        buf[20 + uint32_fmt (buf + 20, u)] = '\0';

        for (i = 0; i < genalloc_len (struct child, &ga_child); ++i)
            if (genalloc_s (struct child, &ga_child)[i].pid == r)
                break;
        if (i >= genalloc_len (struct child, &ga_child))
        {
            strerr_warnwu2x ("find child for reaped PID#", buf + 20);
            continue;
        }
        child = &genalloc_s (struct child, &ga_child)[i];

        if (child->fd_out > 0)
            handle_fd (child->fd_out, 1);
        if (child->fd_err > 0)
            handle_fd (child->fd_err, 1);
        stralloc_free (&child->sa_out);
        stralloc_free (&child->sa_err);
        ga_remove (struct child, &ga_child, i);

        if (WIFEXITED (wstat))
        {
            byte_copy (buf, 9, "exitcode ");
            buf[9 + uint32_fmt (buf + 9, (uint32) WEXITSTATUS (wstat))] = '\0';
        }
        else
        {
            const char *name;

            name = sig_name (WTERMSIG (wstat));
            byte_copy (buf, 10, "signal SIG");
            byte_copy (buf + 10, strlen (name) + 1, name);
        }

        buffer_putsnoflush (buffer_1small, "PID#");
        buffer_putsnoflush (buffer_1small, buf + 20);
        buffer_putsnoflush (buffer_1small, " reaped, ");
        buffer_putsnoflush (buffer_1small, buf);
        buffer_putsflush (buffer_1small, "\n");
    }

    if (exiting && genalloc_len (struct child, &ga_child) == 0)
        looping = 0;
}

static void
handle_signals (void)
{
    /* signals */
    for (;;)
    {
        char c;

        c = selfpipe_read ();
        switch (c)
        {
            case -1:
                strerr_diefu1sys (RC_IO, "selfpipe_read");

            case 0:
                return;

            case SIGINT:
            case SIGTERM:
                start_exiting ();
                break;

            case SIGCHLD:
                handle_sigchld ();
                break;

            default:
                strerr_dief1x (RC_IO, "internal error: invalid selfpipe_read value");
        }
    }
    return;
}

static int
parse_arg (struct arg *arg, char *s, int len, int in_arg)
{
    int r;

    r = byte_chr (s, len, '%');
    if (r >= len - 1)
    {
        if (!in_arg)
            arg->type = TYPE_DIRECT;
        else
        {
            arg->type = TYPE_PARTIAL_LAST;
            arg->len = len;
        }
        arg->str = s;
    }
    else if (r == 0 && len == 2 && s[1] == 'u')
    {
        if (!in_arg)
            arg->type = TYPE_USERNAME;
        else
        {
            arg->type = TYPE_COMBINE_LAST;
            arg->str = s;
            arg->len = r;
        }
    }
    else if (r == len - 2)
    {
        if (in_arg || (s[r + 1] == 'u' || s[r + 1] == '%'))
        {
            if (s[r + 1] == 'u')
            {
                arg->type = TYPE_COMBINE_LAST;
                arg->len = r;
            }
            else
            {
                arg->type = TYPE_PARTIAL_LAST;
                arg->len = r + ((!in_arg || s[r + 1] == '%') ? 1 : 2);
            }
        }
        else
            arg->type = TYPE_DIRECT;

        arg->str = s;
    }
    else
    {
        if (s[r + 1] == 'u')
        {
            arg->type = TYPE_COMBINE;
            arg->len = r;
        }
        else
        {
            arg->type = TYPE_PARTIAL;
            arg->len = r + ((s[r + 1] == '%') ? 1 : 2);
        }

        arg->str = s;
        return r + 2;
    }

    return 0;
}

static void
dieusage (int rc)
{
    slicd_die_usage (rc, "[OPTION...] ARG...",
            " -R, --ready-fd FD             Prints LF ('\\n') on FD once ready\n"
            " -h, --help                    Show this help screen and exit\n"
            " -V, --version                 Show version information and exit\n"
            );
}

int
main (int argc, char * const argv[])
{
    PROG = "slicd-exec";
    unsigned int fd_ready = 0;
    int r;

    for (;;)
    {
        struct option longopts[] = {
            { "help",               no_argument,        NULL,   'h' },
            { "ready-fd",           required_argument,  NULL,   'R' },
            { "version",            no_argument,        NULL,   'V' },
            { NULL, 0, 0, 0 }
        };
        int c;

        c = getopt_long (argc, argv, "hR:V", longopts, NULL);
        if (c == -1)
            break;
        switch (c)
        {
            case 'h':
                dieusage (RC_OK);

            case 'R':
                if (!uint0_scan (optarg, &fd_ready) || fd_ready <= 2)
                    dieusage (RC_SYNTAX);
                break;

            case 'V':
                slicd_die_version ();

            default:
                dieusage (RC_SYNTAX);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0)
        dieusage (RC_SYNTAX);

    {
        iopause_fd iop;
        sigset_t set;

        iop.fd = selfpipe_init ();
        if (iop.fd < 0)
            strerr_diefu1sys (RC_IO, "init selfpipe");
        iop.events = IOPAUSE_READ;
        genalloc_append (iopause_fd, &ga_iop, &iop);

        sigemptyset (&set);
        sigaddset (&set, SIGTERM);
        sigaddset (&set, SIGINT);
        sigaddset (&set, SIGCHLD);
        if (selfpipe_trapset (&set) < 0)
            strerr_diefu1sys (RC_IO, "trap signals");

        iop.fd = 0;
        iop.events = IOPAUSE_READ;
        genalloc_append (iopause_fd, &ga_iop, &iop);

        if (ndelay_on (0) < 0)
            strerr_diefu1sys (RC_IO, "set stdin non-blocking");
    }

    tain_now_g ();

    {
        struct arg _args[argc];
        genalloc ga_args = GENALLOC_ZERO;
        int in_arg = 0;
        int i;

        n_args = argc;
        args = _args;
        for (i = 0; i < argc; ++i)
        {
            struct arg arg;
            char *s = argv[i];
            int len = strlen (s);

again:
            r = parse_arg (&arg, s, len, in_arg);

            if (arg.type != TYPE_COMBINE && arg.type != TYPE_PARTIAL)
            {
                in_arg = 0;
                if (args)
                    byte_copy (&_args[i], sizeof (struct arg), &arg);
                else
                    genalloc_append (struct arg, &ga_args, &arg);
                continue;
            }

            in_arg = 1;
            if (args)
            {
                if (!genalloc_ready (struct arg, &ga_args, argc + 1))
                    strerr_diefu1sys (RC_MEMORY, "allocate memory parsing args");
                if (i > 0)
                    byte_copy (ga_args.s, sizeof (struct arg) * i, args);
                genalloc_setlen (struct arg, &ga_args, i);

                args = NULL;
            }

            genalloc_append (struct arg, &ga_args, &arg);

            s += r;
            len -= r;
            goto again;
        }
        if (args)
            len_args = argc;
        else
        {
            args = genalloc_s (struct arg, &ga_args);
            len_args = genalloc_len (struct arg, &ga_args);
        }

        if (fd_ready > 0)
        {
            if (fd_write (fd_ready, "\n", 1) < 0)
                strerr_warnwu1sys ("announce readiness");
            fd_close (fd_ready);
        }

        while (looping)
        {
            r = iopause_g (genalloc_s (iopause_fd, &ga_iop),
                    genalloc_len (iopause_fd, &ga_iop), NULL);
            if (r < 0)
                strerr_diefu1sys (RC_IO, "iopause");

            for (i = genalloc_len (iopause_fd, &ga_iop) - 1; i > 0; --i)
            {
                iopause_fd *iop = &genalloc_s (iopause_fd, &ga_iop)[i];

                if (iop->revents & IOPAUSE_READ)
                    handle_fd (iop->fd, 0);
                else if (iop->revents & IOPAUSE_EXCEPT)
                    close_fd (iop->fd);
            }

            if (genalloc_s (iopause_fd, &ga_iop)[0].revents & IOPAUSE_READ)
                handle_signals ();
            else if (genalloc_s (iopause_fd, &ga_iop)[0].revents & IOPAUSE_EXCEPT)
                strerr_dief1sys (RC_IO, "trouble with selfpipe");
        }

        genalloc_free (struct arg, &ga_args);
    }

    stralloc_free (&sa_in);
    genalloc_free (iopause_fd, &ga_iop);
    genalloc_free (struct child, &ga_child);

    return RC_OK;
}
