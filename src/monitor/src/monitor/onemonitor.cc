/* -------------------------------------------------------------------------- */
/* Copyright 2002-2019, OpenNebula Project, OpenNebula Systems                */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */

#include <iostream>
#include <getopt.h>

#include "Monitor.h"
#include "Nebula.h"

using namespace std;

static void print_license()
{
    cout<< "Copyright 2002-2019, OpenNebula Project, OpenNebula Systems        \n\n"
        << Nebula::version() << " is distributed and licensed for use under the"
        << " terms of the\nApache License, Version 2.0 "
        << "(http://www.apache.org/licenses/LICENSE-2.0).\n";
}

static void print_usage(ostream& str)
{
    str << "Usage: monitord [-h] [-c]\n";
}

static void print_help()
{
    print_usage(cout);

    cout << "\n"
         << "SYNOPSIS\n"
         << "  Starts the OpenNebula Monitor daemon. This program should not be"
         << "  executed directly\n\n"
         << "OPTIONS\n"
         << "  -v, --version\toutput version information and exit\n"
         << "  -h, --help\tdisplay this help and exit\n"
         << "  -c, --config\tConfiguration file name (default monitord.conf)\n";
}

int main(int argc, char **argv)
{
    static struct option long_options[] = {
        {"version",no_argument, 0, 'v'},
        {"help",   no_argument, 0, 'h'},
        {"config", no_argument, 0, 'c'},
        {0,        0,           0, 0}
    };

    int long_index = 0;
    int opt;

    std::string config = "monitord.conf";

    while ((opt = getopt_long(argc, argv, "vhc:",
                    long_options, &long_index)) != -1)
    {
        switch(opt)
        {
            case 'v':
                print_license();
                exit(0);
                break;
            case 'h':
                print_help();
                exit(0);
                break;
            case 'c':
                config = optarg;
                break;
            default:
                print_usage(cerr);
                exit(-1);
                break;
        }
    }

    try
    {
        Monitor monitor(config);
        monitor.start();
    }
    catch (exception &e)
    {
        cerr << e.what() << endl;

        return -1;
    }

    return 0;
}
