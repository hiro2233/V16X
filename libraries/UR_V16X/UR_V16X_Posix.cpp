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

#ifdef ENABLE_LOG
#define LOG_FILE	"server.log"
#define LOG(s) { \
    FILE *pf=fopen(LOG_FILE, "a"); \
    fprintf(pf, \
        "%s\n", s); \
    fclose(pf); \
    }

#ifndef stderr
#define stderr pf
#endif // stderr

#define ERROR(s) { \
    LOG(s); \
    fprintf(stderr, \
        "%s\n", s); \
    }
#endif // ENABLE_LOG

pthread_mutex_t tmpmtxunlock;
pthread_mutex_t tmpmtxlock;

#define THREAD_TRY_LOCK_BLOCK(_thr) \
    while (pthread_mutex_trylock(&_thr) != 0) \
    {}
/*
    extern pthread_mutex_t tmpmtxlock; \
    tmpmtxlock = _thr
*/


#define THREAD_UNLOCK(_thr) \
    pthread_mutex_unlock(&_thr)
/*
    extern pthread_mutex_t tmpmtxunlock; \
    tmpmtxunlock = _thr
*/

//UR_Crypton UR_V16X_Posix::ur_crypton;
pthread_mutex_t UR_V16X_Posix::clients_mutex;
pthread_mutex_t UR_V16X_Posix::process_mutex;
pthread_mutexattr_t UR_V16X_Posix::process_mutex_attr;
pthread_mutexattr_t UR_V16X_Posix::clients_mutex_attr;

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
    {".geojson", "application/geo+json"},
    {NULL, NULL},
};

UR_V16X_Posix::UR_V16X_Posix(UR_V16X &v16x) :
    UR_V16X_Driver(v16x)
{
    _endpoint = _frontend.register_endpoint();
    int port = _frontend.get_bindport();
    const char *addr = _frontend.get_bindaddr();
    if (addr) {
        sprintf(default_addr, "%s", addr);
    } else {
        sprintf(default_addr, "%s", "0.0.0.0");
    }
    if (port > 0) {
        default_port = port;
    }
}

bool UR_V16X_Posix::load_certificate(SSL_CTX *ctx, const char *cert, const char *key)
{
#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
    if(SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
        ERROR("[error] certificate is invalid");
        return false;
    }

    if(SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 ) {
        ERROR("[error] private key is invalid");
        return false;
    }

    if(!SSL_CTX_check_private_key(ctx)) {
        ERROR("[error] key pair does not match");
        return false;
    }
#endif
    return true;
}

void UR_V16X_Posix::init(void)
{
    SHAL_SYSTEM::printf("Init V16X endpoint: %d\n", _endpoint);
    fflush(stdout);

    pthread_mutexattr_init(&process_mutex_attr);
    pthread_mutexattr_settype(&process_mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&process_mutex, &process_mutex_attr);

    pthread_mutexattr_init(&clients_mutex_attr);
    pthread_mutexattr_settype(&clients_mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&clients_mutex, &clients_mutex_attr);

    ur_crypton.init();

    cli_count = 0;

    THREAD_TRY_LOCK_BLOCK(clients_mutex);

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        clients[i] = NULL;
    }

    THREAD_UNLOCK(clients_mutex);

#ifdef __MINGW32__
    windows_socket_start();
#endif // __MINGW32__

    listenfd = open_listenfd(default_port);
    if (listenfd > 0) {
        SHAL_SYSTEM::printf("Listen on addres %s port %d, fd is %d\n\n\n", default_addr, default_port, listenfd);
        fflush(stdout);
    } else {
        perror("ERROR");
        exit(listenfd);
    }

    SHAL_SYSTEM::register_timer_process(FUNCTOR_BIND(this, &UR_V16X_Posix::process_event_stream, void));
}

void UR_V16X_Posix::update(void)
{
    fflush(stdout);
    if (poll_in(listenfd, UPDATE_POLLIN_INTERVAL)) {
        if (sig_evt) {
            return;
        }

        struct sockaddr_in clientaddrin;
        socklen_t clientlen = sizeof(clientaddrin);

        int connfd = accept(listenfd, (SA *)&clientaddrin, &clientlen);
        if (connfd < 0) {
            return;
        }

        SHAL_SYSTEM::printf("Waiting connection for client #%d\n\n", (unsigned int)cli_count + 1);

        int one = 1;
        int alive_one = 1;

        struct timeval timeout;
        int timeout_tcp = 9000;
        int retries_tcp = 3;
        int interval_tcp = 2;
        int delay_idle_tcp = 5;
        timeout.tv_sec = 9;
        timeout.tv_usec = 0;

        if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)) < 0) {
            //SHAL_SYSTEM::panic("setsockopt SO_RCVTIMEO failed\n");
        }

        if (setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout)) < 0) {
            //SHAL_SYSTEM::panic("setsockopt SO_SNDTIMEO failed\n");
        }

        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (const void*)&one, sizeof(one));
        setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&one, sizeof(one));

        setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, (const void*)&alive_one, sizeof(alive_one)); // keep alive?

#ifndef _WIN32
        setsockopt(connfd, IPPROTO_TCP, TCP_KEEPIDLE, (const void*)&delay_idle_tcp, sizeof(delay_idle_tcp)); // delay idle
#endif // _WIN32
        setsockopt(connfd, IPPROTO_TCP, TCP_KEEPCNT, (const void*)&retries_tcp, sizeof(retries_tcp)); // retries
        setsockopt(connfd, IPPROTO_TCP, TCP_KEEPINTVL, (const void*)&interval_tcp, sizeof(interval_tcp)); // interval
        setsockopt(connfd, IPPROTO_TCP, TCP_USER_TIMEOUT, (const void*)&timeout_tcp, sizeof(timeout_tcp));

        // Here we catch flooding connections and will wait until
        // timeout occurs on the fire process thread side.
        // Be careful when changing clients numbers connections and times,
        // wrong values could cause overflow, this is relative between to the
        // update and clients process connections timeout.
        while (cli_count + 50 >= V16X_MAX_CLIENTS) {
            if (_has_client_same_ip(clientaddrin)) {
                client_error(connfd, 500, "Error: Max Clients", "Error: Max Clients reached");
                close(connfd);
                return;
            }
            SHAL_SYSTEM::delay_ms(10);
        }

        // Check if max clients is reached. This need to be lower and relative to
        // the above clients numbers connections when catch flooding.
        if ((cli_count + 25) >= V16X_MAX_CLIENTS) {
            SHAL_SYSTEM::printf("max clients reached, reject\n");
            fflush(stdout);
            close(connfd);
            return;
        }

#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
        if (ctxmain != NULL) {
            sslmain = SSL_new(ctxmain);
            SSL_set_fd(sslmain, connfd);

            SSL_accept(sslmain);
            //SSL_set_accept_state(sslmain);
            SSL_set_mode(sslmain, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        }
#endif

        netsocket_inf_t *netsocket_info = (netsocket_inf_t *)malloc(sizeof(netsocket_inf_t));
        memset(netsocket_info, 0, sizeof(netsocket_inf_t));

        netsocket_info->clientaddr = clientaddrin;
        netsocket_info->connfd = connfd;
        netsocket_info->clid = clid++;
        netsocket_info->is_attached = false;
        netsocket_info->evtstr_connected = false;
        netsocket_info->event_stream = false;
        netsocket_info->evtstr_ping = false;
        netsocket_info->event_websocket = false;
#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
        netsocket_info->ssl = sslmain;
#endif

        client_slot_add(netsocket_info);

        SHAL_SYSTEM::printf("Connected client ( CLID: %d ) fd: %d Address:  %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port));
        fflush(stdout);

        free(netsocket_info);
    }
}

