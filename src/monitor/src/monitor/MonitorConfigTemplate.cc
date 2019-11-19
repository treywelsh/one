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

void MonitorConfigTemplate::set_conf_default()
{
    //SingleAttribute *   attribute;
    VectorAttribute *   vattribute;
    string              value;
    map<string,string>  vvalue;

/*
#*******************************************************************************
# Daemon configuration attributes
#-------------------------------------------------------------------------------
#  LOG
#  todo add more configuration attributes
#-------------------------------------------------------------------------------
*/
    // LOG CONFIGURATION
    vvalue.clear();
    vvalue.insert(make_pair("SYSTEM", "file"));
    vvalue.insert(make_pair("DEBUG_LEVEL", "3"));

    vattribute = new VectorAttribute("LOG", vvalue);
    conf_default.insert(make_pair(vattribute->name(), vattribute));
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
