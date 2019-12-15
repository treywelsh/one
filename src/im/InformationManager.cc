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

#include "InformationManager.h"
#include "HostPool.h"
#include "OpenNebulaMessages.h"

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int InformationManager::start()
{
    std::string error;

    using namespace std::placeholders; // for _1

    register_action(OpenNebulaMessages::UNDEFINED, &InformationManager::_undefined);

    register_action(OpenNebulaMessages::HOST_STATE,
            bind(&InformationManager::_host_state, this, _1));

    int rc = DriverManager::start(error);

    if ( rc != 0 )
    {
        NebulaLog::error("InM", "Error starting Information Manager: " + error);
        return -1;
    }

    NebulaLog::info("InM", "Starting Information Manager...");

    im_thread = std::thread([&] {
        NebulaLog::info("InM", "Information Manager started.");

        am.loop(timer_period);

        NebulaLog::info("InM", "Information Manager stopped.");
    });

    am.trigger(ActionRequest::USER);

    return rc;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void InformationManager::stop_monitor(int hid, const string& name, const string& im_mad)
{
    auto * imd = get_driver("monitord");

    if (!imd)
    {
        NebulaLog::error("InM", "Could not find information driver 'monitor'");

        return;
    }

    Template data;
    data.add("NAME", name);
    data.add("IM_MAD", im_mad);
    string tmp;

    Message<OpenNebulaMessages> msg;
    msg.type(OpenNebulaMessages::STOP_MONITOR);
    msg.oid(hid);
    msg.payload(data.to_xml(tmp));
    imd->write(msg);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int InformationManager::start_monitor(Host * host, bool update_remotes)
{
    ostringstream oss;

    oss << "Monitoring host "<< host->get_name()<< " ("<< host->get_oid()<< ")";
    NebulaLog::log("InM",Log::DEBUG,oss);

    auto imd = get_driver("monitord");

    if (!imd)
    {
        host->error("Cannot find driver: 'monitor'");
        return -1;
    }

    Message<OpenNebulaMessages> msg;
    msg.type(OpenNebulaMessages::START_MONITOR);
    msg.oid(host->get_oid());
    msg.payload(to_string(update_remotes));
    imd->write(msg);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void InformationManager::update_host(Host *host)
{
    auto imd = get_driver("monitord");

    if (!imd)
    {
        return;
    }

    string tmp;
    Message<OpenNebulaMessages> msg;
    msg.type(OpenNebulaMessages::UPDATE_HOST);
    msg.oid(host->get_oid());
    msg.payload(host->to_xml(tmp));
    imd->write(msg);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void InformationManager::delete_host(int hid)
{
    auto imd = get_driver("monitord");

    if (!imd)
    {
        return;
    }

    Message<OpenNebulaMessages> msg;
    msg.type(OpenNebulaMessages::DEL_HOST);
    msg.oid(hid);
    imd->write(msg);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void InformationManager::_undefined(unique_ptr<Message<OpenNebulaMessages>> msg)
{
    NebulaLog::warn("InM", "Received undefined message: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void InformationManager::_host_state(unique_ptr<Message<OpenNebulaMessages>> msg)
{
    NebulaLog::warn("InM", "Received host_state message: " + msg->payload());

    string str_state = msg->payload();
    Host::HostState new_state;
    if (Host::str_to_state(str_state, new_state) != 0)
    {
        NebulaLog::warn("InM", "Unable to decode host state: " + str_state);
        return;
    }

    Host* host = hpool->get(msg->oid());

    if (host == nullptr)
    {
        return;
    }

    if (host->get_state() == Host::OFFLINE) // Should not receive any info
    {
        host->unlock();

        return;
    }

    if (host->get_state() != new_state)
    {
        host->set_state(new_state);

        hpool->update(host);
    }

    host->unlock();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void InformationManager::timer_action(const ActionRequest& ar)
{
    /*
    static int mark = 0;

    int    rc;
    time_t now;

    set<int>           discovered_hosts;
    set<int>::iterator it;


    Host * host;

    time_t monitor_length;
    time_t target_time;

    mark = mark + timer_period;

    if ( mark >= 600 )
    {
        NebulaLog::log("InM",Log::INFO,"--Mark--");
        mark = 0;
    }

    Nebula& nd          = Nebula::instance();
    RaftManager * raftm = nd.get_raftm();

    if ( !raftm->is_leader() && !raftm->is_solo() && !nd.is_cache() )
    {
        return;
    }

    hpool->clean_expired_monitoring();

    now = time(0);

    target_time = now - monitor_period;

    rc = hpool->discover(&discovered_hosts, host_limit, target_time);

    if ((rc != 0) || (discovered_hosts.empty() == true))
    {
        return;
    }

    for( it=discovered_hosts.begin() ; it!=discovered_hosts.end() ; ++it )
    {
        host = hpool->get(*it);

        if (host == 0)
        {
            continue;
        }

        monitor_length = now - host->get_last_monitored();

        switch (host->get_state())
        {
            // Not received an update in the monitor period.
            case Host::INIT:
            case Host::MONITORED:
            case Host::ERROR:
            case Host::DISABLED:
                start_monitor(host, (host->get_last_monitored() == 0));
                break;

            // Update last_mon_time to rotate HostPool::discover output. Update
            // monitoring values with 0s.
            case Host::OFFLINE:
                host->touch(true);
                hpool->update_monitoring(host);
                break;

            // Host is being monitored for more than monitor_expire secs.
            case Host::MONITORING_DISABLED:
            case Host::MONITORING_INIT:
            case Host::MONITORING_ERROR:
            case Host::MONITORING_MONITORED:
                if (monitor_length >= monitor_expire )
                {
                    start_monitor(host, (host->get_last_monitored() == 0));
                }
                break;
        }

        hpool->update(host);

        host->unlock();
    }
    */
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void InformationManager::user_action(const ActionRequest& ar)
{
    auto * imd = get_driver("monitord");
    if (!imd)
    {
        NebulaLog::error("InM", "Could not find information driver 'monitor'");

        return;
    }

    string xml_hosts;
    hpool->dump(xml_hosts, "", "", false);

    Message<OpenNebulaMessages> msg;
    msg.type(OpenNebulaMessages::HOST_LIST);
    msg.payload(xml_hosts);
    imd->write(msg);
}

// Old processing of monitoring message
// void MonitorThread::do_message()
// {
//     // -------------------------------------------------------------------------
//     // Decode from base64, check if it is compressed
//     // -------------------------------------------------------------------------
//     string* hinfo = one_util::base64_decode(hinfo64);
//     string* zinfo = one_util::zlib_decompress(*hinfo, false);

//     if ( zinfo != 0 )
//     {
//         delete hinfo;

//         hinfo = zinfo;
//     }

//     Host* host = hpool->get(host_id);

//     if ( host == 0 )
//     {
//         delete hinfo;
//         return;
//     }

//     if ( host->get_state() == Host::OFFLINE ) //Should not receive any info
//     {
//         delete hinfo;

//         host->unlock();

//         return;
//     }

//     // -------------------------------------------------------------------------
//     // Monitoring Error. VMs running on the host are moved to UNKNOWN
//     // -------------------------------------------------------------------------
//     if (result != "SUCCESS")
//     {
//         set<int> vm_ids = host->get_vm_ids();

//         host->error(*hinfo);

//         for (const auto& vm_id : vm_ids)
//         {
//             lcm->trigger(LCMAction::MONITOR_DONE, vm_id);
//         }

//         delete hinfo;

//         hpool->update(host);

//         host->unlock();

//         return;
//     }

//     // -------------------------------------------------------------------------
//     // Parse Moniroting Information
//     // -------------------------------------------------------------------------
//     Template tmpl;
//     char*    error_msg;

//     if ( tmpl.parse(*hinfo, &error_msg) != 0 )
//     {
//         ostringstream ess;

//         ess << "Parse error: " << error_msg;

//         host->error(ess.str());

//         free(error_msg);

//         delete hinfo;

//         return;
//     }

//     delete hinfo;

//     // -------------------------------------------------------------------------
//     // Label local system datastores to include them in the HostShare
//     // -------------------------------------------------------------------------
//     vector<VectorAttribute*> vector_ds;

//     tmpl.get("DS", vector_ds);

//     for (auto& ds : vector_ds)
//     {
//         int dsid;

//         int rc = ds->vector_value("ID", dsid);

//         if (rc != 0 || dsid == -1)
//         {
//             continue;
//         }

//         auto ds_ptr = dspool->get_ro(dsid);

//         if (ds_ptr == 0)
//         {
//             continue;
//         }

//         if (ds_ptr->get_type() == Datastore::SYSTEM_DS && !ds_ptr->is_shared())
//         {
//             ds->replace("LOCAL_SYSTEM_DS", true);
//         }

//         ds_ptr->unlock();
//     }

//     // -------------------------------------------------------------------------
//     // Update Host information
//     // -------------------------------------------------------------------------
//     if (host->update_info(tmpl) != 0)
//     {
//         host->unlock();

//         return;
//     }

//     hpool->update(host);

//     std::ostringstream oss;

//     oss << "Host " << host->get_name() << " (" << host->get_oid() << ")"
//         << " successfully monitored.";

//     NebulaLog::log("InM", Log::DEBUG, oss);

//     host->unlock();
// };
