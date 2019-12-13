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

################################################################################
# Functions to interface hypervisor information
################################################################################

# Loads the rc variables and overrides the default values
# @param hypervisor [Module] The hypervisor Module
# @param file [String] Name of the configuration file
# @param vars [Array] Array of variables to read
def load_vars(hypervisor, file, vars)
    env   = `. "#{file}"; env`
    lines = env.split("\n")

    vars.each do |var|
        lines.each do |line|
            if (a = line.match(/^(#{var})=(.*)$/))
                hypervisor::CONF[var] = a[2]
                break
            end
        end
    end
rescue
end

# Returns an OpenNebula monitor string
# @param name [String] of the monitor metric
# @param value [String] of the monitor metric
# @return [String, nil]
def monitor_data(name, value)
    return if value.nil? || (value.respond_to?(:empty?) && value.empty?)

    if value.instance_of? Array
        data_str = ''
        value.each do |v|
            data_str += monitor_data(name, v)
        end

        return data_str
    elsif value.instance_of? Hash
        values = value.map do |k, v|
            "#{k.to_s.upcase}=#{v}"
        end.join(', ')

        return "#{name.to_s.upcase}=[ #{values} ] "
    else
        return "#{name.to_s.upcase}=#{value}"
    end
end

# Puts to STDOUT a string in the form "VAL1=VAR1 VAL2=VAR2" with the monitor
# attributes of the VM
# @param hypervisor [Module]
# @param vm_id [String] with the VM ID
def one_vm_info(hypervisor, vm_id)
    info = hypervisor.get_vm_info(vm_id)

    exit(-1) unless info

    values = info.map do |key, value|
        monitor_data(key, value)
    end

    values.zip.join(' ')
end

def all_vm_info(hypervisor)
    require 'yaml'
    require 'zlib'

    vms = hypervisor.all_vm_info

    return if vms.nil?

    compressed = Zlib::Deflate.deflate(vms.to_yaml)

    Base64.encode64(compressed).delete("\n")
end

def all_vm_template(hypervisor)
    vms = hypervisor.all_vm_info

    return if vms.nil?

    string = ''

    vms.each do |name, data|
        template, usage = vm_info(name, data)

        string << template
        string << "  POLL=\"#{usage}\" ]\n"
    end

    string
end

def all_vm_status(hypervisor)
    vms = hypervisor.all_vm_status

    return if vms.nil?

    string = ''

    vms.each do |name, data|
        template, usage = vm_info(name, data)

        string << template
        string << "  #{usage} ]\n"
    end

    string
end

def vm_info(name, data)
    if name =~ /^one-\d*$/
        number = name.split('-').last
    else
        number = -1
    end

    template =   "VM=[\n"
    template <<  "  ID=#{number},\n"
    template <<  "  DEPLOY_ID=#{name},\n"
    template << %(  VM_NAME="#{data[:vm_name]}",\n) if data[:vm_name]

    if data[:template]
        template << %(  IMPORT_TEMPLATE="#{data[:template]}",\n)
        data.delete(:template)
    end

    usage = data.map do |key, value|
        monitor_data(key, value)
    end

    usage = usage.zip.join(' ')

    [template, usage]
end
