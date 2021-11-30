/*
    V16X Server is a ultralight and multithreaded HTTP I/O realtime hybrid
    nanoservices server.
    Copyright (c) 2019,2020 Hiroshi Takey F. <htakey@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "v16x_server.h"
#include <getopt.h>

UR_V16X V16X_server::v16x;

const char msg[] = {"V16X Server, HTTP I/O realtime hybrid nanoservices server.\n" \
                    "Copyright (c) 2019,2020 Hiroshi Takey F. <htakey@gmail.com>\n" \
                    "This program comes with ABSOLUTELY NO WARRANTY.\n" \
                    "This is free software; licensed under AGPLv3.\n" \
                    "See source distribution for detailed copyright notices.\n"};

V16X_server::V16X_server()
{
}

void V16X_server::fire_process(void)
{
    while(!sig_evt) {
        v16x.update();
        if (!sig_evt) {
            SHAL_SYSTEM::run_thread_process(FUNCTOR_BIND_MEMBER(&V16X_server::fire_proc_v16x, void));
            fflush(stdout);
        }
    }
    SHAL_SYSTEM::printf("[ Exit fire process ]\n");
    fflush(stdout);
}

void V16X_server::server_shutdown()
{
    v16x.shutdown_all();
}

void V16X_server::printhelp()
{
    SHAL_SYSTEM::printf("%s\n\n", msg);
    SHAL_SYSTEM::printf("V16X server version %s\n", SERVER_VERSION);
    SHAL_SYSTEM::printf("Usage: v16x [-h] [-a <addr>] [-p <port>]\n");
    SHAL_SYSTEM::printf("       -a <address>   - Bind to IP address\n");
    SHAL_SYSTEM::printf("       -p <port>      - Bind to port\n");
    SHAL_SYSTEM::printf("       -h             - Print this help\n");
    exit(0);
}

void V16X_server::configure()
{
    int c;
    while ((c = getopt(SHAL_SYSTEM::system_argcv.argc, SHAL_SYSTEM::system_argcv.argv, "a:p:h")) != EOF) {
        switch(c) {
            case 'a':
                bindaddr = optarg;
                break;
            case 'p':
                bindport = atoi(optarg);
                break;
            case 'h':
                printhelp();
                break;
            default:
                SHAL_SYSTEM::printf("Unrecognized option\n");
                printhelp();
                break;
        }
    }

    SHAL_SYSTEM::printf("V16X server version %s\n\n", SERVER_VERSION);
    SHAL_SYSTEM::printf("Configuring server\n");

    v16x.set_bindaddr(bindaddr);
    v16x.set_bindport(bindport);
    v16x.init();

    SHAL_SYSTEM::printf("Server started\n");
    fflush(stdout);
}

void V16X_server::loop()
{
    v16x.print_endpoints_data();
    SHAL_SYSTEM::delay_sec(10);
}

void V16X_server::fire_proc_v16x()
{
    v16x.fire_process();
}
