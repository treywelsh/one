/* ------------------------------------------------------------------------ */
/* Copyright 2002-2019, OpenNebula Project, OpenNebula Systems              */
/*                                                                          */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may  */
/* not use this file except in compliance with the License. You may obtain  */
/* a copy of the License at                                                 */
/*                                                                          */
/* http://www.apache.org/licenses/LICENSE-2.0                               */
/*                                                                          */
/* Unless required by applicable law or agreed to in writing, software      */
/* distributed under the License is distributed on an "AS IS" BASIS,        */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/* See the License for the specific language governing permissions and      */
/* limitations under the License.                                           */
/* -------------------------------------------------------------------------*/

// -----------------------------------------------------------------------------
// This file includes the SQL schema defintion for OpenNebula objects
// -----------------------------------------------------------------------------
namespace one_db
{
    /* ---------------------------------------------------------------------- */
    /* HOST TABLES                                                            */
    /* ---------------------------------------------------------------------- */
    const char * host_table = "host_pool";

    const char * host_db_names = "oid, name, body, state, uid, gid, owner_u, "
        "group_u, other_u, cid";

    const char * host_db_bootstrap =
        "CREATE TABLE IF NOT EXISTS host_pool ("
        "   oid INTEGER PRIMARY KEY, "
        "   name VARCHAR(128),"
        "   body MEDIUMTEXT,"
        "   state INTEGER,"
        "   uid INTEGER,"
        "   gid INTEGER,"
        "   owner_u INTEGER,"
        "   group_u INTEGER,"
        "   other_u INTEGER,"
        "   cid INTEGER)";

    const char * host_monitor_table = "host_monitoring";

    const char * host_monitor_db_names = "hid, last_mon_time, body";

    const char * host_monitor_db_bootstrap =
        "CREATE TABLE IF NOT EXISTS host_monitoring ("
        "   hid INTEGER,"
        "   last_mon_time INTEGER,"
        "   body MEDIUMTEXT,"
        "   PRIMARY KEY(hid, last_mon_time))";
}
