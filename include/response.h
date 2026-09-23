#pragma once
#include "request.h"
#include <stdio.h>

/* Directory every request path is resolved against. It is relative, so the
 * server has to be started from the directory that contains it. */
#define SITE_DIR "./site"

/* The page sent for a request that matches no file. */
#define NOT_FOUND_PATH SITE_DIR "/not_found.html"

/* The page sent for a request line that could not be parsed. */
#define BAD_REQUEST_PATH SITE_DIR "/bad_request.html"

/* Built into the binary, and sent only when NOT_FOUND_PATH itself cannot be
 * read, so that a broken site directory still gets an answer. */
#define NOT_FOUND_HTML "<h1>This page wasn't found</h1><a href=\"/\">go back to home</a>"

/*
 * A response that serves one static file: the status code to report and the
 * path of the file whose contents form the body.
 */
typedef struct http_static_page_response
{
    int status_code;
    char *path;
} http_static_page_response;

/*
 * Allocates an empty response.
 * Takes no arguments.
 * Returns a heap-allocated, zero-filled http_static_page_response owned by
 * the caller.
 */
http_static_page_response *init_response();

/*
 * Allocates a response and fills it in.
 * status_code: HTTP status to report, for example 200 or 404.
 * path:        path of the file to serve. Stored by pointer, not copied, so
 *              it must stay valid until the response is sent.
 * Returns a heap-allocated response owned by the caller.
 */
http_static_page_response *create_response(int status_code, char *path);

/*
 * Sends the 404 page: the file at NOT_FOUND_PATH when it can be read, and the
 * NOT_FOUND_HTML body built into the binary when it cannot, so that a missing
 * or unreadable not_found.html still produces a response.
 * req: the request being answered; supplies the socket to write to.
 * res: the response to answer with. Its `path` is ignored -- this always
 *      serves NOT_FOUND_PATH and always reports status 404.
 * Returns nothing, and frees both `req` and `res` before returning, so neither
 * may be used afterwards. The socket is left open for the caller to close.
 */
void send_http_not_found_page_response(http_request *req, http_static_page_response *res);

/*
 * Sends one file: serves the body from the page cache when the path is already
 * there, otherwise reads it from disk and caches it, then writes the header
 * block followed by the body to the client socket. Content-Type comes from the
 * file's suffix; a file whose suffix is unknown is sent as
 * application/octet-stream.
 * req: the request being answered; supplies the socket to write to.
 * res: the status code to report and the path of the file to send.
 * Returns nothing, and frees both `req` and `res` before returning, so neither
 * may be used afterwards. `res->path` is not freed, since the response does
 * not own it. Falls back to send_http_not_found_page_response when the file
 * cannot be read. The socket is left open for the caller to close.
 */
void send_file_response(http_request *req, http_static_page_response *res);