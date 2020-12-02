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

#include "UR_V16X_Posix.h"

#include <vector>
#include <string>

#ifndef SOL_TCP
    #define SOL_TCP 6  // socket options TCP level
#endif
#ifndef TCP_USER_TIMEOUT
    #define TCP_USER_TIMEOUT 18  // how long for loss retry before timeout [ms]
#endif
#ifndef TCP_KEEPCNT
    #define TCP_KEEPCNT 8
#endif

#ifndef TCP_KEEPINTVL
    #define TCP_KEEPINTVL 150
#endif

#ifndef TCP_KEEPIDLE
    #define TCP_KEEPIDLE 14400
#endif

#define MAGIC_WEBSOCKET_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

const UR_V16X_Posix::mime_map_t UR_V16X_Posix::mime_types[] = {
    {".css", "text/css;charset=UTF-8"},
    {".gif", "image/gif"},
    {".htm", "text/html;charset=UTF-8"},
    {".html", "text/html;charset=UTF-8"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".ico", "image/x-icon"},
    {".js", "application/javascript;charset=UTF-8"},
    {".pdf", "application/pdf"},
    {".mp4", "video/mp4"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".xml", "text/xml;charset=UTF-8"},
    {".cgi", "application/x-httpd-cgi"},
    {".json", "application/json;charset=UTF-8"},
    {".txt", "text/plain;charset=UTF-8"},
    {".mp3", "audio/mpeg"},
    {".wasm", "application/wasm"},
    {".m3u", "audio/x-mpequrl"},
    {NULL, NULL},
};

UR_V16X_Posix::UR_V16X_Posix(UR_V16X &v16x) :
    UR_V16X_Driver(v16x)
{
    _endpoint = _frontend.register_endpoint();
    init();
}

void UR_V16X_Posix::init(void)
{
    SHAL_SYSTEM::printf("Init V16X endpoint: %d\n", _endpoint);
    fflush(stdout);

    cli_count = 0;
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        clients[i] = NULL;
    }

    listenfd = open_listenfd(default_port);
    if (listenfd > 0) {
        SHAL_SYSTEM::printf("Listen on port %d, fd is %d\n\n\n", default_port, listenfd);
        fflush(stdout);
    } else {
        perror("ERROR");
        exit(listenfd);
    }

    SHAL_SYSTEM::register_timer_process(FUNCTOR_BIND_MEMBER(&UR_V16X_Posix::process_event_stream, void));
}

void UR_V16X_Posix::update(void)
{
    if (poll_in(listenfd, UPDATE_POLLIN_INTERVAL)) {
        if (sig_evt) {
            return;
        }

        struct sockaddr_in clientaddr;
        socklen_t clientlen = sizeof(clientaddr);

        int connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);

        SHAL_SYSTEM::printf("Waiting connection for client #%d\n\n", (unsigned int)cli_count + 1);
        //fflush(stdout);

        int one = 1;
        int alive_one = 1;

        struct timeval timeout;
        int timeout_tcp = 9000;
        int retries_tcp = 3;
        int interval_tcp = 2;
        int delay_idle_tcp = 5;
        timeout.tv_sec = 9;
        timeout.tv_usec = 0;

        if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            //SHAL_SYSTEM::panic("setsockopt SO_RCVTIMEO failed\n");
        }

        if (setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            //SHAL_SYSTEM::panic("setsockopt SO_SNDTIMEO failed\n");
        }

        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (const void*)&one, sizeof(one));
        setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&one, sizeof(one));

        setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, (const void*)&alive_one, sizeof(alive_one)); // keep alive?

#ifndef _WIN32
        setsockopt(connfd, IPPROTO_TCP, TCP_KEEPIDLE, (const void*)&delay_idle_tcp, sizeof(delay_idle_tcp)); // delay idle
#endif // _WIN32
        setsockopt(connfd, SOL_TCP, TCP_KEEPCNT, (const void*)&retries_tcp, sizeof(retries_tcp)); // retries
        setsockopt(connfd, IPPROTO_TCP, TCP_KEEPINTVL, (const void*)&interval_tcp, sizeof(interval_tcp)); // interval
        setsockopt(connfd, SOL_TCP, TCP_USER_TIMEOUT, (const void*)&timeout_tcp, sizeof(timeout_tcp));

        // Here we catch flooding connections and will wait until
        // timeout occurs on the fire process thread side.
        // Be careful when changing clients numbers connections and times,
        // wrong values could cause overflow, this is relative between to the
        // update and clients process connections timeout.
        while (cli_count + 50 >= V16X_MAX_CLIENTS) {
            if (_has_client_same_ip(clientaddr)) {
                close(connfd);
                return;
            }
            SHAL_SYSTEM::delay_ms(1);
        }

        // Check if max clients is reached. This need to be lower and relative to
        // the above clients numbers connections when catch flooding.
        if ((cli_count + 25) == V16X_MAX_CLIENTS) {
            SHAL_SYSTEM::printf("max clients reached, reject\n");
            fflush(stdout);
            close(connfd);
            return;
        }

        netsocket_inf_t *netsocket_info = (netsocket_inf_t *)malloc(sizeof(netsocket_inf_t));
        memset(netsocket_info, 0, sizeof(netsocket_inf_t));

        netsocket_info->clientaddr = clientaddr;
        netsocket_info->connfd = connfd;
        netsocket_info->clid = clid++;
        netsocket_info->is_attached = false;
        netsocket_info->evtstr_connected = false;
        netsocket_info->event_stream = false;
        netsocket_info->evtstr_ping = false;
        netsocket_info->event_websocket = false;

        client_slot_add(netsocket_info);

        SHAL_SYSTEM::printf("Connected client ( CLID: %d ) fd: %d Address:  %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));
        fflush(stdout);
    }
}

UR_V16X_Driver *UR_V16X_Posix::create_endpoint(UR_V16X &v16x)
{
    UR_V16X_Posix *endpoint = nullptr;
    endpoint = new UR_V16X_Posix(v16x);
    return endpoint;
}

int UR_V16X_Posix::open_listenfd(int port)
{
    int optval = 1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    /* Reuse port address */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0) {
        return -1;
    }
/*
    if (setsockopt(listenfd, IPPROTO_TCP, TCP_CORK, (const void *)&optval , sizeof(int)) < 0) {
        return -1;
    }
*/
    if (setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (const void *)&optval , sizeof(int)) < 0) {
        return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    //inet_aton("127.0.2.1", &serveraddr.sin_addr.s_addr);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) {
        return -1;
    }

    if (listen(listenfd, MAX_LISTEN) < 0) {
        return -1;
    }

    return listenfd;
}

