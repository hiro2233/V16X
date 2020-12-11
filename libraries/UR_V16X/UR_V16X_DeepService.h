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

    UR_V16X_DeepService();
    virtual ~UR_V16X_DeepService(void) {};

    void print_query_params(const query_param_t *qparam, uint32_t cnt);
    uint32_t parse_query(const char *query, char delimiter, char setter, query_param_t *params, uint32_t max_params);
    bool has_key(const query_param_t *params, uint32_t offset, uint32_t cnt, const char *strkey, uint32_t &idx);
    uint32_t destroy_qparams(query_param_t *params, uint32_t cnt);

private:

};