UR_V16X_Driver *UR_V16X_Posix::create_endpoint(UR_V16X &v16x)
{
    UR_V16X_Posix *endpoint = nullptr;
    endpoint = new UR_V16X_Posix(v16x);
    endpoint->init();
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

#ifndef _WIN32
    fcntl(listenfd, F_SETFD, FD_CLOEXEC);
#endif // _WIN32

#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    ctxmain = SSL_CTX_new(SSLv23_server_method());
    SSL_CTX_set_options(ctxmain, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(ctxmain, SSL_OP_NO_SSLv3);

    if (!load_certificate(ctxmain, certmain, keymain)) {
        ctxmain = NULL;
    }

    //SSL_set_accept_state(ctxmain);
#endif

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
        //return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
#ifdef __MINGW32__
    serveraddr.sin_addr.s_addr = inet_addr((const void*)default_addr);
#else
    inet_aton((const char*)default_addr, &serveraddr.sin_addr);
#endif
    //serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
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
    THREAD_TRY_LOCK_BLOCK(clients_mutex);

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] == NULL) {
            clients[i] = (netsocket_inf_t *)malloc(sizeof(netsocket_inf_t));
            memset(clients[i], 0, sizeof(netsocket_inf_t));
            memset(&clients[i]->clientaddr, 0, sizeof(sockaddr_in));

            memcpy(clients[i], cl, sizeof(netsocket_inf_t));
            memcpy(&clients[i]->clientaddr, &cl->clientaddr, sizeof(sockaddr_in));
            clients[i]->ssl = NULL;
            if (cl->ssl != NULL) {
                clients[i]->ssl = cl->ssl;
            }
            break;
        }
    }
    THREAD_UNLOCK(clients_mutex);
}

/* Delete client from slot */
void UR_V16X_Posix::client_slot_delete(uint32_t clid_slot)
{
    THREAD_TRY_LOCK_BLOCK(clients_mutex);

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->clid == clid_slot) {
#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
                if (clients[i]->ssl != NULL) {
                    //SSL_clear(clients[i]->ssl);
                    SSL_free(clients[i]->ssl);
                }
#endif
                free(clients[i]);
                clients[i] = NULL;
                break;
            }
        }
    }
    THREAD_UNLOCK(clients_mutex);
}

UR_V16X_Posix::netsocket_inf_t *UR_V16X_Posix::_get_next_unattached_client()
{
    THREAD_TRY_LOCK_BLOCK(clients_mutex);

    uint16_t clientpos = (uint16_t)-1;

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (!clients[i]->is_attached) {
                clients[i]->is_attached = true;
                clientpos = i;
                break;
            }
        }
    }
    THREAD_UNLOCK(clients_mutex);

    return (clientpos != (uint16_t)-1) ? clients[clientpos] : NULL;
}

void UR_V16X_Posix::shuttdown()
{
    SHAL_SYSTEM::printf("Shutting down V16X posix endpoint: %d\n", _endpoint);
    fflush(stdout);
    THREAD_TRY_LOCK_BLOCK(clients_mutex);

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->is_attached) {
                clients[i]->is_attached = false;
                close(clients[i]->connfd);
            }
        }
    }
    close(listenfd);
    THREAD_UNLOCK(clients_mutex);
    //sig_evt = 1;
    SHAL_SYSTEM::printf("Shutdown OK V16X posix endpoint: %d\n", _endpoint);
    fflush(stdout);
    SHAL_SYSTEM::system_shal_shutdown();
}

void UR_V16X_Posix::fire_process()
{

    int mtx = 0;

    while ((mtx = pthread_mutex_trylock(&process_mutex)) != 0) {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("%s - %s Mutex locked mtx: %d\n", SHAL_SYSTEM::get_date().c_str(), __func__, mtx);
#endif // V16X_DEBUG
        //return;
    }

    netsocket_inf_t *netsocket_info = _get_next_unattached_client();
    if (netsocket_info == NULL) {
        THREAD_UNLOCK(process_mutex);
        return;
    }

    int rlen = 0;

    netsocket_info->sending = 0;
    cli_count++;

    _copy_client_to_frontend(_endpoint, netsocket_info->clid, netsocket_info->is_attached, (unsigned int)cli_count, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port));
    SHAL_SYSTEM::printf("%s - FIRE_ISR: Start process ( CLID: %d ) mtx: %d\n", SHAL_SYSTEM::get_date().c_str(), netsocket_info->clid, mtx);
    fflush(stdout);

    THREAD_UNLOCK(process_mutex);

    uint32_t timeout = SHAL_SYSTEM::millis32();
    uint32_t lastime = 0;
    uint16_t pollcnt = 0;
    while(rlen == 0 && netsocket_info->is_attached && pollcnt < TIMEOUT_FIREPROC) {
        if (poll_in(netsocket_info->connfd, FIREPROC_POLLIN_INTERVAL)) {
            rlen = process(netsocket_info->connfd, &netsocket_info->clientaddr);    // Process all incomming data
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("%sPOLLCNT: %d %s rlen:%d ( CLID: %d ) fd: %d Address: %s:%d\n\n", COLOR_PRINTF_RED(0), pollcnt, COLOR_PRINTF_RESET, rlen, netsocket_info->clid, netsocket_info->connfd, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port));
#endif // V16X_DEBUG
        }

        if (netsocket_info->sending == 1) {
            pollcnt = 0;
        }

        if (!netsocket_info->event_stream) {
            uint32_t now = (SHAL_SYSTEM::millis32() - timeout) / 1000;
            if (lastime != now) {
                lastime = now;
                SHAL_SYSTEM::printf("%sTimeLeft: %d POLLCNT CNT: %d rlen:%d ( CLID: %d ) fd: %d Address: %s:%d %s\n", COLOR_PRINTF_RED(1), (int)((TIMEOUT_FIREPROC * FIREPROC_POLLIN_INTERVAL) / 1000) - now, pollcnt, rlen, netsocket_info->clid, netsocket_info->connfd, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port), COLOR_PRINTF_RESET);
            }
            pollcnt++;
        }
    }

    if (pollcnt >= TIMEOUT_FIREPROC) {
        SHAL_SYSTEM::printf("TIMEOUT ( CLID: %d ) fd: %d Address: %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port));
    }

    if (rlen != 0) {
        SHAL_SYSTEM::printf("RLEN: %d ( CLID: %d ) fd: %d Address: %s:%d\n\n", rlen, netsocket_info->clid, netsocket_info->connfd, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port));
    }

    if (!netsocket_info->is_attached) {
        SHAL_SYSTEM::printf("CLOSE FORCED - rlen:%d ( CLID: %d ) fd: %d Address: %s:%d\n\n", rlen, netsocket_info->clid, netsocket_info->connfd, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port));
    }

    THREAD_TRY_LOCK_BLOCK(process_mutex);

    SHAL_SYSTEM::printf("Closed connection ( CLID: %d ) fd: %d Address: %s:%d\n\n", netsocket_info->clid, netsocket_info->connfd, inet_ntostr((uint32_t)netsocket_info->clientaddr.sin_addr.s_addr).c_str(), ntohs(netsocket_info->clientaddr.sin_port));

#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
    SSL_shutdown(netsocket_info->ssl);
#endif

    shutdown(netsocket_info->connfd, SHUT_WR);
    close(netsocket_info->connfd);

    cli_count--;
    _delete_client_from_frontend(_endpoint, netsocket_info->clid, (unsigned int)cli_count);
    client_slot_delete(netsocket_info->clid);

    THREAD_UNLOCK(process_mutex);
}

bool UR_V16X_Posix::poll_in(int fd, uint32_t timeout_ms)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000UL;

    if (select(FD_SETSIZE, &fds, NULL, NULL, &tv) != 1) {
        return false;
    }

    return true;
}

bool UR_V16X_Posix::_poll_out(int fd, uint32_t timeout_ms)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000UL;

    if (select(FD_SETSIZE, NULL, &fds, NULL, &tv) != 1) {
        return false;
    }
    return true;
}

