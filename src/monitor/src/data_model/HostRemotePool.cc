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

#include "HostRemotePool.h"
#include "NebulaLog.h"

const char * monit_table = "host_monitoring";
const char * monit_db_names = "hid, last_mon_time, body";

int HostRemotePool::load_info(xmlrpc_c::value &result)
{
    try
    {
        client->call("one.hostpool.info", "", &result);

        return 0;
    }
    catch (exception const& e)
    {
        ostringstream   oss;
        oss << "Exception raised: " << e.what();

        NebulaLog::log("HOST", Log::ERROR, oss);

        return -1;
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int HostRemotePool::update_monitoring(HostBase* h)
{
    auto sql_xml = db->escape_str(h->to_xml());

    if (sql_xml == 0)
    {
        NebulaLog::log("HPL", Log::WARNING, "Could not transform Host to XML");
        return -1;
    }

    if (ObjectXML::validate_xml(sql_xml) != 0)
    {
        NebulaLog::log("HPL", Log::WARNING, "Could not transform Host to XML" + string(sql_xml));
        db->free_str(sql_xml);
        return -1;
    }

    ostringstream oss;
    oss << "REPLACE INTO " << monit_table << " ("<< monit_db_names <<") VALUES ("
        <<          h->get_id()             << ","
        <<          h->get_last_monitored() << ","
        << "'" <<   sql_xml                 << "')";

    db->free_str(sql_xml);

    auto rc = db->exec_local_wr(oss);

    return rc;
}
