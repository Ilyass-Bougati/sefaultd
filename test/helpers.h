#pragma once
#include <stddef.h>

/*
 * One captured response: the raw bytes the server wrote, plus the few fields
 * the tests assert on. `body` points into `raw`, so it dies with it.
 */
typedef struct {
    char *raw;
    size_t raw_len;
    int status_code;
    long content_length; /* as declared in the header, -1 if absent */
    char *body;
    size_t body_len; /* bytes actually received after the blank line */
} captured_response;

/*
 * Creates a private temp directory with a `site/` in it and chdir's there, so
 * the SITE_DIR relative path resolves inside it. Criterion runs each test in
 * its own process, so the chdir and the page cache it fills stay local to the
 * calling test.
 * Writes site/index.html and site/not_found.html with known contents.
 */
void enter_temp_site(void);

/* Writes `len` bytes to site/<name>, creating or truncating it. */
void write_site_file(const char *name, const void *data, size_t len);

/* Deletes site/<name>, for tests that need a fixture file to be absent. */
void remove_site_file(const char *name);

/*
 * Reads site/<name> back off disk, so a test can compare what was served
 * against what is actually in the file. Caller frees the buffer.
 */
char *read_site_file(const char *name, size_t *out_len);

/*
 * Feeds `request` through handle_client() over a socketpair and captures
 * everything written back. No listening socket and no threads involved.
 * Caller frees with free_captured().
 */
captured_response do_request(const char *request);

void free_captured(captured_response *res);
