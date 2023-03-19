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

#ifndef STATS_HPP
#define STATS_HPP

#include <cstdint>
#include <cinttypes>

class cStats
{
public:
    cStats () : m_sentPackets(0), m_sentOctets(0), m_receivedPackets(0), m_receivedOctets(0), m_errors(0), m_timeouts(0)
    {
    }

    cStats operator+ (const cStats& val) const
    {
        cStats result;
        result.m_sentPackets     = m_sentPackets     + val.m_sentPackets;
        result.m_sentOctets      = m_sentOctets      + val.m_sentOctets;
        result.m_receivedPackets = m_receivedPackets + val.m_receivedPackets;
        result.m_receivedOctets  = m_receivedOctets  + val.m_receivedOctets;
        result.m_errors          = m_errors          + val.m_errors;
        result.m_timeouts        = m_timeouts        + val.m_timeouts;
        return result;
    }
    cStats operator- (const cStats& val) const
    {
        cStats result;
        result.m_sentPackets     = m_sentPackets     - val.m_sentPackets;
        result.m_sentOctets      = m_sentOctets      - val.m_sentOctets;
        result.m_receivedPackets = m_receivedPackets - val.m_receivedPackets;
        result.m_receivedOctets  = m_receivedOctets  - val.m_receivedOctets;
        result.m_errors          = m_errors          - val.m_errors;
        result.m_timeouts        = m_timeouts        - val.m_timeouts;
        return result;
    }
    cStats& operator+= (const cStats& val)
    {
        m_sentPackets     += val.m_sentPackets;
        m_sentOctets      += val.m_sentOctets;
        m_receivedPackets += val.m_receivedPackets;
        m_receivedOctets  += val.m_receivedOctets;
        m_errors          += val.m_errors;
        m_timeouts        += val.m_timeouts;
        return *this;
    }
    cStats& operator-= (const cStats& val)
    {
        m_sentPackets     -= val.m_sentPackets;
        m_sentOctets      -= val.m_sentOctets;
        m_receivedPackets -= val.m_receivedPackets;
        m_receivedOctets  -= val.m_receivedOctets;
        m_errors          -= val.m_errors;
        m_timeouts        -= val.m_timeouts;
        return *this;
    }

    int_fast64_t m_sentPackets;
    int_fast64_t m_sentOctets;
    int_fast64_t m_receivedPackets;
    int_fast64_t m_receivedOctets;
    int_fast64_t m_errors;
    int_fast64_t m_timeouts;
};


#endif