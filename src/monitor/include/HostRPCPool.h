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
#ifndef HOST_RPC_POOL_H_
#define HOST_RPC_POOL_H_

#include "HostBase.h"
#include "RPCPool.h"

// Provides list of HostBase objects
class HostRPCPool : public RPCPool
{
public:
    explicit HostRPCPool(SqlDB* db)
    : RPCPool(db)
    {}

    HostBase* get(int oid) const
    {
        return RPCPool::get<HostBase>(oid);
    }

    void add_object(const std::string& xml_string)
    {
        // todo Handle error state, when the object can't be constructed from xml
        RPCPool::add_object(std::unique_ptr<HostBase>(new HostBase(xml_string)));
    }

    /**
     *  Write monitoring data to DB
     */
    int update_monitoring(HostBase* h);

protected:
    int load_info(xmlrpc_c::value &result) override;

    int get_nodes(const ObjectXML& xml,
        std::vector<xmlNodePtr>& content) const override
    {
        // todo Limit the list only to active hosts?
        // return xml.get_nodes("/HOST_POOL/HOST[STATE=1 or STATE=2]", content);
        return xml.get_nodes("/HOST_POOL/HOST", content);
    }

    void add_object(xmlNodePtr node) override
    {
        RPCPool::add_object<HostBase>(node);
    }

private:
};

#endif // HOST_REMOTE_POOL_H_