/* Add client to slot */
void UR_V16X_Posix::client_slot_add(netsocket_inf_t *cl)
{
    pthread_mutex_lock(&clients_mutex);
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] == NULL) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Delete client from slot */
void UR_V16X_Posix::client_slot_delete(int clid)
{
    pthread_mutex_lock(&clients_mutex);
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->clid == clid) {
                clients[i] = NULL;
                free(clients[i]);
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

UR_V16X_Posix::netsocket_inf_t *UR_V16X_Posix::_get_next_unattached_client()
{
    netsocket_inf_t *client = nullptr;

    pthread_mutex_lock(&clients_mutex);
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (!clients[i]->is_attached) {
                client = clients[i];
                client->is_attached = true;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return client;
}

void UR_V16X_Posix::shuttdown()
{
    SHAL_SYSTEM::printf("Shutting down V16X posix endpoint: %d\n", _endpoint);
    fflush(stdout);
    pthread_mutex_lock(&clients_mutex);
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->is_attached) {
                clients[i]->is_attached = false;
                close(clients[i]->connfd);
            }
        }
    }
    close(listenfd);
    pthread_mutex_unlock(&clients_mutex);
    sig_evt = 1;
    SHAL_SYSTEM::printf("Shutdown OK V16X posix endpoint: %d\n", _endpoint);
    fflush(stdout);
}

void UR_V16X_Posix::fire_process()
{
    pthread_mutex_lock(&process_mutex);
    netsocket_inf_t *netsocket_info = _get_next_unattached_client();
    if (netsocket_info == nullptr) {
        pthread_mutex_unlock(&process_mutex);
        return;
    }

    int rlen = 0;
    netsocket_info->sending = 0;

    cli_count++;
    _copy_client_to_frontend(_endpoint, netsocket_info->clid, netsocket_info->is_attached, (unsigned int)cli_count, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));

#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("FIRE_ISR: Start process ( CLID: %d )\n", netsocket_info->clid);
    fflush(stdout);
#endif

    pthread_mutex_unlock(&process_mutex);

    uint16_t pollcnt = 0;
    while(rlen == 0 && netsocket_info->is_attached && pollcnt < TIMEOUT_FIREPROC) {
        if (poll_in(netsocket_info->connfd, FIREPROC_POLLIN_INTERVAL)) {
            rlen = process(netsocket_info->connfd, &netsocket_info->clientaddr);    // Process all incomming data
            pollcnt = 0;
            //SHAL_SYSTEM::printf("POLL IN ( CLID: %d ) fd: %d Address: %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));
        }

        //SHAL_SYSTEM::printf("WHILE POLL IN ( CLID: %d ) fd: %d Address: %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));
        if (!netsocket_info->event_stream) {
            pollcnt++;
        }
    }

    if (pollcnt >= TIMEOUT_FIREPROC) {
        SHAL_SYSTEM::printf("TIMEOUT ( CLID: %d ) fd: %d Address: %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));
    }

    pthread_mutex_lock(&process_mutex);
    SHAL_SYSTEM::printf("Closed connection ( CLID: %d ) fd: %d Address: %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));
    fflush(stdout);

    close(netsocket_info->connfd);
    shutdown(netsocket_info->connfd, SHUT_RDWR);
    client_slot_delete(netsocket_info->clid);
    cli_count--;
    _delete_client_from_frontend(_endpoint, netsocket_info->clid, (unsigned int)cli_count);
    free(netsocket_info);
    pthread_mutex_unlock(&process_mutex);
}

bool UR_V16X_Posix::poll_in(int fd, uint32_t timeout_ms)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000UL;

    if (select(fd+1, &fds, NULL, NULL, &tv) != 1) {
        return false;
    }
    return true;
}

