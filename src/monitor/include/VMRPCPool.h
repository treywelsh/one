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
#ifndef VM_RPC_POOL_H_
#define VM_RPC_POOL_H_

#include "VirtualMachineBase.h"
#include "RPCPool.h"

// Provides list of HostBase objects
class VMRPCPool : public RPCPool
{
public:
    explicit VMRPCPool(SqlDB* db)
    : RPCPool(db)
    {}

    VirtualMachineBase* get(int oid) const
    {
        return RPCPool::get<VirtualMachineBase>(oid);
    }

protected:
    int load_info(xmlrpc_c::value &result) override;

    int get_nodes(const ObjectXML& xml,
        std::vector<xmlNodePtr>& content) const override
    {
        return xml.get_nodes("/VM_POOL/VM[STATE=1]", content);
    }

    void add_object(xmlNodePtr node) override
    {
        RPCPool::add_object<VirtualMachineBase>(node);
    }

private:
};

#endif // VM_RPC_POOL_H_
