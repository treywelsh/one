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
#include "NebulaLog.h"

#include "HostRPCPool.h"
#include "HostMonitorManager.h"
#include "Monitor.h"

#include "Driver.h"
#include "DriverManager.h"
#include "MonitorDriver.h"
#include "UDPMonitorDriver.h"
#include "OneMonitorDriver.h"

#include <condition_variable>
#include <chrono>

using namespace std;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

HostMonitorManager::HostMonitorManager(
        HostRPCPool *      hp,
        const std::string& addr,
        unsigned int       port,
        unsigned int       threads,
        const std::string& driver_path)
    : hpool(hp)
    , udp_threads(threads)
    , timer_period(30)
{
    oned_driver    = new OneMonitorDriver(this);
    udp_driver     = new UDPMonitorDriver(addr, port);
    driver_manager = new driver_manager_t(driver_path);
};

HostMonitorManager::~HostMonitorManager()
{
    delete oned_driver;
    delete udp_driver;
    delete driver_manager;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int HostMonitorManager::load_monitor_drivers(
        const vector<const VectorAttribute*>& mads_config)
{
    return driver_manager->load_drivers(mads_config);
}

int HostMonitorManager::start(std::string& error)
{
    condition_variable end_cv;
    mutex end_mtx;
    bool  end = false;

    //Start Monitor drivers and associated listener threads
    if ( driver_manager->start(error) != 0 )
    {
        return -1;
    }

    //Start UDP listener threads
    if ( udp_driver->action_loop(udp_threads, error) == -1 )
    {
        return -1;
    }

    //Start the timer action thread
    thread timer_thr = thread([&, this]{
        unique_lock<mutex> end_lck(end_mtx);

        while(!end_cv.wait_for(end_lck, chrono::seconds(timer_period),
                    [&](){return end;}))
        {
            timer_action();
        }
    });

    //Wait for oned messages. FINALIZE will end the driver
    oned_driver->start_driver();

    //End timer thread
    {
        lock_guard<mutex> end_lck(end_mtx);
        end = true;
    }

    end_cv.notify_one();

    timer_thr.join();

    //End UDP listener threads
    udp_driver->stop();

    //End monitor drivers
    driver_manager->stop();

    return 0;
}

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

    auto driver = driver_manager->get_driver(host->im_mad());

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

    auto driver = driver_manager->get_driver(host->im_mad());

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

        oned_driver->host_state(oid, state);
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostMonitorManager::timer_action()
{
    static int mark = 0;
    static int tics = timer_period;

    mark += timer_period;
    tics += timer_period;

    if ( mark >= 600 )
    {
        NebulaLog::info("HMM", "--Mark--");
        mark = 0;
    }

    hpool->clean_expired_monitoring();
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
