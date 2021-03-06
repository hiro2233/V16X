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

typedef struct __query_param_t {
    char *key;
    char *val;
} query_param_t;

class UR_V16X_DeepService
{
public:

    typedef struct __cmd_lst_t {
        const char *cmd;
    } cmd_lst_t;

    UR_V16X_DeepService();
    virtual ~UR_V16X_DeepService(void) {};

    // Print query params keys and values.
    void print_query_params(const query_param_t *qparam, uint32_t cnt);
    // Parse query, extract and sets the keys and values into the params giving the delimiter and the setter tokens.
    // Return param's pairs count extracted."
    uint32_t parse_query(const char *query, char delimiter, char setter, query_param_t *params, uint32_t max_params);
    // Check if params has keys.
    bool has_key(const query_param_t *params, uint32_t offset, uint32_t cnt, const char *strkey, uint32_t &idx);
    // Ensure destroying keys and values pairs.
    uint32_t destroy_qparams(query_param_t *params, uint32_t cnt);
    // Process query params already parsed.
    bool process_qparams(const query_param_t *qparams, uint32_t cnt, char **retmsg);

private:

    static const cmd_lst_t cmd_lst[];
    // Execute query params string type already parsed.
    bool execute_qstr(const query_param_t *qparams, uint32_t cnt, char **retmsg);
    // Verify if command exist on the list.
    bool cmd_avail(char *pcmd);
    bool exe_cmd(const char *cmd, char **retmsg);
};
