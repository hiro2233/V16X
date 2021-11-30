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

#pragma once

#include <UR_V16X/UR_V16X.h>

class V16X_server {
public:

    V16X_server();

    // configure or initialize all parts that we will need when run (loop)
    // the system.
    void configure();

    // run and loop the system forever.
    void loop();

    // fire internal process from a class inside the run timer thread.
    void fire_process(void);

    // shutdown all system
    void server_shutdown();
    void printhelp();
    void fire_proc_v16x();

private:

    static UR_V16X v16x;
    int bindport;
    char *bindaddr;
};
