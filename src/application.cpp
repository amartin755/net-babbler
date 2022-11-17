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

#include <poll.h>
#include <cstring>
#include "bug.hpp"
#include "application.hpp"
#include "client.hpp"
#include "tcplistener.hpp"
#include "event.hpp"
#include "signal.hpp"
#include "valueformatter.hpp"
#include "comsettings.hpp"
#include "valueparser.hpp"



cApplication::cApplication(const char* name, const char* brief, const char* usage, const char* description)
: cCmdlineApp (name, brief, usage, description)
{
    addCmdLineOption (true, 'v', "verbose",
            "When parsing and printing, produce verbose output. This option can be supplied multiple times\n\t"
            "(max. 4 times, i.e. -vvvv) for even more debug output. "
            , &m_options.verbosity);
    addCmdLineOption (true, 'l', "listen", "PORT",
            "Listen for incoming connections on PORT.", &m_options.serverPorts);
    addCmdLineOption (true, '4', nullptr,
            "Use IPv4 addresses only.", &m_options.ipv4Only);
    addCmdLineOption (true, '6', nullptr,
            "Use IPv6 addresses only.", &m_options.ipv6Only);
    addCmdLineOption (true, 'i', "interval", "TIME",
            "Wait TIME in seconds between sending packets. Default is 0.0s.", &m_options.interval);
    addCmdLineOption (true, 'c', "count", "COUNT",
            "Stop after sending and receiving COUNT packets.", &m_options.count);
    addCmdLineOption (true, 't', "time", "SECONDS",
            "Stop after running SECONDS.", &m_options.time);
    addCmdLineOption (true, 0, "buf-size", "BYTES",
            "Set internal buffer for send/receive to BYTES (default 64k)", &m_options.sockBufSize);
    addCmdLineOption (true, 'n', nullptr, "CONNECTIONS",
            "Number of parallel connections to server.", &m_options.clientConnections);
    addCmdLineOption (true, 's', "status", "SECONDS",
            "Print every SECONDS periodic status (default 3s).", &m_options.statusUpdateTime);
    addCmdLineOption (true, 0, "proto-settings", "SETTINGS",
            "TODO, maybe split to single options", &m_options.comSettings);
}

cApplication::~cApplication ()
{

}

int cApplication::execute (const std::list<std::string>& args)
{
    bool isServer = !!m_options.serverPorts;
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

    if (!isServer)
    {
        if (args.size() != 1)
        {
            Console::PrintError ("Missing destination\n");
            return -2;
        }

        cSocket::Properties protocol;
        std::string remoteHost;
        std::list<std::pair<unsigned long, unsigned long>> remotePorts;
        std::string localAddress;
        uint16_t localPort=0;
        cValueParser::clientConnection (*args.cbegin(), protocol, remoteHost, remotePorts, localAddress, localPort);
        protocol.setIpFamily (!m_options.ipv6Only, !m_options.ipv4Only);

        cComSettings comSettings (m_options.comSettings);

        cSignal sigInt (SIGINT);
        cSignal sigAlarm (SIGALRM);
        cEvent evClientTerminated;
        std::list<cClient> clients;
        unsigned clientID = 1;
        auto ports = args.cbegin(); ports++;
        for (int n = 0; n < m_options.clientConnections; n++)
        {
            for (const auto& range : remotePorts)
            {
                for (auto dport = range.first; dport <= range.second; dport++)
                {
                    clients.emplace_back (clientID++, evClientTerminated, remoteHost,
                        (uint16_t)dport, localPort,
                        interval_us, (unsigned)m_options.count,
                        (unsigned)m_options.sockBufSize, comSettings,
                        protocol);
                }
            }
        }

        int runningClientThreads = clients.size();
        int remainingTime = m_options.time;
        int ticks = m_options.statusUpdateTime;
        struct pollfd pollfds[4];
        pollfds[0].fd = sigInt;
        pollfds[1].fd = evClientTerminated;
        pollfds[2].fd = STDIN_FILENO;
        pollfds[3].fd = sigAlarm;
        pollfds[0].events = POLLIN;
        pollfds[1].events = POLLIN;
        pollfds[2].events = POLLIN;
        pollfds[3].events = POLLIN;

        if (m_options.time)
            ticks = std::min (remainingTime, m_options.statusUpdateTime);

        alarm ((unsigned)ticks);

        while ((runningClientThreads > 0) &&
            (poll (pollfds, sizeof (pollfds) / sizeof (pollfds[0]), -1) > 0))
        {
            bool terminate = false;
            bool printStatus = false;

            if (pollfds[3].revents & POLLIN)
            {
                sigAlarm.wait ();
                printStatus = ticks == m_options.statusUpdateTime;
                if (m_options.time)
                {
                    remainingTime -= ticks;
                    BUG_ON (remainingTime < 0);
                    ticks = std::min (remainingTime, m_options.statusUpdateTime);
                    if (remainingTime <= 0)
                        terminate = true;
                }
                alarm ((unsigned)ticks);
            }
            if (pollfds[0].revents & POLLIN)
            {
                sigInt.wait();
                terminate = true;
            }
            if (pollfds[1].revents & POLLIN)
            {
                evClientTerminated.wait ();
                runningClientThreads--;
            }
            if (pollfds[2].revents & POLLIN)
            {
                Console::PrintError ("stdin\n");
            }

            if (terminate)
            {
                cClient::terminateAll ();
/*                for (auto &cl : clients)
                {
                    cl.terminate ();
                }*/
            }
            if (printStatus)
            {
                printStatus = false;
                for (auto &cl : clients)
                {
                    cStats statsDelta, statsSummary;
                    auto duration = cl.statistics (statsDelta, statsSummary);
//                        Console::Print ("[%.1f sec][%s]\n", duration.second /1000.0, cl.getConnDescr().c_str());
                    Console::Print ("\n[%u] [%s] [%.2f sec]\n", cl.getClientID(), cl.getConnDescr().c_str(), duration.second /1000.0);
                    printStatistics (statsDelta, duration.first, statsSummary, duration.second);
                }
            }
        }
        Console::Print ("\n- - - - - - - - - - - - - - - - - - - - - - - - -\n");
        cStats summaryAll;
        unsigned durationAll = 0;
        for (auto &cl : clients)
        {
            cStats statsDelta, statsSummary;
            auto duration = cl.statistics (statsDelta, statsSummary);
            Console::Print ("[%u][%s]\n", cl.getClientID(), cl.getConnDescr().c_str());
            printStatistics (statsSummary, duration.second);

            summaryAll  += statsSummary;
            durationAll += duration.second;
        }

        if (clients.size () > 1)
        {
            Console::Print ("[all]\n");
            printStatistics (summaryAll, durationAll / clients.size());
        }
    }
    else
    {
        cSemaphore maxConnThreadCount (1000);
        std::list<cTcpListener> servers;
        auto portList = cValueParser::rangeList (m_options.serverPorts);
        for (const auto& range : portList)
        {
            for (auto port = range.first; port <= range.second; port++)
            {
                servers.emplace_back (cSocket::Properties::tcp(!m_options.ipv6Only, !m_options.ipv4Only),
                    (uint16_t)port, (unsigned)m_options.sockBufSize, maxConnThreadCount);
                servers.emplace_back (cSocket::Properties::sctp(!m_options.ipv6Only, !m_options.ipv4Only),
                    (uint16_t)port, (unsigned)m_options.sockBufSize, maxConnThreadCount);
                servers.emplace_back (cSocket::Properties::dccp(!m_options.ipv6Only, !m_options.ipv4Only),
                    (uint16_t)port, (unsigned)m_options.sockBufSize, maxConnThreadCount);
            }
        }
    }

    return 0;
}


