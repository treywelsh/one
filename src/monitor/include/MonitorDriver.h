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

#ifndef MONITOR_DRIVER_H_
#define MONITOR_DRIVER_H_

#include <thread>

#include "HostRPCPool.h"
#include "VMRPCPool.h"
#include "MonitorDriverManager.h"
#include "MonitorDriverMessages.h"
#include "OpenNebulaDriver.h"
#include "OpenNebulaMessages.h"

class MonitorDriver : public OpenNebulaDriver
{
public:
    MonitorDriver(MonitorDriverManager* mdm,
                  udp_streamer_t*       udp,
                  HostRPCPool*          host_pool,
                  VMRPCPool*            vm_pool);

    void start();

protected:
    /**
     *  Register callbacks for messages
     */
    void register_messages();

    /**
     *  The main execution loop, handles the monitoring logic
     */
    void thread_execute();

    /**
     *  Reads data from oned
     *   @return false if the OpenNebula service is not available
     **/
    bool pull_from_oned();

    // Oned message handlers
    static void _undefined(std::unique_ptr<Message<OpenNebulaMessages>> msg);
    void _update_host(std::unique_ptr<Message<OpenNebulaMessages>> msg);
    void _del_host(std::unique_ptr<Message<OpenNebulaMessages>> msg);
    void _start_monitor(std::unique_ptr<Message<OpenNebulaMessages>> msg);
    void _stop_monitor(std::unique_ptr<Message<OpenNebulaMessages>> msg);

    // Monitor driver message handlers
    static void _monitor_undefined(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void _monitor_vm(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void _monitor_host(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void _system_host(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void _state_vm(std::unique_ptr<Message<MonitorDriverMessages>> msg);

private:
    std::unique_ptr<std::thread> monitor_thread;
    std::atomic<bool>            terminate{false};

    MonitorDriverManager* dm;

    /**
     *  Stream receiving UDP data from monitor agents.
     */
    udp_streamer_t* udp_stream;

    // ---------------------------------------------------------------
    // Pools
    // ---------------------------------------------------------------
    HostRPCPool* hpool;
    VMRPCPool*   vmpool;
};

#endif // MONITOR_DRIVER_H_
