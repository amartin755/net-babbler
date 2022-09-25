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

#include <unistd.h>

#include "bug.hpp"
#include "socket.hpp"

cSocket::cSocket (int domain, int type, int protocol)
{
    m_fd = socket (domain, type, protocol);

    if (m_fd < 0)
    {
        throwException (errno);
    }
}

cSocket::cSocket (int fd)
{
    m_fd = fd;
}

cSocket::~cSocket ()
{
    if (m_fd >= 0)
        close (m_fd);
}

void cSocket::bind (const struct sockaddr *adr, socklen_t adrlen)
{
    int ret = ::bind (m_fd, adr, adrlen);
    if (ret)
        throwException (errno);
}

cSocket cSocket::accept (struct sockaddr *adr, socklen_t *adrlen)
{
    int ret = ::accept (m_fd, adr, adrlen);
    if (ret < 0)
    {
        throwException (errno);
    }

    return cSocket (ret);
}

ssize_t cSocket::recv (void *buf, size_t len, size_t atleast, int flags)
{
    BUG_ON (atleast > len);

    uint8_t* p = reinterpret_cast<uint8_t*>(buf);
    size_t received = 0;
    do
    {
        errno = 0;
        ssize_t ret = ::recv (m_fd, p, len - received, flags);
        if (ret <= 0)
        {
            throwException (errno);
        }
        received += ret;
        p += ret;

    } while (received < atleast);

    return received;
}

ssize_t cSocket::send (const void *buf, size_t len, int flags)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);
    ssize_t toBeSent = (ssize_t)len;
    do
    {
        ssize_t ret = ::send (m_fd, p, (size_t)toBeSent, flags | MSG_NOSIGNAL);
        if (ret < 0)
        {
            throwException (errno);
        }
        toBeSent -= ret;
        p += ret;

    } while (toBeSent > 0);

    return len;
}

void cSocket::throwException (int err)
{
    throw cSocketException (err);
}
