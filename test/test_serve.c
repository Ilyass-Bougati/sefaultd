/*
 * End to end, one request in and one response out, driven over a socketpair
 * rather than a listening socket. This covers the whole path the server takes
 * for a connection -- parse_request -> global_req_handler -> the cache and the
 * response writer -- with no ports, no threads and no timing involved.
 */
#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "helpers.h"

/* Asserts a response carries exactly the bytes of site/<name>. */
static void assert_body_matches_file(captured_response *res, const char *name)
{
    size_t expected_len = 0;
    char *expected = read_site_file(name, &expected_len);
    cr_assert_not_null(expected, "site/%s is missing from the test fixture", name);

    cr_assert_eq(res->body_len, expected_len,
        "served %zu bytes of site/%s, the file holds %zu", res->body_len, name, expected_len);
    cr_assert_arr_eq(res->body, expected, expected_len,
        "the body served does not match site/%s byte for byte", name);

    free(expected);
}

Test(serve, root_serves_index_html)
{
    enter_temp_site();

    captured_response res = do_request("GET / HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(res.status_code, 200, "expected 200 for /, got %d", res.status_code);
    assert_body_matches_file(&res, "index.html");
    free_captured(&res);
}

Test(serve, an_existing_file_is_served_verbatim)
{
    enter_temp_site();
    const char *page = "<h1>about</h1><p>hello</p>";
    write_site_file("about.html", page, strlen(page));

    captured_response res = do_request("GET /about.html HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(res.status_code, 200);
    assert_body_matches_file(&res, "about.html");
    free_captured(&res);
}

Test(serve, an_unknown_path_serves_not_found_with_404)
{
    enter_temp_site();

    captured_response res = do_request("GET /nope.html HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(res.status_code, 404, "expected 404 for a missing page, got %d", res.status_code);
    assert_body_matches_file(&res, "not_found.html");
    free_captured(&res);
}

Test(serve, the_response_declares_content_length_and_close)
{
    enter_temp_site();

    captured_response res = do_request("GET / HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(res.content_length, (long)res.body_len,
        "declared Content-Length %ld but sent %zu bytes", res.content_length, res.body_len);
    cr_assert_not_null(strstr(res.raw, "Connection: close"));
    cr_assert_not_null(strstr(res.raw, "Content-Type: text/html"));
    free_captured(&res);
}

/*
 * The second request for a path is answered from the cache rather than from
 * disk. It has to come back identical.
 */
Test(serve, a_repeated_request_serves_the_same_bytes)
{
    enter_temp_site();
    const char *page = "<h1>about</h1><p>hello</p>";
    write_site_file("about.html", page, strlen(page));

    captured_response first = do_request("GET /about.html HTTP/1.1\r\nHost: x\r\n\r\n");
    captured_response second = do_request("GET /about.html HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(second.status_code, first.status_code);
    cr_assert_eq(second.body_len, first.body_len,
        "first request served %zu bytes, the cached one served %zu", first.body_len, second.body_len);
    cr_assert_arr_eq(second.body, first.body, first.body_len);

    free_captured(&first);
    free_captured(&second);
}

/*
 * FAILS TODAY, on purpose. On a cache hit, send_http_static_page_response()
 * recomputes the body length as strlen(content) instead of keeping the length
 * read_file() measured, and get_cached() hands back a strdup. Both stop at the
 * first NUL, so a file holding one is served whole on the first request and
 * truncated on every request after it.
 *
 * Fixing it means storing the length alongside the body in the cache. Until
 * then this is the test that says so.
 */
Test(serve, a_cached_file_is_not_truncated_at_a_nul_byte)
{
    enter_temp_site();
    const char raw[] = "<h1>AAAA</h1>\0<h1>BBBB</h1>xx"; /* 29 bytes */
    size_t raw_len = sizeof(raw) - 1;
    write_site_file("nul.html", raw, raw_len);

    captured_response first = do_request("GET /nul.html HTTP/1.1\r\nHost: x\r\n\r\n");
    captured_response second = do_request("GET /nul.html HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(first.body_len, raw_len,
        "first request served %zu of %zu bytes", first.body_len, raw_len);
    cr_assert_eq(second.body_len, raw_len,
        "the cached response served %zu of %zu bytes", second.body_len, raw_len);
    cr_assert_arr_eq(second.body, raw, raw_len);

    free_captured(&first);
    free_captured(&second);
}

/*
 * ---------------------------------------------------------------------------
 * The tests below all fail against the current code. Each one describes a bug
 * rather than a deliberate limitation, and is written as the behaviour that
 * should hold, so fixing the bug turns the test green. KNOWN_ISSUES.md has the
 * matching write-up for each.
 * ---------------------------------------------------------------------------
 */

/*
 * FAILS TODAY (crashes). When read_file() cannot load a page,
 * send_http_static_page_response() retries itself with NOT_FOUND_PATH -- but it
 * does that unconditionally, including when the file it failed to read *was*
 * NOT_FOUND_PATH. With site/not_found.html missing or unreadable, one request
 * for any absent page recurses until the stack runs out.
 *
 * That kills the whole process, not just the connection, because a stack
 * overflow on a connection thread takes the server down with it. Verified: the
 * server exits with signal 11 on the first such request.
 *
 * The recursion needs a base case: if the path being loaded is already
 * NOT_FOUND_PATH, send something built in rather than recursing.
 */
Test(serve, a_missing_not_found_page_does_not_take_the_process_down, .timeout = 10)
{
    enter_temp_site();
    remove_site_file("not_found.html");

    captured_response res = do_request("GET /nope.html HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_neq(res.status_code, -1, "the server sent nothing back at all");
    free_captured(&res);
}

/*
 * FAILS TODAY (404). The request path is used to build a filename exactly as it
 * arrived, so the query string becomes part of the path that gets stat'ed:
 * "./site/index.html?v=1" does not exist, and a page that works without a query
 * string 404s with one. Browsers append these constantly for cache busting.
 *
 * The path needs truncating at the first '?' before it is joined to SITE_DIR.
 */
Test(serve, a_query_string_is_ignored_when_resolving_the_path)
{
    enter_temp_site();

    captured_response res = do_request("GET /index.html?v=1 HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(res.status_code, 200,
        "expected 200 for /index.html?v=1, got %d", res.status_code);
    assert_body_matches_file(&res, "index.html");
    free_captured(&res);
}

/*
 * FAILS TODAY. When a file passes stat() but cannot be opened -- a permission
 * change, or the file going away between the two calls -- read_file() returns
 * NULL and send_http_not_found_page_response() takes over. That function caches
 * what it served under res->path, which is the path of the file that was asked
 * for, not NOT_FOUND_PATH.
 *
 * So the built-in 404 body ends up stored under the real page's key. Every later
 * request for that path is a cache hit, and since it comes back through the
 * normal path it is sent with status 200: the wrong body under a success code,
 * for as long as the process lives, even once the file is readable again.
 *
 * The fallback should cache under NOT_FOUND_PATH, or not cache at all when it is
 * serving the built-in body.
 */
Test(serve, a_transient_read_failure_does_not_poison_the_cache)
{
    enter_temp_site();
    const char *page = "<h1>real</h1>";
    write_site_file("flaky.html", page, strlen(page));

    if (geteuid() == 0) {
        cr_skip("running as root, chmod 000 would not stop the read");
    }

    /* stat() still succeeds on a 000 file; fopen() is what fails. */
    cr_assert_eq(chmod("site/flaky.html", 0000), 0);
    captured_response first = do_request("GET /flaky.html HTTP/1.1\r\nHost: x\r\n\r\n");
    free_captured(&first);

    cr_assert_eq(chmod("site/flaky.html", 0644), 0);
    captured_response second = do_request("GET /flaky.html HTTP/1.1\r\nHost: x\r\n\r\n");

    cr_assert_eq(second.status_code, 200,
        "the file is readable again, expected 200, got %d", second.status_code);
    assert_body_matches_file(&second, "flaky.html");
    free_captured(&second);
}

/*
 * A client that connects and sends nothing at all: read() returns 0.
 *
 * This used to be a sanitizer-only failure -- parse_request() read into a
 * malloc'd scratch buffer that sscanf never wrote to on an empty request, and
 * strlen() on it ran past the end. handle_client()'s request buffer is a
 * zero-initialised stack array now, so that specific read is gone.
 *
 * It fails again today for an unrelated reason: handle_client() treats
 * read() returning 0 as a special case, closing the socket and returning
 * before parse_request_buf ever runs -- so an empty request gets no response
 * at all, not even the 400 that a merely-unparseable-but-nonempty one now
 * gets. Whether that silence is the intended behaviour for a client that
 * never sent anything is a real question, not a stale assumption in this
 * test; until it's answered, this stays failing on purpose.
 */
Test(serve, an_empty_request_is_handled_without_reading_uninitialised_memory)
{
    enter_temp_site();

    captured_response res = do_request("");

    cr_assert_neq(res.status_code, -1, "the server sent nothing back at all");
    free_captured(&res);
}
