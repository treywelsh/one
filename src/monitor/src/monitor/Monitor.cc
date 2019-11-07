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
#include "MonitorTemplate.h"
#include "NebulaLog.h"
#include "Client.h"
#include "StreamManager.h"

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

using namespace std;

void Monitor::start()
{
    // System directories
    string log_file;
    string etc_path;

    const char *nl = getenv("ONE_LOCATION");

    if (nl == 0) // OpenNebula installed under root directory
    {
        log_file = "/var/log/one/monitor.log";
        etc_path = "/etc/one/";
    }
    else
    {
        log_file = string(nl) + "/var/monitor.log";
        etc_path = string(nl) + "/etc/";
    }

    // Configuration File
    MonitorTemplate conf(etc_path);
    if ( conf.load_configuration() != 0 )
    {
        throw runtime_error("Error reading configuration file.");
    }
    // todo load configuration values

    // Log system
    NebulaLog::LogType log_system = NebulaLog::STD;
    Log::MessageType clevel = Log::WARNING;

    const VectorAttribute *log = conf.get("LOG");

    if (log != 0)
    {
        string value;
        int ilevel;

        value      = log->vector_value("SYSTEM");
        log_system = NebulaLog::str_to_type(value);

        value  = log->vector_value("DEBUG_LEVEL");
        ilevel = atoi(value.c_str());

        if (Log::ERROR <= ilevel && ilevel <= Log::DDDEBUG)
        {
            clevel = static_cast<Log::MessageType>(ilevel);
        }
    }

    if (log_system != NebulaLog::UNDEFINED)
    {
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
    oss << conf;
    oss << "----------------------------------------";

    NebulaLog::log("MON", Log::INFO, oss);

    // -----------------------------------------------------------
    // XML-RPC Client
    // -----------------------------------------------------------
    {
        string       one_xmlrpc;
        long long    message_size;
        unsigned int timeout;

        conf.get("ONE_XMLRPC", one_xmlrpc);
        conf.get("MESSAGE_SIZE", message_size);
        conf.get("TIMEOUT", timeout);

        Client::initialize("", one_xmlrpc, message_size, timeout);

        oss.str("");

        oss << "XML-RPC client using " << (Client::client())->get_message_size()
            << " bytes for response buffer.\n";

        NebulaLog::log("MON", Log::INFO, oss);
    }

    xmlInitParser();

    // -------------------------------------------------------------------------
    // Pools
    // -------------------------------------------------------------------------
    // int machines_limit = 100;
    // conf.get("MAX_VM", machines_limit);

    hpool.reset(new HostRemotePool());
    vmpool.reset(new VMRemotePool());

    // -----------------------------------------------------------
    // Close stds, we no longer need them
    // -----------------------------------------------------------
    if (NebulaLog::log_type() != NebulaLog::STD )
    {
        int fd = open("/dev/null", O_RDWR);

        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);

        close(fd);

        fcntl(0, F_SETFD, 0); // Keep them open across exec funcs
        fcntl(1, F_SETFD, 0);
        fcntl(2, F_SETFD, 0);
    }
    else
    {
        fcntl(0, F_SETFD, FD_CLOEXEC);
        fcntl(1, F_SETFD, FD_CLOEXEC);
        fcntl(2, F_SETFD, FD_CLOEXEC);
    }

    // -----------------------------------------------------------
    // Block all signals before creating any Nebula thread
    // -----------------------------------------------------------
    sigset_t    mask;
    int         signal;

    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    one_util::SSLMutex::initialize();

    // -----------------------------------------------------------
    // Create the monitor loop
    // -----------------------------------------------------------

    NebulaLog::log("MON", Log::INFO, "Starting monitor loop...");

    monitor_thread.reset(new std::thread(&Monitor::thread_execute, this));
    // pthread_attr_t pattr;
    // pthread_attr_init(&pattr);
    // pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE);

    // int rc = pthread_create(&monitor_thread, &pattr, scheduler_action_loop, (void *) this);

    // if (rc != 0)
    // {
    //     NebulaLog::log("MON", Log::ERROR,
    //         "Could not start monitor loop, exiting");

    //     return;
    // }

    // -----------------------------------------------------------
    // Wait for a SIGTERM or SIGINT signal
    // -----------------------------------------------------------

    sigemptyset(&mask);

    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    sigwait(&mask, &signal);

    terminate = true;
    //am.finalize();

    monitor_thread->join();

    xmlCleanupParser();

    NebulaLog::finalize_log_system();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::thread_execute()
{
    // Do initial pull from oned, then read only changes
    hpool->update();

    // Maybe it's not necessery to have two threads (Monitor::thread_execute
    // and OnedStream::run), we will see later
    OnedStream oned_reader(0);
    using namespace std::placeholders; // for _1
    oned_reader.register_action(OnedMessages::ADD_HOST, std::bind(&Monitor::process_add_host, this, _1));
    oned_reader.register_action(OnedMessages::DEL_HOST, std::bind(&Monitor::process_del_host, this, _1));
    auto oned_msg_thread = std::thread(&OnedStream::run, &oned_reader);

    while (!terminate)
    {
        const auto& hosts = hpool->get_objects();
        NebulaLog::log("MON", Log::INFO, "Number of hosts = " + std::to_string(hosts.size()));
        for (const auto& o : hosts)
        {
            NebulaLog::log("MON", Log::INFO, "\t" + o.second->get_name());
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

    oned_reader.stop();
    oned_msg_thread.join();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_add_host(std::unique_ptr<OnedMessage> msg)
{
    // todo Thread safety, lock here or inside pools?
    hpool->add_object(msg->payload());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Monitor::process_del_host(std::unique_ptr<OnedMessage> msg)
{
    HostBase host(msg->payload());

    hpool->erase(host.get_id());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