int UR_V16X_Posix::process(int fd, struct sockaddr_in *clientaddr)
{
    http_request_t req;
    char filenametmp[MAX_BUFF] = {0};
    char querytmp[MAX_BUFF] = {0};
    int cgi_query = 0;
    char *query_string = nullptr;

    memset(&req, 0, sizeof(http_request_t));
#if V16X_DEBUG >= 3
    SHAL_SYSTEM::printf("parse init...\n");
    fflush(stdout);
#endif // V16X_DEBUG

    int ret = parse_request(fd, &req);

    if (ret || (strlen(req.filename) <= 0)) {
        return ret;
    }

    memcpy(filenametmp, req.filename, sizeof(req.filename));
#if V16X_DEBUG >= 3
    SHAL_SYSTEM::printf("\tINIT FILENAME: %s BEFORE req.filename: %s\n", filenametmp, req.filename);
#endif // V16X_DEBUG
    query_string = req.filename;

    while((*query_string != '?') && (*query_string != '\0')) {
        query_string++;
    }

    if (*query_string == '?') {
        cgi_query = 1;
        *query_string = '\0';
        query_string++;
    }

    int status = 200;
    char val1[20] = {0};
    char val2[20] = {0};

    memcpy(querytmp, query_string, strlen(query_string));
    if (!cgi_query) {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\treq.filename: %s REQ CGI: %s queryP: NONE Addr:%s:%d\n", req.filename, filenametmp, inet_ntoa(clientaddr->sin_addr), ntohs(clientaddr->sin_port));
#endif // V16X_DEBUG
    } else {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\treq.filename: %s req PARAM FILENAME: %s [queryP]: %s Addr:%s:%d\n", req.filename, filenametmp, querytmp, inet_ntoa(clientaddr->sin_addr), ntohs(clientaddr->sin_port));
        //SHAL_SYSTEM::printf("\toffset: %d \n", (int)req.offset);
#endif // V16X_DEBUG

        char msg[MAX_BUFF] = {0};
        if (strcmp(req.filename, "data") == 0) {
            status = 200;
            query_param_t params1[10];
            int ret1 = 0;
            int lenquery1 = strlen(query_string);
            char query_temp1[lenquery1 + 1] = {0};

            if (lenquery1 > 0) {
                memcpy(query_temp1, query_string, lenquery1 + 1);
                ret1 = parse_query(query_temp1,'&', '=', params1, 10);
            }

            if (ret1 > 0) {
                for (int i = 0; i < ret1; i++) {
                    char qmsg[MAX_BUFF] = {0};
                    sprintf(qmsg, "KEY 1: [ %s ]%-3.s\t-\tVAL: [ %s ]", params1[i].key, "", params1[i].val);
                    if (strcmp(params1[i].key,"id") == 0) {
                        sprintf(val1, "%s",params1[i].val);
#if V16X_DEBUG >= 3
                        SHAL_SYSTEM::printf("\tHANDLING QUERY 1 msg----------------%s - val: %s\n", qmsg, val1);
#endif // V16X_DEBUG
                    }
                }
            }
#if V16X_DEBUG >= 3
            SHAL_SYSTEM::printf("\tDATA PARSING STORED ////////  %s\n", data_parsed.data);
#endif // V16X_DEBUG

            query_param_t params2[10];
            int ret2 = 0;
            int lenquery2 = strlen(data_parsed.data);
            char query_temp2[lenquery2 + 1] = {0};

            if (lenquery2 > 0) {
                memcpy(query_temp2, data_parsed.data, lenquery2 + 1);
                ret2 = parse_query(query_temp2,'&', '=', params2, 10);
            }

            if (ret2 > 0) {
                for (int i = 0; i < ret2; i++) {
                    char qmsg[MAX_BUFF] = {0};
                    sprintf(qmsg, "KEY 2: [ %s ]%-3.s\t-\tVAL: [ %s ]", params2[i].key, "", params2[i].val);
                    if (strcmp(params2[i].key,"id") == 0) {
                        sprintf(val2, "%s",params2[i].val);
#if V16X_DEBUG >= 3
                        SHAL_SYSTEM::printf("\tHANDLING STORED 2 msg----------------%s - val: %s\n", qmsg, val2);
#endif // V16X_DEBUG
                    }
                }
            }
#if V16X_DEBUG >= 3
            SHAL_SYSTEM::printf("length DATA: val1: %lu - val2: %lu\n", strlen(val1), strlen(val2));
#endif // V16X_DEBUG
            if (strcmp(val1, val2) == 0) {
                sprintf(msg, "DATA_URL=%lu&%s&%s", req.allsize + strlen(msg), "OK=1", data_parsed.data);
#if V16X_DEBUG >= 3
                SHAL_SYSTEM::printf("DATA_URL CMP TRUE getlen: %lu - msg: %s\n", req.end, msg);
#endif // V16X_DEBUG
                handle_message_outhttp(fd, msg);
            } else {
                //sprintf(msg, "DATA_URL=%lu&%s", req.allsize + strlen(msg), "OK=0");
                //sprintf(msg, "retry:200\ndata:{\"len\":%lu,\"dat\":\"%s\"}\n\n", req.allsize + strlen(msg), "OK=0");
                sprintf(msg, "{\"len\":%lu,\"dat\":\"%s\"}", req.allsize + strlen(msg), "OK");
#if V16X_DEBUG >= 3
                SHAL_SYSTEM::printf("DATA_URL CMP FALSE getlen: %lu - msg: %s\n", req.end, msg);
#endif // V16X_DEBUG
                handle_message_outhttp(fd, msg);
            }

            return ret;
        } else {
            sprintf(msg, "STD_DATA_URL: %s - File: %s", querytmp, filenametmp);
#if V16X_DEBUG >= 3
            SHAL_SYSTEM::printf("\tMSG DATA: %s\n", msg);
#endif // V16X_DEBUG
        }
        log_access(status, clientaddr, &req);
    }

    struct stat sbuf;
    int ffd = open(req.filename, O_RDONLY, 0);

    if (ffd <= 0) {
        status = 404;
        const char *msg = "File not found";
        client_error(fd, status, "Not found", msg);
    } else {
        fstat(ffd, &sbuf);
        if (S_ISREG(sbuf.st_mode)) {
            if (req.end == 0) {
                req.end = sbuf.st_size;
            }

            if (req.offset > 0) {
                status = 206;
            }

            serve_static(fd, ffd, clientaddr, &req, sbuf.st_size);

            if (cgi_query) {
                query_param_t params3[10];
                int ret3 = 0;
                int lenquery3 = strlen(query_string);
                char query_temp3[lenquery3 + 1] = {0};
                if (lenquery3 > 0) {
                    memcpy(query_temp3, query_string, lenquery3 + 1);
                    ret3 = parse_query(query_temp3,'&', '=', params3, 10);
                }

                if (ret3 > 0) {
                    for (int i = 0; i < ret3; i++) {
                        char qmsg[MAX_BUFF] = {0};
                        sprintf(qmsg, "KEY DATA: [ %s ]%-5.s\t- VAL: [ %s ]", params3[i].key, "", params3[i].val);
#if V16X_DEBUG >= 3
                        SHAL_SYSTEM::printf("\tSTORED DATA msg ********** %s\n", qmsg);
#endif
                    }
                }
                char msg[MAX_BUFF] = {0};
                memset(data_parsed.data, '\0', MAX_BUFF);
                sprintf(data_parsed.data, "%s", query_string);
                sprintf(msg, "STD_DATA_URL_STORED_OK: %s - File: %s", querytmp, filenametmp);
#if V16X_DEBUG >= 3
                SHAL_SYSTEM::printf("\tMSG STORED: %s\n", msg);
#endif // V16X_DEBUG
            }
        } else if(S_ISDIR(sbuf.st_mode)) {
            status = 200;

            char dirtmp[256] = {0};
            memcpy(dirtmp, req.filename, strlen(req.filename));

            if (dirtmp[strlen(dirtmp) - 1] != '/') {
                dirtmp[strlen(dirtmp)] = '/';
            } else {
                memset(dirtmp, 0, sizeof(dirtmp));
            }

            handle_directory_request(fd, ffd, dirtmp);
            char msgtmp[MAX_BUFF] = {0};
            sprintf(msgtmp, "&STD_URL=0&%s", data_parsed.data);
#if V16X_DEBUG >= 3
            SHAL_SYSTEM::printf("\tMSG STD: %s - File: %s\n", msgtmp, filenametmp);
            fflush(stdout);
#endif // V16X_DEBUG
            //if (strcmp(val1, val2) == 0) {
                //handle_message(fd, ffd, req.filename, msgtmp);
                //SHAL_SYSTEM::printf("send message 1\n");
            //}
        } else {
            status = 400;
            const char *msg = "Unknow Error";
            client_error(fd, status, "Error", msg);
        }
    }

    if (ffd > 0) {
        close(ffd);
    }

    log_access(status, clientaddr, &req);
    return ret;
}

int Base64Encode(const char* message, char** buffer)
{
  BIO *bio, *b64;
  FILE* stream;
  int encodedSize = 4*ceil((double)strlen(message)/3);
  *buffer = (char *)malloc(encodedSize+1);

  stream = fmemopen(*buffer, encodedSize+1, "w");
  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new_fp(stream, BIO_NOCLOSE);
  bio = BIO_push(b64, bio);
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Ignore newlines - write everything in one line
  BIO_write(bio, message, strlen(message));
  (void)BIO_flush(bio);
  BIO_free_all(bio);
  fclose(stream);

  return (0); //success
}

int ws_b64_ntop(const char * src, size_t srclen, char * dst, size_t dstlen) {
    size_t len = 0;
    int total_len = 0;

    BIO *buff;
    BIO *b64f;
    BUF_MEM *ptr;

    b64f = BIO_new(BIO_f_base64());
    buff = BIO_new(BIO_s_mem());
    buff = BIO_push(b64f, buff);

    BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
    (void)BIO_set_close(buff, BIO_CLOSE);
    do {
        len = BIO_write(buff, src + total_len, srclen - total_len);
        if (len > 0)
            total_len += len;
    } while (len && BIO_should_retry(buff));

    (void)BIO_flush(buff);

    BIO_get_mem_ptr(buff, &ptr);
    len = ptr->length;

    memcpy(dst, ptr->data, dstlen < len ? dstlen : len);
    dst[dstlen < len ? dstlen : len] = '\0';

    BIO_free_all(buff);

    if (dstlen < len)
        return -1;

    return len;
}

