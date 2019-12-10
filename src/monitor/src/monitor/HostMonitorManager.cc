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

#include "HostMonitorManager.h"
#include "NebulaLog.h"

using namespace std;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostMonitorManager::update_host(int oid, const std::string &xml)
{
    auto host = hpool->get(oid);

    if (host)
    {
        host->from_xml(xml);

        string str_state;
        NebulaLog::debug("HMM", "Updated Host " + to_string(host->oid())
            + ", state " + Host::state_to_str(str_state, host->state()));
    }
    else
    {
        hpool->add_object(xml);
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostMonitorManager::delete_host(int oid)
{
    hpool->erase(oid);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostMonitorManager::start_host_monitor(int oid)
{
    auto host = hpool->get(oid);

    if (!host)
    {
        NebulaLog::warn("HMM", "start_monitor: unknown host " + to_string(oid));
        return;
    }

    auto driver = driver_manager.get_driver(host->im_mad());

    if (!driver)
    {
        NebulaLog::error("HMM", "start_monitor: Cannot find driver " + host->im_mad());
        return;
    }

    NebulaLog::debug("HMM", "Monitoring host " +host->name() + "("
            + to_string(host->oid()) + ")");

    string xml = host->to_xml();

    driver->start_monitor(oid, xml);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostMonitorManager::stop_host_monitor(int oid)
{
    auto host = hpool->get(oid);

    if (!host)
    {
        NebulaLog::warn("HMM", "stop_monitor: unknown host " + to_string(oid));
        return;
    }

    auto driver = driver_manager.get_driver(host->im_mad());

    if (!driver)
    {
        NebulaLog::error("HMM", "stop_monitor: Cannot find driver " + host->im_mad());
        return;
    }

    NebulaLog::debug("HMM", "Stopping Monitoring on host " +host->name() + "("
            + to_string(host->oid()) + ")");

    string xml = host->to_xml();

    driver->stop_monitor(oid, xml);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostMonitorManager::monitor_host(int oid, Template &tmpl)
{
    string str;

    auto host = hpool->get(oid);

    if (!host)
    {
        NebulaLog::warn("HMM", "monitor_host: unknown host " + to_string(oid));
        return;
    }

    tmpl.get("RESULT", str);

    if (str != "SUCCESS")
    {
        // TODO Handle monitor failure
        return;
    }

    HostMonitoringTemplate monitoring;

    monitoring.timestamp(time(nullptr));

    if (monitoring.from_template(tmpl) != 0 || monitoring.oid() == -1)
    {
        NebulaLog::log("MON", Log::ERROR, "Error parsing monitoring template: "
                + tmpl.to_str(str));
        return;
    }

    if (hpool->update_monitoring(monitoring) != 0)
    {
        NebulaLog::log("MON", Log::ERROR, "Unable to write monitoring to DB");
        return;
    };

    host->last_monitored(monitoring.timestamp());

    NebulaLog::info("MON", "Successfully monitored host: " + to_string(oid));

    // Send host state update to oned
    if (host->state() != Host::HostState::MONITORED &&
        host->state() != Host::HostState::DISABLED)
    {
        string state;
        Host::state_to_str(state, Host::HostState::MONITORED);

        oned_driver.host_state(oid, state);
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/*
void MonitorDriver::thread_execute()
{
    // Initial pull from oned
    if (pull_from_oned())
    {
        NebulaLog::info("MON", "Succesfully connected to oned");
    }
    else
    {
        NebulaLog::error("MON", "Unable to connect to oned!");
    }

    // Replace this loop with some timer action,
    // which should do the monitoring logic
    while (!terminate)
    {
        const auto& hosts = hpool->get_objects();
        NebulaLog::log("MON", Log::INFO, "Number of hosts = " + to_string(hosts.size()));
        for (const auto& o : hosts)
        {
            NebulaLog::log("MON", Log::INFO, "\t" + o.second->name());
        }

        // vmpool->update();
        // auto vms = vmpool->get_objects();
        // NebulaLog::log("MON", Log::INFO, "Number of VMs = " + to_string(vms.size()));
        // for (auto o : vms)
        // {
        //     NebulaLog::log("MON", Log::INFO, "\t" + o.second->get_name());
        // }

        sleep(5);
    }

    for (auto& host : hpool->get_objects())
    {
        auto h = static_cast<HostBase*>(host.second.get());
        dm->stop_monitor(h->oid(), h->name(), h->im_mad());
    }
}
*/

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/*
bool MonitorDriver::pull_from_oned()
{
    int oned_connected = -1;
    int retries = 5;
    do
    {
        sleep(2);
        oned_connected = hpool->update();
        if (oned_connected != 0 && retries-- > 0)
        {
            NebulaLog::warn("MON", "Unable to connect to oned, trying again");
        }
    } while (oned_connected != 0 && retries > 0);

    for (auto& host : hpool->get_objects())
    {
        dm->start_monitor(static_cast<HostBase*>(host.second.get()), true);
    }

    return oned_connected == 0;
}
*/
