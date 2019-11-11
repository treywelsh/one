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
        'WAIT_DEPLOY' => :wait_deploy,
        'WAIT_UNDEPLOY' => :wait_undeploy
    }

    FAILURE_STATES = %w[
        BOOT_FAILURE
        BOOT_MIGRATE_FAILURE
        PROLOG_MIGRATE_FAILURE
        PROLOG_FAILURE
        EPILOG_FAILURE
        EPILOG_STOP_FAILURE
        EPILOG_UNDEPLOY_FAILURE
        PROLOG_MIGRATE_POWEROFF_FAILURE
        PROLOG_MIGRATE_SUSPEND_FAILURE
        PROLOG_MIGRATE_UNKNOWN_FAILURE
        BOOT_UNDEPLOY_FAILURE
        BOOT_STOPPED_FAILURE
        PROLOG_RESUME_FAILURE
        PROLOG_UNDEPLOY_FAILURE
    ]

    def initialize(concurrency, client)
        @zmq_endpoint = 'tcp://localhost:2101'
        @lcm = nil
        @am = ActionManager.new(concurrency, true)
        @context = ZMQ::Context.new(1)
        @client = client

        # Register Action Manager actions
        @am.register_action(ACTIONS['WAIT_DEPLOY'], method('wait_deploy_action'))
        @am.register_action(ACTIONS['WAIT_UNDEPLOY'], method('wait_undeploy_action'))

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
        Log.info LOG_COMP, "Waiting (ACTIVE,RUNNING) for #{nodes}"
        wait(nodes, 'ACTIVE', 'RUNNING')

        # Todo, check if OneGate confirmation is needed
        # Todo, return false (5th parameter) if timeout reached and polling fails
        @lcm.trigger_action(:deploy_cb, service_id, service_id, role_name, true)
    end

    # Wait for nodes to be in DONE
    # @param [service_id] the service id
    # @param [role_name] the role name of the role which contains the VMs
    # @param [nodes] the list of nodes (VMs) to wait for
    def wait_undeploy_action(service_id, role_name, nodes)
        Log.info LOG_COMP, "Waiting (DONE,LCM_INIT) for #{nodes}"
        wait(nodes, 'DONE', 'LCM_INIT')

        @lcm.trigger_action(:undeploy_cb, service_id, service_id, role_name, true)
    end

    ############################################################################
    # Helpers
    ############################################################################
    def gen_subscriber
        subscriber = @context.socket(ZMQ::SUB)
        # Set timeout (TODO add option for customize timeout)
        subscriber.setsockopt(ZMQ::RCVTIMEO, 15*1000)
        subscriber.connect(@zmq_endpoint)

        subscriber
    end

    def subscribe(vm_id, state, lcm_state, subscriber)
        subscriber.setsockopt(ZMQ::SUBSCRIBE,
                              gen_filter(vm_id, state, lcm_state))
    end

    def unsubscribe(vm_id, state, lcm_state, subscriber)
        subscriber.setsockopt(ZMQ::UNSUBSCRIBE,
                              gen_filter(vm_id, state, lcm_state))
    end

    def gen_filter(vm_id, state, lcm_state)
        "EVENT STATE VM/#{state}/#{lcm_state}/#{vm_id}"
    end

    def retrieve_id(key)
        key.split('/')[-1].to_i
    end

    def wait(nodes, state, lcm_state)
        subscriber = gen_subscriber

        nodes.each do |node|
            # TODO, use enums for states
            subscribe(node, state, lcm_state, subscriber)
        end

        key = ''
        content = ''

        until nodes.empty?
            # Read key
            timeo = subscriber.recv_string(key)

            if timeo == -1 && ZMQ::Util.errno != ZMQ::EAGAIN
                raise 'Error reading from subscriber.'
            end

            if timeo == -1
                empty, fail_nodes = check_nodes(nodes, state, lcm_state)

                next if !empty && fail_nodes.empty?

                if !fail_nodes.empty?
                    # TODO, propagate error
                    return
                end

                # TODO, what if !empty && !fail_nodes.empty?

                break
            end

            # Read content (if there is no errors)
            subscriber.recv_string(content)

            id = retrieve_id(key)

            nodes.delete(id)
            unsubscribe(id, state, lcm_state, subscriber)
        end
    end

    def check_nodes(nodes, state, lcm_state)
        failure_nodes = []

        nodes.delete_if do |node|
            vm = OpenNebula::VirtualMachine
                 .new_with_id(node, @client)

            vm.info

            vm_state     = OpenNebula::VirtualMachine::VM_STATE[vm.state]
            vm_lcm_state = OpenNebula::VirtualMachine::LCM_STATE[vm.lcm_state]

            if vm_state == 'DONE' ||
               (vm_state == state && vm_lcm_state == lcm_state)
                unsubscribe(node, state, lcm_state, subscriber)

                true
            end

            if FAILURE_STATES.include? vm_lcm_state
                failure_nodes.append(node)

                true
            end

            false
        end

        [nodes.empty?, failure_nodes]
    end

end
