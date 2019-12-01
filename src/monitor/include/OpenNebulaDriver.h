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

#ifndef _OPENNEBULA_DRIVER_H
#define _OPENNEBULA_DRIVER_H

#include <atomic>
#include <string>
#include <unistd.h>

#include "OpenNebulaStream.h"
#include "NebulaLog.h"

/**
 * Class providing interface to OpenNebula deamon
 * To handle oned messages register callbacks in oned_reader
 */
class OpenNebulaDriver
{
public:

    OpenNebulaDriver() : oned_reader(0, &OpenNebulaDriver::_undefined)
    {
        oned_reader.register_action(OpenNebulaMessages::INIT,
                &OpenNebulaDriver::_init);

        oned_reader.register_action(OpenNebulaMessages::FINALIZE,
                &OpenNebulaDriver::_finalize);
    }

    virtual ~OpenNebulaDriver() = default;

    /**
     * Start reading messages from oned, blocking call. Messages will be
     * processed sequentially in the main thread.
     */
    void start_driver()
    {
        oned_reader.action_loop(0);
    }

    // TODO method for write Message<E> to oned

protected:
    using message_t = std::unique_ptr<Message<OpenNebulaMessages>>;

    /**
     *  Streamer for stdin
     */
    one_stream_t oned_reader;

    /**
     * Process INIT message from oned, send SUCCESS response
     */
    static void _init(message_t msg)
    {
        write2one("INIT SUCCESS\n");
    }

    /**
     * Process FINALIZE message from oned, send SUCCESS response,
     * terminate execution loop
     */
    static void _finalize(message_t msg)
    {
        write2one("FINALIZE SUCCESS\n");

        close(0); //will end start_driver()
    }

    /**
     * Default action for undefined messages
     */
    static void _undefined(message_t msg)
    {
        NebulaLog::log("MON", Log::WARNING, "Undefined message: " + msg->payload());
    }

    /**
     * Write string message to oned
     */
    static void write2one(const std::string& buf)
    {
        write(1, buf.c_str(), buf.size());
    }
};

#endif // _OPENNEBULA_DRIVER_H
