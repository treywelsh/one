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


#ifndef VIRTUAL_MACHINE_BASE_H_
#define VIRTUAL_MACHINE_BASE_H_

#include <ostream>
#include <set>
#include <map>

#include "BaseObject.h"
#include "VirtualMachineTemplate.h"

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

class VirtualMachineBase : public BaseObject
{
public:

    explicit VirtualMachineBase(const std::string &xml_doc)
        : BaseObject(xml_doc)
    {
        init_attributes();
    };

    explicit VirtualMachineBase(const xmlNodePtr node)
        : BaseObject(node)
    {
        init_attributes();
    }

    virtual ~VirtualMachineBase()
    {
        // if (vm_template != 0)
        // {
        //     delete vm_template;
        // }

        // if (user_template != 0)
        // {
        //     delete user_template;
        // }
    }

    //--------------------------------------------------------------------------
    // Get Methods for VirtaulMachineBase class
    //--------------------------------------------------------------------------
    int get_hid() const { return hid; }

    int get_dsid() const { return dsid; }

    time_t get_stime() const { return stime; }

    bool is_resched() const { return resched; }

    bool is_resume() const { return resume; }

    bool is_public_cloud() const { return public_cloud; }

    bool is_active() const { return active; }

    //--------------------------------------------------------------------------
    // Scheduling requirements and rank
    //--------------------------------------------------------------------------
    const std::string& get_requirements() { return requirements; }

    const std::string& get_ds_requirements() { return ds_requirements; }

    const std::string& get_rank() { return rank; }

    const std::string& get_ds_rank() { return ds_rank; }

    /**
     *  Adds (logical AND) new placement requirements to the current ones
     *    @param reqs additional requirements
     */
    void add_requirements(const std::string& reqs);

    /**
     *  @return storage usage for the VM
     */
    std::map<int, long long> get_storage_usage()
    {
        return ds_usage;
    }

    //--------------------------------------------------------------------------
    // Logging
    //--------------------------------------------------------------------------
    /**
     *  Function to write a Virtual Machine in an output stream
     */
    friend std::ostream& operator<<(std::ostream& os, VirtualMachineBase& vm);

protected:
    /**
     *  For constructors
     */
    void init_attributes();

    void init_storage_usage();

    /* ----------------------- VIRTUAL MACHINE ATTRIBUTES ------------------- */
    int hid;
    int dsid;

    bool resched;
    bool resume;
    bool active;
    bool public_cloud;

    long int    memory;
    float       cpu;
    long long   system_ds_usage;

    std::map<int,long long> ds_usage;

    std::string rank;
    std::string requirements;

    std::string ds_requirements;
    std::string ds_rank;

    time_t stime;

    std::set<int> nics_ids_auto;

    // std::map<int, VirtualMachineNicXML *> nics;

    VirtualMachineTemplate * vm_template;   /**< The VM template */
    VirtualMachineTemplate * user_template; /**< The VM user template */
};

#endif // VIRTUAL_MACHINE_BASE_H_
