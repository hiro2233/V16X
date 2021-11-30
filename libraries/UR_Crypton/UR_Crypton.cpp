/*
   URUS Cryptographic library driver.
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

#include "UR_Crypton.h"
#include "UR_Crypton_Backend.h"
#include "UR_Crypton_Native.h"
#include "UR_Crypton_Openssl.h"

UR_Crypton::UR_Crypton() :
    _backend_count(0)
{}

void UR_Crypton::configure(ProcessMode process_mode)
{
    _configure_backends(process_mode);
}

void UR_Crypton::_configure_backends()
{
    if (_backends_configuring) {
        return;
    }

    _backends_configuring = true;

#if (CRYPTON_TYPE == CRYPTON_NATIVE)
    _add_backend(UR_Crypton_Native::configure(*this));
#elif (CRYPTON_TYPE == CRYPTON_OPENSSL)
    _add_backend(UR_Crypton_Openssl::configure(*this));
#endif

    _backends_configuring = false;
}

void UR_Crypton::_configure_backends(ProcessMode process_mode)
{
    _configure_backends();

    for (uint8_t i = 0; i < _backend_count; i++) {
        if (_backends[i] != NULL) {
            _backends[i]->init_process();
        }
    }
}

bool UR_Crypton::_add_backend(UR_Crypton_Backend *backend)
{
    if (!backend) {
        return false;
    }

    if (_backend_count == UR_CRYPTON_MAX_BACKENDS) {
        SHAL_SYSTEM::printf("UR_Crypton: MAX BACKEND REACHED!\n");
    }

    _backends[_backend_count++] = backend;

    return true;
}

int UR_Crypton::b64_enc(char *output, int outputLen, const char *input, int inputLen)
{
    return _backends[0]->b64_enc(output, outputLen, input, inputLen);
}

int UR_Crypton::b64_dec(char * output, char * input, int inputLen)
{
    return _backends[0]->b64_dec(output, input, inputLen);
}

void UR_Crypton::sha1_apply(const unsigned char* src, unsigned char* digest)
{
    _backends[0]->sha1_apply(src, digest);
}
