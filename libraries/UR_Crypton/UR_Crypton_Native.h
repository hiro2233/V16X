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

class UR_Crypton_Native : public UR_Crypton_Backend
{
public:
    ~UR_Crypton_Native();

    /** see process function on backend class
      */
    void init_process() override;

    /** see configure function on top class
      */
    static UR_Crypton_Backend *configure(UR_Crypton &ur_crypton);

    int b64_enc(char *output, int outputLen, const char *input, int inputLen) override;
    int b64_dec(char * output, char * input, int inputLen) override;
    void sha1_apply(const unsigned char* src, unsigned char* digest) override;
    void sha1_apply_file(const char* filepath, unsigned char* digest) override;

private:
    typedef struct __SHA_CTX_t {
        uint32_t state[5];
        uint32_t count[2];
        unsigned char buffer[64];
    } SHA_CTX;

    static const char _b64_alphabet[];

    UR_Crypton_Native(UR_Crypton &ur_crypton);

    bool _configure();

    // Base64 functions.
    inline void _a3_to_a4(unsigned char * a4, unsigned char * a3);
    inline void _a4_to_a3(unsigned char * a3, unsigned char * a4);
    inline unsigned char _b64_lookup(char c);
    int _b64_enc_len(int plainLen);
    int _b64_dec_len(char * input, int inputLen);

    // SHA1 functions.
    void _SHA1_Transform(uint32_t state[5], const unsigned char buffer[64]);
    void _SHA1_Init(SHA_CTX* context);
    void _SHA1_Update(SHA_CTX* context, const unsigned char* data, uint32_t len);
    void _SHA1_Final(unsigned char digest[20], SHA_CTX* context);
};
