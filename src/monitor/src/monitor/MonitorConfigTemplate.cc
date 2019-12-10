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

#include "MonitorConfigTemplate.h"

/* -------------------------------------------------------------------------- */
/*  Configuration Defaults                                                    */
/* -------------------------------------------------------------------------- */
void MonitorConfigTemplate::set_conf_default()
{
    SingleAttribute * sa;
    VectorAttribute * va;
/*
 MESSAGE_SIZE
 XMLRPC_TIMEOUT
 ONE_XMLRPC
 LOG
 DB
 UDP_LISTENER
 */
    sa = new SingleAttribute("MESSAGE_SIZE", "1073741824");
    conf_default.insert(make_pair(sa->name(), sa));

    sa = new SingleAttribute("XMLRPC_TIMEOU", "60");
    conf_default.insert(make_pair(sa->name(), sa));

    sa = new SingleAttribute("ONE_XMLRPC", "http://localhost:2633/RPC2");
    conf_default.insert(make_pair(sa->name(), sa));

    va = new VectorAttribute("LOG", {{"SYSTEM", "FILE"}, {"DEBUG_LEVEL", "3"}});
    conf_default.insert(make_pair(va->name(), va));

    va = new VectorAttribute("DB", {{"BACKEND", "sqlite"}});
    conf_default.insert(make_pair(va->name(), va));

    va = new VectorAttribute("UDP_LISTENER", {{"ADDRESS", "0.0.0.0"},
            {"PORT", "4124"}, {"THREADS", "16"}});
    conf_default.insert(make_pair(va->name(), va));
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
