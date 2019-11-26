# !/usr/bin/env ruby

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

$LOAD_PATH.unshift "#{File.dirname(__FILE__)}/../../../../vmm/lxd/"

require 'container'
require 'client'
require 'base64'
require_relative '../../../lib/poll_common'
require 'sequel'

module LXD

    CLIENT = LXDClient.new

    class << self

        # Get and translate LXD state to Opennebula monitor state
        #  @param state [String] libvirt state
        #  @return [String] OpenNebula state
        #
        # LXD states for the guest are
        #  * 'running' state refers to containers which are currently active.
        #  * 'frozen' after lxc freeze (suspended).
        #  * 'stopped' container not running or in the process of shutting down.
        #  * 'failure' container have failed.
        def one_status(container)
            begin
                status = container.status.downcase
            rescue StandardError
                status = 'unknown'
            end

            case status
            when 'running'
                state = 'a'
            when 'frozen'
                state = 'p'
            when 'stopped'
                state = 'd'

                state = '-' if container.config['user.one_status'] == '0'
            when 'failure'
                state = 'e'
            else
                state = '-'
            end

            state
        end

        def get_all_vm_status
            vms = Container.get_all(CLIENT)

            return unless vms

            vms_info = {}
            vms.each do |container|
                vms_info[container.name] = { :status => one_status(container) }
            end

            vms_info
        end

  end

end

def connect(db_path)
    Sequel.connect("sqlite://#{db_path}")
end

def setup_db(db)
    db.create_table :states do
        primary_key :id
        # String :timestamp # TODO: Add status update-based timestamp
        String :did
        String :status
    end
end

DB_PATH = 'status.db'

################################################################################
# MAIN PROGRAM
################################################################################

data = all_vm_status(LXD)
vms = data.split('VM=')
new_data = ''

db = connect(DB_PATH)

begin
    setup_db(db)
rescue
end

dataset = db[:states]

vms.each do |vm|
    id = vm[/ID=\d+/]
    did = vm[/DEPLOY_ID=[-0-9a-zA-Z_]+/] # TODO: May change ?
    status = vm[/STATUS=\S?/]

    begin
        vminfo = dataset.match(id)

        if vminfo[:state] != status
            dataset.update(vminfo[:state] => data)
            new_data << vm
        end
    rescue
        dataset.insert(:id => id, :did => did, :status => status)
        new_data << vm
    end
end

return if new_data.empty?

puts "VM_STATUS=YES\n#{new_data}"
