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

#include "UR_Crypton_Backend.h"

UR_Crypton_Backend::UR_Crypton_Backend(UR_Crypton &ur_crypton) :
    _ur_crypton(ur_crypton)
{}

void UR_Crypton_Backend::init_process()
{}
