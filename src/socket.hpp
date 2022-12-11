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

#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <netdb.h>

#include <stdexcept>
#include <string>
#include <list>
#include <string>
#include <cstring>

#include "event.hpp"


// tcp: AF_INET/AF_INET6, SOCK_STREAM, 0
// udp: AF_INET/AF_INET6, SOCK_DGRAM, 0
// raw IP: AF_INET/AF_INET6, SOCK_RAW, proto
// sctp: AF_INET/AF_INET6, SOCK_STREAM/SOCK_SEQPACKET, IPPROTO_SCTP
// dccp: AF_INET/AF_INET6, SOCK_DCCP, IPPROTO_DCCP

class cSocket
{
public:
    // no copy constructor and copy operator, we only allow move semantic
    // because file descriptors owned by cSocket will automatically
    // be closed in destructor
    cSocket (const cSocket&) = delete;
    cSocket& operator=(const cSocket&) = delete;

    cSocket (cSocket&&);
    ~cSocket ();
    cSocket& operator= (cSocket&& obj);

    class Properties;
    static cSocket connect (const Properties& properties, const std::string& node,
        uint16_t remotePort, uint16_t localPort);
    static cSocket listen (const Properties& properties, uint16_t port,
        int backlog);

    cSocket accept (std::string& addr, uint16_t& port);
    ssize_t recv (void *buf, size_t len, size_t atleast = 0,
        struct sockaddr * src_addr = nullptr, socklen_t * addrlen = nullptr);
    ssize_t send (const void *buf, size_t len,
        const struct sockaddr *dest_addr = nullptr, socklen_t addrlen = 0);

    // get local address and port of socket
    std::string getsockname ();
    // get remote address and port of socket
    std::string getpeername ();
    static std::string inet_ntop (const struct sockaddr* addr);
    void setCancelEvent (cEvent& eventCancel);
    bool isValid () const {return m_fd >= 0;}

    // expeptions thrown by cSocket
    class eventException : public std::exception
    {
    public:
        eventException ()
        {
        }
    };
    class errorException : public std::exception
    {
    public:
        errorException (int err) : m_err(err)
        {
            const char* ret = strerrordesc_np (m_err);
            if (ret)
                m_what = ret;
        }
        errorException (const char* what)
        {
            m_what = what;
        }
        const char* what() const noexcept
        {
            return m_what.c_str();
        }

    private:
        std::string m_what;
        int m_err;
    };

private:
    cSocket ();
    cSocket (int domain, int type, int protocol, int timeout = -1);
    cSocket (int fd, int timeout = -1);
    void initPoll (int evfd);
    void enableOption (int level, int optname);
    struct info
    {
        info (const struct addrinfo& info)
        {
            family = info.ai_family;
            socktype = info.ai_socktype;
            protocol = info.ai_protocol;
            addrlen = info.ai_addrlen;
            std::memcpy (&addr, info.ai_addr, addrlen);
        }
        int              family;
        int              socktype;
        int              protocol;
        socklen_t        addrlen;
        struct sockaddr_storage  addr;
    };
    static void getaddrinfo (const std::string& node, uint16_t remotePort,
        int family, int sockType, int protocol, std::list<info> &result);
    void bind (const struct sockaddr *adr, socklen_t adrlen);
    bool connect (const struct sockaddr *adr, socklen_t adrlen) noexcept;

private:
    int m_fd;
    struct pollfd m_pollfd[2]; // 0: socket fd, 1: event fd
    int m_timeout_ms;

public:
    class Properties
    {
    public:
        Properties ();
        static Properties tcp (bool ipv4 = true, bool ipv6 = true);
        static Properties udp (bool ipv4 = true, bool ipv6 = true);
        static Properties sctp (bool ipv4 = true, bool ipv6 = true);
        static Properties dccp (bool ipv4 = true, bool ipv6 = true);
        static Properties raw (uint8_t protocol, bool ipv4 = true, bool ipv6 = true);
        void setIpFamily (bool ipv4, bool ipv6);
        int family () const
        {
            return m_family;
        }
        int type () const
        {
            return m_type;
        }
        int protocol () const
        {
            return m_protocol;
        }
        const char* toString () const;

    private:
        Properties (int family, int type, int protocol);
        static int toFamily (bool ipv4, bool ipv6);
        int m_family;
        int m_type;
        int m_protocol;
    };
};

#include <mutex>
#include <map>
class cSockDescr
{
public:
    cSockDescr (int fd) : m_fd (fd)
    {
        if (m_fd >= 0)
        {
            m_lock.lock();
            ++m_fdRefs[fd];
            m_lock.unlock();
        }
    }
    ~cSockDescr ()
    {
        if (m_fd >= 0)
        {
            m_lock.lock();
            unsigned refs = --m_fdRefs[m_fd];
            if (!refs)
                close (m_fd);
            m_lock.unlock();
        }
    }
private:
    int m_fd;
    static std::mutex m_lock;
    static std::map<int, unsigned> m_fdRefs;
};


#endif