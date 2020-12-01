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

#include <UR_V16X/UR_V16X.h>

class UR_V16X_Driver {
public:
    UR_V16X_Driver(UR_V16X &v16x);
    virtual ~UR_V16X_Driver(void) {};

    virtual void update() = 0;
    void driver_update(uint8_t endpoint);

    virtual void fire_process() = 0;
    void driver_fire_process();
    virtual void shuttdown() = 0;
    void driver_shutdown();

protected:
    // reference to V16X frontend.
    UR_V16X &_frontend;

    // copy to V16X frontend.
    void _copy_client_to_frontend(uint8_t endpoint, int client_id, bool attached, unsigned int client_cnt, const char *address, uint16_t port);

    // delete client registered on endpoint.
    void _delete_client_from_frontend(uint8_t endpoint, int client_id, unsigned int client_cnt);
};
