/*
   Synthetic Hardware Abstraction layer (SHAL) for URUS System.
   Copyright (c) 2016-2020 Hiroshi Takey F. <htakey@gmail.com>

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

/*
 *  Synthetic Hardware Abstraction layer (SHAL) for URUS System.
 *  Author: Hiroshi Takey, November 2016-2020
 *
 *  This was based from URUS SHAL system and adapted to work with URUS CORE SHAL.
 */

#pragma once

#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#if defined(__WIN32__)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include "signal_win.h"
#ifndef __GNUC__
#include "time_win.h"
#include "unistd_win.h"
#else
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#define sleep(x) Sleep(x * 1000)
#endif // __GNUC__

#ifdef DLL_EXPORTS
#define API_EXPORT __declspec(dllexport)
#else
#define API_EXPORT
#endif  // DLL_EXPORTS

#ifndef sleep
#define sleep(x) Sleep(x * 1000)
#endif

#else
#define API_EXPORT
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#endif

#include <math.h>

#define STRINGIZEDEF(x) #x
#define STRINGIZEDEF_VAL(x) STRINGIZEDEF(x)

#define COLOR_PRINTF_RESET "\e[0m"
#define COLOR_PRINTF_BLUE(bold) "\e["#bold";34m"
#define COLOR_PRINTF_RED(bold) "\e["#bold";31m"
#define COLOR_PRINTF_PURPLE(bold) "\e["#bold";35m"
#define COLOR_PRINTF_WHITE(bold) "\e["#bold";37m"

#define CRYPTON_NATIVE 1
#define CRYPTON_OPENSSL 2

#ifndef CRYPTON_TYPE
#define CRYPTON_TYPE CRYPTON_OPENSSL
#endif // CRYPTON_TYPE

#include <utility/functor.h>
#include <array>
#include <string>

typedef struct __system_argcv_s {
    int argc;
    char** argv;
} system_argcv_t;

typedef std::string string;

#define NORETURN __attribute__ ((noreturn))
#define FMT_PRINTF(a,b) __attribute__((format(printf, a, b)))

#ifdef __GNUC__
 #define WARN_IF_UNUSED __attribute__ ((warn_unused_result))

 #define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#else
 #define WARN_IF_UNUSED
#endif

#if !(defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L)
# define constexpr const
#endif

#ifndef SHAL_MAIN
#define SHAL_MAIN main
#endif

#define SHAL_SYSTEM_MAIN() \
    void configure(); \
    void loop(); \
    volatile sig_atomic_t sig_evt = 0; \
    system_argcv_t SHAL_SYSTEM::system_argcv; \
    extern "C" {                               \
    int SHAL_MAIN(int argc, char* const argv[]); \
    int SHAL_MAIN(int argc, char* const argv[]) { \
        SHAL_SYSTEM::system_argcv.argc = argc; \
        SHAL_SYSTEM::system_argcv.argv = (char**)argv; \
        configure(); \
        while (!sig_evt) { \
            loop(); \
        } \
        return 0; \
    } \
    }

#define SHAL_SYSTEM_LIB() \
    volatile sig_atomic_t sig_evt = 0; \

#define MAX_TIMER_PROCS 10
#define SERVER_VERSION "V16X/1.0.0"
#define SHAL_DEBUG 0

namespace SHAL_SYSTEM {

    typedef void(*Proc)(void);
    FUNCTOR_TYPEDEF(MemberProc, void);

    void init();
    void panic(const char *errormsg, ...) FMT_PRINTF(1, 2) NORETURN;

    void register_timer_process(MemberProc proc);
    void _run_timer_procs(bool called_from_isr);
    void _timer_event();
    void suspend_timer_procs();
    void resume_timer_procs();

    void run_thread_process(Proc proc);
    void run_thread_process(MemberProc proc);

    void *_fire_thread_proc(void *args);
    void *_fire_isr_timer(void *arg);
    void *_fire_thread_void(void *args);
    void *_fire_thread_member(void *args);
    void system_shutdown();
    void system_shal_shutdown();
    void delay_sec(uint16_t sec);
    void delay_ms(uint32_t ms);
    void printf(const char *errormsg, ...) FMT_PRINTF(1, 2);
    const char *get_date();

    uint64_t _micros64tv ();
    uint64_t get_core_hrdtime();
    uint32_t micros32();
    uint64_t micros64();
    uint32_t millis32();
    uint64_t millis64();

    API_EXPORT
    string* get_vstdout_buf();

    API_EXPORT
    void set_vstdout_console_clbk(MemberProc proc);

    extern volatile bool _isr_timer_running;
    extern system_argcv_t system_argcv;
}; // namespace SHAL_SYSTEM

extern volatile sig_atomic_t sig_evt;
