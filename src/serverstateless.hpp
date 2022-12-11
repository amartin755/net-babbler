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

#ifndef SERVER_STATELESS_HPP
#define SERVER_STATELESS_HPP

#include <string>
#include <thread>
#include <atomic>
#include <list>

#include "socket.hpp"
#include "semaphore.hpp"


class cResponderThread
{
public:
    cResponderThread (cSemaphore& threadLimit, cSocket s, unsigned socketBufSize, const char* proto);
    ~cResponderThread ();
    bool isFinished () {return m_finished;}

    void connectionThreadFunc (cSocket s, unsigned socketBufSize, cSemaphore& threadLimit, const char* proto);

private:
    std::atomic<bool> m_finished;
    std::thread       m_thread;
};

class cStatelessServer
{
public:
    cStatelessServer (const cSocket::Properties& proto, uint16_t localPort, unsigned socketBufSize, cSemaphore& threadLimit);
    ~cStatelessServer ();

private:
    void listenerThreadFunc ();

    std::atomic<bool>       m_terminate;
    cSemaphore&             m_threadLimit;
    std::thread*            m_listenerThread;
    const cSocket::Properties m_protocol;
    uint16_t                m_localPort;
    std::list<cResponderThread*> m_connThreads;
    unsigned                m_socketBufSize;
};

#endif