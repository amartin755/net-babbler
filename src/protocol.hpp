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

#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <arpa/inet.h>
#include <cstdint>
#include <cinttypes>
#include <stdexcept>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>

#include "bug.hpp"
#include "socket.hpp"
#include "stats.hpp"
#include "comsettings.hpp"

//TODO split this header-only implementation

class cProtocolException : public std::runtime_error
{
public:
    cProtocolException (const char* what) : std::runtime_error (what)
    {
    }
};

struct cProtocolHeader
{
    void initRequest(uint64_t sequence, uint32_t payloadLength, uint32_t respLength)
    {
        type     = htonl (0xaaffffee);
        length   = htonl (payloadLength + sizeof (*this));
        seq      = htobe64 (sequence);
        options  = htonl (respLength);
        checksum = htonl (calcChecksum ());
    }
    bool isRequest ()
    {
        return type == htonl(0xaaffffee);
    }
    void initResponse (uint64_t sequence, uint32_t payloadLength)
    {
        type     = htonl (0xeeffffaa);
        length   = htonl (payloadLength + sizeof (*this));
        seq      = htobe64 (sequence);
        options  = 0;
        checksum = htonl (calcChecksum ());
    }
    bool isResponse ()
    {
        return type == htonl(0xeeffffaa);
    }

    uint32_t calcChecksum ()
    {
        uint32_t sum = 0;
        uint8_t *p = reinterpret_cast<uint8_t*>(this);
        while (p < (uint8_t*)&checksum)
        {
            sum += *p++;
        }
        return sum;
    }

    bool checkChecksum ()
    {
        return ntohl (checksum) == calcChecksum ();
    }

    uint32_t getLength ()
    {
        return ntohl (length);
    }
    uint64_t getSequence ()
    {
        return be64toh (seq);
    }
    uint32_t getOptions ()
    {
        return ntohl (options);
    }

private:
    uint32_t type;
    uint32_t length; // whole packet length, including type and length --> min value is 8
    uint64_t seq;
    uint32_t options;
    uint32_t checksum;
//    uint8_t data[];
};

class cBabblerProtocol
{
public:
    cBabblerProtocol (cSocket& sock, unsigned bufsize) : m_socket (sock), m_bufsize (bufsize)
    {
        m_buf = new uint8_t[bufsize];
    }
    ~cBabblerProtocol ()
    {
        delete[] m_buf;
    }
    void sendRequest (uint64_t seq, unsigned reqSize, unsigned respSize)
    {
        reqSize  -= sizeof (cProtocolHeader);
        respSize -= sizeof (cProtocolHeader);
        cProtocolHeader* h = (cProtocolHeader*)m_buf;
        h->initRequest (seq, reqSize, respSize);
        send (h, reqSize, true);
    }
    void sendResponse (uint64_t seq, unsigned respSize,
        const struct sockaddr *dest_addr = nullptr, socklen_t addrlen = 0)
    {
        cProtocolHeader* h = (cProtocolHeader*)m_buf;
        h->initResponse (seq, respSize);
        send (h, respSize, false, dest_addr, addrlen);
    }
    void recvResponse (uint64_t expSeq)
    {
        bool isRequest   = false;
        uint32_t options = 0;
        uint64_t seq = receive (isRequest, options);
        if (isRequest)
            throw cProtocolException ("Unexpected packet type");
        if (expSeq != seq)
            throw cProtocolException ("Unexpected sequence number");
    }
    void recvRequest (uint64_t& seq, uint32_t& expRespLen,
        struct sockaddr * src_addr = nullptr, socklen_t * addrlen = nullptr)
    {
        bool isRequest = true;
        seq = receive (isRequest, expRespLen, src_addr, addrlen);
        if (!isRequest)
            throw cProtocolException ("Unexpected packet type");
    }
    void getStats (cStats& stats)
    {
        m_statsLock.lock ();
        stats = m_stats;
        m_statsLock.unlock ();
    }
    const unsigned MIN_LEN = 32;

private:
    void send (cProtocolHeader* h, unsigned size, int incr,
        const struct sockaddr *dest_addr = nullptr, socklen_t addrlen = 0)
    {
        const unsigned totalLen = size + sizeof(cProtocolHeader);
        uint8_t* p              = m_buf + sizeof (cProtocolHeader);
        unsigned sent           = sizeof (cProtocolHeader);
        uint8_t counter         = (uint8_t)h->getSequence();

        while (sent < totalLen)
        {
            for (; sent < totalLen && p < (m_buf + m_bufsize); sent++)
                *p++ = incr ? ++counter : --counter;

            uint64_t sentLen = (uint64_t)m_socket.send (m_buf, p - m_buf, dest_addr, addrlen);
            updateTransmitStats (sentLen, sent < totalLen ? 0 : 1);
            p = m_buf;
        }
    }

