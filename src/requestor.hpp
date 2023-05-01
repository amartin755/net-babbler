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

#ifndef REQUESTOR_HPP
#define REQUESTOR_HPP

#include <random>

#include "protocol.hpp"

class cRequestor : public cBabblerProtocol
{
public:
    cRequestor (cSocket& sock, unsigned bufsize, const cComSettings comSettings, uint64_t delay, int_fast64_t sendLimit, int_fast64_t recvLimit)
        : cBabblerProtocol (sock, bufsize),
          m_comSettings (comSettings),
          m_currReqSize (m_comSettings.m_requestSizeMin),
          m_currRespSize (m_comSettings.m_responseSizeMin),
          m_reqDelta (0, m_comSettings.m_requestSizeMax - m_comSettings.m_requestSizeMin),
          m_respDelta (0, m_comSettings.m_responseSizeMax - m_comSettings.m_responseSizeMin),
          m_delay (delay),
          m_sendLimitOctets(sendLimit),
          m_recvLimitOctets(recvLimit),
          m_seq (0),
          m_wantStatus (m_delay > 10000)
    {
    }
    bool isLimitReached (int_fast64_t limit, int_fast64_t sentRecvOctetts, unsigned& toBeSentReceived) const
    {
        if (limit > 0)
        {
            if (sentRecvOctetts >= limit)
                return true;

            int_fast64_t rest = limit - sentRecvOctetts;

            toBeSentReceived = std::min ((int_fast64_t)toBeSentReceived, rest);
            if (rest - toBeSentReceived < MIN_LEN && toBeSentReceived < rest)
                toBeSentReceived -= MIN_LEN - (rest - toBeSentReceived);
            if (toBeSentReceived < MIN_LEN) // should never happen
                toBeSentReceived = MIN_LEN;
        }
        return false;
    }
    void doJob ()
    {
        if (isLimitReached (m_sendLimitOctets, getSentOctets(), m_currReqSize) ||
            isLimitReached (m_recvLimitOctets, getReceivedOctets(), m_currRespSize))
        {
            throw cSocket::eventException ();
        }

        auto start = std::chrono::high_resolution_clock::now();
        sendRequest (++m_seq, m_currReqSize, m_currRespSize);
        if (m_currRespSize)
            recvResponse (m_seq);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> roundtrip = end - start;

        if (m_wantStatus)
            Console::Print (" %4" PRIu64 ": sent %u bytes, received %u bytes, roundtrip %.3f ms\n",
                m_seq, m_currReqSize, m_currRespSize, roundtrip);
        if (m_delay)
            std::this_thread::sleep_for (std::chrono::microseconds (m_delay));

        if (m_comSettings.isRand ())
        {
            // generate random delta for request and response
            m_currReqSize  = m_comSettings.m_requestSizeMin  + m_reqDelta (m_rng);
            m_currRespSize = m_comSettings.m_responseSizeMin + m_respDelta (m_rng);
        }
        else if (m_comSettings.isSweep ())
        {
            m_currReqSize  += m_comSettings.m_stepWidth;
            m_currRespSize += m_comSettings.m_stepWidth;
            if (m_currReqSize > m_comSettings.m_requestSizeMax)
                m_currReqSize = m_comSettings.m_requestSizeMin;
            if (m_currRespSize > m_comSettings.m_responseSizeMax)
                m_currRespSize = m_comSettings.m_responseSizeMin;
        }
        else
        {
            m_currReqSize  = m_comSettings.m_requestSizeMin;
            m_currRespSize = m_comSettings.m_responseSizeMin;
        }
    }

    void getStats (cStats& stats)
    {
        cBabblerProtocol::getStats (stats);
    }

private:
    const cComSettings m_comSettings;
    unsigned m_currReqSize;
    unsigned m_currRespSize;
    std::uniform_int_distribution<std::mt19937::result_type> m_reqDelta;
    std::uniform_int_distribution<std::mt19937::result_type> m_respDelta;
    const uint64_t m_delay;
    int_fast64_t m_sendLimitOctets;
    int_fast64_t m_recvLimitOctets;
    uint64_t m_seq;
    std::mt19937 m_rng;

    const bool m_wantStatus;
};


#endif