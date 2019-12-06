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

#ifndef MONITOR_DRIVER_MANAGER_H_
#define MONITOR_DRIVER_MANAGER_H_

#include "DriverManager.h"
#include "MonitorDriverMessages.h"
#include "HostBase.h"

class MonitorDriverManager : public DriverManager<MonitorDriverMessages>
{
public:
    explicit MonitorDriverManager(
            const string& mad_location)
        : DriverManager(mad_location)
    {
    }

    /**
     *  Start monitoring agent
     */
    int start_monitor(HostBase* host, bool update_remotes);

    /**
     *  Start monitoring agent
     */
    int stop_monitor(int hid, const string& host_name, const string& im_mad);

};

#endif // MONITOR_DRIVER_MANAGER_H_
