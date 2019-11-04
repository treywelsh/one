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

#include "VirtualMachineBase.h"
#include "VMActions.h"

using namespace std;

/******************************************************************************/
/******************************************************************************/
/*  INITIALIZE VM object attributes from its XML representation               */
/******************************************************************************/
/******************************************************************************/

void VirtualMachineBase::init_attributes()
{
    std::vector<xmlNodePtr> nodes;
    std::vector<VectorAttribute*> attrs;

    int rc;
    int action;
    int tmp;

    string automatic_requirements;
    string automatic_ds_requirements;
    string automatic_nic_requirements;

    /**************************************************************************/
    /* VM attributes and flags                                                */
    /**************************************************************************/
    xpath(oid, "/VM/ID", -1);
    xpath(uid, "/VM/UID", -1);
    xpath(gid, "/VM/GID", -1);

    xpath(tmp, "/VM/STATE", -1);
    active = tmp == 3;

    xpath(tmp, "/VM/RESCHED", 0);
    resched = tmp == 1;

    xpath(action, "/VM/HISTORY_RECORDS/HISTORY/ACTION", -1);
    resume = (action == VMActions::STOP_ACTION || action == VMActions::UNDEPLOY_ACTION
            || action == VMActions::UNDEPLOY_HARD_ACTION );

    xpath(hid, "/VM/HISTORY_RECORDS/HISTORY/HID", -1);
    xpath(dsid, "/VM/HISTORY_RECORDS/HISTORY/DS_ID", -1);

    xpath(stime, "/VM/STIME", (time_t) 0);

    /**************************************************************************/
    /*  VM Capacity memory, cpu and disk (system ds storage)                  */
    /**************************************************************************/
    xpath<long int>(memory, "/VM/TEMPLATE/MEMORY", 0);

    xpath<float>(cpu, "/VM/TEMPLATE/CPU", 0);

    /**************************************************************************/
    /*  Scheduling rank expresions for:                                       */
    /*    - host                                                              */
    /*    - datastore                                                         */
    /**************************************************************************/
    rc = xpath(rank, "/VM/USER_TEMPLATE/SCHED_RANK", "");

    if (rc != 0)
    {
        // Compatibility with previous versions
        xpath(rank, "/VM/USER_TEMPLATE/RANK", "");
    }

    xpath(ds_rank, "/VM/USER_TEMPLATE/SCHED_DS_RANK", "");

    /**************************************************************************/
    /*  Scheduling requirements for:                                          */
    /*    - host                                                              */
    /*    - datastore                                                         */
    /*    - network                                                           */
    /**************************************************************************/
    // ---------------------------------------------------------------------- //
    // Host requirements                                                      //
    // ---------------------------------------------------------------------- //
    xpath(automatic_requirements, "/VM/TEMPLATE/AUTOMATIC_REQUIREMENTS", "");

    rc = xpath(requirements, "/VM/USER_TEMPLATE/SCHED_REQUIREMENTS", "");

    if (rc == 0)
    {
        if ( !automatic_requirements.empty() )
        {
            ostringstream oss;

            oss << automatic_requirements << " & ( " << requirements << " )";

            requirements = oss.str();
        }
    }
    else if ( !automatic_requirements.empty() )
    {
        requirements = automatic_requirements;
    }

    // ---------------------------------------------------------------------- //
    // Datastore requirements                                                 //
    // ---------------------------------------------------------------------- //
    xpath(automatic_ds_requirements, "/VM/TEMPLATE/AUTOMATIC_DS_REQUIREMENTS", "");

    rc = xpath(ds_requirements, "/VM/USER_TEMPLATE/SCHED_DS_REQUIREMENTS", "");

    if (rc == 0)
    {
        if ( !automatic_ds_requirements.empty() )
        {
            ostringstream oss;

            oss << automatic_ds_requirements << " & ( " << ds_requirements << " )";

            ds_requirements = oss.str();
        }
    }
    else if ( !automatic_ds_requirements.empty() )
    {
        ds_requirements = automatic_ds_requirements;
    }

    /**************************************************************************/
    /*  Template, user template, history information and rescheduling flag    */
    /**************************************************************************/
    if (get_nodes("/VM/TEMPLATE", nodes) > 0)
    {
        vm_template = new VirtualMachineTemplate;

        vm_template->from_xml_node(nodes[0]);

        free_nodes(nodes);
    }
    else
    {
        vm_template = 0;
    }

    nodes.clear();

    if (get_nodes("/VM/USER_TEMPLATE", nodes) > 0)
    {
        user_template = new VirtualMachineTemplate;

        user_template->from_xml_node(nodes[0]);

        free_nodes(nodes);
    }
    else
    {
        user_template = 0;
    }

    public_cloud = (user_template->get("PUBLIC_CLOUD", attrs) > 0);

    if (public_cloud == false)
    {
        attrs.clear();
        public_cloud = (user_template->get("EC2", attrs) > 0);
    }

    if (vm_template != 0)
    {
        init_storage_usage();
    }
    else
    {
        system_ds_usage = 0;
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

static bool isVolatile(const VectorAttribute * disk)
{
    string type = disk->vector_value("TYPE");

    one_util::toupper(type);

    return ( type == "SWAP" || type == "FS");
}

/* -------------------------------------------------------------------------- */

void VirtualMachineBase::init_storage_usage()
{
    vector<Attribute  *>            disks;

    long long   size;
    long long   snapshot_size;
    string      st;
    int         ds_id;
    bool        clone;

    system_ds_usage = 0;

    int num = vm_template->remove("DISK", disks);

    for (auto d : disks)
    {
        const VectorAttribute * disk = dynamic_cast<const VectorAttribute*>(d);

        if (disk == 0)
        {
            continue;
        }

        if (disk->vector_value("SIZE", size) != 0)
        {
            continue;
        }

        if (disk->vector_value("DISK_SNAPSHOT_TOTAL_SIZE", snapshot_size) == 0)
        {
            size += snapshot_size;
        }

        if (isVolatile(disk))
        {
            system_ds_usage += size;
        }
        else
        {
            if (disk->vector_value("DATASTORE_ID", ds_id) != 0)
            {
                continue;
            }

            if (ds_usage.count(ds_id) == 0)
            {
                ds_usage[ds_id] = 0;
            }

            if (disk->vector_value("CLONE", clone) != 0)
            {
                continue;
            }

            if (clone)
            {
                st = disk->vector_value("CLONE_TARGET");
            }
            else
            {
                st = disk->vector_value("LN_TARGET");
            }

            one_util::toupper(st);

            if (st == "SELF")
            {
                ds_usage[ds_id] += size;
            }
            else if (st == "SYSTEM")
            {
                system_ds_usage += size;
            } // else st == NONE
        }
    }

    for (int i = 0; i < num ; i++)
    {
        delete disks[i];
    }
}

/******************************************************************************/
/******************************************************************************/
/*  VM requirements and capacity interface                                    */
/******************************************************************************/
/******************************************************************************/

void VirtualMachineBase::add_requirements(const string& reqs)
{
    if ( reqs.empty() )
    {
        return;
    }
    else if ( requirements.empty() )
    {
        requirements = reqs;
    }
    else
    {
        requirements += " & (" + reqs + ")";
    }
}

//******************************************************************************
// Logging
//******************************************************************************

ostream& operator<<(ostream& os, VirtualMachineBase& vm)
{
    os << "Virtual Machine: " << vm.oid << endl << endl;

    os << endl;

    return os;
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


