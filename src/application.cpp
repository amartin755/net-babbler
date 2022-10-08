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

#include <cstring>
#include "application.hpp"
#include "client.hpp"
#include "tcplistener.hpp"

volatile std::sig_atomic_t cApplication::sigIntStatus;

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
    addCmdLineOption (true, 'i', "--interval", "TIME",
            "Wait TIME in seconds between sending packets. Default is 0.0s.", &m_options.interval);
}

cApplication::~cApplication ()
{

}

int cApplication::execute (const std::list<std::string>& args)
{
    bool isServer = !!m_options.serverPort;
    uint64_t interval_us;
 
    switch (m_options.verbosity)
    {
    case 1:
        Console::SetPrintLevel(Console::Verbose);
        break;
    case 2:
        Console::SetPrintLevel(Console::MoreVerbose);
        break;
    case 3:
        Console::SetPrintLevel(Console::MostVerbose);
        break;
    case 4:
        Console::SetPrintLevel(Console::Debug);
        break;
    }

    if (m_options.interval)
    {
        double interval = std::stod (m_options.interval);
        if (interval >= 1.0) // seconds
            interval_us = (uint64_t)(interval * 1000000.0);
        else if (interval >= 0.001) // miliseconds
            interval_us = (uint64_t)(interval * 1000.0);
        else if (interval >= 0.000001) // microseconds
            interval_us = (uint64_t)interval;
        else
        {
            Console::PrintError ("Invalid interval value '%s'\n", m_options.interval);
            return -2;
        }
    }


//    std::signal (SIGINT, sigintHandler);

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
        cClient (*args.cbegin(), (uint16_t)dport, (uint16_t)2, interval_us);
    }
    else
    {
        cTcpListener ((uint16_t)m_options.serverPort);
    }

    return 0;
}

void cApplication::sigintHandler (int signal)
{
    if (signal == SIGINT)
    {
        sigIntStatus++;
    }
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
