/*
   URUS V16X Synthetic IO driver.
   Copyright (c) 2019,2020 Hiroshi Takey F. <htakey@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "system/system.h"

#define V16X_MAX_ENDPOINTS 3
#define V16X_MAX_DRIVERS 3
#define V16X_MAX_CLIENTS 100
#define V16X_DEBUG 0

class UR_V16X_Driver;

class UR_V16X {

    friend class UR_V16X_Driver;

public:
    UR_V16X();

    /* Do not allow copies */
    UR_V16X(const UR_V16X &other) = delete;
    UR_V16X &operator=(const UR_V16X&) = delete;

    // get singleton
    static UR_V16X *get_instance(void) {
        return _frontend;
    }

    // V16X types
    typedef enum {
        V16X_TYPE_FILE,
        V16X_TYPE_DIR,
        V16X_TYPE_DATA
    } client_type_e;

    // initialise the V16X object, loading endpoints drivers
    void init(void);

    // update the V16X object, asking drivers to push data to
    // the frontend.
    void update(void);

    // register an endpoint into the driver.
    uint8_t register_endpoint(void);

    // print endpoint record data.
    void print_endpoints_data();

    // fire internal process from a class inside the run timer thread.
    void fire_process();

    // shutdown V16X drivers.
    void shutdown_all();

private:
    // singleton.
    static UR_V16X *_frontend;

    // how many drivers do we have?.
    uint8_t _num_drivers;

    UR_V16X_Driver *drivers[V16X_MAX_DRIVERS];

    // how many endpoints do we have?.
    uint8_t _num_endpoints;

    // clients stored data record.
    typedef struct __clients_s {
        client_type_e type;
        uint32_t last_update_ms;
        uint32_t last_change_ms;
        bool attached;
        bool ttl_ok:1;
        int client_id;
        const char *address;
        uint16_t port;
    } clients_t;

    // endpoint driver stored data record slots.
    struct endpoint {
        unsigned int client_cnt;
        clients_t *clients[V16X_MAX_CLIENTS];
    } endpoints[V16X_MAX_ENDPOINTS];

    // add a driver.
    bool _add_driver(UR_V16X_Driver *driver);
};
