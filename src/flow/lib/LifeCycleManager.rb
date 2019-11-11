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

require 'strategy'
require 'ActionManager'

# Service Life Cycle Manager
class ServiceLCM

    attr_writer :event_manager
    attr_reader :am

    LOG_COMP = 'LCM'

    ACTIONS = {
        'DEPLOY' => :deploy,
        'DEPLOY_CB' => :deploy_cb
    }

    def initialize(concurrency, cloud_auth)
        @cloud_auth = cloud_auth
        @event_manager = nil
        @am = ActionManager.new(concurrency, true)
        @srv_pool = ServicePool.new(@cloud_auth.client)

        # Register Action Manager actions
        @am.register_action(ACTIONS['DEPLOY'], method('deploy_action'))
        @am.register_action(ACTIONS['DEPLOY_CB'], method('deploy_cb'))

        Thread.new { @am.start_listener }
    end

    ############################################################################
    # Actions
    ############################################################################
    def deploy_action(service_id)
        File.open('/tmp/loga', 'a') do |file|
            file.write("deploy action (#{service_id})\n")
        end

        @srv_pool.get(service_id) do |service|
            File.open('/tmp/loga', 'a') do |file|
                file.write("deploy action loop\n")
            end

            set_deploy_strategy(service)

            roles = service.roles_deploy

            # Maybe roles.empty? because are being deploying in other threads
            if roles.empty? && service.all_roles_running?
                service.set_state(Service::STATE['RUNNING'])
                service.update
                break
            end

            # TODO, What if there is no roles?

            service.set_state(Service::STATE['DEPLOYING'])

            roles.each do |_name, role|
                role.set_state(Role::STATE['DEPLOYING'])
                role.deploy

                @event_manager.trigger_action(:wait_deploy,
                                              service.id,
                                              service.id,
                                              role.name,
                                              role.nodes_ids)
            end

            service.update
        end

        File.open('/tmp/loga', 'a') do |file|
            file.write("deploy action (something went wrong)\n")
        end
    end

    ############################################################################
    # Callbacks
    ############################################################################

    def deploy_cb(service_id, role_name, result)
        File.open('/tmp/loga', 'a') do |file|
            file.write("deploy cb (#{role_name})\n")
        end
        @srv_pool.get(service_id) do |service|
            if !result
                service.set_state(Service::STATE['ERROR_DEPLOYING'])
                service.update
                break
            end

            service.roles[role_name].set_state(Role::STATE['RUNNING'])

            if service.all_roles_running?
                service.set_state(Service::STATE['RUNNING'])
            elsif service.instance_of?(Straight)
                @am.trigger_action(:deploy, service.id, service_id)
            end

            service.update
        end
        File.open('/tmp/loga', 'a') do |file|
            file.write("deploy cb end (#{role_name})\n")
        end
    end

    # Returns the deployment strategy for the given Service
    # @param [Service] service the service
    # rubocop:disable Naming/AccessorMethodName
    def set_deploy_strategy(service)
        # rubocop:enable Naming/AccessorMethodName
        case service.strategy
        when 'straight'
            service.extend(Straight)
        else
            service.extend(Strategy)
        end
    end

end