int ws_b64_pton(const char * src, char * dst, int dstlen) {
    int len = 0;
    int total_len = 0;
    int pending = 0;

    BIO *buff;
    BIO *b64f;

    b64f = BIO_new(BIO_f_base64());
    buff = BIO_new_mem_buf(src, -1);
    buff = BIO_push(b64f, buff);

    BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
    (void)BIO_set_close(buff, BIO_CLOSE);
    do {
        len = BIO_read(buff, dst + total_len, dstlen - total_len);
        if (len > 0)
            total_len += len;
    } while (len && BIO_should_retry(buff));

    dst[total_len] = '\0';

    pending = BIO_ctrl_pending(buff);

    BIO_free_all(buff);

    if (pending)
        return -1;

    return len;
}

int encode_hybi(char const *src, size_t srclength,
                char *target, size_t targsize, unsigned int opcode)
{
    unsigned long long payload_offset = 2;
    int len = 0;

    if (opcode != OPCODE_TEXT && opcode != OPCODE_BINARY) {
        SHAL_SYSTEM::printf("Invalid opcode. Opcode must be 0x01 for text mode, or 0x02 for binary mode.\n");
        return -1;
    }

    target[0] = (char)((opcode & 0x0F) | 0x80);

    if ((int)srclength <= 0) {
        return 0;
    }

    if (opcode & OPCODE_TEXT) {
        len = ((srclength - 1) / 3) * 4 + 4;
    } else {
        len = srclength;
    }

    if (len <= 125) {
        target[1] = (char) len;
        payload_offset = 2;
    } else if ((len > 125) && (len < 65536)) {
        target[1] = (char) 126;
        *(u_short*)&(target[2]) = htons(len);
        payload_offset = 4;
    } else {
        SHAL_SYSTEM::printf("Sending frames larger than 65535 bytes not supported\n");
        return -1;
        //target[1] = (char) 127;
        //*(u_long*)&(target[2]) = htonl(b64_sz);
        //payload_offset = 10;
    }

    if (opcode & OPCODE_TEXT) {
        len = ws_b64_ntop(src, srclength, target+payload_offset, targsize-payload_offset);
    } else {
        memcpy(target+payload_offset, src, srclength);
        len = srclength;
    }

    if (len < 0) {
        return len;
    }

    return len + payload_offset;
}

int decode_hybi(char *src, size_t srclength,
                char *target, int targsize,
                unsigned int *opcode, unsigned int *left)
{
    char *frame;
    char *mask;
    char *payload;
    char save_char;
    int masked = 0;
    int len = 0;
    int framecount = 0;
    int remaining = 0;
    int target_offset = 0;
    int hdr_length = 0;
    int payload_length = 0;

    *left = srclength;
    frame = src;

    //SHAL_SYSTEM::printf("Deocde new frame\n");
    uint32_t tgtcnt = 0;
    while (1) {
        tgtcnt++;
        // Need at least two bytes of the header
        // Find beginning of next frame. First time hdr_length, masked and
        // payload_length are zero
        frame += hdr_length + 4 * masked + payload_length;
        //SHAL_SYSTEM::printf("frame[0..3]: 0x%x 0x%x 0x%x 0x%x (tot: %d)\n",
        //       (unsigned char) frame[0],
        //       (unsigned char) frame[1],
        //       (unsigned char) frame[2],
        //       (unsigned char) frame[3], srclength);

        if (frame > src + srclength) {
            //SHAL_SYSTEM::printf("Truncated frame from client, need %ld more bytes\n", frame - (src + srclength) );
            break;
        }
        remaining = (src + srclength) - frame;
        if (remaining < 2) {
            //SHAL_SYSTEM::printf("Truncated frame header from client\n");
            break;
        }
        framecount ++;

        *opcode = frame[0] & 0x0f;
        masked = (frame[1] & 0x80) >> 7;

        if (*opcode == 0x8) {
            // client sent orderly close frame
            break;
        }

        payload_length = frame[1] & 0x7f;
        if (payload_length < 126) {
            hdr_length = 2;
            //frame += 2 * sizeof(char);
        } else if (payload_length == 126) {
            payload_length = (frame[2] << 8) + frame[3];
            hdr_length = 4;
        } else {
            //SHAL_SYSTEM::printf("Receiving frames larger than 65535 bytes not supported\n");
            return -1;
        }
        if ((hdr_length + 4*masked + payload_length) > remaining) {
            continue;
        }
        //SHAL_SYSTEM::printf("    payload_length: %u, raw remaining: %u\n", payload_length, remaining);
        payload = frame + hdr_length + 4 * masked;

        if (*opcode != OPCODE_TEXT && *opcode != OPCODE_BINARY) {
            //SHAL_SYSTEM::printf("Ignoring non-data frame, opcode 0x%x\n", *opcode);
            continue;
        }

        if (payload_length == 0) {
            //SHAL_SYSTEM::printf("Ignoring empty frame\n");
            continue;
        }

        if ((payload_length > 0) && (!masked)) {
            //SHAL_SYSTEM::printf("Received unmasked payload from client\n");
            return -1;
        }

        // Terminate with a null for base64 decode
        save_char = payload[payload_length];
        payload[payload_length] = '\0';

        // unmask the data
        mask = payload - 4;
        for (int32_t i = 0; i < payload_length; i++) {
            payload[i] ^= mask[i % 4];
        }

        if (*opcode & OPCODE_TEXT) {
            // base64 decode the data
            len = ws_b64_pton(payload, target+target_offset, targsize);
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("tgtcnt TEXT: %d\n", tgtcnt);
#endif // V16X_DEBUG
        } else {
            memcpy(target+target_offset, payload, payload_length);
            len = payload_length;
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("tgtcnt BINARY: %d\n", tgtcnt);
#endif // V16X_DEBUG
        }

        // Restore the first character of the next frame
        payload[payload_length] = save_char;
        if (len < 0) {
            //SHAL_SYSTEM::printf("Base64 decode error code %d\n", len);
            return len;
        }
        target_offset += len;
    }
#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("    offset: %d hdrlen: %d Mask: %d payload_length: %d len %d\n", target_offset, hdr_length, (4*masked), payload_length, len);
#endif // V16X_DEBUG
    if (*opcode & OPCODE_TEXT) {
        int offsetcp = hdr_length + (4 * masked);
        memset(&target[0], 0, payload_length + 1);
        memcpy(&target[0], src + offsetcp, payload_length);
        //target[payload_length + 1] = '\0';
    }// else {
        //target[payload_length + 1] = '\0';
    //}

    target[payload_length + 1] = '\0';
    *left = remaining;
    return target_offset;
}

