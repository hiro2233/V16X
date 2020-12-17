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
#include "UR_Crypton_Backend.h"

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <openssl/bio.h> /* base64 encode/decode */
#include <openssl/md5.h> /* md5 hash */
#include <openssl/sha.h> /* sha1 hash */

class UR_Crypton_Openssl : public UR_Crypton_Backend
{
public:
    ~UR_Crypton_Openssl();

    /** see process function on backend class
      */
    void init_process() override;

    /** see configure function on top class
      */
    static UR_Crypton_Backend *configure(UR_Crypton &ur_crypton);

    int b64_enc(char *output, int outputLen, const char *input, int inputLen) override;
    int b64_dec(char * output, char * input, int inputLen) override;
    void sha1_apply(const unsigned char* src, unsigned char* digest) override;

private:

    UR_Crypton_Openssl(UR_Crypton &ur_crypton);

    bool _configure();
};
