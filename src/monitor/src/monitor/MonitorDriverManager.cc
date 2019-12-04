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

#include "MonitorDriverManager.h"

using namespace std;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int MonitorDriverManager::start_monitor(HostBase* host, bool update_remotes)
{
    NebulaLog::debug("DrM", "Monitoring host id: " + to_string(host->oid()));

    auto driver = get_driver(host->im_mad());

    if (!driver)
    {
        NebulaLog::error("DrM", "Could not find information driver " + host->im_mad());

        //host->set_error();

        return -1;
    }

    // host->set_monitoring_state(); todo ??

    // Nebula::instance().get_ds_location(dsloc); todo ?? Not needed for kvm push probe

    // driver->monitor(host->oid(), host->name(), "", update_remotes);
    ostringstream oss;
    oss << "MONITOR " << host->oid() << " " << host->name() << " " << "not_defined" << " " << update_remotes << endl;
    driver->write(oss.str());

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int MonitorDriverManager::stop_monitor(HostBase* host)
{
    NebulaLog::debug("DrM", "Stop monitoring host id: " + to_string(host->oid()));

    auto driver = get_driver(host->im_mad());

    if (!driver)
    {
        NebulaLog::error("DrM", "Could not find information driver " + host->im_mad());
        return -1;
    }

    ostringstream oss;
    oss << "STOPMONITOR " << host->oid() << " " << host->name() << " " << endl;
    driver->write(oss.str());

    return 0;
}
