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

#include "MonitorDriver.h"
#include "NebulaLog.h"

using namespace std;
using namespace std::placeholders; // for _1

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

MonitorDriver::MonitorDriver(MonitorDriverManager* mdm,
                             udp_streamer_t*       udp,
                             HostRPCPool*          host_pool,
                             VMRPCPool*            vm_pool)
    : dm(mdm)
    , udp_stream(udp)
    , hpool(host_pool)
    , vmpool(vm_pool)
{
    register_messages();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::register_messages()
{
    dm->register_action(MonitorDriverMessages::UNDEFINED, bind(&MonitorDriver::_monitor_undefined, _1));
    dm->register_action(MonitorDriverMessages::MONITOR_VM, bind(&MonitorDriver::_monitor_vm, this, _1));
    dm->register_action(MonitorDriverMessages::MONITOR_HOST, bind(&MonitorDriver::_monitor_host, this, _1));
    dm->register_action(MonitorDriverMessages::SYSTEM_HOST, bind(&MonitorDriver::_system_host, this, _1));
    dm->register_action(MonitorDriverMessages::STATE_VM, bind(&MonitorDriver::_state_vm, this, _1));

    udp_stream->register_action(MonitorDriverMessages::UNDEFINED, bind(&MonitorDriver::_monitor_undefined, _1));
    udp_stream->register_action(MonitorDriverMessages::MONITOR_VM, bind(&MonitorDriver::_monitor_vm, this, _1));
    udp_stream->register_action(MonitorDriverMessages::MONITOR_HOST, bind(&MonitorDriver::_monitor_host, this, _1));
    udp_stream->register_action(MonitorDriverMessages::SYSTEM_HOST, bind(&MonitorDriver::_system_host, this, _1));
    udp_stream->register_action(MonitorDriverMessages::STATE_VM, bind(&MonitorDriver::_state_vm, this, _1));

    oned_reader.register_action(OpenNebulaMessages::UNDEFINED, bind(&MonitorDriver::_undefined, _1));
    oned_reader.register_action(OpenNebulaMessages::UPDATE_HOST, bind(&MonitorDriver::_update_host, this, _1));
    oned_reader.register_action(OpenNebulaMessages::DEL_HOST, bind(&MonitorDriver::_del_host, this, _1));
    oned_reader.register_action(OpenNebulaMessages::START_MONITOR, bind(&MonitorDriver::_start_monitor, this, _1));
    oned_reader.register_action(OpenNebulaMessages::STOP_MONITOR, bind(&MonitorDriver::_stop_monitor, this, _1));
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::start()
{
    // -----------------------------------------------------------
    // Create the monitor loop
    // -----------------------------------------------------------

    NebulaLog::log("MON", Log::INFO, "Starting monitor loop...");

    monitor_thread = thread(&MonitorDriver::thread_execute, this);

    // -----------------------------------------------------------
    // Start oned reader loop
    // -----------------------------------------------------------

    start_driver(); // blocking call

    //signal monitor_thread
    terminate = true;
    monitor_thread.join();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_undefined(unique_ptr<Message<OpenNebulaMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received UNDEFINED msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_update_host(unique_ptr<Message<OpenNebulaMessages>> msg)
{
    auto h = hpool->get(msg->oid());
    if (h)
    {
        h->from_xml(msg->payload());

        string str_state;
        NebulaLog::debug("MON", "Host updated id = " + to_string(h->oid())
            + ", state = " + Host::state_to_str(str_state, h->state()));
    }
    else
    {
        hpool->add_object(msg->payload());
    }

    // todo Thread safety, lock here or inside pools?
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_del_host(unique_ptr<Message<OpenNebulaMessages>> msg)
{
    hpool->erase(msg->oid());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_start_monitor(unique_ptr<Message<OpenNebulaMessages>> msg)
{
    int oid = msg->oid();
    auto host = hpool->get(oid);
    if (!host)
    {
        NebulaLog::warn("MON", "Received start_monitor for unknown host id = " + to_string(oid));
        return;
    }

    bool update_remotes;
    istringstream iss(msg->payload());
    iss >> update_remotes;

    if (iss.fail())
    {
        NebulaLog::warn("MON", "Wrong format of start monitor msg: " + msg->payload());
        return;
    }

    dm->start_monitor(host, true);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_stop_monitor(unique_ptr<Message<OpenNebulaMessages>> msg)
{
    int oid = msg->oid();
    string name;
    string im_mad;

    Template data;
    data.from_xml(msg->payload());
    if (!data.get("NAME", name) || !data.get("IM_MAD", im_mad))
    {
        NebulaLog::warn("MON", "Wrong format of stop monitor msg: " + msg->payload());
        return;
    }

    dm->stop_monitor(oid, name, im_mad);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_monitor_undefined(unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received UNDEFINED msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_monitor_vm(unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received MONITOR_VM msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_monitor_host(unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "process_monitor_host: " + to_string(msg->oid()));

    try
    {
        auto msg_string = msg->payload();
        char* error_msg;
        Template tmpl;
        int rc = tmpl.parse(msg_string, &error_msg);

        if (rc != 0)
        {
            NebulaLog::error("MON", string("Unable to parse monitoring template") + error_msg);
            free(error_msg);
            return;
        }

        int oid = msg->oid();
        if (!tmpl.get("OID", oid))
        {
            NebulaLog::error("MON", "Monitor information does not contain host id");
            return;
        };

        auto host = hpool->get(oid);
        if (!host)
        {
            NebulaLog::warn("MON", "Received monitoring for unknown host" + to_string(oid));
            return;
        }

        string result;
        tmpl.get("RESULT", result);
        if (result != "SUCCESS")
        {
            // todo Handle monitor failure
            return;
        }

        HostMonitoringTemplate monitoring;

        monitoring.timestamp(time(nullptr));

        if (monitoring.from_template(tmpl) != 0 || monitoring.oid() == -1)
        {
            NebulaLog::log("MON", Log::ERROR, "Error parsing monitoring msg: " + msg->payload());
            return;
        }

        if (hpool->update_monitoring(monitoring) != 0)
        {
            NebulaLog::log("MON", Log::ERROR, "Unable to write monitoring to DB");
            return;
        };

        host->last_monitored(monitoring.timestamp());

        NebulaLog::log("MON", Log::INFO, "Monitoring succesfully written to DB");

        // Send host state update to oned
        if (host->state() != Host::HostState::MONITORED &&
            host->state() != Host::HostState::DISABLED)
        {
            string state;
            Host::state_to_str(state, Host::HostState::MONITORED);

            Message<OpenNebulaMessages> oned_msg;
            oned_msg.type(OpenNebulaMessages::HOST_STATE);
            oned_msg.oid(msg->oid());
            oned_msg.payload(state);
            write2one(oned_msg);
        }
    }
    catch(const exception &e)
    {
        NebulaLog::log("MON", Log::ERROR, string("process_monitor_host: ") + e.what());
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_system_host(unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received SYSTEM_HOST msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void MonitorDriver::_state_vm(unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received STATE_VM msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
