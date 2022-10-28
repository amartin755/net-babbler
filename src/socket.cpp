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
#include <arpa/inet.h>
#include <sstream>

#include "bug.hpp"
#include "socket.hpp"

cSocket::cSocket () : m_fd (-1), m_timeout_ms (-1)
{
    initPoll (-1);
}

cSocket::cSocket (cSocket&& obj)
{
    *this = std::move(obj);
}

cSocket::cSocket (int domain, int type, int protocol, int timeout)
    : m_timeout_ms (timeout)
{
    m_fd = socket (domain, type, protocol);

    if (m_fd < 0)
    {
        throwException (errno);
    }

    initPoll (-1);
}

cSocket::cSocket (int fd, int timeout)
    : m_fd(fd), m_timeout_ms (timeout)
{
    initPoll (-1);
}

cSocket::~cSocket ()
{
    if (m_fd >= 0)
        close (m_fd);
}

cSocket& cSocket::operator= (cSocket&& obj)
{
    std::memcpy (&m_pollfd, &obj.m_pollfd, sizeof (m_pollfd));
    m_timeout_ms = obj.m_timeout_ms;
    m_fd         = obj.m_fd;

    obj.m_fd = -1;
    return *this;
}

void cSocket::setCancelEvent (cEvent& eventCancel)
{
    initPoll (eventCancel);
}

void cSocket::initPoll (int evfd)
{
    m_pollfd[0].fd = m_fd;
    m_pollfd[1].fd = evfd;
    m_pollfd[0].events = POLLIN;
    m_pollfd[1].events = POLLIN;
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

void cSocket::connect (const struct sockaddr *adr, socklen_t adrlen)
{
    int ret = ::connect (m_fd, adr, adrlen);
    if (ret)
        throwException (errno);
}

ssize_t cSocket::recv (void *buf, size_t len, size_t atleast, int flags)
{
    BUG_ON (atleast > len);

    uint8_t* p = reinterpret_cast<uint8_t*>(buf);
    size_t received = 0;
    do
    {
        errno = 0;
        int pollret = poll (m_pollfd, 2, m_timeout_ms);
        if (pollret < 0)
        {
            throwException (errno);
        }
        else if (pollret == 0)
        {
            throw cSocketException ("Receive timeout", true);
        }

        // data received
        if (m_pollfd[0].revents & POLLIN)
        {
            errno = 0;
            ssize_t ret = ::recv (m_fd, p, len - received, flags);
            if (ret <= 0)
            {
                throwException (errno);
            }
            received += ret;
            p += ret;
        }
        // termination request
        if (m_pollfd[1].revents & POLLIN)
            throw cSocketException ("", false);
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

void cSocket::getaddrinfo (const std::string& node, uint16_t remotePort,
    int family, int sockType, int protocol, std::list<info>& result)
{
    struct addrinfo hints;
    struct addrinfo *res, *rp;

    std::memset (&hints, 0, sizeof (hints));
    hints.ai_family   = family;
    hints.ai_socktype = sockType;
    hints.ai_protocol = protocol;

    int s = ::getaddrinfo (node.c_str (), std::to_string(remotePort).c_str(), &hints, &res);
    if (s != 0)
    {
        throwException (gai_strerror(s));
    }
    for (rp = res; rp != NULL; rp = rp->ai_next)
    {
        result.emplace_back (*rp);
    }
    freeaddrinfo (res);
}

std::string cSocket::getsockname ()
{
    std::ostringstream out;
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);

    if (::getsockname(m_fd, (struct sockaddr *) &addr, &len))
    {
        throwException (errno);
    }

    out << inet_ntop((struct sockaddr *) &addr) << ":" << ntohs(((struct sockaddr_in6*)&addr)->sin6_port);
    return out.str();
}

std::string cSocket::inet_ntop (const struct sockaddr* addr)
{
    const char* ret = "";
    if (addr->sa_family == AF_INET)
    {
        char ip[INET_ADDRSTRLEN];
        ret = ::inet_ntop(AF_INET, &((struct sockaddr_in *)addr)->sin_addr , ip, sizeof(ip));
    }
    if (addr->sa_family == AF_INET6)
    {
        char ip[INET6_ADDRSTRLEN];
        ret = ::inet_ntop(AF_INET6, &((struct sockaddr_in6 *)addr)->sin6_addr , ip, sizeof(ip));
    }

    if (!ret)
    {
        throwException (errno);
    }
    return ret;
}

void cSocket::throwException (int err)
{
    if (err)
        throw cSocketException (err);
    else
        throw cSocketException ("Connection terminated", false);
}

void cSocket::throwException (const char* err)
{
    throw cSocketException (err, true);
}
