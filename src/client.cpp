// SPDX-License-Identifier: GPL-3.0-only
/*
 * NET-BABBLER <https://github.com/amartin755/tcppump>
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

#include <netdb.h>
#include <unistd.h>

#include <cstring>
#include "client.hpp"

#include "console.hpp"

cClient::cClient (const std::string &server, uint16_t remotePort, uint16_t localPort)
    : m_thread (nullptr), m_server (server), m_remotePort (remotePort), m_localPort (localPort)
{
    m_thread = new std::thread (&cClient::threadFunc, this);
}

cClient::~cClient ()
{
    if (m_thread)
        m_thread->join ();
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
    freeaddrinfo(result);

    if (!rp) 
    {               /* No address succeeded */
        Console::PrintError ("Could not connect to %s (%s)\n", m_server.c_str(), std::strerror(errno));
        return;
    }
    uint8_t buf[1024];
    while(1)
    {
    Console::PrintError ("A\n");
    send (sfd, "AAAAAAAAAAAAAAA", 10, MSG_NOSIGNAL);
    Console::PrintError ("B\n");
    recv (sfd, buf, sizeof(buf), 0);
    }
    Console::PrintError ("Z\n");
    close(sfd);

}
