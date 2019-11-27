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

#include "Host.h"
#include "Nebula.h"
#include "ClusterPool.h"
#include "InformationManager.h"
#include "VirtualMachinePool.h"

#include <sstream>

/* ************************************************************************ */
/* Host :: Constructor/Destructor                                           */
/* ************************************************************************ */

Host::Host(
    int id,
    const string& _hostname,
    const string& _im_mad_name,
    const string& _vmm_mad_name,
    int           _cluster_id,
    const string& _cluster_name):
        PoolObjectSQL(id,HOST,_hostname,-1,-1,"","",table),
        ClusterableSingle(_cluster_id, _cluster_name),
        state(INIT),
        prev_state(INIT),
        im_mad_name(_im_mad_name),
        vmm_mad_name(_vmm_mad_name),
        last_monitored(0),
        vm_collection("VMS")
{
    obj_template = new HostTemplate;

    add_template_attribute("RESERVED_CPU", "");
    add_template_attribute("RESERVED_MEM", "");

    replace_template_attribute("IM_MAD", im_mad_name);
    replace_template_attribute("VM_MAD", vmm_mad_name);
}

/* ************************************************************************ */
/* Host :: Database Access Functions                                        */
/* ************************************************************************ */

const char * Host::table = "host_pool";

const char * Host::db_names =
    "oid, name, body, state, last_mon_time, uid, gid, owner_u, group_u, other_u, cid";

const char * Host::db_bootstrap = "CREATE TABLE IF NOT EXISTS host_pool ("
    "oid INTEGER PRIMARY KEY, name VARCHAR(128), body MEDIUMTEXT, state INTEGER, "
    "last_mon_time INTEGER, uid INTEGER, gid INTEGER, owner_u INTEGER, "
    "group_u INTEGER, other_u INTEGER, cid INTEGER)";


const char * Host::monit_table = "host_monitoring";

const char * Host::monit_db_names = "hid, last_mon_time, body";

const char * Host::monit_db_bootstrap = "CREATE TABLE IF NOT EXISTS "
    "host_monitoring (hid INTEGER, last_mon_time INTEGER, body MEDIUMTEXT, "
    "PRIMARY KEY(hid, last_mon_time))";


const char * Host::monit_table_new = "host_monitoring_new";

const char * Host::monit_db_names_new = "hid, timestamp, body";

