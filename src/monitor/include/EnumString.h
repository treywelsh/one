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

#ifndef ENUM_STRING_H
#define ENUM_STRING_H

#include <map>
#include <algorithm>

template <typename T>
class EString
{
public:
    EString(const std::map<std::string, T>&& _em):enum_map(_em) {};

    T _from_str(const std::string& sv) const
    {
        const auto it = enum_map.find(sv);

        if ( it == enum_map.end() )
        {
            return T::UNDEFINED;
        }

        return it->second;
    }

    const std::string& _to_str(T ev) const
    {
        const auto it = std::find_if(enum_map.begin(), enum_map.end(),
            [ev](const std::pair<std::string, T> & t) -> bool
            {
                return t.second == ev;
            });

        return it->first;
    }

private:
    const std::map<std::string, T> enum_map;
};

#endif /*ENUM_STRING_H*/
