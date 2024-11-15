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

#define UR_CRYPTON_MAX_BACKENDS 2

class UR_Crypton_Backend;

class UR_Crypton {

    friend class UR_Crypton_Backend;

public:

    enum ProcessMode {
        AutoProcess = 0,
        LoopProcess
    };

    UR_Crypton();
    ~UR_Crypton();

    /** Wrap only, make some people happy, this will be
      * removed in the future.
      * By default init() call to configure() in auto process
      * @param None.
      * @return None.
      */
    void init() {
        configure();
    }

    /** Configure the backend with their implementations.
      * By default configure() set "auto process" mode on
      * process() backend function.
      * @param  process_mode:
      *         [AutoProcess] - Update process run in the scheduled
      *         callback. This is the default mode.
      *         [LoopProcess] - Update process run not in scheduled
      *         callback. udpate() need to be called in somewhere
      *         to see the action, otherwise nothing happen.
      * @return None.
      */
    void configure(ProcessMode process_mode = ProcessMode::AutoProcess);
    int b64_enc(char *output, int outputLen, const char *input, int inputLen);
    int b64_dec(char * output, char * input, int inputLen);

    void sha1_apply(const unsigned char* src, unsigned char* digest);
    void sha1_apply_file(const char* filepath, unsigned char* digest);

private:

    UR_Crypton_Backend *_backends[UR_CRYPTON_MAX_BACKENDS] = {NULL};
    uint8_t _backend_count;
    bool _backends_configuring;

    // load backend drivers
    bool _add_backend(UR_Crypton_Backend *backend);
    void _configure_backends();
    void _configure_backends(ProcessMode process_mode);
};
