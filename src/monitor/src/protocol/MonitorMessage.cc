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

#include <sstream>

#include "MonitorMessage.h"

const EString<MonitorMessage::Type> MonitorMessage::type_str({
        {"MONITOR_VM", VMMONITOR},
        {"MONITOR_HOST", HOSTMONITOR},
        {"SYSTEM_HOST", SYSTEM_HOST},
        {"STATE_VM", STATE_VM}
        });

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int MonitorMessage::parse_from(const std::string& input)
{
    std::istringstream is(message);

    if ( is.good() )
    {
        std::string type_s;

        is >> type_s >> ws;

        type = type_str._from_str(type_s);

        if ( type == UNDEFINED )
        {
            return -1;
        }

    }
    else
    {
        return;
    }


}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorMessage::write_to(std::string& output) const;

void MonitorMessage::write_to(int fd) const;

void MonitorMessage::write_to(std::ostream) const;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
