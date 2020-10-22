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
#endif // SHAL_DEBUG
    sig_evt = 1;
    SHAL_SYSTEM::system_shutdown();
}

namespace SHAL_SYSTEM {

    MemberProc _timer_proc[MAX_TIMER_PROCS];
    uint8_t _num_timer_procs;
    bool _in_timer_proc;
    volatile bool _timer_suspended;
    volatile bool _timer_event_missed;
    bool _isr_timer_running;

    pthread_t run_proc_thread;
    pthread_attr_t thread_attr_run_proc;

    pthread_t isr_timer_thread;
    pthread_attr_t thread_attr_timer;

    static struct {
        struct timeval start_time;
    } state_tv;
} // namespace SHAL_SYSTEM

#ifndef ENABLE_SYSTEM_SHUTDOWN
void SHAL_SYSTEM::system_shutdown()
{
    sig_evt = 1;
}
#endif // ENABLE_SYSTEM_SHUTDOWN

void SHAL_SYSTEM::init()
{
	struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = signal_handler;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGHUP, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);

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
    return get_core_hrdtime();
}

uint64_t SHAL_SYSTEM::micros64()
{
    return micros32();
}

uint32_t SHAL_SYSTEM::millis32()
{
    return round((float)get_core_hrdtime() / 1000.0f);
}

uint64_t SHAL_SYSTEM::millis64()
{
    return millis32();
}

void SHAL_SYSTEM::panic(const char *errormsg, ...)
{
    va_list ap;

    fflush(stdout);
    va_start(ap, errormsg);
    vprintf(errormsg, ap);
    va_end(ap);
    printf("\n");

    for(;;);
}

void *SHAL_SYSTEM::_fire_isr_timer(void *arg)
{

    if (_isr_timer_running) {
        return NULL;
    }

    _isr_timer_running = true;

    while(_isr_timer_running) {
        sleep(1);
        _timer_event();
    }

    _isr_timer_running = false;
#if SHAL_DEBUG >= 1
    printf("isr timer stoped!\n");
#endif // SHAL_DEBUG
    fflush(stdout);

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

void *SHAL_SYSTEM::_fire_thread_proc(void *args)
{
    UR_V16X &v16x = *(UR_V16X*)args;
    v16x.fire_process();

    pthread_detach(pthread_self());
    return NULL;
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

void SHAL_SYSTEM::run_thread_process(UR_V16X &proc)
{
    pthread_create(&run_proc_thread, &thread_attr_run_proc, _fire_thread_proc, (void*)&proc);
}

void SHAL_SYSTEM::run_thread_process(Proc proc)
{
    pthread_create(&run_proc_thread, &thread_attr_run_proc, _fire_thread_void, (void*)proc);
}

void SHAL_SYSTEM::run_thread_process(MemberProc proc)
{
    MemberProc *proctmp = new MemberProc;
    *proctmp = proc;
    pthread_create(&run_proc_thread, &thread_attr_run_proc, _fire_thread_member, (void*)proctmp);
}

void SHAL_SYSTEM::delay_ms(uint16_t ms)
{
    usleep(ms);
}

void SHAL_SYSTEM::delay_sec(uint16_t sec)
{
    sleep(sec);
}

void SHAL_SYSTEM::printf(const char *printmsg, ...)
{
    va_list vl;

    fflush(stdout);
    va_start(vl, printmsg);
    vprintf(printmsg, vl);
    va_end(vl);
}

#ifndef __WXGTK__
SHAL_SYSTEM_MAIN()
#else
SHAL_SYSTEM_WX_MAIN()
#endif
