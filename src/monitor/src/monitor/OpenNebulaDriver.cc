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

#include "OpenNebulaDriver.h"


OpenNebulaDriver::OpenNebulaDriver()
    : oned_reader(0, [](std::unique_ptr<Message<OpenNebulaMessages>> ms) {
            NebulaLog::log("MON", Log::WARNING, "Received undefined message: " + ms->payload());
        })
{
    using namespace std::placeholders; // for _1
    oned_reader.register_action(OpenNebulaMessages::INIT, bind(&OpenNebulaDriver::process_init, this, _1));
    oned_reader.register_action(OpenNebulaMessages::FINALIZE, bind(&OpenNebulaDriver::process_finalize, this, _1));
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void OpenNebulaDriver::start_driver()
{
    oned_reader.action_loop(false);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void OpenNebulaDriver::process_init(std::unique_ptr<Message<OpenNebulaMessages>> msg)
{
    write2one("INIT SUCCESS\n");
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void OpenNebulaDriver::process_finalize(std::unique_ptr<Message<OpenNebulaMessages>> msg)
{
    write2one("FINALIZE SUCCESS\n");
    stop_driver();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void OpenNebulaDriver::stop_driver()
{
    terminate = true;

    // Close comminication pipes
    close(0);
    close(1);
}

