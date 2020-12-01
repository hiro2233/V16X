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

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h> /* base64 encode/decode */
#include <openssl/md5.h> /* md5 hash */
#include <openssl/sha.h> /* sha1 hash */

#define MAX_LISTEN  1000  /* max connections */
#define MAX_BUFF 2048
#define PROCESS_EVENT_INTERVAL 2000 // Time in ms
#define UPDATE_POLLIN_INTERVAL 40 // Time in ms
#define FIREPROC_POLLIN_INTERVAL 20 // Time in ms
#define OPCODE_TEXT    0x01
#define OPCODE_BINARY  0x02
#define TIMEOUT_FIREPROC   300 // Time in ms

class UR_V16X_Posix : public UR_V16X_Driver
{
public:

    typedef struct __events_transaction_t {
        int fd;
        bool event_stream;
        bool event_websocket;
        bool method_get;
    } events_transaction_t;

    enum TYPE_TRANSACTION_E {
        SET_EVENT_STREAM,
        GET_EVENT_STREAM,
        SET_CLIENT_METHOD_GET,
        GET_CLIENT_METHOD_GET,
        SET_EVENT_WEBSOCKET,
        GET_EVENT_WEBSOCKET
    };

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
        bool evtstr_ping;
        uint32_t alive_cnt;
        bool event_websocket;
        bool evtwebsock_ping;
        bool method_get;
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
    int default_port = 9998;
    int listenfd;

    pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t process_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    bool poll_in(int fd, uint32_t timeout_ms);
    int parse_request(int fd, http_request_t *req);
    int parse_query(char *query, char delimiter, char setter, query_param_t *params, int max_params);
    void handle_message_outhttp(int fd, char *longmsg);
    void log_access(int status, struct sockaddr_in *c_addr, http_request_t *req);
    void client_error(int fd, int status, const char *msg, const char *longmsg);
    void serve_static(int out_fd, int in_fd, struct sockaddr_in *c_addr, http_request_t *req, size_t total_size);
    void handle_directory_request(int out_fd, int dir_fd, char filename[]);
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
    void _client_transaction(TYPE_TRANSACTION_E typetr, events_transaction_t &data_transaction);
    void _set_client_event_websocket(int fd, bool event_websocket);
    bool _has_client_event_websocket(int fd);
    bool _has_client_events(int fd);
    bool _has_client_same_ip(sockaddr_in cliaddr);
    void _set_client_method_get(int fd, bool method);
    bool _has_client_method_get(int fd);
    bool _has_ip_method_get(sockaddr_in cliaddr);
    void _get_client(TYPE_TRANSACTION_E typetr, netsocket_inf_t &client);
};