    uint64_t receive (bool& isRequest, uint32_t& options,
        struct sockaddr * src_addr = nullptr, socklen_t * addrlen = nullptr)
    {
        // first try to at least receive the cProtocolHeader
        ssize_t rcvLen = m_socket.recv (m_buf, m_bufsize, sizeof (cProtocolHeader),
            src_addr, addrlen);
        // is this our cProtocolHeader?
        cProtocolHeader* h = (cProtocolHeader*)m_buf;
        if (!h->checkChecksum())
            throw cProtocolException ("Wrong header checksum");

        isRequest = h->isRequest();
        if (!isRequest && !h->isResponse())
            throw cProtocolException ("Unknown packet type");

        updateReceiveStats (rcvLen, 0);

        const uint32_t len    = h->getLength();
        const uint64_t seq    = h->getSequence();
        options               = h->getOptions();
        ssize_t toBeReceived  = len - rcvLen;
        uint8_t expPayloadVal = (uint8_t)seq;

        checkPayload (m_buf + sizeof (cProtocolHeader), rcvLen - sizeof (cProtocolHeader), isRequest, expPayloadVal);

        // receive the remaining part of the message
        while (toBeReceived > 0)
        {
            rcvLen = m_socket.recv (m_buf, sizeof(m_bufsize), 0, src_addr, addrlen);
            updateReceiveStats (rcvLen, 0);
            toBeReceived -= rcvLen;
            checkPayload (m_buf, rcvLen, isRequest, expPayloadVal);
        }
        if (toBeReceived < 0)
        {
            Console::PrintVerbose ("Warning: chunk received\n");
        }
        updateReceiveStats (0, 1);

        return seq;
    }

    void checkPayload (const uint8_t* data, unsigned len, bool incr, uint8_t& expVal) const
    {
        while (len--)
        {
            incr ? ++expVal : --expVal;
            if (*data++ != expVal)
                throw cProtocolException ("Corrupted packet");
        }
    }

    void updateTransmitStats (uint64_t sentOctets, uint64_t sentPackets)
    {
        m_statsLock.lock ();
        m_stats.m_sentOctets  += sentOctets;
        m_stats.m_sentPackets += sentPackets;
        m_statsLock.unlock ();
    }
    void updateReceiveStats (uint64_t receivedOctets, uint64_t receivedPackets)
    {
        m_statsLock.lock ();
        m_stats.m_receivedOctets  += receivedOctets;
        m_stats.m_receivedPackets += receivedPackets;
        m_statsLock.unlock ();
    }

protected:
    int_fast64_t getSentOctets () const
    {
        return m_stats.m_sentOctets;
    }
    int_fast64_t getReceivedOctets () const
    {
        return m_stats.m_receivedOctets;
    }

private:
    cSocket& m_socket;
    const size_t m_bufsize;
    uint8_t* m_buf;
    cStats m_stats;
    std::mutex m_statsLock;
};


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

        sendRequest (++m_seq, m_currReqSize, m_currRespSize);
        auto start = std::chrono::high_resolution_clock::now();
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