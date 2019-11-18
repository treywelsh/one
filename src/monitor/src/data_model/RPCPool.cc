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

#include "RPCPool.h"

int RPCPool::update()
{
    clear();

    // ---------------------------------------------------------------------
    // Load the ids (to get an updated list of the pool)
    // ---------------------------------------------------------------------

    xmlrpc_c::value result;

    int rc = load_info(result);

    if (rc != 0)
    {
        NebulaLog::log("POOL", Log::ERROR, "Could not retrieve pool info.");
        return -1;
    }

    vector<xmlrpc_c::value> values =
                    xmlrpc_c::value_array(result).vectorValueValue();

    bool   success = xmlrpc_c::value_boolean(values[0]);
    string message = xmlrpc_c::value_string(values[1]);

    if ( !success )
    {
        ostringstream oss;

        oss << "ONE returned error while retrieving pool info:" << endl;
        oss << message;

        NebulaLog::log("POOL", Log::ERROR, oss);
        return -1;
    }

    ObjectXML xml(message);

    vector<xmlNodePtr> nodes;

    get_nodes(xml, nodes);

    for (const auto& node : nodes)
    {
        add_object(node);
    }

    xml.free_nodes(nodes);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
