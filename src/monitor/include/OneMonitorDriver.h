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

#ifndef ONE_MONITOR_DRIVER_H_
#define ONE_MONITOR_DRIVER_H_

#include "HostMonitorManager.h"

#include "OpenNebulaDriver.h"
#include "OpenNebulaMessages.h"

/**
 *  This class implements the Monitor Driver interface for oned.
 */
class OneMonitorDriver : public OpenNebulaDriver<OpenNebulaMessages>
{
public:
    OneMonitorDriver(HostMonitorManager * _hm)
    {
        hm = _hm;

        register_action(OpenNebulaMessages::UNDEFINED,
                &OneMonitorDriver::_undefined);

        register_action(OpenNebulaMessages::UPDATE_HOST,
                &OneMonitorDriver::_update_host);

        register_action(OpenNebulaMessages::DEL_HOST,
                &OneMonitorDriver::_del_host);

        register_action(OpenNebulaMessages::START_MONITOR,
                &OneMonitorDriver::_start_monitor);

        register_action(OpenNebulaMessages::STOP_MONITOR,
                &OneMonitorDriver::_stop_monitor);
    }

    /**
     *  Send a host state message to oned
     *    @param oid host id
     *    @param state for the host
     */
    void host_state(int oid, const std::string& state)
    {
        Message<OpenNebulaMessages> oned_msg;

        oned_msg.type(OpenNebulaMessages::HOST_STATE);
        oned_msg.oid(oid);
        oned_msg.payload(state);

        write2one(oned_msg);
    }

private:
    using message_t = std::unique_ptr<Message<OpenNebulaMessages>>;

    //--------------------------------------------------------------------------
    // Message callbacks, implements the driver protocol
    //--------------------------------------------------------------------------
    static void _undefined(message_t msg)
    {
        NebulaLog::info("MON", "Received UNDEFINED msg: " + msg->payload());
    }

    /**
     *  Update information from a host
     */
    static void _update_host(message_t msg)
    {
        hm->update_host(msg->oid(), msg->payload());
    }

    /**
     *  Remove host from the pool
     */
    static void _del_host(message_t msg)
    {
        hm->delete_host(msg->oid());
    }

    /**
     *  Start the monitor agent/ or active monitor the host
     */
    static void _start_monitor(message_t msg)
    {
        hm->start_host_monitor(msg->oid());
    }

    /**
     *  Stop the monitor agent/ or stop monitor the host
     */
    static void _stop_monitor(message_t msg)
    {
        hm->stop_host_monitor(msg->oid());
    }

private:
    static HostMonitorManager* hm;
};

#endif // MONITOR_DRIVER_H_
