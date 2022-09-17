// SPDX-License-Identifier: GPL-3.0-only
/*
 * NET-BABBLER <https://github.com/amartin755/tcppump>
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

#include <cstring>
#include "application.hpp"
#include "client.hpp"

cApplication::cApplication(const char* name, const char* brief, const char* usage, const char* description)
: cCmdlineApp (name, brief, usage, description)
{
    std::memset (&m_options, 0, sizeof(m_options));

    addCmdLineOption (true, 'v', "verbose",
            "When parsing and printing, produce verbose output. This option can be supplied multiple times\n\t"
            "(max. 4 times, i.e. -vvvv) for even more debug output. "
            , &m_options.verbosity);
    addCmdLineOption (true, 'l', "listen", "PORT",
            "Listen for incoming connections on PORT.", &m_options.serverPort);
    addCmdLineOption (true, 'u', "udp",
            "Use UDP instead of TCP.",
            &m_options.udp);
}

cApplication::~cApplication ()
{

}

int cApplication::execute (const std::list<std::string>& args)
{
    bool isServer = !!m_options.serverPort;

    if (!isServer)
    {
        if (args.size() != 2)
        {
            Console::PrintError ("Missing destination and port \n");
            return -2;
        }
        auto a = args.cbegin(); a++;
        int dport = std::atoi ((*a).c_str());
        if (dport < 1 || dport > 0xffff)
        {
            Console::PrintError ("Invalid port number\n");
            return -2;
        }
        cClient (*args.cbegin(), (uint16_t)dport, (uint16_t)2);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    cApplication app (
            "tcppump",
            "An Ethernet packet generator",
            "tcppump [OPTIONS] [destination] [port]",
            "Homepage: <https://github.com/amartin755/tcppump>");
    return app.main (argc, argv);
}
