#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "http.h"
#include "request.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* handle_client() never hands parse_request_buf() more than
     * REQUEST_BUFFER_SIZE - 1 bytes of real data, into a buffer it has
     * already zero-filled -- so `buf` is always NUL-terminated somewhere at
     * or before REQUEST_BUFFER_SIZE - 1. Match that here rather than fuzzing
     * an input shape the real caller can never produce. */
    if (size >= REQUEST_BUFFER_SIZE)
        return 0;

    char buf[REQUEST_BUFFER_SIZE] = {0};
    memcpy(buf, data, size);

    /* Stack, not init_request(): parse_request_buf() doesn't care whether
     * `req` is heap or stack, and this skips a malloc/free per run, which
     * matters at the iteration rates libFuzzer runs at. It allocates nothing
     * of its own, so there's nothing for this harness to free either. */
    http_request req = {0};
    parse_request_buf(buf, &req);
    return 0;
}