int UR_V16X_Posix::parse_request(int fd, http_request_t *req)
{
    data_io_t dat_io;
    char buf[MAX_BUFF] = {0};
    char method[MAX_BUFF] = {0};
    char uri[MAX_BUFF] = {0};
    req->offset = 0;
    req->end = 0;
    int closed = 0;

    memset(req->filename, 0, MAX_BUFF);
    io_data_init(&dat_io, fd);
    io_data_read(&dat_io, buf, MAX_BUFF, &closed);

    if (closed || dat_io.io_cnt <= 0) {
        return closed;
    }

    query_param_t split_params[15];
    int ret_params = parse_query(buf, '\n', 0, split_params, 15);

    for (uint8_t i = 1; i < ret_params; i++) {
        if (strlen(split_params[i].key)  < 2) {
            continue;
        }

        if (strstr(split_params[i].key, "event-stream")) {
            char bufevt[MAX_BUFF] = {0};
            _set_client_event_stream(fd, true);
            sprintf(bufevt, "HTTP/1.1 %d %s\r\n", 200, "OK");
            sprintf(bufevt + strlen(bufevt), "Server:%s\r\n", SERVER_VERSION);
            sprintf(bufevt + strlen(bufevt), "Access-Control-Allow-Origin:%s\r\n", "*");
            sprintf(bufevt + strlen(bufevt), "Cache-Control:no-cache\r\n");
            sprintf(bufevt + strlen(bufevt), "%s", "Content-Type:text/event-stream\r\n");
            sprintf(bufevt + strlen(bufevt), "Date:%s\r\n\r\n", SHAL_SYSTEM::get_date());
            writen(fd, bufevt, strlen(bufevt));
            SHAL_SYSTEM::printf("\n++++++++Event stream parsed!++++++++\n\n");
            return closed;
        }

        if (strstr(split_params[i].key, "WebSocket-Key")) {
            _set_client_event_websocket(fd, true);
            char bufevt[MAX_BUFF] = {0};
            char digest[MAX_BUFF] = {0};
            char b64enc[MAX_BUFF] = {0};
            char field[MAX_BUFF] = {0};
            char fldval[MAX_BUFF] = {0};

            sscanf(split_params[i].key, "%s %s", field, fldval);
            sprintf(fldval + strlen(fldval), "%s", MAGIC_WEBSOCKET_KEY);

            SHA_CTX shactx;
            SHA1_Init(&shactx);
            SHA1_Update(&shactx, (const unsigned char*)fldval, strlen(fldval));
            SHA1_Final((unsigned char*)digest, &shactx);

            //Base64Encode((const char*)digest, b64enc);
            ws_b64_ntop(digest, strlen(digest), b64enc, MAX_BUFF);

            SHAL_SYSTEM::printf("\n++++++++Web Socket parsed!++++++++\n");
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("\nfldval: %s++++++++\n", fldval);
            SHAL_SYSTEM::printf("b64: %s b64enc_len: %lu digest_len: %lu\n", b64enc, strlen(b64enc), strlen(digest));
#endif // V16X_DEBUG
            sprintf(bufevt, "HTTP/1.1 %d Switching Protocols\r\n", 101);
            sprintf(bufevt + strlen(bufevt), "Server:%s\r\n", SERVER_VERSION);
            sprintf(bufevt + strlen(bufevt), "Connection:%s\r\n", "Upgrade");
            sprintf(bufevt + strlen(bufevt), "Access-Control-Allow-Origin:%s\r\n", "*");
            sprintf(bufevt + strlen(bufevt), "%s", "Upgrade:websocket\r\n");
            sprintf(bufevt + strlen(bufevt), "Date:%s\r\n", SHAL_SYSTEM::get_date());
            sprintf(bufevt + strlen(bufevt), "Sec-WebSocket-Accept:%s\r\n\r\n", b64enc);
            writen(fd, bufevt, strlen(bufevt));

#if V16X_DEBUG >= 99
            SHAL_SYSTEM::printf("digest: ");
            for (uint16_t i = 0; i < strlen((char*)digest); i++) {
                SHAL_SYSTEM::printf("0x%2x ", (uint8_t)digest[i]);
            }
            SHAL_SYSTEM::printf("\n\n");
#endif
            return  closed;
        }

        if (strstr(split_params[i].key, "Range")) {
            int64_t offset;
            char field[MAX_BUFF] = {0};
            sscanf(split_params[i].key,"%s bytes=%lu-", field, &offset);
            req->offset = offset;
        }
    }

    sscanf(buf, "%s %s", method, uri);

    if (closed) {
        return closed;
    }

    char buftmp[MAX_BUFF] = {0};
    char wsdec[MAX_BUFF] = {0};
    unsigned int opcode, left;

    memcpy(buftmp, buf, dat_io.io_cnt);
    int len = decode_hybi(buftmp, dat_io.io_cnt, wsdec, MAX_BUFF, &opcode, &left);
    (void)len;

#if V16X_DEBUG >= 1
    netsocket_inf_t *client = _get_client(fd);
#endif // V16X_DEBUG
    if (strstr(wsdec, "V16X")) {
#if V16X_DEBUG >= 1
        if (client != NULL) {
            SHAL_SYSTEM::printf("V16X METHOD msg: %s len: %d ( CLID: %d ) fd: %d Address:  %s:%d closed: %d\n\n", wsdec, len, client->clid, client->connfd, inet_ntoa(client->clientaddr.sin_addr), ntohs(client->clientaddr.sin_port), closed);
        }
#endif // V16X_DEBUG
        return closed;
    } else if (!strstr(method, "GET")) {
        if (strlen(method) > 0) {
#if V16X_DEBUG >= 1
            if (client != NULL) {
                SHAL_SYSTEM::printf("ERROR METHOD msg: %s wsdec: %s len: %d ( CLID: %d ) fd: %d Address:  %s:%d closed: %d\n\n", method, wsdec, len, client->clid, client->connfd, inet_ntoa(client->clientaddr.sin_addr), ntohs(client->clientaddr.sin_port), closed);
            }
#endif // V16X_DEBUG
        }
#if V16X_DEBUG >= 1
        if (client != NULL) {
            SHAL_SYSTEM::printf("NO GET METHOD ( CLID: %d ) fd: %d Address:  %s:%d closed: %d\n\n", client->clid, client->connfd, inet_ntoa(client->clientaddr.sin_addr), ntohs(client->clientaddr.sin_port), closed);
        }
#endif // V16X_DEBUG
        return closed;
    } else if (strstr(method, "GET")) {
        _set_client_method_get(fd, true);
    }

    req->allsize = dat_io.io_cnt;
    char* filename = uri;
    while (*filename == '/') {
        filename++;
    }

    int length = strlen(filename);
    if (length == 0) {
        sprintf(req->filename, "%s", "index.html");
        sprintf(filename, "%s", "index.html");
    }

    if (strlen(filename) > 0) {
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("URL filename: %s\n", filename);
#endif // V16X_DEBUG
        if (filename[0] == '?') {
            sprintf(req->filename, "%s", "index.html");
            sprintf(filename, "%s", "index.html");
        }
        url_decode(filename, req->filename, strlen(filename) + 1);
    }

    return closed;
}

