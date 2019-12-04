#!/usr/bin/ruby

require 'sequel'
require 'yaml'

# SQlite Interface to the caching database for the status probes
class DB

    DB_PATH = "#{__dir__}/../status.db"
    CONFIG  = "#{__dir__}/../../etc/im/probe_db.conf"

    def initialize(time, hypervisor)
        @config = YAML.load_file(CONFIG)

        @db = Sequel.connect("sqlite://#{DB_PATH}")

        setup_db rescue nil
        @dataset = @db[:states]

        @stored_ids = @dataset.map(:id)

        @time = time
        @hypervisor = hypervisor

        clean_old
    end

    # Returns the VM status that changed compared to the DB info
    def new_status(vms, timestamp = @time)
        new_data = ''
        @real_ids = []

        vms.each do |vm|
            id = vm[/ID=[0-9-]+/].split('=').last.to_i
            status = vm[/STATUS="\S"?/].split('=').last

            @real_ids << id

            begin
                vminfo = get(id)
                next unless vminfo[:status] != status

                update(id, status)

                new_data << "VM=[\n#{vm}"
            rescue # no match found
                did = vm[/DEPLOY_ID=[-0-9a-zA-Z_]+/].split('=').last.to_i

                insert(:id => id, :did => did, :status => status,
                    :timestamp => timestamp, :hypervisor => @hypervisor)

                new_data << "VM=[\n#{vm}"
            end
        end

        clean_deleted

        new_data
    end

    # Updates the status of an existing VM entry
    def update(id, status, time = @time, hypervisor = @hypervisor)
        @dataset.where(:id => id).update(:status => status,
            :timestamp => time, :hypervisor => hypervisor)
    end

    # Adds a new full VM entry
    # @param [Hash] row the attributes for the VM
    def insert(row)
        @dataset.insert(row)
    end

    # Deletes a VM entry that no longer exists on the host
    def delete(condition)
        @dataset.where(condition).delete
    end

    # Returns a VM status hash
    def get(id)
        @dataset.first(:id => id)
    end

    private

    def clean_deleted
        dead_vm_ids = @stored_ids - @real_ids

        dead_vm_ids.each {|id| delete(:id => id) }
    end

    # TODO: Maybe configure via monitord ?
    # Deletes VM entries prior to the current time
    def clean_old
        obsolete = @config[:obsolete] * 60 # conf in minutes
        now = Time.now.to_i

        delete((now - Sequel[:timestamp]) >= obsolete)
    end

    def setup_db
        @db.create_table :states do
            primary_key :id
            String      :did
            Integer     :timestamp
            String      :status
            String      :hypervisor
        end
    end

end
