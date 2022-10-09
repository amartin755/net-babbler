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

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <thread>
#include <atomic>

class cClient
{
public:
    cClient (const std::string &server, uint16_t remotePort,
        uint16_t localPort, uint64_t delay, unsigned count, unsigned time);
    ~cClient ();

    void threadFunc ();

private:
    std::atomic<bool> m_terminate;
    std::thread*  m_thread;
    std::string   m_server;
    uint16_t      m_remotePort;
    uint16_t      m_localPort;
    uint64_t      m_delay;
    unsigned      m_count;
    unsigned      m_time;
};

#endif