int UR_V16X_Posix::parse_query(char *query, char delimiter, char setter, query_param_t *params, int max_params)
{
    int i = 0;

    if (NULL == query || '\0' == *query) {
        return -1;
    }

    params[i++].key = query;
    while (i < max_params && NULL != (query = strchr(query, delimiter))) {
        *query = '\0';
        params[i].key = ++query;
        params[i].val = NULL;

        /* Go back and split previous param */
        if (i > 0 && setter != 0) {
            if ((params[i - 1].val = strchr(params[i - 1].key, setter)) != NULL) {
                *(params[i - 1].val)++ = '\0';
                char * pchar = NULL;
                pchar = strtok(params[i-1].val," ");
                while (pchar != NULL) {
                    pchar = strtok(NULL, " ");
                }
                params[i].val = pchar;
            }
        }
        i++;
    }

    if (setter != 0) {
        /* Go back and split last param */
        if ((params[i - 1].val = strchr(params[i - 1].key, setter)) != NULL) {
            *(params[i - 1].val)++ = '\0';
        }
    }

    return i;
}

void UR_V16X_Posix::handle_message_outhttp(int fd, char *longmsg)
{
    char buf[MAX_BUFF] = {0};

    if (!_has_client_events(fd)) {
        sprintf(buf, "HTTP/1.1 %d %s\r\n", 200, "OK");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
        sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
        sprintf(buf + strlen(buf), "Cache-Control:no-cache\r\n");
        sprintf(buf + strlen(buf), "%s", "Content-Type:text/plain;charset=UTF-8\r\n");
        sprintf(buf + strlen(buf), "Date:%s\r\n", SHAL_SYSTEM::get_date());
        sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", strlen(longmsg));
        sprintf(buf + strlen(buf), "%s", longmsg);
        writen(fd, buf, strlen(buf));
    }
}

void UR_V16X_Posix::log_access(int status, struct sockaddr_in *c_addr, http_request_t *req)
{
    char filetmp[MAX_BUFF] = {0};
    memcpy(filetmp, req->filename, sizeof(req->filename));
#if V16X_DEBUG >= 3
    SHAL_SYSTEM::printf("---[ LOGACCESS %s:%d Status:%d - %s ]---\n\n", inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port), status, filetmp);
#endif
}

void UR_V16X_Posix::client_error(int fd, int status, const char *msg, const char *longmsg)
{
    char buf[MAX_BUFF] = {0};
    char longmsgtmp[MAX_BUFF] = {0};

    sprintf(longmsgtmp, "<HTML><head><meta charset=\"UTF-8\"/><TITLE>%s</TITLE></head>", msg);
    sprintf(longmsgtmp + strlen(longmsgtmp), "<BODY><center><H3>%s</H3></center>", msg);
    sprintf(longmsgtmp + strlen(longmsgtmp), "<center><p>%s.</p></center>", longmsg);
    sprintf(longmsgtmp + strlen(longmsgtmp), "<hr><center>%s</center></BODY></HTML>\n", SERVER_VERSION);

    sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
    sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
    sprintf(buf + strlen(buf), "%s", "Content-Type:text/html;charset=UTF-8\r\n");
    sprintf(buf + strlen(buf), "Cache-Control:no-cache\r\n");
    sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
    sprintf(buf + strlen(buf), "Date:%s\r\n", SHAL_SYSTEM::get_date());
    sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", strlen(longmsgtmp));
    writen(fd, buf, strlen(buf));
    sprintf(buf, "%s", longmsgtmp);
    writen(fd, buf, strlen(buf));
}

void UR_V16X_Posix::serve_static(int out_fd, int in_fd, struct sockaddr_in *c_addr, http_request_t *req, size_t total_size)
{
    char buf[MAX_BUFF] = {0};
    int status;

    if (strstr(req->filename, "v16x")) {
        status = 404;
        const char *msg = "File not found";
        client_error(out_fd, status, "Not found", msg);
        return;
    }

    if (req->offset > req->end) {
        char msg[] = "Offset file error\n";
        handle_message_outhttp(out_fd, msg);
        SHAL_SYSTEM::printf("\t[ Offset > end ] #1 fd:%d offset: %d end: %d diff: %d - %s Addr:%s:%d\n", in_fd, (int)req->offset, (int)req->end, (int)(req->end - req->offset), req->filename, inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port));
        return;
    }

    if (req->offset > 0) {
        sprintf(buf, "%s", "HTTP/1.1 206 Partial\r\n");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
        sprintf(buf + strlen(buf), "Content-Range:bytes %lu-%lu/%lu\r\n", req->offset, req->end - 1, total_size);
    } else {
        sprintf(buf, "%s", "HTTP/1.1 200 OK\r\nAccept-Ranges:bytes\r\n");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
    }

    sprintf(buf + strlen(buf), "Access-Control-Allow-Origin:%s\r\n", "*");
    //sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
    sprintf(buf + strlen(buf), "Cache-Control:no-cache\r\n");
    // sprintf(buf + strlen(buf), "Cache-Control:public,max-age=315360000\r\nExpires:Thu,31 Dec 2037 23:55:55 GMT\r\n");

    sprintf(buf + strlen(buf), "Content-type:%s\r\n", get_mime_type(req->filename));
    sprintf(buf + strlen(buf), "Date:%s\r\n", SHAL_SYSTEM::get_date());
    sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", req->end - req->offset);

    writen(out_fd, buf, strlen(buf));

    char filetmp[MAX_BUFF] = {0};
    memcpy(filetmp, req->filename, sizeof(req->filename));
    int64_t offset = (int64_t)req->offset;
    int64_t offsettmp = (int64_t)req->offset;
    int64_t end = (int64_t)req->end;

    // split at 1M if file > 2M
    //if (end > (int64_t)(1024 * 1024 * 2)) {
        //offsettmp = end / round(end / (1024 * 1024));
    //}

    SHAL_SYSTEM::printf("\tServing static #1 fd:%d offset: %d offsettmp: %d end: %d diff: %d - %s Addr:%s:%d\n", in_fd, (int)offset, (int)offsettmp, (int)end, (int)(end - offset), filetmp, inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port));
    fflush(stdout);
#ifdef __MSYS__
    char buftmp[end - offsettmp];
#endif

    while(offset < end) {
#ifdef __MSYS__
        read(in_fd, buftmp, end - offsettmp);
        offset = write(out_fd, buftmp, end - offsettmp);
#else
        if(sendfile(out_fd, in_fd, &offset, end - offset) <= 0) {
            break;
        }
#endif // __MSYS__
        //offsettmp = offset;
        //filetmp[strlen(req->filename)] = '\0';
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("\tServing static #2 fd:%d offset: %d end: %d diff: %d - %s Addr:%s:%d\n", in_fd, (int)offset, (int)end, (int)(end - offset), filetmp, inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port));
        fflush(stdout);
