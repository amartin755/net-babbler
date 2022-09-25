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

cTcpListener::cTcpListener (uint16_t localPort)
    : m_terminate(false), m_listenerThread (nullptr), m_localPort (localPort)
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
            Console::PrintVerbose ("Client %s connected to tcp port %d\n",
                inet_ntop (address.sin_family, &address.sin_addr, ip, addrLen),
                ntohs (address.sin_port));

            m_connThreads.push_back (new std::thread (&cTcpListener::connectionThreadFunc, this, sConn));
        }
        else //if (errno == EINTR)
            break;
    }

    Console::PrintDebug ("tcp listener terminated\n");

err_out:
    close (sListener);
}

struct header
{
    uint32_t type;
    uint32_t length; // 0 means, nothing follows
    uint64_t seq;
    uint32_t crc;
    uint32_t options;
//    uint8_t data[];
    
    bool isRequest ()
    {
        return type == 0xaaffffee;
    }
    bool initResponse (uint32_t sequence, uint32_t payloadLength)
    {
        type    = 0xaaffffee;
        length  = payloadLength + sizeof (*this) - (sizeof (length) - sizeof (type));
        seq     = sequence;
        crc     = 0;
        options = 0;
    }
    bool isResponse ()
    {
        return type == 0xeeffffaa;
    }

};

void cTcpListener::connectionThreadFunc (int sockfd)
{
    ssize_t ret = 1;
    while (!m_terminate && ret > 0)
    {
        uint8_t data[2048];
        ssize_t rxOffset = 0;

        // first try to at least receive the header
        do
        {
            ret = recv (sockfd, &data[rxOffset], sizeof(data), 0);
            if (ret > 0)
            {
                rxOffset += ret;
            }
        } while (rxOffset < sizeof (header));

        // is this our header?
        header* h = (header*)data;
        if (h->isRequest())
        {
            uint32_t len = h->length;
            uint64_t seq = h->seq;
            uint32_t respLen = h->options;
            ssize_t toBeReceived = len + xxx - rxOffset;

            // receive the remaining part of the message
            while (toBeReceived > 0)
            {
                ret = recv (sockfd, data, sizeof(data), 0);
                if (ret > 0)
                {
                    toBeReceived -= ret;
                }
            }
            if (toBeReceived < 0)
            {
                Console::PrintVerbose ("Warning: chunk received\n");
            }
            h->initResponse (seq, respLen);
        }






        do
        {
            ret = recv (sockfd, &data[rxOffset], sizeof(data), 0);
            if (ret > 0)
            {
                rxOffset += ret;
            }
        } while (rxOffset < sizeof (header));


        uint8_t data[2048];
        ssize_t rxOffset = 0;
        ret = recv (sockfd, data, sizeof(data), 0);

        if (ret > 0)
        {
            Console::PrintDebug ("-- %d\n", ret);
            ret = send (sockfd, data, 1024*1024*1024, 0);
            Console::PrintDebug ("++ %d\n", ret);
        }
        else if (ret == 0)
        {
            Console::PrintVerbose ("Client connection terminated\n");
        }
        else
        {
            Console::PrintError ("Receive error (%s)\n", std::strerror(errno));
        }
    }
out:
    close (sockfd);
    Console::PrintDebug ("tcp connection terminated\n");
}