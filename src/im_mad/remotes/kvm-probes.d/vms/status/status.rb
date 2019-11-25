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
# See the License for the specific language governing permissions and        #a
# limitations under the License.                                             #
#--------------------------------------------------------------------------- #

$LOAD_PATH << File.join(File.dirname(__FILE__), '../monitor')

ENV['LANG'] = 'C'
ENV['LC_ALL'] = 'C'

################################################################################
#
#  KVM Status Monitor Module
#
################################################################################
module KVM

    # Constants for KVM operations
    # Translate libvirt state to Opennebula monitor state
    #  @param state [String] libvirt state
    #  @return [String] OpenNebula state
    #
    # Libvirt states for the guest are
    #  * 'running' state refers to guests which are currently active on a CPU.
    #  * 'idle' ('blocked') not running or runnable (waiting on I/O or in a sleep mode).
    #  * 'paused' after virsh suspend.
    #  * 'in shutdown' ('shutdown') guest in the process of shutting down.
    #  * 'dying' the domain has not completely shutdown or crashed.
    #  * 'crashed' guests have failed while running and are no longer running.
    #  * 'pmsuspended' suspended by guest power management (e.g. S3 state)
    # TODO: Support pause/resume
    def self.get_state(state, reason = 'missing')
        case state.delete('-')
        when 'running', 'idle', 'blocked', 'in shutdown', 'shutdown', 'dying'
            'a'
        when 'paused'
            case reason
            when 'migrating'
                'a'
            when 'I/O error', 'watchdog', 'crashed', 'post-copy failed', 'user', 'unknown'
                'e'
            else
                'a'
            end
        when 'crashed', 'pmsuspended'
            'e'
        else
            '-'
        end
    end

    # Gets the information of all VMs
    #
    # @return [Hash, nil] Hash with the VM information or nil in case of error
    def self.get_all_vm_status
        vms_info = {}
        vms      = {}

        text = `#{virsh(:list)}`

        return if $CHILD_STATUS.exitstatus != 0

        lines = text.split(/\n/)[2..-1]

        names = lines.map do |line|
            line.split(/\s+/).delete_if {|d| d.empty? }[1]
        end

        return vms_info if names.empty?

        names.each do |vm|
            dominfo = dom_info(vm)

            next unless dominfo

            psinfo = process_info(dominfo['UUID'])

            info = {}

            info[:dominfo] = dominfo
            info[:name]    = vm
            info[:reason]  = dom_state_reason(vm)

            vms[vm] = info
        end

        vms.each do |name, vm|
            dominfo = vm[:dominfo]

            values = {}
            values[:state] = get_state(dominfo['State'], vm[:reason])

            if (values[:state] == 'a') && (vm[:reason] != 'migrating')
                values.merge!(get_io_statistics(name, xml))
            end

            vms_info[vm[:name]] = values
        end

        vms_info
    end

end

################################################################################
# MAIN PROGRAM
################################################################################

hypervisor = KVM
file       = '../../../../etc/vmm/kvm/kvmrc'
vars       = %w[LIBVIRT_URI]

load_vars(hypervisor, file, vars)

print_all_vm_status(hypervisor)