#endif // V16X_DEBUG
        break;
    }
}

void UR_V16X_Posix::handle_directory_request(int out_fd, int dir_fd, char *filename)
{
    const uint64_t max_buff = MAX_BUFF * 2;
    char bufdir[max_buff] = {0};
    char m_time[32] = {0};
    char size[16] = {0};
    char longmsg[max_buff] = {0};
    struct stat statbuf;
    char *norm_filename = nullptr;

    sprintf(bufdir, "HTTP/1.1 %d %s\r\n", 200, "OK");
    sprintf(bufdir + strlen(bufdir), "Server:%s\r\n", SERVER_VERSION);
    sprintf(bufdir + strlen(bufdir), "%s", "Content-Type:text/html;charset=UTF-8\r\n");
    sprintf(bufdir + strlen(bufdir), "Cache-Control:no-cache\r\n");
    sprintf(bufdir + strlen(bufdir), "Connection:%s\r\n", "close");
    sprintf(bufdir + strlen(bufdir), "Date:%s\r\n\r\n", SHAL_SYSTEM::get_date());
    //sprintf(bufdir + strlen(bufdir), "Content-length:%lu\r\n\r\n", strlen(longmsg));

    writen(out_fd, bufdir, strlen(bufdir));

    sprintf(longmsg, "%s", "<html><head><meta charset=\"UTF-8\"/><style>");
    sprintf(longmsg + strlen(longmsg), "%s", "body{font-family:monospace;font-size:13px;}");
    sprintf(longmsg + strlen(longmsg), "%s", "td {padding: 1.5px 6px;}");
    sprintf(longmsg + strlen(longmsg), "%s", "</style></head><body><table>");
    writen(out_fd, longmsg, strlen(longmsg));

    DIR *d = fdopendir(dir_fd);
    struct dirent *dp;
    int ffd;
    int filecnt = 0;
    int dircnt = 0;

    norm_filename = new char[strlen(filename) + 1];
    memcpy(norm_filename, filename, strlen(filename) + 1);

    while((*norm_filename != '/') && (*norm_filename != '\0')) {
        norm_filename++;
    }

    if (*norm_filename == '/') {
        *norm_filename = '\0';
        norm_filename++;
    }

    while((dp = readdir(d)) != NULL) {
        if (strlen(longmsg) >= ((max_buff) - 256)) {
            break;
        }

        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
            continue;
        }

        ffd = openat(dir_fd, dp->d_name, O_RDONLY);
        if (ffd == -1) {
            perror(dp->d_name);
            continue;
        }

        fstat(ffd, &statbuf);
        strftime(m_time, sizeof(m_time), "%Y-%m-%d %H:%M", localtime(&statbuf.st_mtime));

        format_size(size, &statbuf, &filecnt, &dircnt);

        if (S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)) {
            const char *d1 = S_ISDIR(statbuf.st_mode) ? "/" : "";
            char dname[256] = {0};
            char color[20] = {0};
            if (*d1 == '/') {
                 sprintf(color, "%s", "color:#e50000");
            }
            if (!strstr(dp->d_name, "v16x")) {
                if ((strlen(filename) > 0) && (strlen(norm_filename) > 0)) {
                    sprintf(dname, "%s", norm_filename);
                } else {
                    sprintf(dname, "%s", filename);
                }
                sprintf(dname + strlen(dname), dp->d_name, strlen(dp->d_name));
                char lngmsg[MAX_BUFF] = {0};
                sprintf(lngmsg, "<tr><td><a style='%s' href=\"%s%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>",
                        color, dname, d1, dp->d_name, d1, m_time, size);
                writen(out_fd, lngmsg, strlen(lngmsg));
            } else {
                filecnt = filecnt - 1;
            }
        }

        close(ffd);
    }

    sprintf(longmsg, "<td><br>Total items: %d Files: %d Dirs: %d</br></td></table><hr><center>%s</center></body></html>", filecnt + dircnt, filecnt, dircnt, SERVER_VERSION);
    sprintf(bufdir, "%s", longmsg);
    writen(out_fd, bufdir, strlen(bufdir));

    closedir(d);
    close(out_fd);
}

ssize_t UR_V16X_Posix::io_data_read(data_io_t *data_iop, char *usrbuf, size_t maxlen, int *closed)
{
    memset(data_iop->io_buf, '\0', MAX_BUFF);
    memset(usrbuf, '\0', maxlen);
    data_iop->io_cnt = recv(data_iop->io_fd, data_iop->io_buf, MAX_BUFF, MSG_DONTWAIT);
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("io_data_read 2 cnt: %d buf: %s\n", data_iop->io_cnt, data_iop->io_buf);
    //fflush(stdout);
#endif // V16X_DEBUG
    if (data_iop->io_cnt > 0) {
        memcpy(usrbuf, data_iop->io_buf, data_iop->io_cnt);
    }

    if (data_iop->io_cnt <= 0) {
        SHAL_SYSTEM::printf("io_data_read 3 cnt: %d\n", data_iop->io_cnt);
        *closed = 1;
        return -1;
    }

    return data_iop->io_cnt;
}

void UR_V16X_Posix::io_data_init(data_io_t *data_iop, int fd)
{
    data_iop->io_fd = fd;
    data_iop->io_cnt = 0;
    data_iop->io_bufptr = data_iop->io_buf;
}

void UR_V16X_Posix::url_decode(char* src, char* dest, int max)
{
    char *p = src;
    char code[3] = { 0 };
    while(*p && --max) {
        if(*p == '%') {
            memcpy(code, ++p, 2);
            *dest++ = (char)strtoul(code, NULL, 16);
            p += 2;
        } else {
            *dest++ = *p++;
        }
    }

    *dest = '\0';
}

