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

#include "serverstateful.hpp"
#include "console.hpp"

cStatefulServer::cStatefulServer (const cSocket::Properties& proto, uint16_t localPort, unsigned socketBufSize, cSemaphore& threadLimit)
    : m_terminate (false),
      m_threadLimit (threadLimit),
      m_listenerThread (nullptr),
      m_protocol (proto),
      m_localPort (localPort),
      m_socketBufSize (socketBufSize)
{
    m_listenerThread = new std::thread (&cStatefulServer::listenerThreadFunc, this);
}

cStatefulServer::~cStatefulServer ()
{
    if (m_listenerThread)
    {
        m_listenerThread->join ();
        delete m_listenerThread;
    }

    for (auto thread : m_connThreads)
    {
        delete thread;
    }
}

void cStatefulServer::listenerThreadFunc ()
{
    try
    {
        cSocket sListener = cSocket::listen (m_protocol, m_localPort, 50);
        while (!m_terminate)
        {
            m_threadLimit.wait ();
            std::string remoteIp;
            uint16_t remotePort;

            cSocket sConn = sListener.accept (remoteIp, remotePort);
            Console::PrintVerbose ("Client %s:%u connected to %s port %u\n",
                remoteIp.c_str(), remotePort, m_protocol.toString(), m_localPort);

            // create thread for this connection
            m_connThreads.push_back (new cResponderThread (m_threadLimit, std::move(sConn),
                m_socketBufSize, m_protocol.toString()));

            // cleanup terminated threads
            for (auto it = m_connThreads.begin(); it != m_connThreads.end();)
            {
                if ((*it)->isFinished())
                {
                    delete *it;
                    it = m_connThreads.erase (it);
                }
                else
                    ++it;
            }
        }
    }
    catch (const cSocket::errorException& e)
    {
        Console::PrintError ("%s\n", e.what());
    }
}
