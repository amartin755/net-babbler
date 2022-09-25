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

#include <sys/socket.h>

#include "socket.hpp"

cSocket::cSocket (int domain, int type, int protocol)
{
    m_fd = socket (domain, type, protocol);

    if (m_fd < 0)
    {
        throwException ();
    }
}

cSocket::~cSocket ()
{
    if (m_fd >= 0)
        close (m_fd);
}

void cSocket::bind (const struct sockaddr *adr, socklen_t adrlen)
{
    int ret = bind (m_fd, adr, adrlen);
    if (ret)
        throwException ();
}

cSocket cSocket::accept (struct sockaddr *restrict adr, socklen_t *restrict adrlen)
{
    int ret = accept (m_fd, adr, adrlen);
    if (ret < 0)
    {
        throwException ();
    }

    return cSocket (ret);
}

cSocket::throwException ()
{

}
