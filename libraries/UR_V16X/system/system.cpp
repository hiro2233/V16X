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

#include "system.h"

#include <signal.h>
#include <stdarg.h>
#include <memory>
#include <thread>
#include <atomic>

#if defined(__WIN32__)
void usleep(long usec)
{
/*
    struct timeval tv;
    fd_set dummy;
    SOCKET s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    FD_ZERO(&dummy);
    FD_SET(s, &dummy);
    tv.tv_sec = usec/1000000L;
    tv.tv_usec = usec%1000000L;
    select(0, 0, 0, &dummy, &tv);
*/

    LARGE_INTEGER perfCnt_time, start_time, now_time;

    QueryPerformanceFrequency(&perfCnt_time);
    QueryPerformanceCounter(&start_time);

    do {
        QueryPerformanceCounter((LARGE_INTEGER*) &now_time);
    } while ((now_time.QuadPart - start_time.QuadPart) / float(perfCnt_time.QuadPart) * 1000 * 1000 < usec);

}
#endif // defined(__WIN32__)

void signal_handler(int sig)
{
    char msg[10];
    switch(sig) {
        case SIGHUP:
            sprintf(msg, "%s", "SIGHUP");
            break;
        case SIGTERM:
            sprintf(msg, "%s", "SIGTERM");
            break;
        case SIGSEGV:
            sprintf(msg, "%s", "SIGSEGV");
            break;
        case SIGINT:
            sprintf(msg, "%s", "SIGINT");
            break;
    }

#if SHAL_DEBUG >= 1
    printf("Signal %s received, Shutting down V16X system.\n", msg);
    fflush(stdout);
#endif
    sig_evt = 1;
    SHAL_SYSTEM::system_shutdown();
}

namespace SHAL_SYSTEM {

    static MemberProc _timer_proc[MAX_TIMER_PROCS] = {NULL};
    uint8_t _num_timer_procs;
    volatile bool _in_timer_proc;
    volatile bool _timer_suspended;
    volatile bool _timer_event_missed;

    pthread_t run_proc_thread;
    pthread_attr_t thread_attr_run_proc;

    pthread_t isr_timer_thread;
    pthread_attr_t thread_attr_timer;

    pthread_mutex_t getvstdout_mutex;

    static struct {
        struct timeval start_time;
    } state_tv;

    static MemberProc vstdout_proc = NULL;
    string vstdout_buf[20];
    string strdatres[20];
    volatile uint16_t vstdout_buf_index = 0;
}

volatile bool SHAL_SYSTEM::_isr_timer_running = false;

void SHAL_SYSTEM::system_shal_shutdown()
{
    pthread_mutex_unlock(&getvstdout_mutex);
    sig_evt = 1;
    _isr_timer_running = false;
    pthread_join(run_proc_thread, NULL);
    pthread_join(isr_timer_thread, NULL);
    delay_sec(1);
    SHAL_SYSTEM::printf("Shutdown system OK\n");
}

#ifndef ENABLE_SYSTEM_SHUTDOWN
void SHAL_SYSTEM::system_shutdown()
{
    system_shal_shutdown();
}
#endif // ENABLE_SYSTEM_SHUTDOWN

void SHAL_SYSTEM::init()
{
    if (_isr_timer_running) {
        printf("system already running\n");
        return;
    }

    _in_timer_proc = false;
    _timer_suspended = false;
    _timer_event_missed = false;
    _num_timer_procs = 0;

    pthread_mutex_init(&getvstdout_mutex, NULL);

    for (uint16_t i = 0; i < 20; i++) {
        vstdout_buf[i] = "";
    }

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
#ifndef __WIN32__
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
#endif
    signal(SIGPIPE, SIG_IGN);

    pthread_attr_init(&thread_attr_timer);
    pthread_attr_setstacksize(&thread_attr_timer, 2048);
    pthread_attr_setschedpolicy(&thread_attr_timer, SCHED_FIFO);
    pthread_create(&isr_timer_thread, &thread_attr_timer, _fire_isr_timer, nullptr);

    pthread_attr_init(&thread_attr_run_proc);
    pthread_attr_setstacksize(&thread_attr_run_proc, 2048);
    pthread_attr_setschedpolicy(&thread_attr_run_proc, SCHED_FIFO);

    gettimeofday(&state_tv.start_time, nullptr);

    printf("System init\n");
}

