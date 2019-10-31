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

module OpenNebula

    # Service class as wrapper of DocumentJSON
    class Service < DocumentJSON

        attr_reader :roles, :client

        DOCUMENT_TYPE = 100

        STATE = {
            'PENDING'            => 0,
            'DEPLOYING'          => 1,
            'RUNNING'            => 2,
            'UNDEPLOYING'        => 3,
            'WARNING'            => 4,
            'DONE'               => 5,
            'FAILED_UNDEPLOYING' => 6,
            'FAILED_DEPLOYING'   => 7,
            'SCALING'            => 8,
            'FAILED_SCALING'     => 9,
            'COOLDOWN'           => 10,
            'DELETING'           => 11,
            'FAILED_DELETING'    => 12
        }

        STATE_STR = %w[
            PENDING
            DEPLOYING
            RUNNING
            UNDEPLOYING
            WARNING
            DONE
            FAILED_UNDEPLOYING
            FAILED_DEPLOYING
            SCALING
            FAILED_SCALING
            COOLDOWN
            DELETING
            FAILED_DELETING
        ]

        LOG_COMP = 'SER'

        # Returns the service state
        # @return [Integer] the service state
        def state
            @body['state'].to_i
        end

        # Returns the service strategy
        # @return [String] the service strategy
        def strategy
            @body['deployment']
        end

        # Returns the string representation of the service state
        # @return the state string
        def state_str
            STATE_STR[state]
        end

        # Sets a new state
        # @param [Integer] the new state
        # @return [true, false] true if the value was changed
        # rubocop:disable Naming/AccessorMethodName
        def set_state(state)
            # rubocop:enable Naming/AccessorMethodName
            if state < 0 || state > STATE_STR.size
                return false
            end

            @body['state'] = state

            msg = "New state: #{STATE_STR[state]}"
            Log.info LOG_COMP, msg, id
            log_info(msg)

            true
        end

        # Returns the owner username
        # @return [String] the service's owner username
        def owner_name
            self['UNAME']
        end

        # Replaces this object's client with a new one
        # @param [OpenNebula::Client] owner_client the new client
        def replace_client(owner_client)
            @client = owner_client
        end

        # Returns true if all the nodes are correctly deployed
        # @return [true, false] true if all the nodes are correctly deployed
        def all_roles_running?
            @roles.each do |_name, role|
                if role.state != Role::STATE['RUNNING']
                    return false
                end
            end

            true
        end

        # Returns true if all the nodes are in done state
        # @return [true, false] true if all the nodes are correctly deployed
        def all_roles_done?
            @roles.each do |_name, role|
                if role.state != Role::STATE['DONE']
                    return false
                end
            end

            true
        end

        # Returns true if any of the roles is in failed state
        # @return [true, false] true if any of the roles is in failed state
        def any_role_failed?
            failed_states = [
                Role::STATE['FAILED_DEPLOYING'],
                Role::STATE['FAILED_UNDEPLOYING'],
                Role::STATE['FAILED_DELETING']
            ]

            @roles.each do |_name, role|
                if failed_states.include?(role.state)
                    return true
                end
            end

            false
        end

        # Returns the running_status_vm option
        # @return [true, false] true if the running_status_vm option is enabled
        def ready_status_gate
            @body['ready_status_gate']
        end

        def any_role_scaling?
            @roles.each do |_name, role|
                if role.state == Role::STATE['SCALING']
                    return true
                end
            end

            false
        end

        def any_role_failed_scaling?
            @roles.each do |_name, role|
                if role.state == Role::STATE['FAILED_SCALING']
                    return true
                end
            end

            false
        end

        def any_role_cooldown?
            @roles.each do |_name, role|
                if role.state == Role::STATE['COOLDOWN']
                    return true
                end
            end

            false
        end

        # Create a new service based on the template provided
        # @param [String] template_json
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def allocate(template_json)
            template = JSON.parse(template_json)
            template['state'] = STATE['PENDING']

            if template['roles']
                template['roles'].each do |elem|
                    elem['state'] ||= Role::STATE['PENDING']
                end
            end

            super(template.to_json, template['name'])
        end

        # Shutdown the service. This action is called when user wants to
        # shutdwon the Service
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def shutdown
            if ![Service::STATE['FAILED_SCALING'],
                 Service::STATE['DONE']].include?(state)

                set_state(Service::STATE['UNDEPLOYING'])

                update
            else
                OpenNebula::Error.new('Action shutdown: Wrong state' \
                                      " #{state_str}")
            end
        end

        # Recover a failed service.
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def recover
            if [Service::STATE['FAILED_DEPLOYING']].include?(state)
                @roles.each do |_name, role|
                    if role.state == Role::STATE['FAILED_DEPLOYING']
                        role.set_state(Role::STATE['PENDING'])
                        role.recover_deployment
                    end
                end

                set_state(Service::STATE['DEPLOYING'])

            elsif state == Service::STATE['FAILED_SCALING']
                @roles.each do |_name, role|
                    if role.state == Role::STATE['FAILED_SCALING']
                        role.recover_scale
                        role.set_state(Role::STATE['SCALING'])
                    end
                end

                set_state(Service::STATE['SCALING'])

            elsif state == Service::STATE['FAILED_UNDEPLOYING']
                @roles.each do |_name, role|
                    if role.state == Role::STATE['FAILED_UNDEPLOYING']
                        role.set_state(Role::STATE['RUNNING'])
                    end
                end

                set_state(Service::STATE['UNDEPLOYING'])

            elsif state == Service::STATE['COOLDOWN']
                @roles.each do |_name, role|
                    if role.state == Role::STATE['COOLDOWN']
                        role.set_state(Role::STATE['RUNNING'])
                    end
                end

                set_state(Service::STATE['RUNNING'])

            elsif state == Service::STATE['WARNING']
                @roles.each do |_name, role|
                    if role.state == Role::STATE['WARNING']
                        role.recover_warning
                    end
                end

            else
                return OpenNebula::Error.new('Action recover: Wrong state' \
                                             " #{state_str}")
            end

            update
        end

        # Delete the service. All the VMs are also deleted from OpenNebula.
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise

        def delete
            networks = JSON.parse(self['TEMPLATE/BODY'])['networks_values']

            networks.each do |net|
                next unless net[net.keys[0]].key? 'template_id'

                net_id = net[net.keys[0]]['id'].to_i

                rc = OpenNebula::VirtualNetwork
                     .new_with_id(net_id, @client).delete

                if OpenNebula.is_error?(rc)
                    log_info("Error deleting vnet #{net_id}: #{rc}")
                end
            end

            super()
        end

        def delete_roles
            @roles.each do |_name, role|
                role.set_state(Role::STATE['DELETING'])
                role.delete
            end
        end

        # Retrieves the information of the Service and all its Nodes.
        #
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def info
            rc = super
            if OpenNebula.is_error?(rc)
                return rc
            end

            @roles = {}

            if @body['roles']
                @body['roles'].each do |elem|
                    elem['state'] ||= Role::STATE['PENDING']
                    role = Role.new(elem, self)
                    @roles[role.name] = role
                end
            end

            nil
        end

        # Add an info message in the service information that will be stored
        #   in OpenNebula
        # @param [String] message
        def log_info(message)
            add_log(Logger::INFO, message)
        end

        # Add an error message in the service information that will be stored
        #   in OpenNebula
        # @param [String] message
        def log_error(message)
            add_log(Logger::ERROR, message)
        end

        # Changes the owner/group
        #
        # @param [Integer] uid the new owner id. Use -1 to leave the current one
        # @param [Integer] gid the new group id. Use -1 to leave the current one
        #
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def chown(uid, gid)
            old_uid = self['UID'].to_i
            old_gid = self['GID'].to_i

            rc = super(uid, gid)

            if OpenNebula.is_error?(rc)
                return rc
            end

            @roles.each do |_name, role|
                rc = role.chown(uid, gid)

                break if rc[0] == false
            end

            if rc[0] == false
                log_error('Chown operation failed, will try to rollback ' \
                          'all VMs to the old user and group')

                update

                super(old_uid, old_gid)

                @roles.each do |_name, role|
                    role.chown(old_uid, old_gid)
                end

                return OpenNebula::Error.new(rc[1])
            end

            nil
        end

        # Updates a role
        # @param [String] role_name
        # @param [String] template_json
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def update_role(role_name, template_json)
            if ![Service::STATE['RUNNING'], Service::STATE['WARNING']]
               .include?(state)

                return OpenNebula::Error.new('Update role: Wrong state' \
                                             " #{state_str}")
            end

            template = JSON.parse(template_json)

            # TODO: Validate template?

            role = @roles[role_name]

            if role.nil?
                return OpenNebula::Error.new("ROLE \"#{role_name}\" " \
                                             'does not exist')
            end

            rc = role.update(template)

            if OpenNebula.is_error?(rc)
                return rc
            end

            # TODO: The update may not change the cardinality, only
            # the max and min vms...

            role.set_state(Role::STATE['SCALING'])

            role.set_default_cooldown_duration

            set_state(Service::STATE['SCALING'])

            update
        end

        def shutdown_action
            @body['shutdown_action']
        end

        # Replaces the template contents
        #
        # @param template_json [String] New template contents
        # @param append [true, false] True to append new attributes instead of
        #   replace the whole template
        #
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def update(template_json = nil, append = false)
            if template_json
                template = JSON.parse(template_json)

                if append
                    rc = info

                    if OpenNebula.is_error? rc
                        return rc
                    end

                    template = @body.merge(template)
                end

                template_json = template.to_json
            end

            super(template_json, append)
        end

        # Replaces the raw template contents
        #
        # @param template [String] New template contents, in the form KEY = VAL
        # @param append [true, false] True to append new attributes instead of
        #   replace the whole template
        #
        # @return [nil, OpenNebula::Error] nil in case of success, Error
        #   otherwise
        def update_raw(template_raw, append = false)
            super(template_raw, append)
        end

        private

        # Maximum number of log entries per service
        # TODO: Make this value configurable
        MAX_LOG = 50

        # @param [Logger::Severity] severity
        # @param [String] message
        def add_log(severity, message)
            severity_str = Logger::SEV_LABEL[severity][0..0]

            @body['log'] ||= []
            @body['log'] << {
                :timestamp => Time.now.to_i,
                :severity  => severity_str,
                :message   => message
            }

            # Truncate the number of log entries
            @body['log'] = @body['log'].last(MAX_LOG)
        end

    end

end
