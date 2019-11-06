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

#ifndef STREAM_MANAGER_H
#define STREAM_MANAGER_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <thread>
#include <memory>
#include <string>
#include <functional>

#include "Message.h"

template <typename E>
class MessageAction
{
public:
    virtual void operator()(std::unique_ptr<Message<E> > ms) const = 0;

protected:
};

typedef MessageAction<DriverMessages> DriverAction;

typedef MessageAction<OnedMessages> OnedAction;

/**
 *  This class manages a stream to process MonitorMessages. The StreamManager
 *  thread reads from the stream for input messages and executed the associated
 *  action in a separated (detached) thread.
 */
#define STREAM_MANAGER_BUFFER_SIZE 512

template <typename E>
class StreamManager
{
public:
    StreamManager(int _fd):fd(_fd)
    {
        buffer  = (char *) malloc(STREAM_MANAGER_BUFFER_SIZE * sizeof(char));
    };

    ~StreamManager()
    {
        free(buffer);

        close(fd);
    };

    using msg_callback_t = std::function<void(std::unique_ptr<Message<E>>)>;
    void register_action(E t, msg_callback_t a)
    {
        actions.insert({t, a});
    }

    void do_action(std::unique_ptr<Message<E> >& msg)
    {
        const auto it = actions.find(msg->type());

        if (it == actions.end())
        {
            return;
        }

        const auto action = it->second;
        Message<E> * mptr = msg.release();

        std::thread action_thread([=]{
            action(std::unique_ptr<Message<E>>{mptr});
        });

        action_thread.detach();
    }

private:
    int fd;

    std::map<E, msg_callback_t> actions;

    char * buffer;

    int read_line(std::string& line)
    {
        static size_t cur_sz  = STREAM_MANAGER_BUFFER_SIZE;

        char * cur_ptr = buffer;
        size_t line_sz = 0;

        do
        {
            int rc = ::read(fd, (void *) cur_ptr, cur_sz - line_sz - 1);

            if ( rc <= 0 )
            {
                return -1;
            }

            cur_ptr[rc] = '\0';

            line_sz += rc;

            if ( strchr(cur_ptr, '\n') == 0)
            {
                cur_sz += STREAM_MANAGER_BUFFER_SIZE;

                buffer  = (char *) realloc((void *) buffer, cur_sz);
                cur_ptr = buffer + line_sz;

                continue;
            }

            line.assign(buffer, line_sz + 1);

            return 0;
        }
        while (true);
    }
};

typedef StreamManager<DriverMessages> DriverStream;

typedef StreamManager<OnedMessages> OnedStream;

#endif /*STREAM_MANAGER_H*/
