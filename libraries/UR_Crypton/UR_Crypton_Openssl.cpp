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

#include "UR_Crypton_Openssl.h"

UR_Crypton_Openssl::UR_Crypton_Openssl(UR_Crypton &ur_crypton) :
    UR_Crypton_Backend(ur_crypton)
{
}

UR_Crypton_Openssl::~UR_Crypton_Openssl()
{}

void UR_Crypton_Openssl::init_process()
{
}

UR_Crypton_Backend *UR_Crypton_Openssl::configure(UR_Crypton &ur_crypton)
{
    UR_Crypton_Openssl *crypton = new UR_Crypton_Openssl(ur_crypton);

    if (!crypton || !crypton->_configure()) {
        delete crypton;
        return nullptr;
    }

    return crypton;
}

bool UR_Crypton_Openssl::_configure()
{
    SHAL_SYSTEM::printf("Configure UR Crypton Openssl\n");
    return true;
}

int UR_Crypton_Openssl::b64_enc(char *output, int outputLen, const char *input, int inputLen)
{
    size_t len = 0;
    int total_len = 0;

    BIO *buff;
    BIO *b64f;
    BUF_MEM *ptr;

    b64f = BIO_new(BIO_f_base64());
    buff = BIO_new(BIO_s_mem());
    buff = BIO_push(b64f, buff);

    BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
    (void)BIO_set_close(buff, BIO_CLOSE);
    do {
        len = BIO_write(buff, input + total_len, inputLen - total_len);
        if (len > 0)
            total_len += len;
    } while (len && BIO_should_retry(buff));

    (void)BIO_flush(buff);

    BIO_get_mem_ptr(buff, &ptr);
    len = ptr->length;

    memcpy(output, ptr->data, outputLen < len ? outputLen : len);
    output[outputLen < len ? outputLen : len] = '\0';

    BIO_free_all(buff);

    if (outputLen < len)
        return -1;

    return len;
}

int UR_Crypton_Openssl::b64_dec(char * output, char * input, int inputLen)
{
    int len = 0;
    int total_len = 0;
    int pending = 0;

    BIO *buff;
    BIO *b64f;

    b64f = BIO_new(BIO_f_base64());
    buff = BIO_new_mem_buf(input, -1);
    buff = BIO_push(b64f, buff);

    BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
    (void)BIO_set_close(buff, BIO_CLOSE);
    do {
        len = BIO_read(buff, output + total_len, inputLen - total_len);
        if (len > 0)
            total_len += len;
    } while (len && BIO_should_retry(buff));

    output[total_len] = '\0';

    pending = BIO_ctrl_pending(buff);

    BIO_free_all(buff);

    if (pending)
        return -1;

    return len;
}

void UR_Crypton_Openssl::sha1_apply(const unsigned char* src, unsigned char* digest)
{
    SHA_CTX shactx;
    SHA1_Init(&shactx);
    SHA1_Update(&shactx, src, strlen((const char*)src));
    SHA1_Final(digest, &shactx);
}
