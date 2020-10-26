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
    for (int i = 0; i < V16X_MAX_CLIENTS; ++i) {
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
}

void UR_V16X_Posix::update(void)
{
    if (poll_in(listenfd, 20)) {
        if (sig_evt) {
            return;
        }

        socklen_t clientlen = sizeof(clientaddr);
        SHAL_SYSTEM::printf("Waiting connection for client #%d\n\n", (unsigned int)cli_count + 1);
        fflush(stdout);

        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        int one = 1;
        int alive_one = 1;

        //struct timeval timeout;
        int timeout_tcp = 9000;
        int retries_tcp = 3;
        int interval_tcp = 2;
        int delay_idle_tcp = 5;
        //timeout.tv_sec = 9;
        //timeout.tv_usec = 0;
/*
        if (setsockopt (connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
            SHAL_SYSTEM::panic("setsockopt failed\n");

        if (setsockopt (connfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
            SHAL_SYSTEM::panic("setsockopt failed\n");
*/
        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (const void*)&one, sizeof(one));
        setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&one, sizeof(one));

        setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, (const void*)&alive_one, sizeof(alive_one)); // keep alive?

#ifndef _WIN32
        setsockopt(connfd, IPPROTO_TCP, TCP_KEEPIDLE, (const void*)&delay_idle_tcp, sizeof(delay_idle_tcp)); // delay idle
