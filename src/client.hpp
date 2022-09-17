#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <thread>

class cClient
{
public:
    cClient (const std::string &server, uint16_t remotePort, uint16_t localPort);
    ~cClient ();

    void threadFunc ();

private:
    std::thread*  m_thread;
    std::string   m_server;
    uint16_t      m_remotePort;
    uint16_t      m_localPort;

};

#endif