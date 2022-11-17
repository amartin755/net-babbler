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
#include "protocol.hpp"

cTcpListener::cTcpListener (const cSocket::Properties& proto, uint16_t localPort, unsigned socketBufSize, cSemaphore& threadLimit)
    : m_terminate (false),
      m_threadLimit (threadLimit),
      m_listenerThread (nullptr),
      m_protocol (proto),
      m_localPort (localPort),
      m_socketBufSize (socketBufSize)
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
    try
    {
        cSocket sListener = cSocket::listen (m_protocol, m_localPort, 50);
        while (!m_terminate)
        {
            m_threadLimit.wait ();
            std::string remoteIp;
            uint16_t remotePort;

            cSocket sConn = sListener.accept (remoteIp, remotePort);
            m_connThreads.push_back (new std::thread (&cTcpListener::connectionThreadFunc, this, std::move(sConn)));

            Console::PrintVerbose ("Client %s connected to tcp port %u\n",
                remoteIp.c_str(), remotePort);
        }
    }
    catch (const cSocket::errorException& e)
    {
        Console::PrintError ("%s\n", e.what());
    }
}


void cTcpListener::connectionThreadFunc (cSocket s)
{
    try
    {
        cResponder responder (s, m_socketBufSize);

        while (!m_terminate)
        {
            responder.doJob ();
        }
    }
    catch (const cSocket::errorException& e)
    {
            Console::PrintError ("%s\n", e.what());
    }
    Console::PrintDebug ("tcp connection terminated\n");
    m_threadLimit.post ();
}