const char * Host::monit_db_bootstrap_new = "CREATE TABLE IF NOT EXISTS "
    "host_monitoring_new (hid INTEGER, timestamp INTEGER, body MEDIUMTEXT, "
    "PRIMARY KEY(hid, timestamp))";
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int Host::insert_replace(SqlDB *db, bool replace, string& error_str)
{
    ostringstream   oss;

    int    rc;
    string xml_body;

    char * sql_hostname;
    char * sql_xml;

    // Set the owner and group to oneadmin
    set_user(0, "");
    set_group(GroupPool::ONEADMIN_ID, GroupPool::ONEADMIN_NAME);

   // Update the Host

    sql_hostname = db->escape_str(name);

    if ( sql_hostname == 0 )
    {
        goto error_hostname;
    }

    sql_xml = db->escape_str(to_xml(xml_body));

    if ( sql_xml == 0 )
    {
        goto error_body;
    }

    if ( validate_xml(sql_xml) != 0 )
    {
        goto error_xml;
    }

    if (replace)
    {
        oss << "UPDATE " << table << " SET "
            << "name = '"         << sql_hostname   << "', "
            << "body = '"         << sql_xml        << "', "
            << "state = "         << state          << ", "
            << "last_mon_time = " << last_monitored << ", "
            << "uid = "           << uid            << ", "
            << "gid = "           << gid            << ", "
            << "owner_u = "       << owner_u        << ", "
            << "group_u = "       << group_u        << ", "
            << "other_u = "       << other_u        << ", "
            << "cid = "           << cluster_id
            << " WHERE oid = " << oid;
    }
    else
    {
        // Construct the SQL statement to Insert or Replace
        oss << "INSERT INTO "<< table <<" ("<< db_names <<") VALUES ("
            <<          oid                 << ","
            << "'" <<   sql_hostname        << "',"
            << "'" <<   sql_xml             << "',"
            <<          state               << ","
            <<          last_monitored      << ","
            <<          uid                 << ","
            <<          gid                 << ","
            <<          owner_u             << ","
            <<          group_u             << ","
            <<          other_u             << ","
            <<          cluster_id          << ")";
    }

    rc = db->exec_wr(oss);

    db->free_str(sql_hostname);
    db->free_str(sql_xml);

    return rc;

error_xml:
    db->free_str(sql_hostname);
    db->free_str(sql_xml);

    error_str = "Error transforming the Host to XML.";

    goto error_common;

error_body:
    db->free_str(sql_hostname);
    goto error_generic;

error_hostname:
    goto error_generic;

error_generic:
    error_str = "Error inserting Host in DB.";
error_common:
    return -1;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int Host::update_info(Template &tmpl)
{
    if ( state == OFFLINE )
    {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Remove expired information from current template
    // -------------------------------------------------------------------------
    clear_template_error_message();

    remove_template_attribute("ZOMBIES");
    remove_template_attribute("TOTAL_ZOMBIES");

    remove_template_attribute("WILDS");
    remove_template_attribute("TOTAL_WILDS");

    remove_template_attribute("VM");
    remove_template_attribute("VM_POLL");

    // -------------------------------------------------------------------------
    // Copy monitor, extract share info & update last_monitored and state
    // -------------------------------------------------------------------------
    obj_template->merge(&tmpl);

    touch(true);

    string rcpu;
    string rmem;

    reserved_capacity(rcpu, rmem);

    host_share.set_monitorization(*obj_template, rcpu, rmem);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Host::enable()
{
    if (state == OFFLINE)
    {
        Nebula::instance().get_im()->start_monitor(this, true);
    }

    state = INIT;
};

/* -------------------------------------------------------------------------- */

void Host::disable()
{
    if (state == OFFLINE)
    {
        Nebula::instance().get_im()->start_monitor(this, true);
    }

    state = DISABLED;
};

/* -------------------------------------------------------------------------- */

void Host::offline()
{
    Nebula::instance().get_im()->stop_monitor(get_oid(),get_name(),get_im_mad());

    state = OFFLINE;

    host_share.reset_capacity();

    remove_template_attribute("TOTALCPU");
    remove_template_attribute("TOTALMEMORY");

    remove_template_attribute("FREECPU");
    remove_template_attribute("FREEMEMORY");

    remove_template_attribute("USEDCPU");
    remove_template_attribute("USEDMEMORY");
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

static Host::HostState trigger_state(Host::HostState state)
{
    switch(state)
    {
        case Host::INIT:
        case Host::MONITORED:
        case Host::ERROR:
        case Host::DISABLED:
        case Host::OFFLINE:
            return state;
        case Host::MONITORING_ERROR:
            return Host::ERROR;
        case Host::MONITORING_DISABLED:
            return Host::DISABLED;
        case Host::MONITORING_MONITORED:
            return Host::MONITORED;
        case Host::MONITORING_INIT:
            return Host::INIT;
    }

    return state;
}

// -----------------------------------------------------------------------------

bool Host::has_changed_state()
{
    return trigger_state(prev_state) != trigger_state(state);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Host::error_info(const string& message, set<int> &vm_ids)
{
    ostringstream oss;

    vm_ids = vm_collection.clone();

    oss << "Error monitoring Host " << get_name() << " (" << get_oid() << ")"
        << ": " << message;

    NebulaLog::log("ONE", Log::ERROR, oss);

    touch(false);

    set_template_error_message(oss.str());
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int Host::update_monitoring(SqlDB * db)
{
    ostringstream   oss;
    int             rc;

    string xml_body;
    string error_str;
    char * sql_xml;

    sql_xml = db->escape_str(to_xml(xml_body));

    if ( sql_xml == 0 )
    {
        goto error_body;
    }

    if ( validate_xml(sql_xml) != 0 )
    {
        goto error_xml;
    }

    oss << "REPLACE INTO " << monit_table << " ("<< monit_db_names <<") VALUES ("
        <<          oid             << ","
        <<          last_monitored       << ","
        << "'" <<   sql_xml         << "')";

    db->free_str(sql_xml);

    rc = db->exec_local_wr(oss);

    return rc;

error_xml:
    error_str = "could not transform the Host to XML: ";
    error_str += sql_xml;

    db->free_str(sql_xml);

    goto error_common;

error_body:
    error_str = "could not insert the Host in the DB.";

error_common:
    oss.str("");
    oss << "Error updating Host monitoring information, " << error_str;

    NebulaLog::log("ONE",Log::ERROR, oss);

    return -1;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

bool Host::is_public_cloud() const
{
    bool is_public_cloud = false;

    get_template_attribute("PUBLIC_CLOUD", is_public_cloud);

    return is_public_cloud;
}

/* ************************************************************************ */
/* Host :: Misc                                                             */
/* ************************************************************************ */

string& Host::to_xml(string& xml) const
{
    string template_xml;
    string share_xml;

    ostringstream oss;
    string        vm_collection_xml;

    oss <<
    "<HOST>"
       "<ID>"            << oid              << "</ID>"              <<
       "<NAME>"          << name             << "</NAME>"            <<
       "<STATE>"         << state            << "</STATE>"           <<
       "<PREV_STATE>"    << prev_state       << "</PREV_STATE>"      <<
       "<IM_MAD>"        << one_util::escape_xml(im_mad_name)  << "</IM_MAD>" <<
       "<VM_MAD>"        << one_util::escape_xml(vmm_mad_name) << "</VM_MAD>" <<
       "<LAST_MON_TIME>" << last_monitored   << "</LAST_MON_TIME>"   <<
       "<CLUSTER_ID>"    << cluster_id       << "</CLUSTER_ID>"      <<
       "<CLUSTER>"       << cluster          << "</CLUSTER>"         <<
       host_share.to_xml(share_xml)  <<
       vm_collection.to_xml(vm_collection_xml) <<
       obj_template->to_xml(template_xml) <<
    "</HOST>";

    xml = oss.str();

    return xml;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

int Host::from_xml(const string& xml)
{
    vector<xmlNodePtr> content;

    int int_state;
    int int_prev_state;
    int rc = 0;

    // Initialize the internal XML object
    update_from_str(xml);

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

    state = static_cast<HostState>( int_state );
    prev_state = static_cast<HostState>( int_prev_state );

    // Set the owner and group to oneadmin
    set_user(0, "");
    set_group(GroupPool::ONEADMIN_ID, GroupPool::ONEADMIN_NAME);

    // ------------ Host Share ---------------

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

    rc += obj_template->from_xml_node(content[0]);

    ObjectXML::free_nodes(content);

    content.clear();

    // ------------ VMS collection ---------------
    rc += vm_collection.from_xml(this, "/HOST/");

    if (rc != 0)
    {
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Host::reserved_capacity(string& rcpu, string& rmem) const
{
    string cluster_rcpu = "";
    string cluster_rmem = "";

    if (cluster_id != -1)
    {
        auto cpool = Nebula::instance().get_clpool();

        Cluster * cluster = cpool->get_ro(cluster_id);

        if (cluster != nullptr)
        {
            cluster->get_reserved_capacity(cluster_rcpu, cluster_rmem);
            cluster->unlock();
        }
    }

    get_template_attribute("RESERVED_CPU", rcpu);
    get_template_attribute("RESERVED_MEM", rmem);

    if ( rcpu.empty() )
    {
        rcpu = cluster_rcpu;
    }

    if ( rmem.empty() )
    {
        rmem = cluster_rmem;
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int Host::post_update_template(string& error)
{
    string new_im_mad;
    string new_vm_mad;
    
    string rcpu;
    string rmem;

    get_template_attribute("IM_MAD", new_im_mad);
    get_template_attribute("VM_MAD", new_vm_mad);

    if (!new_im_mad.empty())
    {
        im_mad_name = new_im_mad;
    }

    if (!new_im_mad.empty())
    {
        vmm_mad_name = new_vm_mad;
    }

    replace_template_attribute("IM_MAD", im_mad_name);
    replace_template_attribute("VM_MAD", vmm_mad_name);

    reserved_capacity(rcpu, rmem);

    host_share.update_capacity(*obj_template, rcpu, rmem);

    return 0;
};
