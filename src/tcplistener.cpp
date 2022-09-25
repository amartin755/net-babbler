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
    
    void initRequest(uint64_t sequence, uint32_t payloadLength, uint32_t respLength)
    {
        type     = htonl (0xaaffffee);
        length   = htonl (payloadLength + sizeof (*this));
        seq      = htobe64 (sequence);
        options  = htonl (respLength);
        checksum = htonl (calcChecksum ());
    }
    bool isRequest ()
    {
        return type == htonl(0xaaffffee);
    }
    void initResponse (uint64_t sequence, uint32_t payloadLength)
    {
        type     = htonl (0xeeffffaa);
        length   = htonl (payloadLength + sizeof (*this));
        seq      = htobe64 (sequence);
        options  = 0;
        checksum = htonl (calcChecksum ());
    }
    bool isResponse ()
    {
        return type == htonl(0xeeffffaa);
    }

    uint32_t calcChecksum ()
    {
        uint32_t sum = 0;
        uint8_t *p = reinterpret_cast<uint8_t*>(this);
        do
        {
            sum += *p;
        } while (p++ < (uint8_t*)&checksum);
        return sum;
    }

    bool checkChecksum ()
    {
        return ntohl (checksum) == calcChecksum ();
    }

    uint32_t getLength ()
    {
        return ntohl (length);
    }
    uint64_t getSequence ()
    {
        return be64toh (seq);
    }
    uint32_t getOptions ()
    {
        return ntohl (options);
    }

private:
    uint32_t type;
    uint32_t length; // whole packet length, including type and length --> min value is 8
    uint64_t seq;
    uint32_t options;
    uint32_t checksum;
//    uint8_t data[];
};

void cTcpListener::connectionThreadFunc (int sockfd)
{
    try
    {
        ssize_t ret = 1;
        cSocket sock (sockfd);

        while (!m_terminate && ret > 0)
        {
            uint8_t data[2048];

            // first try to at least receive the header
            ret = sock.recv (data, sizeof(data), sizeof (header));
            // is this our header?
            header* h = (header*)data;
            if (h->isRequest())
            {
                if (h->checkChecksum ())
                {
                    const uint32_t len = h->getLength();
                    const uint64_t seq = h->getSequence();
                    const uint32_t respLen = h->getOptions();
                    ssize_t toBeReceived = len - ret;

                    // receive the remaining part of the message
                    while (toBeReceived > 0)
                    {
                        ret = sock.recv (data, sizeof(data));
                        toBeReceived -= ret;
                        //TODO check packet content
                    }
                    if (toBeReceived < 0)
                    {
                        Console::PrintVerbose ("Warning: chunk received\n");
                    }
                    //TODO check packet content

                    // send response
                    h->initResponse (seq, respLen);
                    uint8_t counter = (uint8_t)seq;
                    uint8_t* p = data + sizeof (header);
                    unsigned sent = sizeof (header);
                    const unsigned totalLen = respLen + sizeof(header);

                    while (sent < totalLen)
                    {
                        for (; sent < totalLen && p < (p + sizeof(data)); sent++)
                            *p++ = --counter;
                        sock.send (data, p - data);
                        p = data;
                    }
                }
                else
                {
                    Console::PrintVerbose ("Invalid packet header\n");
                }
            }
            else
            {
                Console::PrintMoreVerbose ("Packet ignored\n");
            }
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