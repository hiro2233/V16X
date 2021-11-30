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
#include "UR_V16X_DeepService.h"
#include <UR_Crypton/UR_Crypton.h>

#include <stdarg.h>
#if !defined(__MINGW32__)
#include <arpa/inet.h>
#include <dirent.h>
#endif
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#if !defined(__MINGW32__)
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__MSYS__) && !defined(__MINGW32__)
#include <sys/sendfile.h>
#endif // __MSYS__
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#if !defined(__MINGW32__)
#include <poll.h>
#endif
#include <stdbool.h>
#include <atomic>
#include <math.h>
#if !defined(__MINGW32__)
#include <netdb.h>
#endif

#if defined(__MINGW32__)
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <wininet.h>
#include "system/missing/netsocket_win.h"
#include "dirent_win.h"
#else
#include <sys/socket.h>
#endif

#define MAX_LISTEN  1000  /* max connections */
#define MAX_BUFF 2048
#define PROCESS_EVENT_INTERVAL 2000 // Time in ms
#define UPDATE_POLLIN_INTERVAL 40 // Time in ms
#define FIREPROC_POLLIN_INTERVAL 20 // Time in ms
#define OPCODE_TEXT    0x01
#define OPCODE_BINARY  0x02
#define TIMEOUT_FIREPROC   500 // Time in ms

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

    typedef struct __netsocket_inf_t {
        struct sockaddr_in clientaddr;
        int connfd;
        uint32_t clid;
        bool is_attached;
        int sending;
        bool event_stream;
        bool evtstr_connected;
        bool evtstr_ping;
        uint32_t alive_cnt;
        bool event_websocket;
        bool evtwebsock_ping;
        bool method_get;
        bool keep_alive;
        bool method_head;
    } netsocket_inf_t;

    UR_V16X_Posix(UR_V16X &v16x);
    void update(void) override;
    void init(void);
    static UR_V16X_Driver *create_endpoint(UR_V16X &v16x);
    void fire_process() override;
    void shuttdown() override;
    int process(int fd, struct sockaddr_in *clientaddr);
    void process_event_stream();

private:
    static UR_Crypton ur_crypton;

    uint8_t _endpoint;
    int default_port = 9998;
    char default_addr[16];
    int listenfd;

    pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t process_mutex = PTHREAD_MUTEX_INITIALIZER;

    netsocket_inf_t *clients[V16X_MAX_CLIENTS];

    //std::atomic<uint32_t> cli_count;
    static volatile uint32_t cli_count;
    uint32_t clid = 10;

    static const mime_map_t mime_types[];
    const char *default_mime_type = {"text/plain"};

    typedef struct sockaddr SA;
    data_parsed_t data_parsed;

    // open port to listen
    int open_listenfd(int port);

    // add client to slot.
    void client_slot_add(netsocket_inf_t *cl);
    void client_slot_delete(uint32_t clid_slot);
    bool poll_in(int fd, uint32_t timeout_ms);
    uint8_t parse_request(int fd, http_request_t *req);
    void handle_message_outhttp(int fd, const char *longmsg);
    void log_access(int status, struct sockaddr_in *c_addr, http_request_t *req);
    void client_error(int fd, int status, const char *msg, const char *longmsg);
    void serve_static(int out_fd, int in_fd, struct sockaddr_in *c_addr, http_request_t *req, size_t total_size);
    void handle_directory_request(int out_fd, int dir_fd, char filename[]);
    ssize_t io_data_read(data_io_t *rp, char *usrbuf, size_t maxlen, int *closed);
    void io_data_init(data_io_t *rp, int fd);
    void url_decode(char* src, char* dest, int max);
    ssize_t writen(int fd, const void *usrbuf, size_t n);
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
    int _decode_hybi(char *src, size_t srclength, char *target, int targsize, unsigned int *opcode, unsigned int *left);
    int _encode_hybi(char const *src, size_t srclength, char *target, size_t targsize, unsigned int opcode);
    int _get_ip_host(char *host, int len);
};
