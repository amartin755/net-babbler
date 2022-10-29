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
#include <sys/eventfd.h>

#include <netdb.h>
#include <unistd.h>

#include <cstring>
#include <chrono>
#include <new> // bad_alloc

#include "client.hpp"
#include "console.hpp"
#include "socket.hpp"
#include "protocol.hpp"

cEvent cClient::m_eventCancel;

cClient::cClient (cEvent& evTerminated, const std::string &server, uint16_t remotePort,
    uint16_t localPort, uint64_t delay, unsigned count, unsigned socketBufSize)
    : m_evTerminated (evTerminated),
      m_terminate(false),
      m_thread (nullptr),
      m_server (server),
      m_remotePort (remotePort),
      m_localPort (localPort),
      m_delay (delay),
      m_count (count),
      m_socketBufSize (socketBufSize),
      m_requestor (nullptr),
      m_lastStatsTime (0)
{
    m_thread = new std::thread (&cClient::threadFunc, this);
}

cClient::~cClient ()
{
    if (m_thread)
        m_thread->join ();
    delete m_thread;
    delete m_requestor;
}

void cClient::terminateAll ()
{
    m_eventCancel.send ();
}


void print_ips(struct addrinfo *lst) {
    /* IPv4 */
    char ipv4[INET_ADDRSTRLEN];
    struct sockaddr_in *addr4;

    /* IPv6 */
    char ipv6[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *addr6;

    for (; lst != NULL; lst = lst->ai_next) {
        if (lst->ai_addr->sa_family == AF_INET) {
            addr4 = (struct sockaddr_in *) lst->ai_addr;
            inet_ntop(AF_INET, &addr4->sin_addr, ipv4, INET_ADDRSTRLEN);
            printf("resolved IP: %s\n", ipv4);
        }
        else if (lst->ai_addr->sa_family == AF_INET6) {
            addr6 = (struct sockaddr_in6 *) lst->ai_addr;
            inet_ntop(AF_INET6, &addr6->sin6_addr, ipv6, INET6_ADDRSTRLEN);
            printf("IP: %s\n", ipv6);
        }
    }
}

unsigned cClient::statistics (cStats& stats, bool summary)
{
    using namespace std::chrono;
    unsigned duration;

    auto now = steady_clock::now();

    m_requestor->getStats (stats);
    duration = duration_cast<milliseconds>(now - m_startTime).count();

    if (!summary)
    {
        unsigned lastTime   = m_lastStatsTime;
        cStats   lastStats  = m_lastStats;
        m_lastStatsTime     = duration;
        m_lastStats         = stats;
        stats    -= lastStats;
        duration -= lastTime;
    }
    else
    {
        m_lastStatsTime = duration;
        m_lastStats     = stats;
        if (m_finishedTime)
            duration = m_finishedTime;
    }
    return duration;
}

void cClient::threadFunc ()
{
    using namespace std::chrono;
    cSocket sock;
    m_requestor = new cRequestor (sock, m_socketBufSize, 1230, 123, 12340, 1234, m_delay);

    try
    {
        bool connected = false;
        std::list <cSocket::info> r;
        cSocket::getaddrinfo (m_server, m_remotePort, AF_UNSPEC, SOCK_STREAM, 0, r);
        for (const auto& addrInfo : r)
        {
            try
            {
                cSocket s (addrInfo.family, addrInfo.socktype, addrInfo.protocol);
                s.setCancelEvent (m_eventCancel);
                s.connect ((sockaddr*)&addrInfo.addr, addrInfo.addrlen);
                sock = std::move(s);
                connected = true;

                Console::Print ("Connected to %s:%u via %s\n",
                    sock.inet_ntop ((sockaddr*)&addrInfo.addr).c_str(),
                    m_remotePort,
                    sock.getsockname().c_str());
            }
            catch (const cSocket::errorException& e)
            {
                continue;
            }
            break;
        }

        if (connected)
        {
            // TODO pattern: fixed size, random range (as today), sweep
            bool infinite = m_count == 0;

            m_startTime = steady_clock::now();
            while (!m_terminate)
            {
                m_requestor->doJob ();

                if (!infinite && --m_count == 0)
                    m_terminate = true;
            }
        }
        else
        {
            Console::PrintError ("Could not connect to %s\n", m_server.c_str());
        }
    }
    catch (const cSocket::errorException& e)
    {
        Console::PrintError ("%s\n", e.what());
    }
    catch (const cSocket::eventException& e)
    {
    }
    auto end = steady_clock::now();
    m_finishedTime = duration_cast<milliseconds>(end - m_startTime).count();

    m_evTerminated.send();
}
