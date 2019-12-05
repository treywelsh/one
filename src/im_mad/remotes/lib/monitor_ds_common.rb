#!/usr/bin/ruby

#!/usr/bin/ruby

class DSMonitor

    def initialize(probe_args)
        @ds_location = probe_args[:ds_location]
        @ds_location ||= '/var/lib/one/datastores'
        FileUtils.mkdir_p @ds_location
    end

    def dss_metrics
        puts location_metrics

        Dir.chdir @ds_location

        datastores = Dir.glob('*').select do |f|
            File.directory?(f) && f.match(/^\d+$/)
        end

        datastores.each do |ds|
            dir = "#{@ds_location}/#{ds}"

            puts data(dir)

            # Skip if datastore is not marked for local monitoring
            mark = "#{dir}/.monitor"

            next unless File.exist? mark
            driver = File.read mark

            run_tm(dir, driver)
        end
    end

    # implement in child class
    def location_metrics
        # "DS_LOCATION_TOTAL_MB=#{metric(ds_location, 2)}"
    end

    def data(dir)
        # implement in child class
        #         data = <<EOT
        # DS = [
        #   ID = #{dir},
        # EOT
    end

    def run_tm(dir, mark)
        driver ||= 'ssh'

        tm_script = "#{__dir__}/../../../../tm/#{driver}/monitor_ds"
        `#{tm_script} #{dir}` if File.exist? tm_script
    end

    def used(dir)
        "USED_MB = #{metric(dir, 3)}"
    end

    def total(dir)
        "TOTAL_MB = #{metric(dir, 2)}"
    end

    def free(dir)
        "FREE_MB = #{metric(dir, 4)}"
    end

    private

    def metric(dir, column)
        metric =    `df -B1M -P #{dir} 2>/dev/null | tail -n 1 | \
                     awk '{print $#{column}}'`.chomp

        return 0 if metric.empty?

        metric
    end

end
