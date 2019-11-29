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
#include "StreamManager.h"

/**
 * Class providing interface to OpenNebula deamon
 * To handle oned messages register callbacks in oned_reader
 */
class OpenNebulaDriver
{
public:

    OpenNebulaDriver();

    virtual ~OpenNebulaDriver()
    {
        stop_driver();
    }

    /**
     * Start reading messages from oned, blocking call
     */
    void start_driver();

    // todo method for write Message<E> to oned

protected:

    /**
     * Process INIT message from oned, send SUCCESS response
     */
    void process_init(std::unique_ptr<Message<OpenNebulaMessages>> msg);

    /**
     * Process FINALIZE message from oned, send SUCCESS response,
     * terminate execution loop
     */
    void process_finalize(std::unique_ptr<Message<OpenNebulaMessages>> msg);

    /**
     * Write string message to oned
     */
    void write2one(const std::string& buf) const
    {
        write(1, buf.c_str(), buf.size());
    }

    /**
     * Stops the driver main execution loop
     */
    void stop_driver();

    std::atomic<bool> terminate{false};

    one_stream_t oned_reader;
};

#endif // _OPENNEBULA_DRIVER_H
