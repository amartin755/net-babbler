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

#ifndef SOCKET_HPP
#define SOCKET_HPP

// tcp: AF_INET/AF_INET6, SOCK_STREAM, 0
// udp: AF_INET/AF_INET6, SOCK_DGRAM, 0
// raw IP: AF_INET/AF_INET6, SOCK_RAW, proto 
// sctp: AF_INET/AF_INET6, SOCK_STREAM/SOCK_SEQPACKET, IPPROTO_SCTP 
// dccp: AF_INET/AF_INET6, SOCK_DCCP, IPPROTO_DCCP

class cSocket
{
public:
    cSocket (int domain, int type, int protocol);
    ~cSocket ();
    void bind (const struct sockaddr *adr, socklen_t adrlen);
    cSocket accept (struct sockaddr *restrict adr,
                  socklen_t *restrict adrlen);
private:
    void throwException ();
    
private:
    int m_fd;
};

#endif