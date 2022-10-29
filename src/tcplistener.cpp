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
#include <sys/types.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>

#include "tcplistener.hpp"
#include "console.hpp"
#include "socket.hpp"
#include "protocol.hpp"

cTcpListener::cTcpListener (bool ipv4Only, bool ipv6Only, uint16_t localPort, unsigned socketBufSize)
    : m_terminate(false), m_listenerThread (nullptr),
      m_inetFamily (ipv4Only ? AF_INET : (ipv6Only ? AF_INET6 : AF_UNSPEC)),
      m_localPort (localPort), m_socketBufSize (socketBufSize)
{
    BUG_ON (ipv4Only && ipv6Only);
    m_listenerThread = new std::thread (&cTcpListener::listenerThreadFunc, this);
}

cTcpListener::~cTcpListener ()
{
    if (m_listenerThread)
    {
        m_listenerThread->join ();
        delete m_listenerThread;
    }

    for (auto thread : m_connThreads)
    {
        thread->join ();
        delete thread;
    }
}

void cTcpListener::listenerThreadFunc ()
{
    int sListener;
    const int enable = 1;
    struct sockaddr_storage address;
    struct sockaddr_in*  addr4 = (struct sockaddr_in*)&address;
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&address;


    sListener = socket (m_inetFamily == AF_INET ? AF_INET : AF_INET6, SOCK_STREAM, 0);
    if (sListener < 0)
    {
        Console::PrintError ("Could not create client socket (%s)\n", std::strerror(errno));
        return;
    }
    if (setsockopt (sListener, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
    {
        Console::PrintError ("Could not set socket options (%s)\n", std::strerror(errno));
        goto err_out;
    }
    if (m_inetFamily == AF_INET6 &&
        setsockopt (sListener, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(enable)))
    {
        Console::PrintError ("Could not set socket options (%s)\n", std::strerror(errno));
        goto err_out;
    }

    std::memset (&address, 0, sizeof (address));
    if (m_inetFamily == AF_INET)
    {
        addr4->sin_family      = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY;
        addr4->sin_port        = htons (m_localPort);
    }
    else
    {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr   = in6addr_any;
        addr6->sin6_port   = htons (m_localPort);
    }

    if (bind (sListener, (struct sockaddr *) &address, sizeof (address)))
    {
        Console::PrintError ("Could not bind port %d (%s)\n", m_localPort, std::strerror(errno));
        goto err_out;
    }

    if (listen (sListener, 50))
    {
        Console::PrintError ("Could not listen (%s)\n", std::strerror(errno));
        goto err_out;
    }

    while (!m_terminate)
    {
        int sConn;
        std::memset (&address, 0, sizeof(address));
        socklen_t addrLen = sizeof (address);
        sConn = accept (sListener, (struct sockaddr *)&address, &addrLen);

        if (sConn >= 0)
        {
            char ip[INET6_ADDRSTRLEN];
            Console::PrintVerbose ("Client %s:%u connected to tcp port %u\n",
                address.ss_family == AF_INET ?
                    inet_ntop (address.ss_family, &addr4->sin_addr, ip, addrLen) :
                    inet_ntop (address.ss_family, &addr6->sin6_addr, ip, addrLen),
                (unsigned)ntohs (addr6->sin6_port), (unsigned)m_localPort);

            m_connThreads.push_back (new std::thread (&cTcpListener::connectionThreadFunc, this, sConn));
        }
        else //if (errno == EINTR)
            break;
    }

    Console::PrintDebug ("tcp listener terminated\n");

err_out:
    close (sListener);
}


void cTcpListener::connectionThreadFunc (int sockfd)
{
    try
    {
        ssize_t ret = 1;
        cSocket sock (sockfd);
        cResponder responder (sock, m_socketBufSize);

        while (!m_terminate && ret > 0)
        {
            responder.doJob ();
        }
    }
    catch (const cSocket::errorException& e)
    {
            Console::PrintError   ("%s\n", e.what());
    }
    Console::PrintDebug ("tcp connection terminated\n");
}