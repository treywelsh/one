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

#include "HostBase.h"
// #include "GroupPool.h"

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

using namespace std;

static const int ONEADMIN_ID = 0;
static const char* ONEADMIN_NAME = "oneadmin";

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int HostBase::from_xml(const std::string &xml_str)
{
    int rc = update_from_str(xml_str);

    if (rc != 0)
    {
        return rc;
    }

    return init_attributes();
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

string HostBase::to_xml() const
{
    // todo
    return "";
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int HostBase::init_attributes()
{
    int int_state;
    int int_prev_state;
    int rc = 0;

    // Get class base attributes
    rc += xpath(oid, "/HOST/ID", -1);
    rc += xpath(name, "/HOST/NAME", "not_found");
    rc += xpath(int_state, "/HOST/STATE", 0);
    rc += xpath(int_prev_state, "/HOST/PREV_STATE", 0);

    rc += xpath(im_mad_name, "/HOST/IM_MAD", "not_found");
    rc += xpath(vmm_mad_name, "/HOST/VM_MAD", "not_found");

    rc += xpath<time_t>(last_monitored, "/HOST/LAST_MON_TIME", 0);

    rc += xpath(cluster_id, "/HOST/CLUSTER_ID", -1);
    rc += xpath(cluster,    "/HOST/CLUSTER",    "not_found");

    state = static_cast<Host::HostState>( int_state );
    prev_state = static_cast<Host::HostState>( int_prev_state );

    // Set the owner and group to oneadmin
    set_user(0, "");
    // ONEADMIN_ID and ONEADMIN_NAME defined locally, as using definitions
    // from GroupPool force linking of nebula_group, nebula_acl, nebula_rm,
    // nebula_um, nebula_vm and others.
    // set_group(GroupPool::ONEADMIN_ID, GroupPool::ONEADMIN_NAME);
    set_group(ONEADMIN_ID, ONEADMIN_NAME);

    // ------------ Host Share ---------------
    vector<xmlNodePtr> content;
    ObjectXML::get_nodes("/HOST/HOST_SHARE", content);

    if (content.empty())
    {
        return -1;
    }

    rc += host_share.from_xml_node(content[0]);

    ObjectXML::free_nodes(content);
    content.clear();

    // ------------ Host Template ---------------
    ObjectXML::get_nodes("/HOST/TEMPLATE", content);

    if (content.empty())
    {
        return -1;
    }

    rc += obj_template.from_xml_node(content[0]);
    obj_template.get("PUBLIC_CLOUD", public_cloud);

    ObjectXML::free_nodes(content);
    content.clear();

    // ------------ VMS collection ---------------
    rc += vm_ids.from_xml(this, "/HOST/");

    if (rc != 0)
    {
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

ostream& operator<<(ostream& o, const HostBase& host)
{
    o << "ID          : " << host.oid        << endl;
    o << "CLUSTER_ID  : " << host.cluster_id << endl;
    o << "PUBLIC      : " << host.public_cloud << endl;
    // o << host.share; // todo

    return o;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
