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
    addCmdLineOption (true, 'v', "verbose",
            "When parsing and printing, produce verbose output. This option can be supplied multiple times\n\t"
            "(max. 4 times, i.e. -vvvv) for even more debug output. "
            , &m_options.verbosity);
    addCmdLineOption (true, 'l', "listen", "PORT",
            "Listen for incoming connections on PORT.", &m_options.serverPort);
    addCmdLineOption (true, 'i', "interval", "TIME",
            "Wait TIME in seconds between sending packets. Default is 0.0s.", &m_options.interval);
    addCmdLineOption (true, 'c', "count", "COUNT",
            "Stop after sending and receiving COUNT packets.", &m_options.count);
    addCmdLineOption (true, 't', "time", "SECONDS",
            "Stop after running SECONDS.", &m_options.time);
    addCmdLineOption (true, 0, "buf-size", "BYTES",
            "Set internal buffer for send/receive to BYTES (default 64k)", &m_options.sockBufSize);
}

cApplication::~cApplication ()
{

}

int cApplication::execute (const std::list<std::string>& args)
{
    bool isServer = !!m_options.serverPort;
    uint64_t interval_us = 0;;

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
        if (interval < 0.000001) // microseconds
        {
            Console::PrintError ("Invalid interval value '%s'\n", m_options.interval);
            return -2;
        }
        interval_us = (uint64_t)(interval * 1000000.0);
    }

    if (m_options.sockBufSize < 64)
    {
        Console::PrintError ("Invalid socket buffer size '%d'\n", m_options.sockBufSize);
        return -2;
    }
/*
    struct sigaction act;
    std::memset (&act, 0, sizeof(act));
    act.sa_handler = sigintHandler;
    act.sa_flags = 0;
    if (sigaction (SIGINT, &act, NULL) == -1)
    {
        return -3;
    }
*/
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
        cClient client(*args.cbegin(), (uint16_t)dport, (uint16_t)2 /*TODO*/,
            interval_us, (unsigned)m_options.count, (unsigned)m_options.time, (unsigned)m_options.sockBufSize);
//        sleep (10);
//        client.finish();

    }
    else
    {
        cTcpListener ((uint16_t)m_options.serverPort, (unsigned)m_options.sockBufSize);
    }

    return 0;
}

void cApplication::sigintHandler (int signal)
{
    if (signal == SIGINT)
    {
            Console::PrintError ("X\n");
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
