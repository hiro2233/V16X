#pragma once

#include <emscripten/emscripten.h>
#define EXPORTFUNC EMSCRIPTEN_KEEPALIVE
#define __EMS__ __EMSCRIPTEN__

typedef struct sockaddr SA;

#pragma pack(push, 1)
struct test_s {
    uint8_t data1 = 'N';
    uint16_t data2 = 0;
    uint8_t data3 = 'N';
};
#pragma pack(pop)

template <typename T, size_t N>
char (&_ARRAY_SIZE_H(T (&_arr)[N]))[N];

template <typename T>
char (&_ARRAY_SIZE_H(T (&_arr)[0]))[0];

#define ARRAY_SIZE_P(_arr) sizeof(_ARRAY_SIZE_H(_arr))
