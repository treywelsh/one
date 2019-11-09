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

#ifndef MONITOR_DRIVER_H_
#define MONITOR_DRIVER_H_

#include "Driver.h"

/**
 * Messages between the Monitor daemon and drivers
 */
enum class MonitorDriverMessages : unsigned short int
{
    MONITOR_VM   = 0,
    MONITOR_HOST = 1,
    SYSTEM_HOST  = 2,
    STATE_VM     = 3,
    UNDEFINED    = 4
};

typedef Driver<MonitorDriverMessages> monitor_driver_t;

#endif /*MONIOTR_DRIVER_H_*/
