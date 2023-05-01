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


#include <arpa/inet.h>
#include <cstdint>
#include <cinttypes>
#include <stdexcept>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>

#include "protocol.hpp"

#include "bug.hpp"
#include "socket.hpp"
#include "stats.hpp"
#include "comsettings.hpp"



cBabblerProtocol::cBabblerProtocol (cSocket& sock, unsigned bufsize) : m_socket (sock), m_bufsize (bufsize)
{
    m_buf  = new uint8_t[bufsize];
    m_pBuf = m_buf;
    m_bufContentSize = 0;
}
cBabblerProtocol::~cBabblerProtocol ()
{
    m_pBuf = nullptr;
    delete[] m_buf;
}
void cBabblerProtocol::sendRequest (uint64_t seq, unsigned reqSize, unsigned respSize)
{
    // zero is allowed -> no response
    if (respSize)
    {
        BUG_ON (respSize < sizeof (cProtocolHeader));
        respSize -= sizeof (cProtocolHeader);
    }
    BUG_ON (reqSize < sizeof (cProtocolHeader));
    reqSize  -= sizeof (cProtocolHeader);

    cProtocolHeader* h = (cProtocolHeader*)m_buf;
    h->initRequest (seq, reqSize, respSize);
    send (h, reqSize, true);
}
void cBabblerProtocol::sendResponse (uint64_t seq, unsigned respSize,
    const struct sockaddr *dest_addr, socklen_t addrlen)
{
    cProtocolHeader* h = (cProtocolHeader*)m_buf;
    h->initResponse (seq, respSize);
    send (h, respSize, false, dest_addr, addrlen);
}
void cBabblerProtocol::recvResponse (uint64_t expSeq)
{
    bool isRequest   = false;
    uint32_t options = 0;
    uint64_t seq = receive (isRequest, options);
    if (isRequest)
        throw cProtocolException ("Unexpected packet type");
    if (expSeq != seq)
        throw cProtocolException ("Unexpected sequence number");
}
void cBabblerProtocol::recvRequest (uint64_t& seq, uint32_t& expRespLen,
    struct sockaddr * src_addr, socklen_t * addrlen)
{
    bool isRequest = true;
    seq = receive (isRequest, expRespLen, src_addr, addrlen);
    if (!isRequest)
        throw cProtocolException ("Unexpected packet type");
}
void cBabblerProtocol::getStats (cStats& stats)
{
    m_statsLock.lock ();
    stats = m_stats;
    m_statsLock.unlock ();
}
void cBabblerProtocol::send (cProtocolHeader* h, unsigned size, int incr,
    const struct sockaddr *dest_addr, socklen_t addrlen)
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

uint64_t cBabblerProtocol::receive (bool& isRequest, uint32_t& options,
    struct sockaddr * src_addr, socklen_t * addrlen)
{
    ssize_t rcvLen = m_bufContentSize;

    if (!rcvLen)
    {
        // first try to at least receive the cProtocolHeader
        rcvLen = m_socket.recv (m_buf, m_bufsize, sizeof (cProtocolHeader),
            src_addr, addrlen);
        m_bufContentSize = rcvLen;
        updateReceiveStats (rcvLen, 0);
    }

    // FIXME currently we can't handle fragmented headers
    BUG_ON ((size_t)rcvLen < sizeof (cProtocolHeader));

    // is this our cProtocolHeader?
    cProtocolHeader* h = (cProtocolHeader*)m_pBuf;
    if (!h->checkChecksum())
        throw cProtocolException ("Wrong header checksum");
    isRequest = h->isRequest();
    if (!isRequest && !h->isResponse())
        throw cProtocolException ("Unknown packet type");

    const uint32_t len    = h->getLength();
    const uint64_t seq    = h->getSequence();
    options               = h->getOptions();
    ssize_t toBeReceived  = len - rcvLen;
    uint8_t expPayloadVal = (uint8_t)seq;

    checkPayload (m_pBuf + sizeof (cProtocolHeader),
                    std::min (len, (uint32_t)rcvLen) - sizeof (cProtocolHeader),
                    isRequest, expPayloadVal);

    // receive the remaining part of the message and check the content
    while (toBeReceived > 0)
    {
        rcvLen = m_socket.recv (m_buf, std::min ((size_t)toBeReceived, m_bufsize), 0, src_addr, addrlen);
        m_bufContentSize = rcvLen;
        updateReceiveStats (rcvLen, 0);
        toBeReceived -= rcvLen;
        checkPayload (m_buf, rcvLen, isRequest, expPayloadVal);
    }
    // receive buffer contains more then one message
    if (toBeReceived < 0)
    {
        m_pBuf += len;
        m_bufContentSize -= len;
        BUG_ON (m_pBuf >= m_buf + m_bufsize);
    }
    else
    {
        m_pBuf = m_buf;
        m_bufContentSize = 0;
    }
    updateReceiveStats (0, 1);

    return seq;
}

void cBabblerProtocol::checkPayload (const uint8_t* data, unsigned len, bool incr, uint8_t& expVal) const
{
    while (len--)
    {
        incr ? ++expVal : --expVal;
        if (*data++ != expVal)
            throw cProtocolException ("Corrupted packet");
    }
}

void cBabblerProtocol::cBabblerProtocol::updateTransmitStats (uint64_t sentOctets, uint64_t sentPackets)
{
    m_statsLock.lock ();
    m_stats.m_sentOctets  += sentOctets;
    m_stats.m_sentPackets += sentPackets;
    m_statsLock.unlock ();
}
void cBabblerProtocol::updateReceiveStats (uint64_t receivedOctets, uint64_t receivedPackets)
{
    m_statsLock.lock ();
    m_stats.m_receivedOctets  += receivedOctets;
    m_stats.m_receivedPackets += receivedPackets;
    m_statsLock.unlock ();
}
