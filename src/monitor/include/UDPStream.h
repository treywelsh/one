/* -------------------------------------------------------------------------- */
/* Copyright 2002-2019, OpenNebula Project, OpenNebula Systems                */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */

#ifndef UDP_STREAM_H
#define UDP_STREAM_H

#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#include "StreamManager.h"

/**
 *
 */
template<typename E>
class UDPStream : public StreamManager<E>
{
public:
    using callback_t = std::function<void(std::unique_ptr<Message<E>>)>;

    UDPStream(const std::string &address, unsigned int port, callback_t error):
        StreamManager<E>(error), _socket(-1), _address(address), _port(port)
    {
    }

    UDPStream(const std::string &address, unsigned int port):
        _socket(-1), _address(address), _port(port)
    {
    }

    virtual ~UDPStream() = default;

    /**
     *  This functions initializes the UDP socket for the stream. It must be
     *  called once before using the streamer
     *    @param threads number od threads to listen on the socket
     */
    int action_loop(int threads, std::string& error);

protected:

    int read_line(std::string& line) override;

private:
    int _socket;

    std::string _address;

    short unsigned int _port;
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* UDPStream Implementation                                                   */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
template<typename E>
int UDPStream<E>::action_loop(int threads, std::string& error)
{
    struct sockaddr_in udp_server;
    int rc;

    /* ---------------------------------------------------------------------- */
    /* Create UDP socket for incoming driver messages                         */
    /* ---------------------------------------------------------------------- */
    _socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if ( _socket < 0 )
    {
        error = strerror(errno);
        return -1;
    }

    udp_server.sin_family = AF_INET;
    udp_server.sin_port   = htons(_port);

    if (_address == "0.0.0.0")
    {
        udp_server.sin_addr.s_addr = htonl (INADDR_ANY);
    }
    else if (inet_pton(AF_INET, _address.c_str(), &udp_server.sin_addr.s_addr) < 0)
    {
        error = strerror(errno);
        return -1;
    }

    rc = bind(_socket, (struct sockaddr *) &udp_server, sizeof(struct sockaddr_in));

    if ( rc < 0 )
    {
        error = strerror(errno);
        return -1;
    }

    StreamManager<E>::fd(_socket);

    /* ---------------------------------------------------------------------- */
    /* Start a pool of threads to read incoming UDP driver messages           */
    /* ---------------------------------------------------------------------- */

    for (int i = 0 ; i < threads; ++i)
    {
        std::thread action_thread([this]{
            while (true)
            {
                std::string line;

                if (read_line(line) != 0)
                {
                    return -1;
                }

                if (line.empty())
                {
                    continue;
                }

                std::unique_ptr<Message<E>> msg{new Message<E>};

                msg->parse_from(line, false);

                StreamManager<E>::do_action(msg, false);
            }
        });

        action_thread.detach();
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
#define MESSAGE_SIZE 65536

template<typename E>
int UDPStream<E>::read_line(std::string& line)
{
    char   buffer[MESSAGE_SIZE];

    struct sockaddr addr;
    socklen_t addr_size = sizeof(struct sockaddr);

    size_t rc = recvfrom(_socket, buffer, MESSAGE_SIZE, 0, &addr, &addr_size);

    if (rc > 0 && rc < MESSAGE_SIZE)
    {
        line.assign(buffer, rc);
    }
    else
    {
        line.clear();
    }

    return 0;
}

#endif /*UDP_STREAM_H*/
