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

#ifndef MONITOR_MESSAGE_H
#define MONITOR_MESSAGE_H

#include <string>
#include <iostream>

#include "EnumString.h"

/**
 *  This class represents a generic message used by the Monitoring Protocol.
 *  The structure of the message is:
 *
 *  +----+-----+---------+------+
 *  | ID | ' ' | PAYLOAD | '\n' |
 *  +----+-----+---------+------+
 *
 *    ID String (non-blanks) identifying the message type
 *    ' ' A single white space to separate ID and payload
 *    PAYLOAD of the message XML base64 encoded
 *    '\n' End of message delimiter
 *
 */
class MonitorMessage
{
public:

    enum class Type : unsigned short int
    {
        MONITOR_VM   = 0,
        MONITOR_HOST = 1,
        SYSTEM_HOST  = 2,
        STATE_VM     = 3,
        UNDEFINED    = 4
    };

    virtual ~MonitorMessage();

    /**
     *  Parse the Message from an input string
     *    @param input string with the message
     */
    int parse_from(const std::string& input);

    /**
     *  Writes this object to the given string
     */
    void write_to(std::string& output) const;

    /**
     *  Writes this object to the given file descriptor
     */
    void write_to(int fd) const;

    /**
     *  Writes this object to the given output stream
     */
    void write_to(std::ostream) const;

    /**
     *
     */
    Type type()
    {
        return _type;
    }

    /**
     *
     */
    const std::string& payload()
    {
        return _payload;
    }

private:
    static const EString<Type> type_str;

    Type _type;

    std::string _payload;
};

#endif /*MONITOR_MESSAGE_H_*/
