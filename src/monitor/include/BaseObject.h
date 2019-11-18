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

#ifndef BASE_OBJECT_H_
#define BASE_OBJECT_H_

#include "ObjectXML.h"

class BaseObject : public ObjectXML
{
public:
    explicit BaseObject(const std::string &xml_string)
        : ObjectXML(xml_string)
    {
    }

    explicit BaseObject(const xmlNodePtr node)
        : ObjectXML(node)
    {
    }

    virtual ~BaseObject() = default;

    int oid() const
    {
        return _oid;
    };

    const std::string& name() const
    {
        return _name;
    }

    /**
     * Changes the object's owner
     * @param _uid New User ID
     * @param _uname Name of the new user
     */
    void set_user(int uid, const std::string& uname)
    {
        _uid   = uid;
        _uname = uname;
    }

    /**
     * Changes the object's group id
     * @param _gid New Group ID
     * @param _gname Name of the new group
     */
    void set_group(int gid, const std::string& gname)
    {
        _gid   = gid;
        _gname = gname;
    };

    virtual int from_xml(const std::string& xml_string) = 0;

    virtual std::string to_xml() const = 0;

protected:

    int _oid = -1;
    int _uid = -1;
    int _gid = -1;

    std::string _name;
    std::string _uname;
    std::string _gname;
};

#endif //BASE_OBJECT_H
