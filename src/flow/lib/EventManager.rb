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

require 'ActionManager'
require 'ffi-rzmq'

# OneFlow Event Manager
class EventManager

    attr_writer :lcm
    attr_reader :am

    LOG_COMP = 'EM'

    ACTIONS = {
        'WAIT_DEPLOY' => :wait_deploy
    }

    def initialize(concurrency)
        @zmq_endpoint = 'tcp://localhost:2101'
        @lcm = nil
        @am = ActionManager.new(concurrency, true)
        @context = ZMQ::Context.new(1)

        # Register Action Manager actions
        @am.register_action(ACTIONS['WAIT_DEPLOY'], method('wait_deploy_action'))

        Thread.new { @am.start_listener }
    end

    ############################################################################
    # Actions
    ############################################################################

    # Wait for nodes to be in RUNNING if OneGate check required it will trigger
    # another action after VMs are RUNNING
    # @param [Service] service the service
    # @param [Role] the role which contains the VMs
    # @param [Node] nodes the list of nodes (VMs) to wait for
    def wait_deploy_action(service_id, role_name, nodes)
        File.open('/tmp/loga', 'a') do |file|
            file.write("deploy wait (#{service_id})\n")
        end
        subscriber = @context.socket(ZMQ::SUB)
        # Set timeout
        subscriber.setsockopt(ZMQ::RCVTIMEO, 30*1000)
        subscriber.connect(@zmq_endpoint)

        nodes.each do |node|
            # TODO, use enums for states
            subscribe(node, 'ACTIVE', 'RUNNING', subscriber)
        end

        key = ''
        content = ''

        # Todo, add timeout, on timeout poll for the nodes if not running fails
        until nodes.empty?
            timeo = subscriber.recv_string(key)
            subscriber.recv_string(content)

            if timeo == -1
                next unless check_vms(nodes)
            end

            id = retrieve_id(key)

            nodes.delete(id)
            unsubscribe(id, 'ACTIVE', 'RUNNING', subscriber)

        end

        File.open('/tmp/loga', 'a') do |file|
            file.write("deploy wait end (#{service_id})\n")
        end

        # Todo, check if OneGate confirmation is needed
        # Todo, return false (5th parameter) if timeout reached and polling fails
        @lcm.trigger_action(:deploy_cb, service_id, service_id, role_name, true)
    end

    def subscribe(vm_id, state, lcm_state, subscriber)
        subscriber.setsockopt(ZMQ::SUBSCRIBE, gen_filter(vm_id, state, lcm_state))
    end

    def unsubscribe(vm_id, state, lcm_state, subscriber)
        subscriber.setsockopt(ZMQ::UNSUBSCRIBE, gen_filter(vm_id, state, lcm_state))
    end

    def gen_filter(vm_id, state, lcm_state)
        "EVENT STATE VM/#{state}/#{lcm_state}/#{vm_id}"
    end

    def retrieve_id(key)
        key.split('/')[-1].to_i
    end

end
