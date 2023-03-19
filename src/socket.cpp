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
#include "console.hpp"

std::mutex cSocket::cHandle::m_lock;
std::map<int, unsigned> cSocket::cHandle::m_fdRefs;


cSocket::cSocket () : m_fd (-1), m_timeout_ms (-1)
{
    initPoll (-1);
}

cSocket::cSocket (cSocket&& obj) 
    : m_fd (std::move(obj.m_fd))
{
    std::memcpy (&m_pollfd, &obj.m_pollfd, sizeof (m_pollfd));
    m_timeout_ms = obj.m_timeout_ms;
}

/*
 * supported protcols (domain, type, protocol):
 *
 * tcp: AF_INET/AF_INET6, SOCK_STREAM, 0
 * udp: AF_INET/AF_INET6, SOCK_DGRAM, 0
 * raw IP: AF_INET/AF_INET6, SOCK_RAW, proto
 * sctp: AF_INET/AF_INET6, SOCK_STREAM/SOCK_SEQPACKET, IPPROTO_SCTP
 * dccp: AF_INET/AF_INET6, SOCK_DCCP, IPPROTO_DCCP
 */
cSocket::cSocket (int domain, int type, int protocol, int timeout)
    : m_fd (-1), m_timeout_ms (timeout)
{
    m_fd = socket (domain, type, protocol);

    if (!m_fd.valid())
    {
        throw errorException (errno);
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
}

cSocket& cSocket::operator= (cSocket&& obj)
{
    std::memcpy (&m_pollfd, &obj.m_pollfd, sizeof (m_pollfd));
    m_timeout_ms = obj.m_timeout_ms;
    m_fd         = std::move(obj.m_fd);

    return *this;
}

cSocket cSocket::clone () const
{
    cSocket theClone (m_fd, m_timeout_ms);
    std::memcpy (&theClone.m_pollfd, &m_pollfd, sizeof (m_pollfd));

    return theClone;
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

cSocket cSocket::connect (const Properties& prop, const std::string& node, uint16_t remotePort,
        uint16_t localPort)
{
    std::list <cSocket::info> r;
    cSocket::getaddrinfo (node, remotePort, prop.family(), prop.type(), prop.protocol(), r);
    for (const auto& addrInfo : r)
    {
        cSocket s (addrInfo.family, addrInfo.socktype, addrInfo.protocol);

        if (localPort)
        {
            struct sockaddr_storage address;
            struct sockaddr_in*  addr4 = (struct sockaddr_in*)&address;
            struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&address;
            std::memset (&address, 0, sizeof (address));
            if (prop.family() == AF_INET)
            {
                addr4->sin_family      = AF_INET;
                addr4->sin_addr.s_addr = INADDR_ANY;
                addr4->sin_port        = htons (localPort);
            }
            else
            {
                addr6->sin6_family = AF_INET6;
                addr6->sin6_addr   = in6addr_any;
                addr6->sin6_port   = htons (localPort);
            }
            s.bind ((struct sockaddr *) &address, sizeof (address));
        }

        if (!s.connect ((sockaddr*)&addrInfo.addr, addrInfo.addrlen))
            continue;

        return s; // success
    }
    return cSocket(); // error
}

cSocket cSocket::listen (const Properties& prop, uint16_t port, int backlog)
{
    int domain = prop.family();
    // AF_UNSPEC means IPv4 AND IPv6
    cSocket sListener (domain == AF_UNSPEC ? AF_INET6 : AF_INET, prop.type(), prop.protocol());

    sListener.enableOption (SOL_SOCKET, SO_REUSEADDR);
    if (domain == AF_INET6)
    {
        sListener.enableOption (IPPROTO_IPV6, IPV6_V6ONLY);
    }

    struct sockaddr_storage address;
    struct sockaddr_in*  addr4 = (struct sockaddr_in*)&address;
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&address;
    std::memset (&address, 0, sizeof (address));
    if (domain == AF_INET)
    {
        addr4->sin_family      = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY;
        addr4->sin_port        = htons (port);
    }
    else
    {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr   = in6addr_any;
        addr6->sin6_port   = htons (port);
    }

    sListener.bind ((struct sockaddr *) &address, sizeof (address));
    if (!prop.isConnectionless() && ::listen (sListener.m_fd, backlog))
    {
        throw errorException (errno);
    }
    return sListener;
}

void cSocket::bind (const struct sockaddr *adr, socklen_t adrlen)
{
    int ret = ::bind (m_fd, adr, adrlen);
    if (ret)
        throw errorException (errno);
}

cSocket cSocket::accept (std::string& addr, uint16_t& port)
{
    struct sockaddr_storage address;
    std::memset (&address, 0, sizeof(address));
    socklen_t addrLen = sizeof (address);

    int ret = ::accept (m_fd, (struct sockaddr *)&address, &addrLen);
    if (ret < 0)
    {
        throw errorException (errno);
    }
    addr = inet_ntop ((struct sockaddr *)&address);
    port = ntohs (((struct sockaddr_in6*)&address)->sin6_port);

    return cSocket (ret, m_timeout_ms);
}

bool cSocket::connect (const struct sockaddr *adr, socklen_t adrlen) noexcept
{
    return ::connect (m_fd, adr, adrlen) == 0;
}

ssize_t cSocket::recv (void *buf, size_t len, size_t atleast,
    struct sockaddr * src_addr, socklen_t * addrlen)
{
    BUG_ON (atleast > len);

    uint8_t* p = reinterpret_cast<uint8_t*>(buf);
    size_t received = 0;
    do
    {
        int pollret = poll (m_pollfd, 2, m_timeout_ms);
        if (pollret < 0)
        {
            throw errorException (errno);
        }
        else if (pollret == 0)
        {
            throw errorException ("Receive timeout");
        }

        // connection terminated
        if (m_pollfd[0].revents & (POLLERR | POLLHUP))
        {
            throw errorException (ECONNRESET);
        }

        // data received
        if (m_pollfd[0].revents & POLLIN)
        {
            /*
             We don't want to block here because we are using poll to be able to
             react on timeouts and termination requests.
             recvfrom will only block when two or more threads are sharing a socket. 
             When data arrives both are unblocked (poll returns) and the first thread that calls 
             recfrom will get the data. The second thread will be blocked by recfrom because there
             is no more data to receive.
             */
            ssize_t ret = ::recvfrom (m_fd, p, len - received, MSG_DONTWAIT, src_addr, addrlen);
            if (ret <= 0)
            {
                // in case recfrom would block we ignore it and continue. 
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                    throw errorException (ret == 0 ? ECONNRESET : errno);
            }
            else
            {
                received += ret;
                p += ret;
            }
        }

        // termination request
        if (m_pollfd[1].revents & POLLIN)
        {
            throw eventException ();
        }
    } while (received < atleast);

    return received;
}

ssize_t cSocket::send (const void *buf, size_t len,
    const struct sockaddr *dest_addr, socklen_t addrlen)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);
    ssize_t toBeSent = (ssize_t)len;
    do
    {
        ssize_t ret = ::sendto (m_fd, p, (size_t)toBeSent, MSG_NOSIGNAL, dest_addr, addrlen);
        if (ret < 0)
        {
            throw errorException (errno);
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
    result.clear ();

    int s = ::getaddrinfo (node.c_str (), std::to_string(remotePort).c_str(), &hints, &res);
    if (s != 0)
    {
        throw errorException (gai_strerror(s));
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
        throw errorException (errno);
    }

    out << inet_ntop((struct sockaddr *) &addr) << ":" << ntohs(((struct sockaddr_in6*)&addr)->sin6_port);
    return out.str();
}

std::string cSocket::getpeername ()
{
    std::ostringstream out;
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);

    if (::getpeername(m_fd, (struct sockaddr *) &addr, &len))
    {
        throw errorException (errno);
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
        throw errorException (errno);
    }
    return ret;
}

void cSocket::enableOption (int level, int optname)
{
    const int enable = 1;
    if (setsockopt (m_fd, level, optname, &enable, sizeof(enable)))
    {
        throw errorException (errno);
    }
}

cSocket::Properties::Properties (int family, int type, int protocol)
: m_family (family), m_type (type), m_protocol (protocol)
{
}

cSocket::Properties::Properties () : Properties(toFamily (true, true), SOCK_STREAM, 0)
{
}

cSocket::Properties cSocket::Properties::tcp (bool ipv4, bool ipv6)
{
    Properties obj (toFamily (ipv4, ipv6), SOCK_STREAM, 0);
    return obj;
}

cSocket::Properties cSocket::Properties::udp (bool ipv4, bool ipv6)
{
    Properties obj (toFamily (ipv4, ipv6), SOCK_DGRAM, 0);
    return obj;
}

cSocket::Properties cSocket::Properties::sctp (bool ipv4, bool ipv6)
{
    Properties obj (toFamily (ipv4, ipv6), SOCK_STREAM, IPPROTO_SCTP);
    return obj;
}

cSocket::Properties cSocket::Properties::dccp (bool ipv4, bool ipv6)
{
    Properties obj (toFamily (ipv4, ipv6), SOCK_DCCP, IPPROTO_DCCP);
    return obj;
}

cSocket::Properties cSocket::Properties::raw (uint8_t protocol, bool ipv4, bool ipv6)
{
    Properties obj (toFamily (ipv4, ipv6), SOCK_RAW, (int)protocol);
    return obj;
}

void cSocket::Properties::setIpFamily (bool ipv4, bool ipv6)
{
    m_family = toFamily (ipv4, ipv6);
}

const char* cSocket::Properties::toString () const
{
    switch (m_type)
    {
    case SOCK_STREAM:
        return m_protocol ? "sctp" : "tcp";
    case SOCK_DGRAM:
        return "udp";
    case SOCK_DCCP:
        return "dccp";
    case SOCK_RAW:
        return "ip";
    }
    BUG ("unkown protocol");
    return "";
}

int cSocket::Properties::toFamily (bool ipv4, bool ipv6)
{
    BUG_ON (!ipv4 && !ipv6);
    if (ipv4 && ipv6)
        return  AF_UNSPEC;
    return  ipv4 ? AF_INET : AF_INET6;
}

bool cSocket::Properties::isConnectionless () const
{
    return !(m_type == SOCK_STREAM || m_type == SOCK_DCCP);
}
