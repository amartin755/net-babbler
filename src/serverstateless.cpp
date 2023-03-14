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

#include "serverstateless.hpp"
#include "console.hpp"

cStatelessServer::cStatelessServer (const cSocket::Properties& proto, uint16_t localPort, unsigned socketBufSize, cSemaphore& threadLimit)
    : m_terminate (false),
      m_threadLimit (threadLimit),
      m_protocol (proto),
      m_localPort (localPort),
      m_socketBufSize (socketBufSize)
{
    try
    {
        cSocket sListener = cSocket::listen (m_protocol, m_localPort, 0);
        long numberOfCPUs = sysconf(_SC_NPROCESSORS_ONLN);

        for (int n = std::max ((int)numberOfCPUs, 4); n > 0; n--)
        {
            m_connThreads.push_back (new cResponderThread(threadLimit, std::move(sListener.clone()), socketBufSize, proto.toString()));
        }
    }
    catch (const cSocket::errorException& e)
    {
        Console::PrintError ("%s\n", e.what());
    }

}

cStatelessServer::~cStatelessServer ()
{
    for (auto thread : m_connThreads)
    {
        delete thread;
    }
}
