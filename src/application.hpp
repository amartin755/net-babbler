// SPDX-License-Identifier: GPL-3.0-only
/*
 * TCPPUMP <https://github.com/amartin755/tcppump>
 * Copyright (C) 2012-2021 Andreas Martin (netnag@mailbox.org)
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
#include "cmdlineapp.hpp"

typedef struct
{
    const char*  serverIP;
    int          verbosity;
    int          serverPort;
    int          udp;
}appOptions;


class cApplication : public cCmdlineApp
{
public:
    cApplication (const char* name, const char* brief, const char* usage, const char* description);
    virtual ~cApplication();

    int execute (const std::list<std::string>& args);

private:

    appOptions m_options;
};

#endif /* APPLICATION_HPP */