#endif // _WIN32
        setsockopt(connfd, SOL_TCP, TCP_KEEPCNT, (const void*)&retries_tcp, sizeof(retries_tcp)); // retries
        setsockopt(connfd, IPPROTO_TCP, TCP_KEEPINTVL, (const void*)&interval_tcp, sizeof(interval_tcp)); // interval
        setsockopt(connfd, SOL_TCP, TCP_USER_TIMEOUT, (const void*)&timeout_tcp, sizeof(timeout_tcp));

        /* Check if max clients is reached */
        if ((cli_count + 1) == V16X_MAX_CLIENTS) {
            SHAL_SYSTEM::printf("max clients reached, reject\n");
            fflush(stdout);
            close(connfd);
            return;
        }

        netsocket_inf_t *netsocket_info = (netsocket_inf_t *)malloc(sizeof(netsocket_inf_t));

        netsocket_info->clientaddr = clientaddr;
        netsocket_info->connfd = connfd;
        netsocket_info->clid = clid++;
        netsocket_info->is_attached = false;

        client_slot_add(netsocket_info);

        SHAL_SYSTEM::printf("Connected client ( CLID: %d ) Address:  %s : %d\n\n", netsocket_info->clid, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));
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
    for (int i = 0; i < V16X_MAX_CLIENTS; ++i) {
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
    for (int i = 0; i < V16X_MAX_CLIENTS; ++i) {
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
    for (int i = 0; i < V16X_MAX_CLIENTS; ++i) {
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
    for (int i = 0; i < V16X_MAX_CLIENTS; ++i) {
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
    netsocket_inf_t *netsocket_info = _get_next_unattached_client();
    if (netsocket_info == nullptr) {
        return;
    }

    int rlen = 0;
    netsocket_info->sending = 0;

    pthread_mutex_lock(&process_mutex);
    cli_count++;
    _copy_client_to_frontend(_endpoint, netsocket_info->clid, netsocket_info->is_attached, (unsigned int)cli_count);

#if V16X_DEBUG >= 1
    SHAL_SYSTEM::printf("FIRE_ISR: Start process ( CLID: %d )\n", netsocket_info->clid);
    fflush(stdout);
#endif

    pthread_mutex_unlock(&process_mutex);

    while(rlen == 0 && netsocket_info->is_attached) {
        if (poll_in(netsocket_info->connfd, 20)) {
            rlen = process(netsocket_info->connfd, &netsocket_info->clientaddr);    // Process all incomming data
        }
    }

    SHAL_SYSTEM::printf("Closed connection ( CLID: %d ) Address: %s : %d\n\n", netsocket_info->clid, inet_ntoa(netsocket_info->clientaddr.sin_addr), ntohs(netsocket_info->clientaddr.sin_port));
    fflush(stdout);

    close(netsocket_info->connfd);
    shutdown(netsocket_info->connfd, SHUT_WR);
    client_slot_delete(netsocket_info->clid);
    pthread_mutex_lock(&process_mutex);
    cli_count--;
    _delete_client_from_frontend(_endpoint, netsocket_info->clid, (unsigned int)cli_count);
    pthread_mutex_unlock(&process_mutex);
    free(netsocket_info);
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
    char filenametmp[MAX_BUFF];
    char querytmp[MAX_BUFF];
    int cgi_query = 0;
    char *query_string = nullptr;

    memset(req.filename, '\0', sizeof(req.filename));
#if V16X_DEBUG >= 3
    SHAL_SYSTEM::printf("parse init...\n");
    fflush(stdout);
#endif // V16X_DEBUG

    int ret = parse_request(fd, &req);
    if (ret) {
        return 1;
    }

    memcpy(filenametmp, req.filename, sizeof(req.filename));
#if V16X_DEBUG >= 3
    SHAL_SYSTEM::printf("\tINIT FILENAME %s\n", filenametmp);
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
    char val1[20];
    char val2[20];

    memcpy(querytmp, query_string, strlen(query_string));
    if (!cgi_query) {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\tREQ CGI: %s queryP: NONE Addr:%s:%d\n", filenametmp, inet_ntoa(clientaddr->sin_addr), ntohs(clientaddr->sin_port));
#endif // V16X_DEBUG
    } else {
#if V16X_DEBUG >= 2
        SHAL_SYSTEM::printf("\treq PARAM FILENAME: %s [queryP]: %s Addr:%s:%d\n", filenametmp, querytmp, inet_ntoa(clientaddr->sin_addr), ntohs(clientaddr->sin_port));
        //SHAL_SYSTEM::printf("\toffset: %d \n", (int)req.offset);
#endif // V16X_DEBUG

        char msg[MAX_BUFF];
        if (strcmp(req.filename, "data") == 0) {
            status = 200;
            query_param_t params1[10];
            int ret1 = 0;
            int lenquery1 = strlen(query_string);
            char query_temp1[lenquery1 + 1];

            if (lenquery1 > 0) {
                memcpy(query_temp1, query_string, lenquery1 + 1);
                ret1 = parse_query(query_temp1,'&', params1, 10);
            }

            if (ret1 > 0) {
                for (int i = 0; i < ret1; i++) {
                    char qmsg[MAX_BUFF];
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
            char query_temp2[lenquery2 + 1];

            if (lenquery2 > 0) {
                memcpy(query_temp2, data_parsed.data, lenquery2 + 1);
                ret2 = parse_query(query_temp2,'&', params2, 10);
            }

            if (ret2 > 0) {
                for (int i = 0; i < ret2; i++) {
                    char qmsg[MAX_BUFF];
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
                sprintf(msg, "DATA_URL=%lu&%s", req.allsize + strlen(msg), "OK=0");
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
                char query_temp3[lenquery3 + 1];
                if (lenquery3 > 0) {
                    memcpy(query_temp3, query_string, lenquery3 + 1);
                    ret3 = parse_query(query_temp3,'&', params3, 10);
                }

                if (ret3 > 0) {
                    for (int i = 0; i < ret3; i++) {
                        char qmsg[MAX_BUFF];
                        sprintf(qmsg, "KEY DATA: [ %s ]%-5.s\t- VAL: [ %s ]", params3[i].key, "", params3[i].val);
#if V16X_DEBUG >= 3
                        SHAL_SYSTEM::printf("\tSTORED DATA msg ********** %s\n", qmsg);
#endif
                    }
                }
                char msg[MAX_BUFF];
                memset(data_parsed.data, '\0', MAX_BUFF);
                sprintf(data_parsed.data, "%s", query_string);
                sprintf(msg, "STD_DATA_URL_STORED_OK: %s - File: %s", querytmp, filenametmp);
#if V16X_DEBUG >= 3
                SHAL_SYSTEM::printf("\tMSG STORED: %s\n", msg);
#endif // V16X_DEBUG
            }
        } else if(S_ISDIR(sbuf.st_mode)) {
            status = 200;
            handle_directory_request(fd, ffd, filenametmp);
            char msgtmp[MAX_BUFF];
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

int UR_V16X_Posix::parse_request(int fd, http_request_t *req)
{
    data_io_t dat_io;
    char buf[MAX_BUFF] = {0};
    char method[MAX_BUFF] = {0};
    char uri[MAX_BUFF] = {0};
    req->offset = 0;
    req->end = 0;
    int closed = 0;

    io_data_init(&dat_io, fd);
    io_data_read(&dat_io, buf, MAX_BUFF, &closed);

    sscanf(buf, "%s %s", method, uri);

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
        url_decode(filename, req->filename, strlen(filename) + 1);
    }

    return closed;
}

int UR_V16X_Posix::parse_query(char *query, char delimiter, query_param_t *params, int max_params)
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
		if (i > 0) {
			if ((params[i - 1].val = strchr(params[i - 1].key, '=')) != NULL) {
				*(params[i - 1].val)++ = '\0';
                char * pchar;
                pchar = strtok(params[i-1].val," ");
                while (pchar != NULL) {
                    pchar = strtok(NULL, " ");
                }
                params[i].val = pchar;
			}
		}
		i++;
	}

	/* Go back and split last param */
	if ((params[i - 1].val = strchr(params[i - 1].key, '=')) != NULL) {
		*(params[i - 1].val)++ = '\0';
	}

	return i;
}

void UR_V16X_Posix::handle_message_outhttp(int fd, char *longmsg)
{
    char buf[MAX_BUFF];
    memset(buf, '\0', MAX_BUFF);

    sprintf(buf, "HTTP/1.1 %d %s\r\n", 200, "OK");
    sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
    sprintf(buf + strlen(buf), "%s", "Content-Type:text/plain;charset=UTF-8\r\n");
    //sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
    sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", strlen(longmsg));
    sprintf(buf + strlen(buf), "%s", longmsg);
    writen(fd, buf, strlen(buf));
}

void UR_V16X_Posix::log_access(int status, struct sockaddr_in *c_addr, http_request_t *req)
{
    char filetmp[MAX_BUFF];
    memcpy(filetmp, req->filename, sizeof(req->filename));
#if V16X_DEBUG >= 3
    SHAL_SYSTEM::printf("---[ LOGACCESS %s:%d Status:%d - %s ]---\n\n", inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port), status, filetmp);
#endif
}

void UR_V16X_Posix::client_error(int fd, int status, const char *msg, const char *longmsg)
{
    char buf[MAX_BUFF];

    char longmsgtmp[MAX_BUFF];
    sprintf(longmsgtmp, "<HTML><head><meta charset=\"UTF-8\"/><TITLE>%s</TITLE></head>", msg);
    sprintf(longmsgtmp + strlen(longmsgtmp), "<BODY><center><H3>%s</H3></center>", msg);
    sprintf(longmsgtmp + strlen(longmsgtmp), "<center><p>%s.</p></center>", longmsg);
    sprintf(longmsgtmp + strlen(longmsgtmp), "<hr><center>%s</center></BODY></HTML>", SERVER_VERSION);

    sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
    sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
    sprintf(buf + strlen(buf), "%s", "Content-Type:text/html;charset=UTF-8\r\n");
    //sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
    sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", strlen(longmsgtmp));
    sprintf(buf + strlen(buf), "%s", longmsgtmp);
    writen(fd, buf, strlen(buf));
}

void UR_V16X_Posix::serve_static(int out_fd, int in_fd, struct sockaddr_in *c_addr, http_request_t *req, size_t total_size)
{
    char buf[MAX_BUFF];
    memset(buf, '\0', MAX_BUFF);

    if (strstr(req->filename, "v16x")) {
        int status = 404;
        const char *msg = "File not found";
        client_error(out_fd, status, "Not found", msg);
        return;
    }

    if (req->offset > 0) {
        sprintf(buf, "%s", "HTTP/1.1 206 Partial\r\n");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
        sprintf(buf + strlen(buf), "Content-Range:bytes %lu-%lu/%lu\r\n", req->offset, req->end, total_size);
    } else {
        sprintf(buf, "%s", "HTTP/1.1 200 OK\r\nAccept-Ranges:bytes\r\n");
        //sprintf(buf + strlen(buf), "Connection:%s\r\n", "close");
        sprintf(buf + strlen(buf), "Server:%s\r\n", SERVER_VERSION);
    }

    sprintf(buf + strlen(buf), "Cache-Control:no-cache\r\n");
    // sprintf(buf + strlen(buf), "Cache-Control:public,max-age=315360000\r\nExpires:Thu,31 Dec 2037 23:55:55 GMT\r\n");

    sprintf(buf + strlen(buf), "Content-type:%s\r\n", get_mime_type(req->filename));
    sprintf(buf + strlen(buf), "Content-length:%lu\r\n\r\n", req->end - req->offset);

    writen(out_fd, buf, strlen(buf));

    char filetmp[MAX_BUFF];
    memcpy(filetmp, req->filename, sizeof(req->filename));
    int64_t offset = (int64_t)req->offset;
    int64_t offsettmp = (int64_t)req->offset;
    int64_t end = (int64_t)req->end;

    // split at 1M if file > 2M
    //if (end > (int64_t)(1024 * 1024 * 2)) {
        //offsettmp = end / round(end / (1024 * 1024));
    //}

    SHAL_SYSTEM::printf("\tServing static #1 fd:%d offset: %d offsettmp: %d end: %d - %s Addr:%s:%d\n", in_fd, (int)offset, (int)offsettmp, (int)end, filetmp, inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port));
    fflush(stdout);
#ifdef __MSYS__
    char buftmp[end - offsettmp];
#endif

    while(offset < end) {
#ifdef __MSYS__
        read(in_fd, buftmp, end - offsettmp);
        offset = write(out_fd, buftmp, end - offsettmp);
#else
        if(sendfile(out_fd, in_fd, &offset, end - offsettmp) <= 0) {
            break;
        }
#endif // __MSYS__
        //offsettmp = offset;
        //filetmp[strlen(req->filename)] = '\0';
#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("\tServing static #2 fd:%d offset: %d end: %d - %s Addr:%s:%d\n", in_fd, (int)offset, (int)end, filetmp, inet_ntoa(c_addr->sin_addr), ntohs(c_addr->sin_port));
        fflush(stdout);
#endif // V16X_DEBUG
        break;
    }
}

void UR_V16X_Posix::handle_directory_request(int out_fd, int dir_fd, char *filename)
{
    char bufdir[MAX_BUFF], m_time[32], size[16];
    char longmsg[MAX_BUFF * 2];
    struct stat statbuf;

    memset(longmsg, '\0', MAX_BUFF * 2);
    sprintf(longmsg, "%s", "<html><head><meta charset=\"UTF-8\"/><style>");
    sprintf(longmsg + strlen(longmsg), "%s", "body{font-family:monospace;font-size:13px;}");
    sprintf(longmsg + strlen(longmsg), "%s", "td {padding: 1.5px 6px;}");
    sprintf(longmsg + strlen(longmsg), "%s", "</style></head><body><table>");

    DIR *d = fdopendir(dir_fd);
    struct dirent *dp;
    int ffd;
    int filecnt = 0;
    int dircnt = 0;

    while((dp = readdir(d)) != NULL) {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
            continue;
        }

        ffd = openat(dir_fd, dp->d_name, O_RDONLY);
        if (ffd == -1) {
            perror(dp->d_name);
            continue;
        }

        fstat(ffd, &statbuf);
        strftime(m_time, sizeof(m_time),
                 "%Y-%m-%d %H:%M", localtime(&statbuf.st_mtime));

        format_size(size, &statbuf, &filecnt, &dircnt);

        if (S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)) {
            const char *d1 = S_ISDIR(statbuf.st_mode) ? "/" : "";
            if (!strstr(dp->d_name, "v16x")) {
                sprintf(longmsg + strlen(longmsg), "<tr><td><a href=\"%s%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>",
                        dp->d_name, d1, dp->d_name, d1, m_time, size);
            } else {
                filecnt = filecnt - 1;
            }
        }
        close(ffd);
    }

    sprintf(longmsg + strlen(longmsg), "<td><br>Total items: %d Files: %d Dirs: %d</br></td></table><hr><center>%s</center></body></html>", filecnt + dircnt, filecnt, dircnt, SERVER_VERSION);

    memset(bufdir, '\0', MAX_BUFF);
    sprintf(bufdir, "HTTP/1.1 %d %s\r\n", 200, "OK");
    sprintf(bufdir + strlen(bufdir), "Server:%s\r\n", SERVER_VERSION);
    sprintf(bufdir + strlen(bufdir), "%s", "Content-Type:text/html;charset=UTF-8\r\n");
    //sprintf(bufdir + strlen(bufdir), "Connection:%s\r\n", "close");
    sprintf(bufdir + strlen(bufdir), "Content-length:%lu\r\n\r\n", strlen(longmsg));
    sprintf(bufdir + strlen(bufdir), "%s", longmsg);

    writen(out_fd, bufdir, strlen(bufdir));

    closedir(d);
}

ssize_t UR_V16X_Posix::io_data_read(data_io_t *data_iop, char *usrbuf, size_t maxlen, int *closed)
{
    memset(data_iop->io_buf, '\0', MAX_BUFF);
    memset(usrbuf, '\0', maxlen);
    data_iop->io_cnt = recv(data_iop->io_fd, data_iop->io_buf, MAX_BUFF, 0);
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("io_data_read 2 cnt: %d buf: %s\n", data_iop->io_cnt, data_iop->io_buf);
    fflush(stdout);
#endif // V16X_DEBUG
    if (data_iop->io_cnt > 0) {
        memcpy(usrbuf, data_iop->io_buf, data_iop->io_cnt);
    }

    if (data_iop->io_cnt <= 0) {
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
