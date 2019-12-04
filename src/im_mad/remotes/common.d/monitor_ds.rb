#!/usr/bin/ruby

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

require 'yaml'
require 'fileutils'

def metric(dir, column)
    metric =    `df -B1M -P #{dir} 2>/dev/null | tail -n 1 | \
                 awk '{print $#{column}}'`.chomp

    return 0 if metric.empty?

    metric
end

yaml = STDIN.read
yaml = YAML.safe_load yaml

ds_location = yaml[:ds_location]
ds_location ||= '/var/lib/one/datastores'
FileUtils.mkdir_p ds_location

puts "DS_LOCATION_USED_MB=#{metric(ds_location, 3)}"
puts "DS_LOCATION_TOTAL_MB=#{metric(ds_location, 2)}"
puts "DS_LOCATION_FREE_MB=#{metric(ds_location, 4)}"

Dir.chdir ds_location
datastores = Dir.glob('*').select {|f| File.directory?(f) && f.match(/^\d+$/) }
datastores.each do |ds|
    dir = "#{ds_location}/#{ds}"

    data = <<EOT
DS = [
  ID = #{ds_id},
  USED_MB = #{metric(dir, 3)},
  TOTAL_MB = #{metric(dir, 2)},
  FREE_MB = #{metric(dir, 4)}
]
EOT

    puts data

    # Skip if datastore is not marked for local monitoring
    mark = "#{dir}/.monitor"

    next unless File.exist? mark

    driver = File.read mark
    driver ||= 'ssh'

    tm_script = "#{__dir__}/../../../../tm/#{driver}/monitor_ds"
    `#{tm_script} #{dir}` if File.exist? tm_script
end
