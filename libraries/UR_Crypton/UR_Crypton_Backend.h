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

#pragma once

#include <UR_V16X/UR_V16X.h>
#include "UR_Crypton.h"

class UR_Crypton_Backend {
public:

    UR_Crypton_Backend(UR_Crypton &ur_crypton);

    // we declare a virtual destructor so that drivers can
    // override with a custom destructor if need be.
    virtual ~UR_Crypton_Backend(void) {}

    /** Process the backend.
      * @return None.
      */
    virtual void init_process();

    virtual int b64_enc(char *output, int outputLen, const char *input, int inputLen) { return 0; }
    virtual int b64_dec(char * output, char * input, int inputLen) { return 0; }

    virtual void sha1_apply(const unsigned char* src, unsigned char* digest) {}

protected:
    // access to frontend
    UR_Crypton &_ur_crypton;
};
