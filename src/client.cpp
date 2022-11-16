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

cClient::cClient (unsigned clientID, cEvent& evTerminated, const std::string &server, uint16_t remotePort,
    uint16_t localPort, uint64_t delay, unsigned count, unsigned socketBufSize, const cComSettings& settings,
    int inetFamily, int type, int protocol)
    : m_clientID (clientID),
      m_evTerminated (evTerminated),
      m_terminate(false),
      m_thread (nullptr),
      m_server (server),
      m_remotePort (remotePort),
      m_localPort (localPort),
      m_delay (delay),
      m_count (count),
      m_socketBufSize (socketBufSize),
      m_settings (settings),
      m_inetFamily (inetFamily),
      m_type (type),
      m_protocol (protocol),
      m_requestor (nullptr),
      m_connected (false),
      m_finishedTime (0),
      m_lastStatsTime (0)
{
    m_connDescription = "NOT CONNECTED -> " + m_server + ":" + std::to_string(m_remotePort);

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


std::pair<unsigned, unsigned> cClient::statistics (cStats& delta, cStats& summary)
{
    if (!m_requestor)
        return std::pair<unsigned, unsigned>(0, 0);

    using namespace std::chrono;
    unsigned duration;

    auto now = steady_clock::now();

    m_requestor->getStats (summary);
    duration = duration_cast<milliseconds>(now - m_startTime).count();

    unsigned lastTime  = m_lastStatsTime;
    cStats   lastStats = m_lastStats;
             delta     = summary - lastStats;

    m_lastStatsTime = duration;
    m_lastStats     = summary;
    if (m_finishedTime)
        duration    = m_finishedTime;

    return std::pair<unsigned, unsigned>(duration - lastTime, duration);
}

void cClient::threadFunc ()
{
    using namespace std::chrono;
    cSocket sock = cSocket::connect (m_server, m_remotePort, m_inetFamily,
        m_type, m_protocol, m_localPort);
    m_requestor = new cRequestor (sock, m_socketBufSize, m_settings, m_delay);

    try
    {
        if (sock.isValid())
        {
            sock.setCancelEvent (m_eventCancel);

            std::string remote = sock.getpeername ();
            std::string local  = sock.getsockname ();
            m_connDescription.reserve (remote.size() + local.size() + 4);
            m_connDescription.clear ();
            m_connDescription.append (local).append(" -> ").append (remote);

            Console::Print ("[%u] Connected with %s to %s via %s\n",
                getClientID(),
                m_type == SOCK_STREAM ? (m_protocol == 0 ? "TCP" : "SCTP") :
                    (m_type == SOCK_DGRAM ? "UDP" : "UNKNOWN"),
                remote.c_str(), local.c_str());

            bool infinite = m_count == 0;
            m_connected = true;

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
            Console::PrintError ("[%u] Could not connect to %s\n", getClientID(), m_server.c_str());
        }
    }
    catch (const cSocket::errorException& e)
    {
        Console::PrintError ("[%u] %s\n", getClientID(), e.what());
    }
    catch (const cSocket::eventException& e)
    {
    }
    auto end = steady_clock::now();
    m_finishedTime = duration_cast<milliseconds>(end - m_startTime).count();

    m_evTerminated.send();
}
