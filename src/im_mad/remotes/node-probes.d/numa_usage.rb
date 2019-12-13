#!/usr/bin/env ruby

# -------------------------------------------------------------------------- #
# Copyright 2002-2019, OpenNebula Project, OpenNebula Systems                #
#                                                                            #
# Licensed under the Apache License, Version 2.0 (the "License"); you may    #
# not use this file except in compliance with the License. You may obtain    #
# a copy of the License at                                                   #
#                                                                            #
# http://www.apache.org/licenses/LICENSE-2.0                                 #
#                                                                            #
# Unless required by applicable law or agreed to in writing, software        #
# distributed under the License is distributed on an "AS IS" BASIS,          #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
# See the License for the specific language governing permissions and        #
# limitations under the License.                                             #
#--------------------------------------------------------------------------- #

require_relative '../../../lib/numa_common'

module NUMA

    def self.node_to_template(node, nid)
        huge = []
        memory = {}

        node.each do |k, v|
            case k
            when 'hugepages'
                v.each do |h|
                    huge << { :size => h['size'], :free => h['free'] }
                end
            when 'memory'
                memory.merge!(:free => v['free'], :used => v['used'])
            end
        end

        builder = Nokogiri::XML::Builder.new do |xml|
            xml.NODE {
                xml.ID nid

                huge.each do |h|
                    xml.HUGEPAGE {
                        xml.SIZE h[:size]
                        xml.FREE h[:free]
                    }
                end

                xml.MEMORY {
                    xml.FREE memory[:free]
                    xml.USED memory[:used]
                }
            }
        end

        builder.doc.root.to_xml
    end

end

# ------------------------------------------------------------------------------
# Get information for each NUMA node.
# ------------------------------------------------------------------------------
nodes = {}

Dir.foreach(NUMA::NODE_PATH) do |node|
    /node(?<node_id>\d+)/ =~ node
    next unless node_id

    nodes[node_id] = {}

    NUMA.huge_pages(nodes, node_id)

    NUMA.memory(nodes, node_id)
end

# nodes_s = ''

# nodes.each {|i, v| nodes_s << NUMA.node_to_template(v, i) }

# puts nodes_s

nodes_xml = Nokogiri::XML('<NUMA_NODES/>')

nodes.each do |i, v|
    nodes_xml.at('NUMA_NODES').add_child(NUMA.node_to_template(v, i))
end

puts nodes_xml.root.to_xml
