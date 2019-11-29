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

#ifndef MONITOR_H_
#define MONITOR_H_

#include <thread>

#include "NebulaService.h"
#include "HostRPCPool.h"
#include "VMRPCPool.h"
#include "DriverManager.h"
#include "MonitorDriver.h"
#include "OpenNebulaDriver.h"
#include "OpenNebulaStream.h"

class Monitor : public NebulaService, public OpenNebulaDriver
{
public:
    /**
     *  Read configuration file and starts monitornig. (Blocking call)
     */
    void start();

    /**
     *  The main execution loop, handles the monitoring logic
     */
    void thread_execute();

protected:
    /**
     *  Reads data from oned
     *   @return false if the OpenNebula service is not available
     **/
    bool pull_from_oned();

    // Oned message handlers
    void process_add_host(std::unique_ptr<Message<OpenNebulaMessages>> msg);
    void process_del_host(std::unique_ptr<Message<OpenNebulaMessages>> msg);

    // Monitor driver message handlers
    void process_monitor_undefined(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void process_monitor_vm(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void process_monitor_host(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void process_system_host(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void process_state_vm(std::unique_ptr<Message<MonitorDriverMessages>> msg);
    void process_undefined(std::unique_ptr<Message<MonitorDriverMessages>> msg);

private:
    /**
     *  Stop the driver, stop all running threads
     **/
    void stop();

    std::unique_ptr<std::thread> monitor_thread;

    // ---------------------------------------------------------------
    // Pools
    // ---------------------------------------------------------------
    std::unique_ptr<HostRPCPool> hpool;
    std::unique_ptr<VMRPCPool>   vmpool;

    std::unique_ptr<SqlDB> sqlDB;

    std::unique_ptr<DriverManager> dm;

    /**
     *  Stream receiving UDP data from monitor agents.
     */
    std::unique_ptr<udp_streamer_t> udp_stream;
};

#endif // MONITOR_H_
