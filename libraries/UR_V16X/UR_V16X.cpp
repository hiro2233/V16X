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

#include <UR_V16X/UR_V16X.h>
#include <UR_V16X/UR_V16X_Posix.h>

// singleton endpoint
UR_V16X *UR_V16X::_frontend;

/*
  UR_V16X constructor
 */
UR_V16X::UR_V16X() :
    _num_drivers(0),
    _num_endpoints(0)
{
    _frontend = this;
}

bool UR_V16X::_add_driver(UR_V16X_Driver *driver)
{
    if (!driver) {
        return false;
    }
    if (_num_drivers >= V16X_MAX_DRIVERS) {
        SHAL_SYSTEM::panic("Too many drivers");
    }
    drivers[_num_drivers++] = driver;
    return true;
}

#define ADD_DRIVER(driver) \
    if (_add_driver(driver)) {     \
       if (_num_drivers == V16X_MAX_DRIVERS || \
           _num_endpoints == V16X_MAX_ENDPOINTS ) { \
          return; \
       } \
    }

/*
  initialise the UR_V16X object, loading driver drivers
 */
void UR_V16X::init(void)
{
#if defined(_UNIX_) || defined(__MSYS__)
    ADD_DRIVER(UR_V16X_Posix::create_endpoint(*this));
#endif

    if (_num_drivers == 0 || _num_endpoints == 0 || drivers[0] == nullptr) {
        SHAL_SYSTEM::panic("V16X: unable to initialise driver");
    }
}

/*
  call update on all drivers
 */
void UR_V16X::update(void)
{
    for (uint8_t i = 0; i < _num_drivers; i++) {
        drivers[i]->driver_update(i);
    }
}

/* register a new endpoint, claiming a endpoint slot. If we are out of
   slots it will panic
*/
uint8_t UR_V16X::register_endpoint(void)
{
    if (_num_endpoints >= V16X_MAX_ENDPOINTS) {
        SHAL_SYSTEM::panic("Too many endpoints");
    }
    return _num_endpoints++;
}

void UR_V16X::print_endpoints_data()
{
    for (uint8_t i = 0; i < _num_endpoints; i++) {
        if (endpoints[i].client_cnt > 0) {
            SHAL_SYSTEM::printf("\nClient cnt: %d\n", endpoints[i].client_cnt);
        }
        for (uint16_t j = 0; j < V16X_MAX_CLIENTS; j++) {
            if (endpoints[i].clients[j]) {
                SHAL_SYSTEM::printf("PRINT DATA Driver: #%d Endpoint #%d Client ID: %u Attached: %d Address: %s:%d\n", _num_drivers, i, endpoints[i].clients[j]->client_id, endpoints[i].clients[j]->attached, endpoints[i].clients[j]->address, endpoints[i].clients[j]->port);
                //fflush(stdout);
            }
        }
    }
}

void UR_V16X::fire_process()
{
    for (uint8_t i = 0; i < _num_drivers; i++) {
        drivers[i]->driver_fire_process();
    }
}

void UR_V16X::shutdown_all()
{
    for (uint8_t i = 0; i < _num_drivers; i++) {
        drivers[i]->driver_shutdown();
    }
}
