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

#include "UR_V16X_Driver.h"

// constructor
UR_V16X_Driver::UR_V16X_Driver(UR_V16X &v16x) :
    _frontend(v16x)
{
    for (uint8_t j = 0; j < V16X_MAX_ENDPOINTS; j++) {
        for (uint16_t i = 0; i < V16X_MAX_CLIENTS; i++) {
            _frontend.endpoints[j].clients[i] = nullptr;
        }
    }
}

void UR_V16X_Driver::driver_update(uint8_t endpoint)
{
    update();
}

/*
  copy latest data to the manager from a driver
 */
void UR_V16X_Driver::_copy_client_to_frontend(uint8_t endpoint, int client_id, bool attached, unsigned int client_cnt, const char *address, uint16_t port)
{
    if (endpoint >= _frontend._num_endpoints) {
        return;
    }

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; i++) {
        if (_frontend.endpoints[endpoint].clients[i] != nullptr) {
            if (_frontend.endpoints[endpoint].clients[i]->client_id == client_id) {
                return;
            }
        }
    }

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; i++) {
        if (_frontend.endpoints[endpoint].clients[i] == nullptr) {
            _frontend.endpoints[endpoint].clients[i] = new UR_V16X::clients_t;
            _frontend.endpoints[endpoint].clients[i]->client_id = client_id;
            _frontend.endpoints[endpoint].clients[i]->attached = attached;
            _frontend.endpoints[endpoint].clients[i]->address = address;
            _frontend.endpoints[endpoint].clients[i]->port = port;
            _frontend.endpoints[endpoint].client_cnt = client_cnt;
            break;
        }
    }
#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("+++++COPY FRONTEND Client ID: %d pos %d\n\n", client_id, client_cnt);
    _frontend.print_endpoints_data();
    SHAL_SYSTEM::printf("\n");
#endif // V16X_DEBUG
}

void UR_V16X_Driver::_delete_client_from_frontend(uint8_t endpoint, int client_id, unsigned int client_cnt)
{
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; i++) {
        if (_frontend.endpoints[endpoint].clients[i]) {
            if (_frontend.endpoints[endpoint].clients[i]->client_id == client_id) {
                delete _frontend.endpoints[endpoint].clients[i];
                _frontend.endpoints[endpoint].clients[i] = nullptr;
#if V16X_DEBUG >= 1
                SHAL_SYSTEM::printf("*****REMOVE Client ID: %d pos %d\n", client_id, i);
                fflush(stdout);
#endif
                //_frontend.endpoints[endpoint].client_cnt = client_cnt;
                break;
            }
        }
    }
    _frontend.endpoints[endpoint].client_cnt = client_cnt;
}

void UR_V16X_Driver::driver_fire_process()
{
    fire_process();
}

void UR_V16X_Driver::driver_shutdown()
{
    shuttdown();
}
