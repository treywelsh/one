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

#include "MonitorDriverProtocol.h"

#include "NebulaLog.h"
#include "HostMonitorManager.h"

HostMonitorManager * MonitorDriverProtocol::hm = nullptr;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriverProtocol::_undefined(message_t msg)
{
    NebulaLog::info("MDP", "Received UNDEFINED msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriverProtocol::_monitor_vm(message_t msg)
{

}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriverProtocol::_monitor_host(message_t msg)
{
    NebulaLog::ddebug("MDP", "Received monitoring for host: " +
            to_string(msg->oid()));

    std::string msg_str = msg->payload();
    char * error_msg;

    Template tmpl;
    int rc = tmpl.parse(msg_str, &error_msg);

    if (rc != 0)
    {
        NebulaLog::error("MDP", string("Error parsing monitoring template: ")
                + error_msg);

        free(error_msg);
        return;
    }

    bool result = msg->status() == "SUCCESS" ? true : false;

    hm->monitor_host(msg->oid(), result, tmpl);
}

void MonitorDriverProtocol::_system_host(message_t msg)
{

}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriverProtocol::_state_vm(message_t msg)
{

}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
