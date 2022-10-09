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

#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <stdexcept>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <sys/poll.h>

// tcp: AF_INET/AF_INET6, SOCK_STREAM, 0
// udp: AF_INET/AF_INET6, SOCK_DGRAM, 0
// raw IP: AF_INET/AF_INET6, SOCK_RAW, proto
// sctp: AF_INET/AF_INET6, SOCK_STREAM/SOCK_SEQPACKET, IPPROTO_SCTP
// dccp: AF_INET/AF_INET6, SOCK_DCCP, IPPROTO_DCCP

class cSocket
{
public:
    // no default and copy constructor
/*    cSocket () = delete;
    cSocket (const cSocket&) = delete;
    cSocket& operator= (const cSocket&) = delete;
    cSocket (cSocket&&) = default;
*/
    cSocket (int domain, int type, int protocol, int evfd = -1, int timeout = -1);
    cSocket (int fd, int evfd = -1, int timeout = -1);
    ~cSocket ();
    void bind (const struct sockaddr *adr, socklen_t adrlen);
    cSocket accept (struct sockaddr * adr, socklen_t * adrlen);
    ssize_t recv (void *buf, size_t len, size_t atleast = 0, int flags = 0);
    ssize_t send (const void *buf, size_t len, int flags = 0);

private:
    void throwException (int err);
    void initPoll ();

private:
    int m_fd;
    int m_evfd;
    struct pollfd m_pollfd[2]; // 0: socket fd, 1: event fd
    int m_timeout_ms;
};

class cSocketException : public std::exception
{
public:
    cSocketException (int err) : m_err(err)
    {
        if (m_err)
            strerror_r (m_err, m_what, sizeof (m_what));
    }
    cSocketException (const char* what, bool isError)
    {
        m_err = isError ? -1 : 0;
        std::strncpy (m_what, what, sizeof(m_what));
    }
    const char* what() const noexcept
    {
        return m_what;
    }
    bool isError () const noexcept
    {
        return m_err != 0;
    }

private:
    char m_what[256];
    int m_err;
};

#endif