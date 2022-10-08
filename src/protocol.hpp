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
#include <stdexcept>
#include <random>
#include <chrono>
#include <thread>

#include "socket.hpp"

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
        do
        {
            sum += *p;
        } while (p++ < (uint8_t*)&checksum);
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
        cProtocolHeader* h = (cProtocolHeader*)m_buf;
        h->initRequest (seq, reqSize, respSize);
        send (h, reqSize, true);
    }
    void sendResponse (uint64_t seq, unsigned respSize)
    {
        cProtocolHeader* h = (cProtocolHeader*)m_buf;
        h->initResponse (seq, respSize);
        send (h, respSize, false);
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
    void recvRequest (uint64_t& seq, uint32_t& expRespLen)
    {
        bool isRequest = true;
        seq = receive (isRequest, expRespLen);
        if (!isRequest)
            throw cProtocolException ("Unexpected packet type");
    }

private:
    void send (cProtocolHeader* h, unsigned size, int incr)
    {
        const unsigned totalLen = size + sizeof(cProtocolHeader);
        uint8_t* p              = m_buf + sizeof (cProtocolHeader);
        unsigned sent           = sizeof (cProtocolHeader);
        uint8_t counter         = (uint8_t)h->getSequence();

        while (sent < totalLen)
        {
            for (; sent < totalLen && p < (m_buf + m_bufsize); sent++)
                *p++ = incr ? ++counter : --counter;

            m_socket.send (m_buf, p - m_buf);
            p = m_buf;
        }
    }
    uint64_t receive (bool& isRequest, uint32_t& options)
    {
        // first try to at least receive the cProtocolHeader
        ssize_t rcvLen = m_socket.recv (m_buf, m_bufsize, sizeof (cProtocolHeader));
        // is this our cProtocolHeader?
        cProtocolHeader* h = (cProtocolHeader*)m_buf;
        if (!h->checkChecksum())
            throw cProtocolException ("Wrong cProtocolHeader checksum");

        isRequest = h->isRequest();
        if (!isRequest && !h->isResponse())
            throw cProtocolException ("Unknown packet type");

        const uint32_t len    = h->getLength();
        const uint64_t seq    = h->getSequence();
        options               = h->getOptions();
        ssize_t toBeReceived  = len - rcvLen;
        uint8_t expPayloadVal = (uint8_t)seq;

        checkPayload (m_buf + sizeof (cProtocolHeader), rcvLen - sizeof (cProtocolHeader), isRequest, expPayloadVal);

        // receive the remaining part of the message
        while (toBeReceived > 0)
        {
            rcvLen = m_socket.recv (m_buf, sizeof(m_bufsize));
            toBeReceived -= rcvLen;
            checkPayload (m_buf, rcvLen, isRequest, expPayloadVal);
        }
        if (toBeReceived < 0)
        {
            Console::PrintVerbose ("Warning: chunk received\n");
        }

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

private:
    cSocket& m_socket;
    const size_t m_bufsize;
    uint8_t* m_buf;
};

class cRequestor : public cBabblerProtocol
{
public:
    cRequestor (cSocket& sock, unsigned bufsize, unsigned reqSize, unsigned reqDelta,
        unsigned respSize, unsigned respDelta, unsigned delay)
        : cBabblerProtocol (sock, bufsize), m_reqSize (reqSize), m_reqDelta (0, reqDelta),
            m_respSize (respSize), m_respDelta (0, respDelta), m_delay (delay), m_seq (0)
    {
    }

    void doJob ()
    {
        // generate random delta for request and response
        sendRequest (++m_seq, m_reqSize + m_reqDelta (m_rng), m_respSize + m_reqDelta (m_rng));
        recvResponse (m_seq);
        if (m_delay)
            std::this_thread::sleep_for (std::chrono::milliseconds (m_delay));
    }

private:
    unsigned m_reqSize;
    std::uniform_int_distribution<std::mt19937::result_type> m_reqDelta;
    unsigned m_respSize;
    std::uniform_int_distribution<std::mt19937::result_type> m_respDelta;
    unsigned m_delay;
    uint64_t m_seq;
    std::mt19937 m_rng;
};

class cResponder : public cBabblerProtocol
{
public:
    cResponder (cSocket& sock, unsigned bufsize)
        : cBabblerProtocol (sock, bufsize)
    {
    }

    void doJob ()
    {
        uint64_t seq;
        uint32_t expSeqLen;
        recvRequest (seq, expSeqLen);
        sendResponse (seq, expSeqLen);
    }
};

#endif