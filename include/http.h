#pragma once
#include "request.h"

/* Largest request, in bytes, that is read off a connection in one go. */
#define REQUEST_BUFFER_SIZE 8192

/*
 * Reads one request off a connected socket and serves it. Reads up to
 * REQUEST_BUFFER_SIZE - 1 bytes into a stack buffer, which is zero-filled
 * first so the read always ends in a NUL, then hands the buffer to
 * parse_request_buf.
 * client_fd: connected client socket, read from and written to. Never closed
 *            by this call; the caller (handle_request) always closes it.
 * Returns nothing. A read() error leaves the request unanswered -- there is
 * nothing to usefully respond to. An empty read is not treated specially: it
 * reaches parse_request_buf like any other input, fails to parse, and gets
 * the same 400 response as any other request line that does not parse.
 * Otherwise logs the request and hands it to global_req_handler. Only the
 * request line is parsed; headers are ignored.
 */
void handle_client(int client_fd);

/*
 * Parses one HTTP request line out of `buf` and fills in `req`. Does no I/O
 * of its own, which is what lets it be driven directly by a fuzzer as well as
 * by handle_client.
 * buf: the request text, NUL-terminated. Only the first line is read;
 *      anything after it (headers, a body) is ignored. Not modified.
 * req: filled in on success: `method` and `version` from the request line,
 *      truncated to 7 bytes each; `path` normalised -- a query string is cut
 *      at the first '?', and `.` and `..` segments are dropped, so
 *      "/a/../b?x=1" becomes "/b". An empty or all-dots path becomes "/".
 *      req->client_fd and the rest of req are not touched.
 * Returns 0 on success. Returns -1, leaving `req` partially filled, if the
 * request line does not split into exactly three whitespace-separated
 * tokens, or if the normalised path would not fit in req->path.
 */
int parse_request_buf(char *buf, http_request *req);

/*
 * Thread entry point for one connection, passed to pthread_create.
 * arg: a heap-allocated int holding the client socket descriptor. This call
 *      frees it, so the caller must not.
 * Returns NULL always; the thread's result is the response it wrote. Owns the
 * connection for the thread's lifetime and closes the socket before returning,
 * which is what the "Connection: close" response header promises.
 */
void *handle_request(void *arg);
