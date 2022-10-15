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
    int          serverPort;
    const char*  interval;
    int          count;
    int          time;
    int          sockBufSize;
    int          clientConnections;

    appOptions () :
        serverIP (nullptr),
        verbosity (0),
        serverPort (0),
        interval (nullptr),
        count (0),
        time (0),
        sockBufSize (1024*64),
        clientConnections (1)
    {
    }
};


class cApplication : public cCmdlineApp
{
public:
    cApplication (const char* name, const char* brief, const char* usage, const char* description);
    virtual ~cApplication();

    int execute (const std::list<std::string>& args);

private:
    static void sigintHandler (int signal);
    static volatile std::sig_atomic_t sigIntStatus;

    appOptions m_options;
};

#endif /* APPLICATION_HPP */