uint64_t SHAL_SYSTEM::_micros64tv()
{
    struct timeval tp;
    gettimeofday(&tp, nullptr);
    uint64_t ret = 1.0e6 * ((tp.tv_sec + (tp.tv_usec * 1.0e-6)) -
                            (state_tv.start_time.tv_sec +
                             (state_tv.start_time.tv_usec * 1.0e-6)));
    return ret;
}

uint64_t SHAL_SYSTEM::get_core_hrdtime()
{
    return _micros64tv();
}

uint32_t SHAL_SYSTEM::micros32()
{
    return (uint32_t)get_core_hrdtime();
}

uint64_t SHAL_SYSTEM::micros64()
{
    return micros32();
}

uint32_t SHAL_SYSTEM::millis32()
{
    return (uint32_t)round((float)(get_core_hrdtime() / 1000.0f));
}

uint64_t SHAL_SYSTEM::millis64()
{
    return millis32();
}

void SHAL_SYSTEM::panic(const char *errormsg, ...)
{
    va_list ap;

    va_start(ap, errormsg);
    vprintf(errormsg, ap);
    va_end(ap);
#ifndef SHAL_LIB
    fprintf(stdout, "\n");
#endif // SHAL_LIB

    for(;;) {
#ifdef SHAL_LIB
        if (sig_evt) {
            break;
        }
#endif // SHAL_LIB
#ifndef SHAL_LIB
        fflush(stdout);
#endif // SHAL_LIB
        SHAL_SYSTEM::delay_sec(1);
        while (pthread_mutex_trylock(&getvstdout_mutex) == 0) {
            SHAL_SYSTEM::delay_ms(1);
        }
#ifndef SHAL_LIB
        fprintf(stdout, "panic!\n");
#endif // SHAL_LIB
        if (vstdout_buf_index < 10) {
            char ctmp[256] = {0};

            va_start(ap, errormsg);
            vsprintf(ctmp, errormsg, ap);
            va_end(ap);

            vstdout_buf[vstdout_buf_index] = ctmp;
            vstdout_buf_index++;

            if (vstdout_proc) {
                vstdout_proc();
            }
        }
        pthread_mutex_unlock(&getvstdout_mutex);
    }

}

void *SHAL_SYSTEM::_fire_isr_timer(void *arg)
{
    if (_isr_timer_running) {
        return NULL;
    }

    _isr_timer_running = true;

    uint32_t last = millis32();
    while(_isr_timer_running) {
        uint32_t now = millis32();
        if ((now - last) >= 20) {
            last = millis32();
            _timer_event();
        }
#if defined(__WIN32__)
        Sleep(2);
#else
        delay_ms(2);
#endif
    }

    _isr_timer_running = false;
#if SHAL_DEBUG >= 1
    printf("isr timer stoped!\n");
#endif // SHAL_DEBUG
#ifndef SHAL_LIB
    fflush(stdout);
#endif // SHAL_LIB

    return NULL;
}

void SHAL_SYSTEM::register_timer_process(MemberProc proc)
{
    for (uint8_t i = 0; i < _num_timer_procs; i++) {
        if (_timer_proc[i] == proc) {
            return;
        }
    }

    if (_num_timer_procs < MAX_TIMER_PROCS) {
        _timer_proc[_num_timer_procs] = proc;
        _num_timer_procs++;
    }
}

void SHAL_SYSTEM::_run_timer_procs(bool called_from_isr)
{
    if (_in_timer_proc) {
        return;
    }

    _in_timer_proc = true;

    if (!_timer_suspended) {
        // now call the timer based drivers
        for (int i = 0; i < _num_timer_procs; i++) {
            if (_timer_proc[i]) {
                _timer_proc[i]();
            }
        }
    } else if (called_from_isr) {
        _timer_event_missed = true;
    }

    _in_timer_proc = false;
}

void SHAL_SYSTEM::_timer_event() {
    _run_timer_procs(true);
}

void SHAL_SYSTEM::suspend_timer_procs() {
    _timer_suspended = true;
}

void SHAL_SYSTEM::resume_timer_procs() {
    _timer_suspended = false;
    if (_timer_event_missed) {
        _timer_event_missed = false;
        _run_timer_procs(false);
    }
}

