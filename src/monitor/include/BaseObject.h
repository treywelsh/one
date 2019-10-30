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
    BaseObject()
    {
    }

    int get_id() const
    {
        return oid;
    };

    const std::string& get_name() const
    {
        return name;
    }

    /**
     * Changes the object's owner
     * @param _uid New User ID
     * @param _uname Name of the new user
     */
    void set_user(int _uid, const std::string& _uname)
    {
        uid   = _uid;
        uname = _uname;
    }

    /**
     * Changes the object's group id
     * @param _gid New Group ID
     * @param _gname Name of the new group
     */
    void set_group(int _gid, const std::string& _gname)
    {
        gid   = _gid;
        gname = _gname;
    };

    virtual int from_xml(const std::string& xml_string) = 0;
    virtual std::string to_xml() const = 0;

protected:
    int oid                 = -1;
    int uid                 = -1;
    int gid                 = -1;

    std::string name;
    std::string uname;
    std::string gname;
};

#endif //BASE_OBJECT_H_
