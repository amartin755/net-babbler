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

#ifndef EVENT_HPP
#define EVENT_HPP

#include <unistd.h>
#include <sys/eventfd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>


class cEvent
{
public:
    cEvent (unsigned int initval = 0) : m_fd (-1)
    {
        errno = 0;
        m_fd = eventfd (initval, EFD_SEMAPHORE);
        if (m_fd == -1)
            throwError (errno);
    }
    void send (const uint64_t count = 1)
    {
        errno = 0;
        if (write (m_fd, &count, sizeof(count)) != sizeof (count))
            throwError (errno);
    }
    void wait ()
    {
        uint64_t count = 0;
        errno = 0;
        if (read (m_fd, &count, sizeof (count)) != sizeof (count))
            throwError (errno);
    }
    operator int()
    {
        return m_fd;
    }

    ~cEvent ()
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