void *SHAL_SYSTEM::_fire_thread_void(void *args)
{
#if SHAL_DEBUG >= 1
    printf("_fire_thread_void\n");
    fflush(stdout);
#endif // SHAL_DEBUG

    Proc proc = (Proc)args;
    proc();

    pthread_detach(pthread_self());
    return NULL;
}

void *SHAL_SYSTEM::_fire_thread_member(void *args)
{
#if SHAL_DEBUG >= 1
    printf("_fire_thread_member\n");
    fflush(stdout);
#endif // SHAL_DEBUG

    MemberProc proc = *(MemberProc*)args;
    proc();
#if SHAL_DEBUG >= 1
    printf("member executed!\n");
    fflush(stdout);
#endif // SHAL_DEBUG

    pthread_detach(pthread_self());
    return NULL;
}

void SHAL_SYSTEM::run_thread_process(Proc proc)
{
    pthread_create(&run_proc_thread, &thread_attr_run_proc, _fire_thread_void, (void*)proc);
}

void SHAL_SYSTEM::run_thread_process(MemberProc proc)
{
    pthread_create(&run_proc_thread, &thread_attr_run_proc, _fire_thread_member, (void*)&proc);
}

void SHAL_SYSTEM::delay_ms(uint32_t ms)
{
    uint32_t start = millis32();
    uint32_t now_micros = micros32();
    uint32_t dt_micros = 0;
    uint32_t centinel_micros = 1000;

    while ((millis32() - start) < ms) {
        dt_micros = micros32() - now_micros;
        now_micros = micros32();
        usleep(centinel_micros);

        if (dt_micros > centinel_micros) {
            centinel_micros = centinel_micros + (dt_micros - centinel_micros);
        }

        if (dt_micros < centinel_micros) {
            centinel_micros = centinel_micros - dt_micros;
        }
    }
}

void SHAL_SYSTEM::delay_sec(uint16_t sec)
{
    delay_ms(sec * 1000);
}

void SHAL_SYSTEM::printf(const char *printmsg, ...)
{
    va_list vl;

    va_start(vl, printmsg);
    vprintf(printmsg, vl);
    va_end(vl);
#ifndef SHAL_LIB
    fflush(stdout);
#endif // SHAL_LIB

    if (!vstdout_proc) {
        return;
    }

    while (pthread_mutex_trylock(&getvstdout_mutex) == 0) {
        SHAL_SYSTEM::delay_ms(1);
    }

    if (vstdout_buf_index < 10) {
        char ctmp[512] = {0};

        va_start(vl, printmsg);
        vsprintf(ctmp, printmsg, vl);
        va_end(vl);

        vstdout_buf[vstdout_buf_index] = ctmp;
        vstdout_buf_index++;
        if (vstdout_proc) {
            vstdout_proc();
        }
    }

    pthread_mutex_unlock(&getvstdout_mutex);

}

const string SHAL_SYSTEM::get_date()
{
    char buf[128] = {0};
    memset(buf, 0, sizeof(buf));
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
    return buf;
}

string *SHAL_SYSTEM::get_vstdout_buf()
{
    while (pthread_mutex_trylock(&getvstdout_mutex) == 0) {
        SHAL_SYSTEM::delay_ms(1);
    }
    SHAL_SYSTEM::suspend_timer_procs();

    for (uint16_t i = 0; i < 20; i++) {
        strdatres[i] = "";
    }

    if (vstdout_buf_index > 0) {
        for (uint16_t i = 0; i < vstdout_buf_index; i++) {
            strdatres[i] = vstdout_buf[i];
            vstdout_buf[i] = "";
        }
        vstdout_buf_index = 0;
    }

    SHAL_SYSTEM::resume_timer_procs();
    pthread_mutex_unlock(&getvstdout_mutex);

    return strdatres;
}

void SHAL_SYSTEM::set_vstdout_console_clbk(MemberProc proc)
{
    if (vstdout_proc == proc) {
        return;
    }

    if (!vstdout_proc) {
        vstdout_proc = proc;
    }
}

#ifndef SHAL_LIB
SHAL_SYSTEM_MAIN()
#else
SHAL_SYSTEM_LIB()
#endif // SHAL_LIB
