// SPDX-License-Identifier: GPL-3.0-only
/*
 * NET-BABBLER <https://github.com/amartin755/net-babbler>
 * Copyright (C) 2022 Andreas Martin (netnag@mailbox.org)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SIGNAL_HPP
#define SIGNAL_HPP

#include <unistd.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>


class cSignal
{
public:
    cSignal (int signal) : m_fd (-1)
    {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, signal);

        errno = 0;
        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
            throwError (errno);

        errno = 0;
        m_fd = signalfd (-1, &mask, 0);
        if (m_fd == -1)
            throwError (errno);
    }
    void wait ()
    {
        struct signalfd_siginfo fdsi;
        errno = 0;
        if (read (m_fd, &fdsi, sizeof (fdsi)) != sizeof (fdsi))
            throwError (errno);
    }
    operator int()
    {
        return m_fd;
    }

    ~cSignal ()
    {
        if (m_fd != -1)
            close (m_fd);
    }

private:
    void throwError (int err) const
    {
        char what[256]; what[0] = '\n';
        strerror_r (err, what, sizeof (what));
        throw std::runtime_error (what);
    }
    int m_fd;
};

#endif