void cApplication::printStatistics (const cStats& stats, unsigned duration) const
{
    // avoid division by zero
    if (!duration)
        duration = 1;
    Console::Print (
        "requests: %8" PRIuFAST64 ", %9sB, %8sbit/s\n"
        "replies:  %8" PRIuFAST64 ", %9sB, %8sbit/s\n",
        stats.m_sentPackets,
        cValueFormatter::toHumanReadable(stats.m_sentOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats.m_sentOctets * 8 * 1000 / duration, false).c_str(),
        stats.m_receivedPackets,
        cValueFormatter::toHumanReadable(stats.m_receivedOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats.m_receivedOctets * 8 * 1000 / duration, false).c_str());
}

void cApplication::printStatistics (const cStats& stats, unsigned duration, const cStats& stats2, unsigned duration2) const
{
    // avoid division by zero
    if (!duration)
        duration = 1;
    Console::Print (
        "requests: %8" PRIuFAST64 ", %9sB, %8sbit/s | %8" PRIuFAST64 ", %9sB, %8sbit/s\n"
        "replies:  %8" PRIuFAST64 ", %9sB, %8sbit/s | %8" PRIuFAST64 ", %9sB, %8sbit/s\n",
        stats2.m_sentPackets,
        cValueFormatter::toHumanReadable(stats2.m_sentOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats2.m_sentOctets * 8 * 1000 / duration2, false).c_str(),
        stats.m_sentPackets,
        cValueFormatter::toHumanReadable(stats.m_sentOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats.m_sentOctets * 8 * 1000 / duration, false).c_str(),
        stats2.m_receivedPackets,
        cValueFormatter::toHumanReadable(stats2.m_receivedOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats2.m_receivedOctets * 8 * 1000 / duration2, false).c_str(),
        stats.m_receivedPackets,
        cValueFormatter::toHumanReadable(stats.m_receivedOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats.m_receivedOctets * 8 * 1000 / duration, false).c_str());
#if 0
    Console::Print (
        "requsts/replies: %" PRIuFAST64 "/%" PRIuFAST64 ", %sB/%sB, %s/%sbit/s\n",
        stats.m_sentPackets, stats.m_receivedPackets,
        cValueFormatter::toHumanReadable(stats.m_sentOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats.m_receivedOctets, true).c_str(),
        cValueFormatter::toHumanReadable(stats.m_sentOctets * 8 * 1000 / duration, false).c_str(),
        cValueFormatter::toHumanReadable(stats.m_receivedOctets * 8 * 1000 / duration, false).c_str());
#endif
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
