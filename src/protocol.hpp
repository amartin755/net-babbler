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


#include <cstdint>
#include <cinttypes>
#include <stdexcept>
#include <mutex>

#include "bug.hpp"
#include "socket.hpp"
#include "stats.hpp"


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
protected:
    cBabblerProtocol (cSocket& sock, unsigned bufsize);

public:
    // no default/copy/move constructor and copy/move operator
    cBabblerProtocol () = delete;
    cBabblerProtocol (const cBabblerProtocol&) = delete;
    cBabblerProtocol& operator=(const cBabblerProtocol&) = delete;
    cBabblerProtocol (cBabblerProtocol&&) = delete;
    cBabblerProtocol& operator= (cBabblerProtocol&& obj) = delete;

    ~cBabblerProtocol ();

    void sendRequest (uint64_t seq, unsigned reqSize, unsigned respSize);
    void sendResponse (uint64_t seq, unsigned respSize,
        const struct sockaddr *dest_addr = nullptr, socklen_t addrlen = 0);
    void recvResponse (uint64_t expSeq);
    void recvRequest (uint64_t& seq, uint32_t& expRespLen,
        struct sockaddr * src_addr = nullptr, socklen_t * addrlen = nullptr);
    void getStats (cStats& stats);
    const unsigned MIN_LEN = 32;

private:
    void send (cProtocolHeader* h, unsigned size, int incr,
        const struct sockaddr *dest_addr = nullptr, socklen_t addrlen = 0);

    uint64_t receive (bool& isRequest, uint32_t& options,
        struct sockaddr * src_addr = nullptr, socklen_t * addrlen = nullptr);

    void checkPayload (const uint8_t* data, unsigned len, bool incr, uint8_t& expVal) const;
    void updateTransmitStats (uint64_t sentOctets, uint64_t sentPackets);
    void updateReceiveStats (uint64_t receivedOctets, uint64_t receivedPackets);

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
    size_t m_bufContentSize;
    uint8_t* m_buf;
    uint8_t* m_pBuf;
    cStats m_stats;
    std::mutex m_statsLock;
};

#endif