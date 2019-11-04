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


#ifndef HOST_BASE_H_
#define HOST_BASE_H_

#include <set>
#include "BaseObject.h"
#include "HostShare.h"
#include "ObjectCollection.h"
#include "Host.h"   // For HostState, which should be moved here

// Class storing Host data, it shouldn't contain any logic
// Scheduler, Monitor, oned should derive from this class
// to reduce amount of copy/pasted code
// Maybe it's not needed to derive, it could be used directly
// logic could be outside the class
class HostBase : public BaseObject, public ClusterableSingle
{
public:
    explicit HostBase(const std::string &xml_string)
        : BaseObject(xml_string)
        , ClusterableSingle(-1, "")
    {
        init_attributes();
    }

    explicit HostBase(const xmlNodePtr node)
        : BaseObject(node)
        , ClusterableSingle(-1, "")
    {
        init_attributes();
    }

    int get_cid() const
    {
        return cluster_id;
    };

    Host::HostState get_state() const
    {
        return state;
    }

    Host::HostState get_prev_state() const
    {
        return prev_state;
    }

    /**
     * Retrieves VMM mad name
     *    @return string vmm mad name
     */
    const std::string& get_vmm_mad() const
    {
        return vmm_mad_name;
    };

    /**
     * Retrieves IM mad name
     *    @return string im mad name
     */
    const std::string& get_im_mad() const
    {
        return im_mad_name;
    };

    const std::set<int>& get_vm_ids() const
    {
        return vm_ids.get_collection();
    }

    bool is_public_cloud() const
    {
        return public_cloud;
    }

    int init_attributes();

    /**
     * Rebuilds the object from an xml formatted string
     * @param xml_str The xml-formatted string
     * @return 0 on success, -1 otherwise
     */
    int from_xml(const std::string &xml_str) override;

    /**
     * Print object to xml string
     *  @return xml formatted string
     */
    std::string to_xml() const override;

    /**
     *  Prints the Host information to an output stream. This function is used
     *  for logging purposes.
     */
    friend ostream& operator<<(ostream& o, const HostBase& host);

private:
    Host::HostState  state          = Host::HostState::INIT;
    Host::HostState  prev_state     = Host::HostState::INIT;
    std::string      vmm_mad_name;
    std::string      im_mad_name;
    time_t           last_monitored = 0;
    bool             public_cloud   = false;

    // HostShare includes Host, uses it only for two methods
    // these methods should be refactored to remove Host dependency
    HostShare        host_share;
    ObjectCollection vm_ids{"VMS"};
    Template         obj_template;
};

#endif // HOST_BASE_H_
