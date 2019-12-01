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

#ifndef DRIVER_MANAGER_H_
#define DRIVER_MANAGER_H_

#include "MonitorDriver.h"
#include "Attribute.h"
#include "HostBase.h"
#include <string>

class DriverManager
{
public:
    DriverManager() {}

    int load_drivers(std::vector<const VectorAttribute*> &conf);

    driver_t* get_driver(const std::string& name);

    /**
     *  Register an action for a given message type. The action is registered
     *  for all installed drivers. Must be called after load_drivers method.
     */
    void register_action(MonitorDriverMessages t,
        std::function<void(std::unique_ptr<Message<MonitorDriverMessages>>)> a);

    /**
     *  Start all drivers
     */
    int start();

    /**
     *  Stop all drivers
     */
    void stop();

    int start_monitor(HostBase* host, bool update_remotes);

    int stop_monitor(HostBase* host);

private:
    std::map<std::string, driver_t*> drivers;
};

#endif // DRIVER_MANAGER_H_
