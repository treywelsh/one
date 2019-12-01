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

#include "DriverManager.h"
#include "MonitorDriver.h"
#include "NebulaLog.h"
#include "NebulaService.h"

using namespace std;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int DriverManager::load_drivers(vector<const VectorAttribute*> &conf)
{
    NebulaLog::info("DrM", "Loading drivers.");

    for (const auto& vattr : conf)
    {
        auto name = vattr->vector_value("NAME");
        auto exec = vattr->vector_value("EXECUTABLE");
        auto args = vattr->vector_value("ARGUMENTS");
        int  threads;

        vattr->vector_value("THREADS", threads, 0);

        NebulaLog::info("InM", "Loading driver: " + name);

        if (exec.empty())
        {
            NebulaLog::error("InM", "\tEmpty executable for driver: " + name);
            return -1;
        }

        if (exec[0] != '/') //Look in ONE_LOCATION/lib/mads or in "/usr/lib/one/mads"
        {
            exec = NebulaService::instance().get_mad_location() + exec;
        }

        if (access(exec.c_str(), F_OK) != 0)
        {
            NebulaLog::error("InM", "File not exists: " + exec);
            return -1;
        }

        auto driver = new driver_t(exec, args, threads);

        auto rc = drivers.insert({name, driver});

        if (rc.second)
        {
            NebulaLog::info("InM", "\tDriver laoded: " + name);
        }
        else
        {
            NebulaLog::error("InM", "\tDriver already exists: " + name);
            return -1;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

driver_t* DriverManager::get_driver(const std::string& name)
{
    auto driver = drivers.find(name);

    if (driver == drivers.end())
    {
        return nullptr;
    }

    return driver->second;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void DriverManager::register_action(MonitorDriverMessages t,
    std::function<void(std::unique_ptr<Message<MonitorDriverMessages>>)> a)
{
    for (auto driver : drivers)
    {
        driver.second->register_action(t, a);
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int DriverManager::start()
{
    string error;
    for (auto driver : drivers)
    {
        auto rc = driver.second->start(error);
        if (rc != 0)
        {
            NebulaLog::error("DrM", "Unable to start driver: " + error);
            return rc;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void DriverManager::stop()
{
    for (auto driver : drivers)
    {
        driver.second->stop();
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int DriverManager::start_monitor(HostBase* host, bool update_remotes)
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

int DriverManager::stop_monitor(HostBase* host)
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
