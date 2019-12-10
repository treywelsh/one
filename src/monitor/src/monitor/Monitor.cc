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

#include "Monitor.h"
#include "MonitorDriver.h"
#include "MonitorConfigTemplate.h"
#include "NebulaLog.h"
#include "Client.h"
#include "StreamManager.h"
#include "SqliteDB.h"
#include "MySqlDB.h"

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

using namespace std;

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

    // Close stds in drivers
    fcntl(0, F_SETFD, FD_CLOEXEC);
    fcntl(1, F_SETFD, FD_CLOEXEC);
    fcntl(2, F_SETFD, FD_CLOEXEC);

    one_util::SSLMutex::initialize();

    // -------------------------------------------------------------------------
    // Drivers
    // -------------------------------------------------------------------------
    vector<const VectorAttribute *> drivers_conf;

    config->get("IM_MAD", drivers_conf);

    hm.reset(new HostMonitorManager(hpool.get(), get_mad_location()));

    if (hm->load_monitor_drivers(drivers_conf) != 0)
    {
        NebulaLog::error("MON", "Unable to load monitor drivers");
        return;
    }

    MonitorDriverProtocol::hm = hm.get();

    // -----------------------------------------------------------
    // UDP action listener
    // -----------------------------------------------------------
    /*
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

    udp_stream.reset(new udp_streamer_t(address, port));
    */


    if (hm->start() == -1)
    {
        NebulaLog::log("MON", Log::ERROR, "Unable to start drivers, exiting");
        return;
    }

    /*
    if (udp_stream->action_loop(threads, error) != 0)
    {
        NebulaLog::error("MON", "Unable to init UDP action listener: " + error);
        return;
    }
    */

    xmlCleanupParser();

    NebulaLog::finalize_log_system();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
