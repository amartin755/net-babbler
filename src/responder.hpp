// SPDX-License-Identifier: GPL-3.0-only
/*
 * NET-BABBLER <https://github.com/amartin755/net-babbler>
 * Copyright (C) 2023 Andreas Martin (netnag@mailbox.org)
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

#ifndef RESPONDER_HPP
#define RESPONDER_HPP

#include "protocol.hpp"

class cResponder : public cBabblerProtocol
{
public:
    cResponder (cSocket& sock, unsigned bufsize, bool isConnectionless = false)
        : cBabblerProtocol (sock, bufsize),
          m_isConnectionless (isConnectionless),
          m_remoteAddr (nullptr)
    {
        if (m_isConnectionless)
        {
            m_remoteAddr = new sockaddr_storage;
            std::memset (m_remoteAddr, 0, sizeof (*m_remoteAddr));
        }
    }
    ~cResponder ()
    {
        delete m_remoteAddr;
    }

    void doJob ()
    {
        uint64_t seq;
        uint32_t expSeqLen;

        if (m_isConnectionless)
        {
            socklen_t addrlen = sizeof (*m_remoteAddr);
            recvRequest (seq, expSeqLen, (sockaddr*)m_remoteAddr, &addrlen);
            sendResponse (seq, expSeqLen, (sockaddr*)m_remoteAddr, addrlen);
        }
        else
        {
            recvRequest (seq, expSeqLen);
            sendResponse (seq, expSeqLen);
        }
    }
private:
    bool m_isConnectionless;
    sockaddr_storage* m_remoteAddr;
};


#endif