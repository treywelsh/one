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

#include "monitor.h"
#include "MonitorConfigTemplate.h"
#include "NebulaLog.h"
#include "Client.h"
#include "StreamManager.h"
#include "MonitorDriver.h"
#include "SqliteDB.h"
#include "MySqlDB.h"

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

using namespace std;
using namespace std::placeholders; // for _1

void Monitor::start()
{
    // Configuration File
    config = new MonitorConfigTemplate(get_defaults_location());
    if (config->load_configuration() != 0)
    {
        throw runtime_error("Error reading configuration file.");
    }

    // Log system
    NebulaLog::LogType log_system = get_log_system(NebulaLog::STD);
    Log::MessageType clevel = get_debug_level(Log::WARNING);

    if (log_system != NebulaLog::UNDEFINED)
    {
        string log_file = get_log_location() + "monitor.log";
        NebulaLog::init_log_system(log_system,
                                   clevel,
                                   log_file.c_str(),
                                   ios_base::trunc,
                                   "one_monitor");
    }
    else
    {
        throw runtime_error("Unknown LOG_SYSTEM.");
    }

    NebulaLog::log("MON", Log::INFO, "Init Monitor Log system");


    ostringstream oss;
    oss << "Starting Monitor Daemon" << endl;
    oss << "----------------------------------------\n";
    oss << "       Monitor Configuration File       \n";
    oss << "----------------------------------------\n";
    oss << *config;
    oss << "----------------------------------------";

    NebulaLog::log("MON", Log::INFO, oss);

    // -----------------------------------------------------------
    // XML-RPC Client
    // -----------------------------------------------------------
    {
        string       one_xmlrpc;
        long long    message_size;
        unsigned int timeout;

        config->get("ONE_XMLRPC", one_xmlrpc);
        config->get("MESSAGE_SIZE", message_size);
        config->get("TIMEOUT", timeout);

        Client::initialize("", one_xmlrpc, message_size, timeout);

        oss.str("");

        oss << "XML-RPC client using " << (Client::client())->get_message_size()
            << " bytes for response buffer.\n";

        NebulaLog::log("MON", Log::INFO, oss);
    }

    xmlInitParser();

    // -----------------------------------------------------------
    // Database
    // -----------------------------------------------------------
    {
        string db_backend_type = "sqlite";
        string server;
        int    port;
        string user;
        string passwd;
        string db_name;
        string encoding;
        int    connections;

        const VectorAttribute * _db = config->get("DB");

        if (_db != 0)
        {
            db_backend_type = _db->vector_value("BACKEND");

            _db->vector_value<string>("SERVER", server, "localhost");
            _db->vector_value("PORT", port, 0);
            _db->vector_value<string>("USER", user, "oneadmin");
            _db->vector_value<string>("PASSWD", passwd, "oneadmin");
            _db->vector_value<string>("DB_NAME", db_name, "opennebula");
            _db->vector_value<string>("ENCODING", encoding, "");
            _db->vector_value("CONNECTIONS", connections, 50);
        }

        if (db_backend_type == "sqlite")
        {
            sqlDB.reset(new SqliteDB(get_var_location() + "one.db"));
        }
        else
        {
            sqlDB.reset(new MySqlDB(server, port, user, passwd, db_name, encoding, connections));
        }
    }

    // -------------------------------------------------------------------------
    // Pools
    // -------------------------------------------------------------------------
    // int machines_limit = 100;
    // config->get("MAX_VM", machines_limit);

    hpool.reset(new HostRPCPool(sqlDB.get()));
    vmpool.reset(new VMRPCPool(sqlDB.get()));

    // -------------------------------------------------------------------------
    // Drivers
    // -------------------------------------------------------------------------
    vector<const VectorAttribute *> drivers_conf;

    config->get("IM_MAD", drivers_conf);

    dm.reset(new MonitorDriverManager());

    if (dm->load_drivers(drivers_conf) != 0)
    {
        NebulaLog::log("MON", Log::ERROR, "Unable to load drivers configuration");
        return;
    }

    dm->register_action(MonitorDriverMessages::UNDEFINED, std::bind(&Monitor::process_undefined, this, _1));
    dm->register_action(MonitorDriverMessages::MONITOR_VM, std::bind(&Monitor::process_monitor_vm, this, _1));
    dm->register_action(MonitorDriverMessages::MONITOR_HOST, std::bind(&Monitor::process_monitor_host, this, _1));
    dm->register_action(MonitorDriverMessages::SYSTEM_HOST, std::bind(&Monitor::process_system_host, this, _1));
    dm->register_action(MonitorDriverMessages::STATE_VM, std::bind(&Monitor::process_state_vm, this, _1));

    if (dm->start() < 0)
    {
        NebulaLog::log("MON", Log::ERROR, "Unable to start DriverManager, exiting");
        return;
    }

    // -----------------------------------------------------------
    // UDP action listener
    // -----------------------------------------------------------
    {
        std::string error;
        std::string address = "0.0.0.0";
        unsigned int port = 4124;
        unsigned int threads = 16;

        auto udp_conf = config->get("UDP_LISTENER");
        if (udp_conf)
        {
            udp_conf->vector_value("ADDRESS", address, address);
            udp_conf->vector_value("PORT", port, port);
            udp_conf->vector_value("THREADS", threads, threads);
        }

        udp_stream.reset(new udp_streamer_t(address, port, std::bind(&Monitor::process_undefined, this, _1)));
        udp_stream->register_action(MonitorDriverMessages::MONITOR_VM, std::bind(&Monitor::process_monitor_vm, this, _1));
        udp_stream->register_action(MonitorDriverMessages::MONITOR_HOST, std::bind(&Monitor::process_monitor_host, this, _1));
        udp_stream->register_action(MonitorDriverMessages::SYSTEM_HOST, std::bind(&Monitor::process_system_host, this, _1));
        udp_stream->register_action(MonitorDriverMessages::STATE_VM, std::bind(&Monitor::process_state_vm, this, _1));
        if (udp_stream->action_loop(threads, error) != 0)
        {
            NebulaLog::error("MON", "Unable to init UDP action listener: " + error);
            return;
        }
    }

    // Close stds in drivers
    fcntl(0, F_SETFD, FD_CLOEXEC);
    fcntl(1, F_SETFD, FD_CLOEXEC);
    fcntl(2, F_SETFD, FD_CLOEXEC);

    one_util::SSLMutex::initialize();

    // -----------------------------------------------------------
    // Create the monitor loop
    // -----------------------------------------------------------

    NebulaLog::log("MON", Log::INFO, "Starting monitor loop...");

    monitor_thread.reset(new std::thread(&Monitor::thread_execute, this));

    // -----------------------------------------------------------
    // Start oned reader loop
    // -----------------------------------------------------------

    oned_reader.register_action(OpenNebulaMessages::UPDATE_HOST, bind(&Monitor::process_update_host, this, _1));
    oned_reader.register_action(OpenNebulaMessages::DEL_HOST, bind(&Monitor::process_del_host, this, _1));
    start_driver(); // blocking call

    //signal monitor_thread
    terminate = true;
    monitor_thread->join();

    dm->stop();

    xmlCleanupParser();

    NebulaLog::finalize_log_system();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::thread_execute()
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
        NebulaLog::log("MON", Log::INFO, "Number of hosts = " + std::to_string(hosts.size()));
        for (const auto& o : hosts)
        {
            NebulaLog::log("MON", Log::INFO, "\t" + o.second->name());
        }

        // vmpool->update();
        // auto vms = vmpool->get_objects();
        // NebulaLog::log("MON", Log::INFO, "Number of VMs = " + std::to_string(vms.size()));
        // for (auto o : vms)
        // {
        //     NebulaLog::log("MON", Log::INFO, "\t" + o.second->get_name());
        // }

        sleep(5);
    }

    for (auto& host : hpool->get_objects())
    {
        dm->stop_monitor(static_cast<HostBase*>(host.second.get()));
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

bool Monitor::pull_from_oned()
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

void Monitor::process_update_host(std::unique_ptr<Message<OpenNebulaMessages>> msg)
{
    auto h = hpool->get(msg->oid());
    if (h)
    {
        h->from_xml(msg->payload());
    }
    else
    {
        hpool->add_object(msg->payload());
    }

    // todo Thread safety, lock here or inside pools?
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_del_host(std::unique_ptr<Message<OpenNebulaMessages>> msg)
{
    hpool->erase(msg->oid());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_monitor_undefined(std::unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received UNDEFINED msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_monitor_vm(std::unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received MONITOR_VM msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_monitor_host(std::unique_ptr<Message<MonitorDriverMessages>> msg)
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
    }
    catch(const std::exception &e)
    {
        NebulaLog::log("MON", Log::ERROR, string("process_monitor_host: ") + e.what());
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_system_host(std::unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received SYSTEM_HOST msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_state_vm(std::unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received STATE_VM msg: " + msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_undefined(std::unique_ptr<Message<MonitorDriverMessages>> msg)
{
    NebulaLog::log("MON", Log::INFO, "Received UNDEFINED msg: " + msg->payload());
}
