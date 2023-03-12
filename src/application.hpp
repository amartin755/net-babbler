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


#ifndef APPLICATION_HPP
#define APPLICATION_HPP


#include <list>
#include <cstddef>
#include <csignal>

#include "cmdlineapp.hpp"

struct appOptions
{
    const char*  serverIP;
    int          verbosity;
    const char*  serverPorts;
    const char*  interval;
    int          count;
    int          time;
    int          sockBufSize;
    int          clientConnections;
    int          statusUpdateTime;
    int          ipv4Only;
    int          ipv6Only;
    const char*  comSettings;

    appOptions () :
        serverIP (nullptr),
        verbosity (0),
        serverPorts (nullptr),
        interval (nullptr),
        count (0),
        time (0),
        sockBufSize (1024*64),
        clientConnections (1),
        statusUpdateTime (3),
        ipv4Only (0),
        ipv6Only (0),
        comSettings ("1230,1400,12340,13500")
    {
    }
};

class cStats;

class cApplication : public cCmdlineApp
{
public:
    cApplication (const char* name, const char* brief, const char* usage, const char* description, const char* version,
        const char* build, const char* buildDetails);
    virtual ~cApplication();

    int execute (const std::list<std::string>& args);

private:
    void printStatistics (const cStats& stats, unsigned duration) const;
    void printStatistics (const cStats& stats, unsigned duration, const cStats& stats2, unsigned duration2) const;
    appOptions m_options;
};

#endif /* APPLICATION_HPP */
