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
      m_requestor (nullptr)
{
    m_evfd = eventfd (0, EFD_SEMAPHORE);
    if (m_evfd == -1)
        throw std::bad_alloc ();
    m_thread = new std::thread (&cClient::threadFunc, this);
}

cClient::~cClient ()
{
    if (m_thread)
        m_thread->join ();
    delete m_thread;
    close (m_evfd);
    delete m_requestor;
}

void cClient::terminate ()
{
    uint64_t val = 1;
    write (m_evfd, &val, sizeof(val));
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
/*
    if (s < 0)
    {
        Console::PrintError ("Could not create client socket (%s)\n", std::strerror(errno));
        return;
    }
*/
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    std::memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */

    int s = getaddrinfo (m_server.c_str (), std::to_string(m_remotePort).c_str(), &hints, &result);
    if (s != 0)
    {
        Console::PrintError ("%s\n", gai_strerror(s));
        return;
    }

    int sfd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        Console::PrintError ("fam %d  type %d proto %d\n", rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);

        sfd = socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sfd);
    }
    print_ips (result);
    freeaddrinfo(result);

    if (!rp)
    {               /* No address succeeded */
        Console::PrintError ("Could not connect to %s (%s)\n", m_server.c_str(), std::strerror(errno));
        return;
    }

        // Get my ip address and port
    struct sockaddr_in my_addr;
    char myIP[16];
    unsigned int myPort;
    bzero(&my_addr, sizeof(my_addr));
    socklen_t len = sizeof(my_addr);
    getsockname(sfd, (struct sockaddr *) &my_addr, &len);
    inet_ntop(AF_INET, &my_addr.sin_addr, myIP, sizeof(myIP));
    myPort = ntohs(my_addr.sin_port);

    printf("Local ip address: %s\n", myIP);
    printf("Local port : %u\n", myPort);


    using namespace std::chrono;
    cSocket sock (sfd, m_evfd);
    m_requestor = new cRequestor (sock, m_socketBufSize, 1230, 123, 12340, 1234, m_delay);
    m_startTime = steady_clock::now();

    try
    {

        // TODO pattern: fixed size, random range (as today), sweep
        bool infinite = m_count == 0;

        while (!m_terminate)
        {
            m_requestor->doJob ();

            if (!infinite && --m_count == 0)
                m_terminate = true;
        }
    }
    catch (const cSocketException& e)
    {
        e.isError() ?
            Console::PrintError   ("%s\n", e.what()) :
            Console::PrintVerbose ("%s\n", e.what());
    }
    auto end = steady_clock::now();
    m_finishedTime = duration_cast<milliseconds>(end - m_startTime).count();

    m_evTerminated.send();
}
