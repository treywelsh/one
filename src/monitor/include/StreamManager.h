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

#include <map>
#include <thread>
#include <memory>

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
template <typename E>
class StreamManager
{
public:
    void register_action(E t,  MessageAction<E> * a)
    {
        actions.insert(std::pair<E, MessageAction<E> *>(t, a));
    }

    void do_action(std::unique_ptr<Message<E> >& msg)
    {
        auto const it = actions.find(msg->type());

        if (it == actions.end())
        {
            return;
        }

        const auto action = it->second;
        Message<E> * mptr = msg.release();

        std::thread action_thread([=]{
            std::unique_ptr<Message<E> > m(mptr);
            (*action)(std::move(m));
        });

        action_thread.detach();
    }

private:
    std::map<E, MessageAction<E> *> actions;
};

typedef StreamManager<DriverMessages> DriverStream;

typedef StreamManager<OnedMessages> OnedStream;

#endif /*STREAM_MANAGER_H*/
