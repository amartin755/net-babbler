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


#include <arpa/inet.h>

#include <stdexcept>
#include <list>
#include <regex>

#include "bug.hpp"
#include "console.hpp"
#include "valueparser.hpp"

//FIXME this class is a huge mistake. It should be replaced as soon as possible


static const std::string regexPort (R"((\d{1,5}))");
static const std::string regexRange (R"((\d+(-\d+)?))");
static const std::string regexRangeList (R"((\d+(-\d+)?)(,(\d+(-\d+)?))*)");
static const std::string regexProtocol (R"((tcp|udp|ip|sctp))");
static const std::string regexIPv4Address (R"(([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}))");
static const std::string regexIPv6Address (R"((([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])))");
static const std::string regexHost (R"([a-zA-Z0-9]+([a-zA-Z0-9.\-])*)");

std::pair<unsigned long, unsigned long> cValueParser::range (const std::string& s)
{
    // yes, this is overkill just for parsing two integers separated by a minus
    // but I don't want to manualy parse and think about the corner cases

    // expected: number-numer, whitespaces are allowed
    if (!std::regex_match (s, std::regex(regexRange)))
        throw std::invalid_argument (s);

    auto const re = std::regex(R"(\d+)");
    auto const vec = std::vector<std::string>(
        std::sregex_token_iterator{begin(s), end(s), re},
        std::sregex_token_iterator{});
    const size_t found = vec.size ();

    // this would mean the expression in regex_match above is buggy
    BUG_ON (found > 2 || found == 0);

    unsigned long a, b;
    a = b = std::stoul (vec[0]);
    if (found == 2)
        b = std::stoul (vec[1]);

    if (a > b)
        throw std::invalid_argument ("The beginning of the range is greater than the end");

    return std::pair <unsigned long, unsigned long> (a, b);
}

std::list<std::pair<unsigned long, unsigned long>> cValueParser::rangeList (const std::string& s)
{
    if (!std::regex_match (s, std::regex(regexRangeList)))
        throw std::invalid_argument (s);

    auto const re = std::regex(regexRange);
    auto const vec = std::vector<std::string>(
        std::sregex_token_iterator{begin(s), end(s), re},
        std::sregex_token_iterator{});
    const size_t found = vec.size ();

    // this would mean the expression in regex_match above is buggy
    BUG_ON (found < 1);

    std::list<std::pair<unsigned long, unsigned long>> ret;

    for (const auto& val : vec)
    {
        ret.push_back (range (val));
    }
    return ret;
}

bool cValueParser::isIPv4Address (const std::string& s)
{
    uint8_t buf[sizeof(struct in6_addr)];
    return inet_pton(AF_INET, s.c_str(), buf) == 1;
}

bool cValueParser::isIPv6Address (const std::string& s)
{
    uint8_t buf[sizeof(struct in6_addr)];
    return inet_pton(AF_INET6, s.c_str(), buf) == 1;
}

void cValueParser::clientConnection (const std::string& s, protocol& proto, std::string& remoteHost,
    std::list<std::pair<unsigned long, unsigned long>>& remotePorts,
    std::string& localAddress, uint16_t& localPort)
{
    std::size_t offset = 0;
    std::smatch match;
    proto = TCP;

    // [proto://]dst_host[:dst_ports][:local_addr][:local_port]
    //  dst_host: ipv4 address OR ipv6 address enclosed in [] OR DNS name
    static std::string regexDestHost = "(" + regexIPv4Address + R"(|(\[)" + regexIPv6Address + R"(\])|)" + regexHost + ")";
    static std::string regexConn = "^(" + regexProtocol + R"(:\/\/)?)" +
        regexDestHost + "(:" + regexRangeList + ")?" +
        "(:" + regexIPv4Address + R"()|(\[)" + regexIPv6Address + R"(\])?)" +
        "(:" + regexPort + ")?$";

    // TODO lets first check the whole connection

    // protocol specified?
    if (std::regex_search (s, match, std::regex("^" + regexProtocol + R"(:\/\/)" )))
    {
        BUG_ON (match.size() == 0);

        offset = match[0].str().size();

        if (s.substr (0, 3) == "udp")
            proto = UDP;
        else if (s.substr (0, 2) == "ip")
            proto = RAW;
        else if (s.substr (0, 4) == "sctp")
            proto = SCTP;
        else if (s.substr (0, 4) == "dccp")
            proto = DCCP;
    }
    std::string remainder (s.substr(offset));

    if (std::regex_search (remainder, match, std::regex(R"(^\[[0-9a-zA-z:%]{3,}\])")))
    {
        BUG_ON (match.size() == 0);
        offset = match[0].str().size();
        remoteHost = match[0].str().substr(1, offset - 2);
    }
    else if (std::regex_search (remainder, match, std::regex(R"(^[a-zA-Z0-9.-]+)")))
    {
        BUG_ON (match.size() == 0);
        offset = match[0].str().size();
        remoteHost = match[0].str();
    }
    else
        throw std::invalid_argument (s);

    remainder = remainder.substr(offset);
    if (remainder.size() > 0)
    {
        if (remainder.at(0) != ':')
            throw std::invalid_argument ("expected ':'");
        remainder = remainder.substr(1);
        if (!std::regex_search (remainder, match, std::regex("^" + regexRangeList)))
            throw std::invalid_argument (remainder);
        BUG_ON (match.size() == 0);
        offset = match[0].str().size();

        remotePorts = cValueParser::rangeList (match[0].str());
    }
}
// https://regex101.com/r/tirg4A/4
//(^((tcp|udp|ip|sctp):\/\/)?(([0-9\.]+)|(\[[0-9a-fA-F:]+\])|([a-zA-Z0-9\-\.]+))(:[0-9][0-9,\-]*)?(:(([0-9\.]+)|(\[[0-9a-fA-F:]+\])))?(:[0-9]+)?)$
/*
tcp://[123:1:222::1]
udp://1.2.3.4
sctp://fritz.box
ip://t-online.de:1234-1235,66:888
tcp://[1.2.3.4]
udp://[localhost]
www.google.de*/