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

cTcpListener::cTcpListener (uint16_t localPort, unsigned socketBufSize)
    : m_terminate(false), m_listenerThread (nullptr),
      m_localPort (localPort), m_socketBufSize (socketBufSize)
{
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
    struct sockaddr_in address;
    const int enable = 1;


    sListener = socket (AF_INET, SOCK_STREAM, 0);
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

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons (m_localPort);

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
                inet_ntop (address.sin_family, &address.sin_addr, ip, addrLen),
                (unsigned)ntohs (address.sin_port), (unsigned)m_localPort);

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
    catch (const cSocketException& e)
    {
        e.isError() ?
            Console::PrintError   ("%s\n", e.what()) :
            Console::PrintVerbose ("%s\n", e.what());
    }
    Console::PrintDebug ("tcp connection terminated\n");
}