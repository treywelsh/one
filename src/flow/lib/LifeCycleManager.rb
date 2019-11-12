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
        # Actions
        'DEPLOY'   => :deploy,
        'UNDEPLOY' => :undeploy,

        # Callbacks
        'DEPLOY_CB'           => :deploy_cb,
        'DEPLOY_FAILURE_CB'   => :deploy_faillure_cb,
        'UNDEPLOY_CB'         => :undeploy_cb,
        'UNDEPLOY_FAILURE_CB' => :UNdeploy_faillure_cb
    }

    def initialize(concurrency, cloud_auth)
        @cloud_auth = cloud_auth
        @event_manager = nil
        @am = ActionManager.new(concurrency, true)
        @srv_pool = ServicePool.new(@cloud_auth.client)

        # Register Action Manager actions
        @am.register_action(ACTIONS['DEPLOY'], method('deploy_action'))
        @am.register_action(ACTIONS['DEPLOY_CB'], method('deploy_cb'))
        @am.register_action(ACTIONS['DEPLOY_FAILURE_CB'], method('deploy_failure_cb'))
        @am.register_action(ACTIONS['UNDEPLOY'], method('undeploy_action'))
        @am.register_action(ACTIONS['UNDEPLOY_CB'], method('undeploy_cb'))
        @am.register_action(ACTIONS['UNDEPLOY_FAILURE_CB'], method('undeploy_failure_cb'))

        Thread.new { @am.start_listener }
    end

    ############################################################################
    # Actions
    ############################################################################
    def deploy_action(service_id)
        @srv_pool.get(service_id) do |service|
            set_deploy_strategy(service)

            roles = service.roles_deploy

            # Maybe roles.empty? because are being deploying in other threads
            if roles.empty?
                if service.all_roles_running?
                    service.set_state(Service::STATE['RUNNING'])
                    service.update
                end

                # If there is no node in PENDING the service is not modified.
                break
            end

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
    end

    def undeploy_action(service_id)
        @srv_pool.get(service_id) do |service|
            set_deploy_strategy(service)

            roles = service.roles_shutdown

            if roles.empty?
                if service.all_roles_done?
                    service.set_state(Service::STATE['DONE'])
                    service.update
                end
                # If there is no node which needs to be shutdown the service is not modified.
                break
            end

            service.set_state(Service::STATE['DELETING'])

            roles.each do |_name, role|
                role.set_state(Role::STATE['DELETING'])
                role.shutdown

                @event_manager.trigger_action(:wait_undeploy,
                                              service.id,
                                              service.id,
                                              role.name,
                                              role.nodes_ids)
            end

            service.update
        end
    end

    ############################################################################
    # Callbacks
    ############################################################################

    def deploy_cb(service_id, role_name)
        @srv_pool.get(service_id) do |service|
            service.roles[role_name].set_state(Role::STATE['RUNNING'])

            if service.all_roles_running?
                service.set_state(Service::STATE['RUNNING'])
            elsif service.strategy == 'straight'
                @am.trigger_action(:deploy, service.id, service_id)
            end

            service.update
        end
    end

    def deploy_failure_cb(service_id, role_name)
        @srv_pool.get(service_id) do |service|
            service.set_state(Service::STATE['FAILED_DEPLOYING'])
            service.roles[role_name].set_state(Role::STATE['FAILED_DEPLOYING'])

            service.update
        end
    end

    def undeploy_cb(service_id, role_name)
        @srv_pool.get(service_id) do |service|
            service.roles[role_name].set_state(Role::STATE['DONE'])

            if service.all_roles_done?
                vnets = JSON.parse(service['TEMPLATE/BODY'])['networks_values']
                delete_networks(vnets)
                service.set_state(Service::STATE['DONE'])
            elsif service.strategy == 'straight'
                @am.trigger_action(:undeploy, service.id, service_id)
            end

            service.update
        end
    end

    def undeploy_failure_cb(service_id, role_name)
        @srv_pool.get(service_id) do |service|
            service.set_state(Service::STATE['FAILED_UNDEPLOYING'])
            service.roles[role_name].set_state(Role::STATE['FAILED_UNDEPLOYING'])

            service.update
        end
    end

    ############################################################################
    # Helpers
    ############################################################################

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

    # Deletes the vnets created for the service
    def delete_networks(vnets)
        vnets.each do |vnet|
            next unless vnet[vnet.keys[0]].key? 'template_id'

            vnet_id = vnet[vnet.keys[0]]['id'].to_i

            rc = OpenNebula::VirtualNetwork
                 .new_with_id(vnet_id, @cloud_auth.client).delete

            if OpenNebula.is_error?(rc)
                Log.info LOG_COMP, "Error deleting vnet #{vnet_id}: #{rc}"
            end
        end
    end

end