int UR_V16X_Posix::process(int fd, struct sockaddr_in *_clientaddr)
{
    http_request_t req;
    char filenametmp[MAX_BUFF] = {0};
    char querytmp[MAX_BUFF] = {0};
    int cgi_query = 0;
    char *query_string = nullptr;
    UR_V16X_DeepService deepsrv;

    memset(&req, 0, sizeof(http_request_t));
#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("parse init...\n");
    fflush(stdout);
#endif // V16X_DEBUG

    int ret = parse_request(fd, &req);
#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("\treq.filename: %s REQ CGI: %s queryP: NONE Addr:%s:%d\n", req.filename, filenametmp, inet_ntostr((uint32_t)_clientaddr->sin_addr), ntohs(_clientaddr->sin_port));
#endif // V16X_DEBUG

    if (ret || (strlen(req.filename) <= 0)) {
        return ret;
    }

    memcpy(filenametmp, req.filename, sizeof(req.filename));
#if V16X_DEBUG >= 1
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
        if (*query_string == 0) {
            *query_string = 0x20;
        }
    }

    int status = 200;
    char val1[20] = {0};
    char val2[20] = {0};
    int ffd;

    memcpy(querytmp, query_string, strlen(query_string));
    if (!cgi_query) {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\treq.filename: %s REQ CGI: %s queryP: NONE Addr:%s:%d\n", req.filename, filenametmp, inet_ntostr((uint32_t)_clientaddr->sin_addr), ntohs(_clientaddr->sin_port));
#endif // V16X_DEBUG
    } else {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\treq.filename: %s req PARAM FILENAME: %s [queryP]: %s Addr:%s:%d\n", req.filename, filenametmp, querytmp, inet_ntostr((uint32_t)_clientaddr->sin_addr), ntohs(_clientaddr->sin_port));
        //SHAL_SYSTEM::printf("\toffset: %d \n", (int)req.offset);
#endif // V16X_DEBUG

        char msg[MAX_BUFF + 50] = {0};
        if (strcmp(req.filename, "data") == 0) {
            status = 200;
            query_param_t params1[10];
            uint32_t ret1 = 0;
            int lenquery1 = strlen(query_string);
            char query_temp1[lenquery1 + 1] = {0};

            if (lenquery1 > 0) {
                memcpy(query_temp1, query_string, lenquery1);
                SHAL_SYSTEM::printf("%sDATA string prints%s\n", COLOR_PRINTF_WHITE(0), COLOR_PRINTF_RESET);
                ret1 = deepsrv.parse_query(query_temp1,'&', '=', params1, 10);
            }

            if (ret1 > 0) {
                for (uint32_t i = 0; i < ret1; i++) {
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
            uint32_t ret2 = 0;
            int lenquery2 = strlen(data_parsed.data);
            char query_temp2[lenquery2 + 1] = {0};

            if (lenquery2 > 0) {
                memcpy(query_temp2, data_parsed.data, lenquery2);
                SHAL_SYSTEM::printf("%sDATA PARSED STORED prints%s\n", COLOR_PRINTF_WHITE(0), COLOR_PRINTF_RESET);
                ret2 = deepsrv.parse_query(query_temp2,'&', '=', params2, 10);
            }

            if (ret2 > 0) {
                for (uint32_t i = 0; i < ret2; i++) {
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
            SHAL_SYSTEM::printf("length DATA: val1: %lu - val2: %lu\n", (long unsigned int)strlen(val1), (long unsigned int)strlen(val2));
#endif // V16X_DEBUG
            int32_t lenval = strlen(val1) + strlen(val2);
            if (strcmp(val1, val2) == 0 && lenval > 0) {
                sprintf(msg, "DATA_URL=%lu&%s&%s", (long unsigned int)(req.allsize + strlen(msg)), "OK=1", data_parsed.data);
#if V16X_DEBUG >= 3
                SHAL_SYSTEM::printf("DATA_URL CMP TRUE getlen: %lu - msg: %s\n", (long unsigned int)req.end, msg);
#endif // V16X_DEBUG
                handle_message_outhttp(fd, msg);
            } else {
                //sprintf(msg, "DATA_URL=%lu&%s", req.allsize + strlen(msg), "OK=0");
                //sprintf(msg, "retry:200\ndata:{\"len\":%lu,\"dat\":\"%s\"}\n\n", req.allsize + strlen(msg), "OK=0");
                sprintf(msg, "{\"len\":%lu,\"dat\":\"%s\"}", (long unsigned int)(req.allsize + strlen(msg)), "OK");
#if V16X_DEBUG >= 3
                SHAL_SYSTEM::printf("DATA_URL CMP FALSE getlen: %lu - msg: %s\n", (long unsigned int)req.end, msg);
#endif // V16X_DEBUG
                handle_message_outhttp(fd, msg);
            }
            deepsrv.destroy_qparams(params1, ret1);
            deepsrv.destroy_qparams(params2, ret2);

            return ret;
        } else {
            sprintf(msg, "STD_DATA_URL: %s - File: %s", querytmp, filenametmp);
#if V16X_DEBUG >= 3
            SHAL_SYSTEM::printf("\tMSG DATA: %s\n", msg);
#endif // V16X_DEBUG
        }

        if (strcmp(req.filename, "sds") == 0) {
            query_param_t split_querystr[10];
            char *ret_msg[1] = {0};
            char qstrtemp[strlen(query_string) + 1] = {0};
            memcpy(qstrtemp, query_string, strlen(query_string));

            uint32_t ret_params = deepsrv.parse_query(qstrtemp, '&', '=', &split_querystr[0], 10);

            SHAL_SYSTEM::printf("\nSDS QUERY Params GET count: %d qstrtemp: %s filenametmp: %s\n", ret_params, qstrtemp, filenametmp);
            deepsrv.print_query_params(split_querystr, ret_params);
            deepsrv.process_qparams(split_querystr, ret_params, ret_msg);
            deepsrv.destroy_qparams(split_querystr, ret_params);
            if (ret_msg[0] != NULL) {
                handle_message_outhttp(fd, ret_msg[0]);
                free(ret_msg[0]);
                return ret;
            }
            memset(msg, 0, MAX_BUFF + 50);
            sprintf(msg, "%s", "NONE");
            handle_message_outhttp(fd, msg);
            return ret;
        }

        log_access(status, _clientaddr, &req);
    }

    DIR* dir = opendir(req.filename);
    if (dir) {
        // Directory exists.
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("Directory exist: %s\n", req.filename);
#endif // V16X_DEBUG
        ffd = dirfd(dir);
    } else if (ENOENT == errno) {
        // Directory does not exist.
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("Directory NO exist: %s\n", req.filename);
#endif // V16X_DEBUG
        closedir(dir);
        ffd = open(req.filename, O_FLAG_RBINARY | O_CLOEXEC, 0);
    } else {
        // opendir() failed for some other reason.
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("Directory FAILED: %s\n", req.filename);
#endif // V16X_DEBUG
        closedir(dir);
        ffd = open(req.filename, O_FLAG_RBINARY | O_CLOEXEC, 0);
    }

    struct stat sbuf;

#if V16X_DEBUG >= 1
    int pid = getpid();
    SHAL_SYSTEM::printf("\n-----FFD------: %d PID:%d \n", ffd, pid);
#endif // V16X_DEBUG

    if (ffd <= 0) {
        status = 404;
        const char *msg = "File not found";
        client_error(fd, status, "Not found", msg);
    } else {
        fstat(ffd, &sbuf);
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("\n-----FSTAT------: %d MODE:%d \n", ffd, sbuf.st_mode);
#endif // V16X_DEBUG
        if (S_ISREG(sbuf.st_mode)) {
            if (req.end == 0) {
                req.end = sbuf.st_size;
            }

            if (req.offset > 0) {
                status = 206;
            }

            serve_static(fd, ffd, _clientaddr, &req, sbuf.st_size);

            if (cgi_query) {
                query_param_t params3[10];
                uint32_t ret3 = 0;
                int lenquery3 = strlen(query_string);
                char query_temp3[lenquery3 + 1] = {0};
                if (lenquery3 > 0) {
                    memcpy(query_temp3, query_string, lenquery3);
                    ret3 = deepsrv.parse_query(query_temp3,'&', '=', params3, 10);
                }

                if (ret3 > 0) {
                    for (uint32_t i = 0; i < ret3; i++) {
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
                deepsrv.destroy_qparams(params3, ret3);
            }
        } else if(dir) {
            status = 200;

/*
            char dirtmp[256] = {0};
            memcpy(dirtmp, req.filename, strlen(req.filename));

            if (dirtmp[strlen(dirtmp) - 1] != '/') {
                dirtmp[strlen(dirtmp)] = '/';
            } else {
                //memset(dirtmp, 0, sizeof(dirtmp));
            }
*/

            handle_directory_request(fd, ffd, req.filename);
            char msgtmp[MAX_BUFF + 50] = {0};
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

    if (dir) {
        closedir(dir);
    } else if (ffd > 0) {
        close(ffd);
    }

    log_access(status, _clientaddr, &req);
    return ret;
}

int UR_V16X_Posix::_encode_hybi(char const *src, size_t srclength, char *target, size_t targsize, unsigned int opcode)
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
    }

    if (opcode & OPCODE_TEXT) {
        len = ur_crypton.b64_enc(target+payload_offset, targsize-payload_offset, src, srclength);
    } else {
        memcpy(target+payload_offset, src, srclength);
        len = srclength;
    }

    if (len < 0) {
        return len;
    }

    return len + payload_offset;
}

int UR_V16X_Posix::_decode_hybi(char *src, size_t srclength, char *target, int targsize, unsigned int *opcode, unsigned int *left)
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

    if (srclength < 5) {
        return -1;
    }

    *left = srclength;
    frame = src;

    uint32_t tgtcnt = 0;
    while (1) {
        tgtcnt++;
        // Need at least two bytes of the header
        // Find beginning of next frame. First time hdr_length, masked and
        // payload_length are zero
        frame += hdr_length + 4 * masked + payload_length;

        if (frame > src + srclength) {
            break;
        }
        remaining = (src + srclength) - frame;
        if (remaining < 2) {
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
        } else if (payload_length == 126) {
            payload_length = (frame[2] << 8) + frame[3];
            hdr_length = 4;
        } else {
            return -1;
        }
        if ((hdr_length + 4*masked + payload_length) > remaining) {
            continue;
        }

        payload = frame + hdr_length + 4 * masked;

        if (*opcode != OPCODE_TEXT && *opcode != OPCODE_BINARY) {
            continue;
        }

        if (payload_length == 0) {
            continue;
        }

        if ((payload_length > 0) && (!masked)) {
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
            len = ur_crypton.b64_dec(target+target_offset, payload, targsize);
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
        memmove(&target[0], src + offsetcp, payload_length);
    }

    target[payload_length + 1] = '\0';
    *left = remaining;
    if (*opcode & OPCODE_TEXT) {
        target_offset = payload_length;
    }

    return target_offset;
}

uint8_t UR_V16X_Posix::parse_request(int fd, http_request_t *req)
{
    data_io_t dat_io;
    char buf[MAX_BUFF] = {0};
    char method[MAX_BUFF] = {0};
    char uri[MAX_BUFF] = {0};
    req->offset = 0;
    req->end = 0;
    int closed = 0;
    UR_V16X_DeepService deepsrv;

    memset(req->filename, 0, MAX_BUFF);
    io_data_init(&dat_io, fd);
    io_data_read(&dat_io, buf, MAX_BUFF, &closed);

    if (closed || dat_io.io_cnt <= 0) {
        return closed;
    }

    query_param_t split_params[20];
    uint32_t ret_params = deepsrv.parse_query(buf, 0x0A, 0, split_params, 20);
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("%sEvents prints%s %s\n", COLOR_PRINTF_WHITE(1), COLOR_PRINTF_RESET, buf);
    deepsrv.print_query_params(split_params, ret_params);
#endif
    uint32_t idx_param;

    if (deepsrv.has_key(split_params, 1, ret_params, "event-stream", idx_param)) {
        char bufevt[MAX_BUFF] = {0};
        _set_client_event_stream(fd, true);
        sprintf(bufevt, "HTTP/1.1 %d %s\r\n", 200, "OK");
        sprintf(bufevt + strlen(bufevt), "Server:%s\r\n", SERVER_VERSION);
        sprintf(bufevt + strlen(bufevt), "Access-Control-Allow-Origin:%s\r\n", "*");
        sprintf(bufevt + strlen(bufevt), "Cache-Control:no-cache\r\n");
        sprintf(bufevt + strlen(bufevt), "%s", "Content-Type:text/event-stream\r\n");
        sprintf(bufevt + strlen(bufevt), "Date:%s\r\n\r\n", SHAL_SYSTEM::get_date().c_str());
        writen(fd, bufevt, strlen(bufevt));

        SHAL_SYSTEM::printf("\n++++++++Event stream parsed!++++++++\n\n");

        req->filename[0] = '\0';
        return closed;
    }

    if (deepsrv.has_key(split_params, 1, ret_params, "WebSocket-Key", idx_param)) {
        _set_client_event_websocket(fd, true);
        char bufevt[MAX_BUFF] = {0};
        char digest[MAX_BUFF] = {0};
        char b64enc[MAX_BUFF] = {0};
        char field[MAX_BUFF] = {0};
        char fldval[MAX_BUFF] = {0};

        sscanf(split_params[idx_param].key, "%s %s", field, fldval);
        //SHAL_SYSTEM::printf("field [ %s ] fldval: [ %s ] key: [ %s ]\n", field, fldval, split_params[idx_param].key);

        sprintf(fldval + strlen(fldval), "%s", MAGIC_WEBSOCKET_KEY);

        ur_crypton.sha1_apply((const unsigned char*)fldval, (unsigned char*)digest);
        ur_crypton.b64_enc(b64enc, MAX_BUFF, digest, strlen(digest));

        SHAL_SYSTEM::printf("\n++++++++Web Socket parsed!++++++++\n");
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\nfldval: %s++++++++\n", fldval);
        SHAL_SYSTEM::printf("b64: %s b64enc_len: %lu digest_len: %lu\n", b64enc, (long unsigned int)strlen(b64enc), (long unsigned int)strlen(digest));
#endif // V16X_DEBUG
        sprintf(bufevt, "HTTP/1.1 %d Switching Protocols\r\n", 101);
        sprintf(bufevt + strlen(bufevt), "Server:%s\r\n", SERVER_VERSION);
        sprintf(bufevt + strlen(bufevt), "Connection:%s\r\n", "Upgrade");
        //sprintf(bufevt + strlen(bufevt), "Access-Control-Allow-Origin:%s\r\n", "*");
        sprintf(bufevt + strlen(bufevt), "Content-type:%s\r\n", "application/octet-stream");
        sprintf(bufevt + strlen(bufevt), "%s", "Upgrade:websocket\r\n");
        sprintf(bufevt + strlen(bufevt), "Date:%s\r\n", SHAL_SYSTEM::get_date().c_str());
        sprintf(bufevt + strlen(bufevt), "Sec-WebSocket-Accept:%s\r\n\r\n", b64enc);
        writen(fd, bufevt, strlen(bufevt));

#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("digest: ");
        for (uint16_t i = 0; i < strlen((char*)digest); i++) {
            SHAL_SYSTEM::printf("0x%2x ", (uint8_t)digest[i]);
        }
        SHAL_SYSTEM::printf("\n\n");
#endif
        req->filename[0] = '\0';
        return  closed;
    }

    if (deepsrv.has_key(split_params, 1, ret_params, "Range", idx_param)) {
        long long int offset;
        char field[MAX_BUFF] = {0};
        sscanf(split_params[idx_param].key,"%s bytes=%lld-", field, &offset);
        req->offset = (off_t)offset;
    }

    if (deepsrv.has_key(split_params, 1, ret_params, "Connection", idx_param)) {
        char field[MAX_BUFF] = {0};
        sscanf(split_params[idx_param].key,"%s %s", field, field);
        if (strstr(field, "Keep-Alive")) {
            netsocket_inf_t *client = _get_client(fd);
            if (client != NULL) {
                client->keep_alive = true;
            }
        }
    }

    deepsrv.destroy_qparams(split_params, ret_params);

    sscanf(buf, "%s %s", method, uri);

    if (closed) {
        return closed;
    }

    char buftmp[MAX_BUFF] = {0};
    char wsdec[MAX_BUFF] = {0};
    unsigned int opcode, left;

    int len = 0;

    memcpy(buftmp, buf, dat_io.io_cnt);
    len = _decode_hybi(buftmp, dat_io.io_cnt, wsdec, MAX_BUFF, &opcode, &left);

    if (left == 0) {
        test_s tstwsocknativ;
        memset(&tstwsocknativ, 0, sizeof(test_s));
        memcpy(&tstwsocknativ, &wsdec[0], sizeof(test_s));
        SHAL_SYSTEM::printf("%s V16X decoded websocket msg:%s %s d1: %d d2: %d d3: %d rawlen: %d lenret: %d leftlen: %d \n", COLOR_PRINTF_PURPLE(1), COLOR_PRINTF_RESET, \
                            wsdec, tstwsocknativ.data1, tstwsocknativ.data2, tstwsocknativ.data3, dat_io.io_cnt, len, left);
    }

    if (strstr(wsdec, "V16X") && ((len - 5) > 1)) {
        _set_client_event_websocket(fd, true);
        char query_str[MAX_BUFF] = {0};
        char *ret_msg[1] = {0};
        query_param_t split_querystr[10];

        memcpy(&query_str[0], &wsdec[5], len - 5);

        ret_params = deepsrv.parse_query_bin(query_str, (uint32_t)(len - 5), '&', '=', split_querystr, 10);

        SHAL_SYSTEM::printf("\nSDS SOCKET Params GET count: %d query_str: %s\n", ret_params, query_str);
        deepsrv.print_query_params(split_querystr, ret_params);
        deepsrv.process_qparams(split_querystr, ret_params, ret_msg);
        deepsrv.destroy_qparams(split_querystr, ret_params);

        if (ret_msg[0] != NULL) {
            free(ret_msg[0]);
        }

#if V16X_DEBUG >= 2
        netsocket_inf_t *client = _get_client(fd);
        if (client != NULL) {
            SHAL_SYSTEM::printf("V16X METHOD msg: %s len: %d ( CLID: %d ) fd: %d Address:  %s:%d closed: %d\n\n", wsdec, len, client->clid, client->connfd, inet_ntostr((uint32_t)client->clientaddr.sin_addr.s_addr).c_str(), ntohs(client->clientaddr.sin_port), closed);
        }
#endif // V16X_DEBUG
        return closed;
    }  else if (strstr(method, "HEAD")) {
        netsocket_inf_t *client = _get_client(fd);
        if (client != NULL) {
            client->method_head = true;
        }
    } else if (!strstr(method, "GET")) {
#if V16X_DEBUG >= 2
        netsocket_inf_t *client = _get_client(fd);
#endif // V16X_DEBUG
        if (strlen(method) > 0) {
#if V16X_DEBUG >= 2
            if (client != NULL) {
                SHAL_SYSTEM::printf("ERROR METHOD msg: %s wsdec: %s len: %d ( CLID: %d ) fd: %d Address:  %s:%d closed: %d\n\n", method, wsdec, len, client->clid, client->connfd, inet_ntostr((uint32_t)client->clientaddr.sin_addr.s_addr).c_str(), ntohs(client->clientaddr.sin_port), closed);
            }
#endif // V16X_DEBUG
        }
#if V16X_DEBUG >= 2
        if (client != NULL) {
            SHAL_SYSTEM::printf("NO GET METHOD ( CLID: %d ) fd: %d Address:  %s:%d closed: %d\n\n", client->clid, client->connfd, inet_ntostr((uint32_t)client->clientaddr.sin_addr.s_addr).c_str(), ntohs(client->clientaddr.sin_port), closed);
        }
#endif // V16X_DEBUG
        closed = 1;
        return closed;
    }

    req->allsize = (size_t)dat_io.io_cnt;
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

void UR_V16X_Posix::handle_message_outhttp(int fd, const char *longmsg)
{
    char buf[1024];
    memset(buf, 0, 1024);

    if (!_has_client_events(fd)) {
        sprintf(buf, "HTTP/1.1 %d %s\r\n", 200, "OK");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
        sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
        sprintf(buf + strlen(buf), "Cache-Control:no-cache\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: no-store\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: max-age=0, must-revalidate\r\n");
        //sprintf(buf + strlen(buf), "Clear-Site-Data: \"cache\"\r\n");
        sprintf(buf + strlen(buf), "%s", "Content-Type:text/plain;charset=UTF-8\r\n");
        sprintf(buf + strlen(buf), "Date:%s\r\n", SHAL_SYSTEM::get_date().c_str());
        sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", (long unsigned int)strlen(longmsg));
        writen(fd, buf, strlen(buf));
        //sprintf(buf + strlen(buf), "%s", longmsg);
        uint64_t lenlng = strlen(longmsg);
        writen(fd, longmsg, lenlng);
    }
}

void UR_V16X_Posix::log_access(int status, struct sockaddr_in *c_addr, http_request_t *req)
{
#if V16X_DEBUG >= 3
    char filetmp[MAX_BUFF] = {0};
    memcpy(filetmp, req->filename, sizeof(req->filename));
    SHAL_SYSTEM::printf("---[ LOGACCESS %s:%d Status:%d - %s ]---\n\n", inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), status, filetmp);
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
    //sprintf(buf + strlen(buf), "Cache-Control: no-store\r\n");
    //sprintf(buf + strlen(buf), "Cache-Control: max-age=0, must-revalidate\r\n");
    //sprintf(buf + strlen(buf), "Clear-Site-Data: \"cache\"\r\n");
    sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
    sprintf(buf + strlen(buf), "Date:%s\r\n", SHAL_SYSTEM::get_date().c_str());
    sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", (long unsigned int)strlen(longmsgtmp));
    writen(fd, buf, strlen(buf));
    sprintf(buf, "%s", longmsgtmp);
    writen(fd, buf, strlen(buf));

    shutdown(fd, SHUT_WR);

/*
    netsocket_inf_t *client = _get_client(fd);
    if (client != NULL) {
        THREAD_TRY_LOCK_BLOCK(clients_mutex);
        client->is_attached = false;
        THREAD_UNLOCK(clients_mutex);
    }
*/
}

void UR_V16X_Posix::serve_static(int out_fd, int in_fd, struct sockaddr_in *c_addr, http_request_t *req, size_t total_size)
{
    char buf[MAX_BUFF] = {0};
    int status;

    char filetmp[MAX_BUFF] = {0};
    memcpy(filetmp, req->filename, strlen(req->filename));

    if (strstr(filetmp, "v16x")) {
        status = 404;
        const char *msg = "File not found";
        client_error(out_fd, status, "Not found", msg);
        return;
    }

    netsocket_inf_t *client = _get_client(out_fd);
    if (client == NULL) {
        return;
    }

    if ((size_t)req->offset > req->end) {
        status = 400;
        const char *msg = "Offset file error";
        client_error(out_fd, status, "Error", msg);
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("\t[ Offset > end ] #1 fd:%d offset: %ld end: %lu diff: %ld - %s Addr:%s:%d clid:%d\n", in_fd, (long int)req->offset, (long unsigned int)req->end, (long int)req->end - (long int)req->offset, filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
#endif
        return;
    }

#if defined(__MSYS__) || defined(__MINGW32__) || (CRYPTON_TYPE == CRYPTON_OPENSSL)
    //int64_t end_rng_siz = MAX_BUFF * 512 * 1;
    int64_t end_rng_siz = 2048;
#endif

    if (req->offset > 0) {
        sprintf(buf, "%s", "HTTP/1.1 206 Partial Content\r\n");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
        sprintf(buf + strlen(buf), "Content-Range:bytes %lu-%lu/%lu\r\n", (long unsigned int)req->offset, (long unsigned int)total_size - 1, (long unsigned int)total_size);
        sprintf(buf + strlen(buf), "Access-Control-Allow-Origin:%s\r\n", "*");
        //sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
        sprintf(buf + strlen(buf), "Connection:%s\r\n", "Keep-Alive");
        sprintf(buf + strlen(buf), "Keep-Alive:%s,%s\r\n", "timeout=10", "max=30");
        //sprintf(buf + strlen(buf), "Cache-Control:max-age=0,stale-while-revalidate=30\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: no-cache\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: no-store\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: max-age=0, must-revalidate\r\n");
        //sprintf(buf + strlen(buf), "Clear-Site-Data: \"cache\"\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control:public,max-age=315360000\r\nExpires:Thu,31 Dec 2037 23:55:55 GMT\r\n");
        sprintf(buf + strlen(buf), "Content-type:%s\r\n", get_mime_type(filetmp));
        sprintf(buf + strlen(buf), "Date:%s\r\n", SHAL_SYSTEM::get_date().c_str());
        sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", (long unsigned int)total_size - (long unsigned int)req->offset);

    } else {
        sprintf(buf, "%s", "HTTP/1.1 200 OK\r\nAccept-Ranges:bytes\r\n");
        //sprintf(buf, "%s", "HTTP/1.1 200 OK\r\n");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
        sprintf(buf + strlen(buf), "Access-Control-Allow-Origin:%s\r\n", "*");
        //sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
        sprintf(buf + strlen(buf), "Connection:%s\r\n", "Keep-Alive");
        sprintf(buf + strlen(buf), "Keep-Alive:%s,%s\r\n", "timeout=10", "max=30");
        sprintf(buf + strlen(buf), "Cache-Control:max-age=0,stale-while-revalidate=30\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: max-age=0, must-revalidate\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: no-store\r\n");
        //sprintf(buf + strlen(buf), "Clear-Site-Data: \"cache\"\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control: no-cache\r\n");
        //sprintf(buf + strlen(buf), "Cache-Control:public,max-age=315360000\r\nExpires:Thu,31 Dec 2037 23:55:55 GMT\r\n");
        sprintf(buf + strlen(buf), "Content-type:%s\r\n", get_mime_type(filetmp));
        sprintf(buf + strlen(buf), "Date:%s\r\n", SHAL_SYSTEM::get_date().c_str());
        sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", (long unsigned int)total_size);
    }

    writen(out_fd, buf, strlen(buf));

    int64_t offset = (int64_t)req->offset;
    int64_t offsettmp = (int64_t)req->offset;
    (void)offset;

#if V16X_DEBUG >= 1
    int64_t end = (int64_t)req->end;
#endif // V16X_DEBUG

    if (client->method_head) {
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("Head received! fd:%d offset: %ld offsettmp: %ld end: %ld diff: %ld - %s Addr:%s:%d clid:%d\n", in_fd, (long int)offset, (long int)offsettmp, (long int)end, (long int)(end - offset), filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
        fflush(stdout);
#endif // V16X_DEBUG
        client->is_attached = false;
        return;
    }
#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("\t[ SERVING STATIC INIT ] #1 fd:%d offset: %ld end: %lu diff: %ld - %s Addr:%s:%d clid:%d\n", in_fd, (long int)req->offset, (long unsigned int)req->end, (long int)(req->end - req->offset), filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
    fflush(stdout);
#endif // V16X_DEBUG

/*
    if (offset == 0) {
        lseek(in_fd, offsettmp, SEEK_SET);
        int64_t readsize = 0;
        char buftmp[MAX_BUFF * 64];
        readsize = (int64_t)read(in_fd, buftmp, MAX_BUFF * 64);
        if (readsize <= 0) {
            SHAL_SYSTEM::printf("\tServing static RETURN READSIZE #2 fd:%d offsettmp: %d offset: %d end: %d diff: %d readsize: %lld - %s Addr:%s:%d clid: %d\n", in_fd, (int)offsettmp, (int)offset, (int)end, (int)(end - offsettmp), readsize, filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
            SHAL_SYSTEM::printf("READSIZE <= 0\n");
            return;
        }

        offset = (int64_t)write(out_fd, buftmp, readsize);
        if (offset <= 0) {
            SHAL_SYSTEM::printf("OFFSET <= 0\n");
            return;
        }

        SHAL_SYSTEM::printf("\tServing static FIRST RESPONSE #2 fd:%d offsettmp: %d sentdat: %d end: %d diff: %d readsize: %lld - %s Addr:%s:%d clid: %d\n", in_fd, (int)offsettmp, (int)offset, (int)end, (int)(end - offsettmp), readsize, filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
        offsettmp += offset;

        //client->sending = 1;
        //client->is_attached = false;
        //return;
    }
*/

#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("\tServing static #1 fd:%d offset: %ld offsettmp: %ld end: %ld diff: %ld - %s Addr:%s:%d clid:%d\n", in_fd, (long int)offset, (long int)offsettmp, (long int)end, (long int)(end - offset), filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
#endif // V16X_DEBUG

#ifdef __unix__
    off_t offtmp = (off_t)offsettmp;
#endif // __unix__
    while(offsettmp < (int64_t)total_size) {
#if defined(__MSYS__) || defined(__MINGW32__) || (CRYPTON_TYPE == CRYPTON_OPENSSL)
        //if (poll_in(out_fd, UPDATE_POLLIN_INTERVAL)) {
        lseek(in_fd, offsettmp, SEEK_SET);

        int64_t readsize = 0;
        char buftmp[end_rng_siz] = {0};

        readsize = (int64_t)read(in_fd, buftmp, end_rng_siz);

        if (readsize < 0) {
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("\tServing static BREAK OFFSET #2 fd:%d offsettmp: %d offset: %d end: %d diff: %d readsize: %ld - %s Addr:%s:%d clid: %d\n", in_fd, (int)offsettmp, (int)offset, (int)end, (int)(end - offsettmp), (long int)readsize, filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
#endif // V16X_DEBUG
            break;
        }

        offset = (int64_t)writen(out_fd, buftmp, readsize);
        if (offset <= 0) {
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("\tServing static BREAK WRITEN #3 fd:%d offsettmp: %d sentdat: %d end: %d diff: %d readsize: %ld - %s Addr:%s:%d clid: %d\n", in_fd, (int)offsettmp, (int)offset, (int)end, (int)(end - offsettmp), (long int)readsize, filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
#endif // V16X_DEBUG
            break;
        }

        offsettmp += offset;
#if V16X_DEBUG >= 1
        if ((end - offsettmp) == 0) {
            SHAL_SYSTEM::printf("\tServing static END #4 fd:%d offsettmp: %d sentdat: %d end: %d diff: %d readsize: %ld - %s Addr:%s:%d clid: %d\n", in_fd, (int)offsettmp, (int)offset, (int)end, (int)(end - offsettmp), (long int)readsize, filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
        }
#endif // V16X_DEBUG

        //}
#else
        if(sendfile(out_fd, in_fd, &offtmp, (size_t)total_size) < 0) {
            offsettmp = offtmp;
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("\tServing static SENDIFLE #2 fd:%d offsettmp: %lld offset: %lld end: %lld diff: %lld - %s Addr:%s:%d clid: %d\n", in_fd, (long long int)offsettmp, (long long int)offset, (long long int)end, (long long int)(end - offsettmp), filetmp, inet_ntostr((uint32_t)c_addr->sin_addr), ntohs(c_addr->sin_port), (int)client->clid);
            fflush(stdout);
#endif // V16X_DEBUG
            break;
        }
        offsettmp = (int64_t)offtmp;
        //break;
#endif // __MSYS__
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
    char *norm_filename = NULL;
    char *norm_filenameptr = NULL;

    sprintf(bufdir, "HTTP/1.1 %d %s\r\n", 200, "OK");
    sprintf(bufdir + strlen(bufdir), "Server:%s\r\n", SERVER_VERSION);
    sprintf(bufdir + strlen(bufdir), "%s", "Content-Type:text/html;charset=UTF-8\r\n");
    sprintf(bufdir + strlen(bufdir), "Cache-Control:no-cache\r\n");
    sprintf(bufdir + strlen(bufdir), "Access-Control-Allow-Origin:%s\r\n", "*");
    //sprintf(bufdir + strlen(bufdir), "Cache-Control: no-store\r\n");
    //sprintf(bufdir + strlen(bufdir), "Cache-Control: max-age=0, must-revalidate\r\n");
    //sprintf(bufdir + strlen(bufdir), "Clear-Site-Data: \"cache\"\r\n");
    sprintf(bufdir + strlen(bufdir), "Connection:%s\r\n", "close");
    sprintf(bufdir + strlen(bufdir), "Date:%s\r\n\r\n", SHAL_SYSTEM::get_date().c_str());
    //sprintf(bufdir + strlen(bufdir), "Content-length:%lu\r\n\r\n", strlen(longmsg));

    writen(out_fd, bufdir, strlen(bufdir));
    memset(&bufdir[0], 0, max_buff);

    sprintf(longmsg, "%s", "<html><head><meta charset=\"UTF-8\"/><style>");
    sprintf(longmsg + strlen(longmsg), "%s", "body{font-family:monospace;font-size:13px;}");
    sprintf(longmsg + strlen(longmsg), "%s", "td {padding: 1.5px 6px;}");
    sprintf(longmsg + strlen(longmsg), "%s", "</style></head><body><table>");
    writen(out_fd, longmsg, strlen(longmsg));
    memset(&longmsg[0], 0, max_buff);

    bool has_slashdirtail = true;
    char dirtmp[256] = {0};
    memcpy(dirtmp, filename, strlen(filename));

    if (dirtmp[strlen(dirtmp) - 1] != '/') {
        dirtmp[strlen(dirtmp)] = '/';
        has_slashdirtail = false;
    }

    norm_filenameptr = new char[strlen(filename) + 1];
    norm_filename = norm_filenameptr;
    memset(norm_filename, 0, strlen(filename) + 1);
    memcpy(norm_filename, filename, strlen(filename));
#if V16X_DEBUG >= 3
    SHAL_SYSTEM::printf("handling dir before: norm: %s [len: %d] - dir: %s - dirtmp: %s\n", norm_filename, (int)strlen(norm_filename), filename, dirtmp);
#endif // V16X_DEBUG

    norm_filename = norm_filename + strlen(norm_filename);
    norm_filename--;
    while((*norm_filename != '/') && (*norm_filename != '\0')) {
        norm_filename--;
    }

    if ((*norm_filename == '/') && (!has_slashdirtail)) {
        *norm_filename = '\0';
        norm_filename++;
    }

#if defined(__MINGW32__ ) || defined(__ANDROID__)
    DIR *dir = opendir(dirtmp);
#else
    DIR *dir = fdopendir(dir_fd);
#endif // __MINGW32__

    if (dir == NULL) {
        SHAL_SYSTEM::printf("handling dir ERROR open dir: %s\n", dirtmp);
    }

    struct dirent *dp = NULL;
    int ffd = 0;
    int filecnt = 0;
    int dircnt = 0;
#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("handling dir after: norm: %s [len: %d] - dir: %s - dirtmp: %s\n", norm_filename, (int)strlen(norm_filename), filename, dirtmp);
#endif // V16X_DEBUG

    while(dir != NULL) {
        dp = readdir(dir);
        if (dp == NULL) {
            break;
        }

        if (strlen(longmsg) >= ((max_buff) - 256)) {
            break;
        }

        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
            continue;
        }

#if defined(__MINGW32__ ) || defined(__ANDROID__)
        char fdirname[256] = {0};
        sprintf(fdirname, "%s%s", dirtmp, dp->d_name);
#if defined(__MINGW32__ )
        ffd = open(fdirname, O_FLAG_RBINARY, 0);
#endif // defined
#else
        ffd = openat(dir_fd, dp->d_name, O_FLAG_RBINARY);
#endif // __MINGW32__
        if (ffd < 0) {
            perror(dp->d_name);
            continue;
        }

#if defined(__MINGW32__ ) || defined(__ANDROID__)
        stat(fdirname, &statbuf);
#else
        fstat(ffd, &statbuf);
#endif // __MINGW32__
        strftime(m_time, sizeof(m_time), "%Y-%m-%d %H:%M", localtime((const time_t*)&statbuf.st_mtime));

        format_size(size, &statbuf, &filecnt, &dircnt);

        if (S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)) {
            const char d1 = S_ISDIR(statbuf.st_mode) ? '/' : ' ';
            char dname[256] = {0};
            char color[20] = {0};

            memset(&dname[0], 0, sizeof(dname));
            if (d1 == '/') {
                 sprintf(&color[0], "%s", "color:#e50000");
            }
            if (!strstr(dp->d_name, "v16x")) {
                if (!has_slashdirtail) {
                    sprintf(dname, "%s/", norm_filename);
                }

                if (strlen(norm_filename) == 0) {
                    sprintf(dname, "%s", dirtmp);
                }
                sprintf(dname + strlen(dname), "%s", dp->d_name);
                char lngmsg[MAX_BUFF] = {0};
                memset(lngmsg, 0, sizeof(lngmsg));
                sprintf(lngmsg, "<tr><td><a style='%s' href=\"%s%c\">%s%c</a></td><td>%s</td><td>%s</td></tr>",
                        color, dname, d1, dp->d_name, d1, m_time, size);
                writen(out_fd, lngmsg, strlen(lngmsg));
            } else {
                filecnt = filecnt - 1;
            }
        }

        if (ffd > 0) {
            close(ffd);
        }
    }

    if (dir) {
        closedir(dir);
    }

    sprintf(longmsg, "<td><br>Total items: %d Files: %d Dirs: %d</br></td></table><hr><center>%s</center></body></html>", filecnt + dircnt, filecnt, dircnt, SERVER_VERSION);
    sprintf(bufdir, "%s", longmsg);
    writen(out_fd, bufdir, strlen(bufdir));

    shutdown(out_fd, SHUT_WR);

/*
    netsocket_inf_t *client = _get_client(out_fd);
    if (client != NULL) {
        THREAD_TRY_LOCK_BLOCK(clients_mutex);
        client->is_attached = false;
        THREAD_UNLOCK(clients_mutex);
    }
*/
    delete[] norm_filenameptr;
}

ssize_t UR_V16X_Posix::io_data_read(data_io_t *data_iop, char *usrbuf, size_t maxlen, int *closed)
{
    memset(data_iop->io_buf, '\0', MAX_BUFF);
    memset(usrbuf, '\0', maxlen);
    data_iop->io_cnt = 0;

    int fd = data_iop->io_fd;
#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
    netsocket_inf_t *client = _get_client(data_iop->io_fd);
    if (client) {
        if (client->ssl != NULL) {
            data_iop->io_cnt = SSL_read(client->ssl, data_iop->io_buf, MAX_BUFF - 1);
            fd = SSL_get_fd(client->ssl);
#if V16X_DEBUG >= 2
            SHAL_SYSTEM::printf("----SSL FD: %d\n", fd);
#endif // V16X_DEBUG
        }
    }
#endif

    if (data_iop->io_cnt <= 0) {
        data_iop->io_cnt = recv(fd, data_iop->io_buf, MAX_BUFF - 1, MSG_DONTWAIT);
    }

#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("%sfunct-io %s 2 cnt:%s %d buf: %s\n", COLOR_PRINTF_RED(0), __func__, COLOR_PRINTF_RESET, data_iop->io_cnt, data_iop->io_buf);
/*
    SHAL_SYSTEM::printf("**********DATA INIT:\n\n");
    for (uint16_t i = 0; i < data_iop->io_cnt; i++) {
        SHAL_SYSTEM::printf("[0x%02x] ", (uint8_t)data_iop->io_buf[i]);
    }
    SHAL_SYSTEM::printf("\n**********DATA END:\n\n");
*/
#endif // V16X_DEBUG
    if (data_iop->io_cnt > 0) {
        memcpy(usrbuf, data_iop->io_buf, data_iop->io_cnt);
    }

    if ((errno == EAGAIN) && (data_iop->io_cnt > 0)) {
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("%sfunct-io %s 4 cnt:%s %d buf: %s\n", COLOR_PRINTF_RED(0), __func__, COLOR_PRINTF_RESET, data_iop->io_cnt, data_iop->io_buf);
#endif // V16X_DEBUG
        return data_iop->io_cnt;
    }

    if ((errno == EAGAIN) || (data_iop->io_cnt <= 0) || (errno == EINTR) || (errno == ECONNRESET) || (errno == EPIPE) || (errno == ETIMEDOUT)) {
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("%sfunct-io %s 3 cnt:%s %d - error: %d\n", COLOR_PRINTF_PURPLE(0), __func__, COLOR_PRINTF_RESET, data_iop->io_cnt, errno);
#endif // V16X_DEBUG
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

ssize_t UR_V16X_Posix::writen(int fd, const void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    const char *bufp = (char*)usrbuf;
    socklen_t clientlen;

    netsocket_inf_t *client = _get_client(fd);
    if (client == NULL) {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\n\n\tfunct %s WRITEN ERROR: %d on fd: %d!!!\n\n", __func__, errno, fd);
#endif
        return -1;
    }

    clientlen = sizeof(struct sockaddr);
    struct sockaddr addr = *(struct sockaddr*)&client->clientaddr;

    if (client->ssl == NULL) {
#if V16X_DEBUG >= 2 && (CRYPTON_TYPE == CRYPTON_OPENSSL)
        SHAL_SYSTEM::printf("\n\n\tfunct %s SSL ERROR: %d on fd: %d!!!\n\n", __func__, errno, fd);
        //return -1;
#endif // V16X_DEBUG
    }

#if (CRYPTON_TYPE == CRYPTON_OPENSSL)
    if (client->ssl != NULL) {
        n = SSL_write(client->ssl, bufp, nleft);
        return n;
    }
#endif

    while (nleft > 0) {
        uint32_t lapcnt = 0;
        if ((nwritten = sendto(fd, bufp, nleft, 0, &addr, clientlen)) <= 0) {
        //if ((nwritten = write(fd, bufp, nleft)) <= 0) {
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("\n\n\tfunct %s INTERRUPT CALL 1: %d !!!\n\n", __func__, errno);
#endif // V16X_DEBUG
            if ((errno == ECONNRESET) || (errno == EPIPE) || (errno == ETIMEDOUT)) {
                nwritten = 0;
#if V16X_DEBUG >= 1
                SHAL_SYSTEM::printf("\n\n\tfunct %s INTERRUPT CALL 2: %d !!!\n\n", __func__, errno);
#endif // V16X_DEBUG
                return n - nleft;
            }
            if (errno == EINTR) {
                nwritten = 0;
#if V16X_DEBUG >= 1
                SHAL_SYSTEM::printf("\n\n\tfunct %s INTERRUPT CALL 3: %d !!!\n\n", __func__, errno);
#endif // V16X_DEBUG
                return n - nleft;
            } else {
#if V16X_DEBUG >= 1
                SHAL_SYSTEM::printf("\n\n\tfunct %s INTERRUPT CALL 4: %d !!!\n\n", __func__, errno);
#endif // V16X_DEBUG
                return n - nleft;
            }
        }

        nleft -= nwritten;
        bufp += nwritten;

        // wait until send all data, otherwise return the amount data sent
        // if the timeout was reached.
        while (!_poll_out(fd, TIMEOUT_POLLOUT_INTERVAL)) {
            if (lapcnt >= LAP_TIMEOUT_WRITEN) {
                return n - nleft;
            }
            lapcnt++;
#if V16X_DEBUG >= 1
            SHAL_SYSTEM::printf("**** [WRITEN WAITING CLID] : %d  fd: %d Address:  %s:%d ****\n\n", client->clid, fd, inet_ntostr((uint32_t)client->clientaddr.sin_addr.s_addr).c_str(), ntohs(client->clientaddr.sin_port));
#endif // V16X_DEBUG
        }

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
            sprintf(buf, "%llu", (long long unsigned)size);
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

    int mtx = 0;
    if ((mtx = pthread_mutex_trylock(&process_mutex)) != 0) {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("%s - %s Mutex locked mtx: %d\n", SHAL_SYSTEM::get_date().c_str(), __func__, mtx);
#endif // V16X_DEBUG
        return;
    }

    char msg[MAX_BUFF] = {0};
    uint32_t now = SHAL_SYSTEM::millis32();

    if ((now - last) < PROCESS_EVENT_INTERVAL) {
        THREAD_UNLOCK(process_mutex);
        return;
    }

    last = SHAL_SYSTEM::millis32();
    netsocket_inf_t client;

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            client = *clients[i];
            if (client.event_stream) {
                if (!client.evtstr_ping) {
                    continue;
                }
                clients[i]->evtstr_ping = false;

                sock_testsse.data1 = sock_testsse.data1 + 10;
                sock_testsse.data2 = 7168;
                sock_testsse.data3 = 59;
                memset(msg, '\0', MAX_BUFF);
                char buftmp[MAX_BUFF] = {0};
                //memcpy(buftmp, &sock_testsse, sizeof(test_s));

                uint16_t lenb64 = ur_crypton.b64_enc(buftmp, MAX_BUFF, (const char*)&sock_testsse, sizeof(test_s));
                sprintf(msg, "retry:1000\nid:%d\ndata:{\"len\":%lu,\"clid\":\"%d\",\"fd\":%d,\"dat\":\"%s\",\"lenb64\":%d}\n\n", client.clid, (long unsigned int)sizeof(msg), (int)client.clid, client.connfd,  buftmp, lenb64);
                writen(client.connfd, msg, strlen(msg));
                clients[i]->event_stream = false;
                //shutdown(clients[i]->connfd, SHUT_RDWR);
            }

            if (client.event_websocket) {
                if (!client.evtwebsock_ping) {
                    continue;
                }

                sock_testweb.data1 = sock_testweb.data1 + 5;
                sock_testweb.data2 = 7936;
                sock_testweb.data3 = 22;
                clients[i]->evtwebsock_ping = false;
                //clients[i]->sending = 1;
                memset(msg, '\0', MAX_BUFF);
                char buftmp[MAX_BUFF] = {0};
                memcpy(buftmp, &sock_testweb, sizeof(test_s));
                //sprintf(buftmp, "{\"len\":%lu,\"clid\":\"%d\",\"fd\":%d}", (long unsigned int)sizeof(buftmp), (int)client.clid, client.connfd);
                int lenenc = _encode_hybi(buftmp, sizeof(test_s), msg, MAX_BUFF, OPCODE_BINARY);
                if (lenenc > 0) {
                    writen(client.connfd, msg, lenenc);

                    memset(msg, '\0', MAX_BUFF);
                    sprintf(buftmp, "{\"len\":%lu,\"clid\":\"%d\",\"fd\":%d}", (long unsigned int)sizeof(buftmp), (int)client.clid, client.connfd);
                    lenenc = _encode_hybi(buftmp, strlen(buftmp), msg, MAX_BUFF, OPCODE_TEXT);
                    //SHAL_SYSTEM::delay_ms(100);
                    writen(client.connfd, msg, lenenc);
                }
                //clients[i]->sending = 0;
            }
        }
    }
    THREAD_UNLOCK(process_mutex);
}

void UR_V16X_Posix::_client_transaction(TYPE_TRANSACTION_E typetr, events_transaction_t &data_transaction)
{
    netsocket_inf_t *client = _get_client(data_transaction.fd);

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
    THREAD_TRY_LOCK_BLOCK(clients_mutex);

    uint16_t clientpos = (uint16_t)-1;

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->connfd == fd) {
                clientpos = i;
                break;
            }
        }
    }
    THREAD_UNLOCK(clients_mutex);

    return (clientpos != (uint16_t)-1) ? clients[clientpos] : NULL;
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

    for (uint16_t i = 0; i < V16X_MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->clientaddr.sin_addr.s_addr == cliaddr.sin_addr.s_addr) {
                ret = true;
                break;
            }
        }
    }

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

int UR_V16X_Posix::_get_ip_host(char *host, int len)
{
#ifdef ENABLE_GET_IP_HOST
    struct addrinfo hints, *res;
    int errcode;
    char addrstr[100];
    void *ptr = NULL;

    memset(addrstr, 0, 100);
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo(host, NULL, &hints, &res);
    if (errcode != 0) {
        printf("getaddrinfo failed!\n");
        return -1;
    }

    printf("Host: %s\n", host);
    while (res) {
        inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 100);

        switch (res->ai_family) {
            case AF_INET:
                ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                ptr = &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
                break;
        }
        inet_ntop(res->ai_family, ptr, addrstr, 100);
        printf("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4, addrstr, res->ai_canonname);
        res = res->ai_next;
    }
    //if (strstr(host, "localhost")) {
        //sprintf(host, "%s", "127.0.0.1");
        //return 0;
    //}
    memcpy(host, addrstr, len);

	char hostbuffer[256];
	char *IPbuffer;
	struct hostent *host_entry;
	int hostname;

	// To retrieve hostname
	hostname = gethostname(hostbuffer, sizeof(hostbuffer));
	(void)hostname;
	//checkHostName(hostname);

	// To retrieve host information
	host_entry = gethostbyname(host);
	//checkHostEntry(host_entry);

	// To convert an Internet network
	// address into ASCII string
	IPbuffer = inet_ntostr((uint32_t)*((struct in_addr*)
						host_entry->h_addr_list[0]));

	printf("Hostname: %s\n", hostbuffer);
	printf("Host IP: %s\n", IPbuffer);
#endif // ENABLE_GET_IP_HOST
    return 0;
}
