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

#include "responderthread.hpp"
#include "console.hpp"
#include "protocol.hpp"


cResponderThread::cResponderThread (cSemaphore& threadLimit, cSocket s, unsigned socketBufSize, const char* proto)
: m_finished (false), m_thread (&cResponderThread::connectionThreadFunc, this, std::move(s), socketBufSize, std::ref(threadLimit), proto)
{

}

cResponderThread::~cResponderThread ()
{
    m_thread.join ();
}

void cResponderThread::connectionThreadFunc (cSocket s, unsigned socketBufSize, cSemaphore& threadLimit, const char* proto)
{
    try
    {
        cResponder responder (s, socketBufSize);

        while (1)
        {
            responder.doJob ();
        }
    }
    catch (const cSocket::errorException& e)
    {
            Console::PrintError ("%s\n", e.what());
    }
    Console::PrintDebug ("%s connection terminated\n", proto);
    m_finished = true;
    threadLimit.post ();
}