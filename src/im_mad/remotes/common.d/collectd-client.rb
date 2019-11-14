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

require 'socket'
require 'base64'
require 'resolv'
require 'ipaddr'
require 'zlib'
require 'probe-manager'

DIRNAME = File.dirname(__FILE__)

class CollectdClient

    def initialize(host, port, hypervisor, ds_location, retries)
        @socket = open_socket(host, port)

        probe_info(port, hypervisor, ds_location, retries)

        @last_update = last_update
    end

    def send(data)
        message, code = data
        code ? result = 'SUCCESS' : result = 'FAILURE'
        @socket.send("MONITOR #{result} #{@retries} #{message}\n", 0)
    end

    # TODO: Send if != DB info
    def optsend(data)
        send(data)
    end

    # Runs the specifed probes and sends the data
    def monitor(probes, push_period)
        before = Time.now

        exit 0 if stop?

        data = run_probes(probes, push_period)
        client.send data unless db_match(data)

        # Sleep during the Cycle
        happened = (Time.now - before).to_i
        sleep_time = push_period - happened
        sleep sleep_time if sleep_time < 0
    end

    private

    def probe_info(port, hypervisor, ds_location, retries)
        @port           = port
        @hypervisor     = hypervisor
        @ds_location    = ds_location
        @retries        = retries

        @run_probes_cmd = File.join(DIRNAME, '..', 'run_probes')
    end

    def probe_cmd(dir, push_period)
        `#{@run_probes_cmd} #{@hypervisor}-probes #{@ds_location} #{@port} #{push_period} #{@retries} #{dir} 2>&1`
    end

    def run_probes(dir, push_period)
        data   = probe_cmd(dir, push_period)
        code   = $CHILD_STATUS.exitstatus == 0

        zdata  = Zlib::Deflate.deflate(data, Zlib::BEST_COMPRESSION)
        data64 = Base64.encode64(zdata).strip.delete("\n")

        [data64, code]
    end

    # TODO: Compare with DB before send
    # Returns true if data matches local DB info
    def db_match(_data)
        false
    end

    def ipv4_address(host)
        addresses = Resolv.getaddresses(host)
        address = nil

        addresses.each do |addr|
            begin
                a = IPAddr.new(addr)
                if a.ipv4?
                    address = addr
                    break
                end
            rescue
            end
        end

        address
    end

    # TODO: Encript socket
    def open_socket(host, port)
        ip = ipv4_address(host)
        @socket = UDPSocket.new
        @socket.bind(ip, port)
    end

    def last_update
        File.stat(REMOTE_DIR_UPDATE).mtime.to_i rescue 0
    end

    def stop?
        last_update.to_i != @last_update.to_i
    end

end

#######################
# Argument processing #
#######################

host         = ENV['SSH_CLIENT'].split.first
port         = ARGV[2]
hypervisor   = ARGV[0]
ds_location  = ARGV[1]
push_periods = ARGV[3]
retries      = ARGV[4]

#############################
# Start push monitorization #
#############################

client = CollectdClient.new(host, port, hypervisor, ds_location, retries)

threads = []
threads << Thread.new { client.monitor('host/system', push_periods[0]) }
threads << Thread.new { client.monitor('host/monitor', push_periods[1]) }
threads << Thread.new { client.monitor('vms/status', push_periods[2]) }
threads << Thread.new { client.monitor('vms/monitor', push_periods[3]) }
threads << Thread.new do
    sleep push_periods[0]
    `bash #{__dir__}/../#{hypervisor}-probes.d/collectd-client-shepherd`
end

threads.each {|thr| thr.join }
