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


#ifndef REMOTE_POOL_H_
#define REMOTE_POOL_H_

#include "Client.h"
#include "BaseObject.h"

#include <memory>

class RemotePool
{
public:
    /**
     * Returns list of objects
     */
    const map<int, std::unique_ptr<BaseObject>>& get_objects() const
    {
        return objects;
    };

    /**
     *  Gets an object from the pool
     *   @param oid the object unique identifier
     *
     *   @return a pointer to the object, 0 in case of failure
     */
    template<typename T>
    T* get(int oid) const
    {
        auto it = objects.find(oid);

        if (it == objects.end())
        {
            return nullptr;
        }
        else
        {
            return static_cast<T*>(it->second.get());
        }
    };

    /**
     *  Removes object from the pool
     *   @param oid the object unique identifier
     */
    void erase(int oid)
    {
        auto it = objects.find(oid);

        if (it != objects.end())
        {
            objects.erase(it);
        }
    };

    void add_object(std::unique_ptr<BaseObject> o)
    {
        objects[o->get_id()] = std::move(o);
    }

    /**
     *  Read data from server, fill internal structures
     *   @return 0 on success
     */
    int update();

protected:
    // ------------------------------------------------------------------------
    RemotePool()
    : client(Client::client())
    {
    }

    virtual ~RemotePool() = default;

    /**
     *  Deletes pool objects and frees resources.
     */
    void clear()
    {
        objects.clear();
    }

    /**
     * Inserts a new BaseObject into the objects map
     */
    virtual void add_object(xmlNodePtr node) = 0;

    /**
     *  Split list of objects to xml nodes
     */
    virtual int get_nodes(const ObjectXML& xml,
        std::vector<xmlNodePtr>& content) const = 0;

    /**
     *  Read objects as xml-formatted string from server
     */
    virtual int load_info(xmlrpc_c::value &result) = 0;


    /**
     *  Inserts object into objects list
     */
    template<typename T>
    void add_object(xmlNodePtr node)
    {
        if (node == 0 || node->children == 0)
        {
            NebulaLog::log("POOL", Log::ERROR,
                        "XML Node does not represent a valid object");

            return;
        }

        auto obj = std::unique_ptr<T>(new T(node));

        objects[obj->get_id()] = std::move(obj);
    }

    // ------------------------------------------------------------------------
    // Attributes
    // ------------------------------------------------------------------------
    /**
     * XML-RPC client
     */
    Client * client;

    /**
     * Hash map contains the suitable [id, object] pairs.
     */
    map<int, std::unique_ptr<BaseObject>> objects;
};

#endif // REMOTE_POOL_H_
