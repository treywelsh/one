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
/* ------------------------------------------------------------------------ */

#include "HostShareDatastore.h"
#include "HostShareCapacity.h"
#include "Nebula.h"

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostShareDatastore::set_monitorization(Template& ht)
{
    auto dspool = Nebula::instance().get_dspool();

    vector<Attribute *> vector_ds;

    // -------------------------------------------------------------------------
    // Get overall datastore information
    // -------------------------------------------------------------------------
    ht.get("DS_LOCATION_TOTAL_MB", max_disk);
    ht.erase("DS_LOCATION_TOTAL_MB");

    ht.get("DS_LOCATION_FREE_MB", free_disk);
    ht.erase("DS_LOCATION_FREE_MB");

    ht.get("DS_LOCATION_USED_MB", used_disk);
    ht.erase("DS_LOCATION_USED_MB");

    // -------------------------------------------------------------------------
    // Get system datastore monitorization (non_shared) 
    // -------------------------------------------------------------------------
    erase("DS"); //clear current DS information

    ht.remove("DS", vector_ds);

    for (auto it = vector_ds.begin(); it != vector_ds.end(); ++it)
    {
        int dsid;

        auto ds_attr = dynamic_cast<VectorAttribute*>(*it);

        if (ds_attr == 0)
        {
            delete *it;
            continue;
        }

        int rc = ds_attr->vector_value("ID", dsid);

        if (rc != 0 || dsid == -1)
        {
            delete *it;
            continue;
        }

        auto ds_ptr = dspool->get_ro(dsid);

        if (ds_ptr == 0)
        {
            delete *it;
            continue;
        }

        if (ds_ptr->get_type() == Datastore::SYSTEM_DS && !ds_ptr->is_shared())
        {
            set(*it);
        }
        else
        {
            delete *it;
        }

        ds_ptr->unlock();

    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void HostShareDatastore::add(HostShareCapacity &sr)
{
    disk_usage += sr.disk;

    replace("DISK_USAGE", disk_usage);
}

/* -------------------------------------------------------------------------- */

void HostShareDatastore::del(HostShareCapacity &sr)
{
    disk_usage -= sr.disk;

    replace("DISK_USAGE", disk_usage);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int HostShareDatastore::from_xml_node(const xmlNodePtr node)
{
    if ( Template::from_xml_node(node) != 0 )
    {
        return -1;
    }

    if ( !get("DISK_USAGE", disk_usage) )
    {
        return -1;
    }

    if ( !get("MAX_DISK", max_disk) )
    {
        return -1;
    }

    if (get("FREE_DISK", free_disk))
    {
        return -1;
    }

    if (get("USED_DISK", used_disk))
    {
        return -1;
    }

    return 0;
}

