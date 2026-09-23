#pragma once
#include <stdio.h>
#include "request.h"

/*
 * Routes a parsed request to a file under SITE_DIR and sends the response.
 * "/" maps to index.html; any other path is appended to SITE_DIR. If the
 * resulting path is not an existing regular file, not_found.html is sent
 * with status 404, otherwise the file is sent with status 200.
 * req: the parsed request; its client_fd is the socket written to. Ownership
 *      passes to this call, which frees it once the response is sent.
 * Returns nothing. The result is the bytes written to the client socket.
 */
void global_req_handler(http_request* req);

/*
 * Sends one file as the response, bypassing the SITE_DIR path routing that
 * global_req_handler does -- the caller supplies the file to serve directly.
 * req:         the parsed request; its client_fd is the socket written to.
 *              Ownership passes to this call, which frees it once the
 *              response is sent.
 * status_code: the HTTP status to report if `path` can be read.
 * path:        path of the file to serve, including SITE_DIR -- this does not
 *              prepend it the way global_req_handler does.
 * Returns nothing. The result is the bytes written to the client socket. If
 * `path` cannot be read, this falls back to the 404 page and reports status
 * 404 regardless of `status_code`.
 */
void render_page(http_request *req, int status_code, char *path);