ssize_t UR_V16X_Posix::writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    const char *bufp = (char*)usrbuf;

    while(nleft > 0){
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

const char* UR_V16X_Posix::get_mime_type(char *filename)
{
    char *dot = strrchr(filename, '.');

    if (dot) {
        const mime_map_t *map = mime_types;

        while(map->extension) {
            if (strcmp(map->extension, dot) == 0) {
                return map->mime_type;
            }
            map++;
        }
    }

    return default_mime_type;
}

void UR_V16X_Posix::format_size(char* buf, struct stat *stat, int *filecnt, int *dircnt)
{
    if (S_ISDIR(stat->st_mode)) {
        sprintf(buf, "%s", "[DIR]");
        *dircnt += 1;
    } else {
        *filecnt += 1;
        off_t size = stat->st_size;
        if (size < 1024) {
            sprintf(buf, "%lu", size);
        } else if (size < 1024 * 1024) {
            sprintf(buf, "%.1fK", (double)size / 1024);
        } else if (size < 1024 * 1024 * 1024) {
            sprintf(buf, "%.1fM", (double)size / 1024 / 1024);
        } else {
            sprintf(buf, "%.1fG", (double)size / 1024 / 1024 / 1024);
        }
    }
}

void UR_V16X_Posix::process_event_stream()
{
    char msg[MAX_BUFF] = {0};
    static uint32_t last = SHAL_SYSTEM::millis32();
    uint32_t now = SHAL_SYSTEM::millis32();

    if ((now - last) < PROCESS_EVENT_INTERVAL) {
        return;
    }

    last = SHAL_SYSTEM::millis32();
    netsocket_inf_t client;

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            pthread_mutex_lock(&clients_mutex);
            client = *clients[i];
            pthread_mutex_unlock(&clients_mutex);
            if (client.event_stream) {
                if (!client.evtstr_ping) {
                    continue;
                }

                //clients[i]->evtstr_ping = false;
                memset(msg, '\0', MAX_BUFF);
                sprintf(msg, "retry:1000\nid:%d\ndata:{\"len\":%lu,\"clid\":\"%d\",\"fd\":%d}\n\n", client.clid, sizeof(msg), client.clid, client.connfd);
                writen(client.connfd, msg, strlen(msg));
                //shutdown(clients[i]->connfd, SHUT_RDWR);
            }

            if (client.event_websocket) {
                if (!client.evtwebsock_ping) {
                    continue;
                }

                //clients[i]->evtwebsock_ping = false;
                memset(msg, '\0', MAX_BUFF);
                char buftmp[MAX_BUFF] = {0};
                sprintf(buftmp, "{\"len\":%lu,\"clid\":\"%d\",\"fd\":%d}", sizeof(buftmp), client.clid, client.connfd);
                encode_hybi(buftmp, strlen(buftmp), msg, MAX_BUFF, OPCODE_BINARY);
                writen(client.connfd, msg, strlen(msg));
                //shutdown(clients[i]->connfd, SHUT_RDWR);
            }
        }
    }
}

void UR_V16X_Posix::_client_transaction(TYPE_TRANSACTION_E typetr, events_transaction_t &data_transaction)
{
    netsocket_inf_t *client = _get_client(data_transaction.fd);

    pthread_mutex_lock(&clients_mutex);
    if (client != NULL) {
        switch (typetr) {
            case TYPE_TRANSACTION_E::SET_EVENT_STREAM: {
                client->event_stream = data_transaction.event_stream;
                client->evtstr_ping = data_transaction.event_stream;
            }
            break;
            case TYPE_TRANSACTION_E::GET_EVENT_STREAM: {
                data_transaction.event_stream = client->event_stream;
            }
            break;
            case TYPE_TRANSACTION_E::SET_CLIENT_METHOD_GET: {
                client->method_get = data_transaction.method_get;
            }
            break;
            case TYPE_TRANSACTION_E::GET_CLIENT_METHOD_GET: {
                data_transaction.method_get = client->method_get;
            }
            break;
            case TYPE_TRANSACTION_E::SET_EVENT_WEBSOCKET: {
                client->event_websocket = data_transaction.event_websocket;
                client->evtwebsock_ping = data_transaction.event_websocket;
            }
            break;
            case TYPE_TRANSACTION_E::GET_EVENT_WEBSOCKET: {
                data_transaction.event_websocket = client->event_websocket;
            }
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void UR_V16X_Posix::_set_client_event_stream(int fd, bool event_stream)
{
    events_transaction_t data_tr;
    data_tr.fd = fd;
    data_tr.event_stream = event_stream;

    _client_transaction(TYPE_TRANSACTION_E::SET_EVENT_STREAM, data_tr);
}

bool UR_V16X_Posix::_has_client_event_stream(int fd)
{
    events_transaction_t data_tr;
    data_tr.fd = fd;
    data_tr.event_stream = false;

    _client_transaction(TYPE_TRANSACTION_E::GET_EVENT_STREAM, data_tr);

    return data_tr.event_stream;
}

UR_V16X_Posix::netsocket_inf_t *UR_V16X_Posix::_get_client(int fd)
{
    netsocket_inf_t * clientinf = NULL;

    pthread_mutex_lock(&clients_mutex);
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->connfd == fd) {
                clientinf = clients[i];
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return clientinf;
}

void UR_V16X_Posix::_set_client_event_websocket(int fd, bool event_websocket)
{
    events_transaction_t data_tr;
    data_tr.fd = fd;
    data_tr.event_websocket = event_websocket;

    _client_transaction(TYPE_TRANSACTION_E::SET_EVENT_WEBSOCKET, data_tr);
}

bool UR_V16X_Posix::_has_client_event_websocket(int fd)
{
    events_transaction_t data_tr;
    data_tr.fd = fd;
    data_tr.event_websocket = false;

    _client_transaction(TYPE_TRANSACTION_E::GET_EVENT_WEBSOCKET, data_tr);

    return data_tr.event_websocket;
}

bool UR_V16X_Posix::_has_client_events(int fd)
{
    bool ret_evt_websocket = _has_client_event_websocket(fd);
    bool ret_evt_stream = _has_client_event_stream(fd);

    return ret_evt_stream | ret_evt_websocket;
}

bool UR_V16X_Posix::_has_client_same_ip(sockaddr_in cliaddr)
{
    bool ret = false;

    pthread_mutex_lock(&clients_mutex);
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->clientaddr.sin_addr.s_addr == cliaddr.sin_addr.s_addr) {
                ret = true;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return ret;
}

void UR_V16X_Posix::_set_client_method_get(int fd, bool method_get)
{
    events_transaction_t data_tr;
    data_tr.fd = fd;
    data_tr.method_get = method_get;

    _client_transaction(TYPE_TRANSACTION_E::SET_CLIENT_METHOD_GET, data_tr);
}

bool UR_V16X_Posix::_has_client_method_get(int fd)
{
    events_transaction_t data_tr;
    data_tr.fd = fd;
    data_tr.method_get = false;

    _client_transaction(TYPE_TRANSACTION_E::GET_CLIENT_METHOD_GET, data_tr);

    return data_tr.method_get;
}

bool UR_V16X_Posix::_has_ip_method_get(sockaddr_in cliaddr)
{
    bool ret = false;
    netsocket_inf_t client = {0};
    _get_client(TYPE_TRANSACTION_E::GET_CLIENT_METHOD_GET, client);

    if (client.clientaddr.sin_addr.s_addr == cliaddr.sin_addr.s_addr) {
        ret = true;
    }

    return ret;
}

void UR_V16X_Posix::_get_client(TYPE_TRANSACTION_E typetr, netsocket_inf_t &client)
{
    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] == NULL) {
            continue;
        }
        if (typetr == TYPE_TRANSACTION_E::GET_CLIENT_METHOD_GET) {
            if (_has_client_method_get(clients[i]->connfd)) {
                client = *clients[i];
                break;
            }
        }
    }
}
