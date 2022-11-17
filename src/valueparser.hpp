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

#ifndef VALUEPARSER_HPP
#define VALUEPARSER_HPP

#include <utility>
#include <string>
#include <list>

#include "socket.hpp"


class cValueParser
{
public:
    static std::pair<unsigned long, unsigned long> range (const std::string& s);
    static std::list<std::pair<unsigned long, unsigned long>> rangeList (const std::string& s);
    static bool isIPv4Address (const std::string& s);
    static bool isIPv6Address (const std::string& s);
    static void clientConnection (const std::string& s, cSocket::Properties& proto, std::string& remoteHost,
        std::list<std::pair<unsigned long, unsigned long>>& remotePorts,
        std::string& localAddress, uint16_t& localPort);

};
#endif