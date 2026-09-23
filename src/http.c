#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "request.h"
#include "handler.h"
#include "response.h"
#include "log.h"
#include <pthread.h>
#include <syscall.h>
#include <string.h>
#include <errno.h>

void *handle_request(void *arg)
{
    LOG_D("thread start, tid %d", (int)syscall(SYS_gettid));

    int client_fd = *(int *)arg;
    handle_client(client_fd);
    free(arg);
    close(client_fd);
    return NULL;
}

int parse_request_buf(char *buf, http_request *req)
{
    char path[2048] = {0};
    if (sscanf(buf, "%7s %2047s %7s", req->method, path, req->version) != 3)
        return -1;

    char *q = strchr(path, '?');
    if (q)
        *q = '\0';

    size_t len = 0;
    char *save = NULL;
    for (char *seg = strtok_r(path, "/", &save); seg; seg = strtok_r(NULL, "/", &save))
    {
        if (strcmp(seg, "..") == 0 || strcmp(seg, ".") == 0)
            continue;
        size_t n = strlen(seg);
        if (len + 1 + n >= sizeof(req->path))
            return -1;
        req->path[len++] = '/';
        memcpy(req->path + len, seg, n);
        len += n;
    }
    if (len == 0)
        req->path[len++] = '/';
    req->path[len] = '\0';
    return 0;
}

void handle_client(int client_fd)
{
    char buf[REQUEST_BUFFER_SIZE] = {0};
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n < 0)
    {
        /* A genuine read error, not an empty request: the socket is already
         * broken, so there is nothing useful to write a response to.
         * handle_request() closes client_fd on every path out of this
         * function, so there is nothing to do here but log it and return. */
        LOG_W("read() failed on client_fd %d: %s", client_fd, strerror(errno));
        return;
    }

    /* n == 0 (the peer sent nothing) falls straight through: buf is still
     * all zeros, parse_request_buf() can't find three tokens in it, and it
     * gets the same 400 response as any other request line that doesn't
     * parse, rather than the silent close this used to get. */
    http_request *req = init_request();
    req->client_fd = client_fd;
    if (parse_request_buf(buf, req) != 0)
    {
        render_page(req, 400, BAD_REQUEST_PATH);
        return;
    }
    log_http_req(req);
    global_req_handler(req);
}
