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

#include "UR_V16X_Driver.h"

#include <stdarg.h>
#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __MSYS__
#include <sys/sendfile.h>
#endif // __MSYS__
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <stdbool.h>
#include <atomic>
#include <math.h>

#include <sys/socket.h>


#define MAX_LISTEN  1024  /* max connections */
#define MAX_BUFF 2048

class UR_V16X_Posix : public UR_V16X_Driver
{
public:

    typedef struct {
        int io_fd;
        int io_cnt;
        char *io_bufptr;
        char io_buf[MAX_BUFF];
    } data_io_t;

    typedef struct {
        char filename[MAX_BUFF];
        off_t offset;
        size_t end;
        size_t allsize;
    } http_request_t;

    typedef struct __mime_map_t {
        const char *extension;
        const char *mime_type;
    } mime_map_t;

    typedef struct __data_parsed_t {
        char data[MAX_BUFF];
    } data_parsed_t;

    typedef struct __query_param_t {
        char *key;
        char *val;
    } query_param_t;

    typedef struct __netsocket_inf_t {
        struct sockaddr_in clientaddr;
        int connfd;
        int clid;
        bool is_attached;
        int sending;
        bool event_stream;
        bool evtstr_connected;
    } netsocket_inf_t;

    UR_V16X_Posix(UR_V16X &v16x);
    void update(void);
    void init(void);
    static UR_V16X_Driver *create_endpoint(UR_V16X &v16x);
    void fire_process();
    void shuttdown();
    int process(int fd, struct sockaddr_in *clientaddr);
    void process_event_stream();

private:
    uint8_t _endpoint;

    struct sockaddr_in clientaddr;

    int default_port = 9998;
    int listenfd;
    int connfd;

    pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t process_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t _proc_thread;
    pthread_attr_t _thread_attr_proc;

    netsocket_inf_t *clients[V16X_MAX_CLIENTS];

    std::atomic<unsigned int> cli_count;
    int clid = 10;

    static const mime_map_t mime_types[];
    const char *default_mime_type = {"application/octet-stream"};

    typedef struct sockaddr SA;
    data_parsed_t data_parsed;

    // open port to listen
    int open_listenfd(int port);

    // add client to slot.
    void client_slot_add(netsocket_inf_t *cl);
    void client_slot_delete(int clid);
    static bool poll_in(int fd, uint32_t timeout_ms);
    int parse_request(int fd, http_request_t *req);
    int parse_query(char *query, char delimiter, char setter, query_param_t *params, int max_params);
    void handle_message_outhttp(int fd, char *longmsg);
    void log_access(int status, struct sockaddr_in *c_addr, http_request_t *req);
    void client_error(int fd, int status, const char *msg, const char *longmsg);
    void serve_static(int out_fd, int in_fd, struct sockaddr_in *c_addr, http_request_t *req, size_t total_size);
    void handle_directory_request(int out_fd, int dir_fd, char *filename);
    ssize_t io_data_read(data_io_t *rp, char *usrbuf, size_t maxlen, int *closed);
    void io_data_init(data_io_t *rp, int fd);
    void url_decode(char* src, char* dest, int max);
    ssize_t writen(int fd, void *usrbuf, size_t n);
    const char* get_mime_type(char *filename);
    void format_size(char* buf, struct stat *stat, int *filecnt, int *dircnt);
    netsocket_inf_t *_get_next_unattached_client();
    void _set_client_event_stream(int fd, bool event_stream);
    bool _has_client_event_stream(int fd);
    netsocket_inf_t *_get_client(int fd);
};
