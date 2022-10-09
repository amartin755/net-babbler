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

#ifndef TCPLISTENER_HPP
#define TCPLISTENER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <list>

class cTcpListener
{
public:
    cTcpListener (uint16_t localPort, unsigned socketBufSize);
    ~cTcpListener ();

    void listenerThreadFunc ();
    void connectionThreadFunc (int sockfd);

private:
    std::atomic<bool>       m_terminate;
    std::thread*            m_listenerThread;
    uint16_t                m_localPort;
    std::list<std::thread*> m_connThreads;
    unsigned                m_socketBufSize;
};

#endif