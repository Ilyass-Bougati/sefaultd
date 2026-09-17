# sefaultd

A small static HTTP/1.1 file server written in C. It listens on a TCP port, serves
files out of `site/`, spawns one detached thread per connection, and keeps page
bodies in an in-memory cache keyed by FNV-1a hash of the request path.

It is a development server. [KNOWN_ISSUES.md](KNOWN_ISSUES.md) lists both the open
bugs and the limits that are deliberate — do not expose it to a network you do not
control.

Everything beyond running it — sanitizer builds, editor setup, the source layout and
the k6 load test — is in [DEVELOPMENT.md](DEVELOPMENT.md).

## Run with Docker

### Pull the published image

Images are published to GitHub Container Registry from `v*` tags. `latest` tracks
the newest release, each release also gets its version as a tag (`1.0.0`).

```bash
docker run --rm -p 8080:8080 ghcr.io/ilyass-bougati/sefaultd:latest
```

To serve your own pages without rebuilding, mount a directory over `/site`:

```bash
docker run --rm -p 8080:8080 -v "$PWD/site:/site" ghcr.io/ilyass-bougati/sefaultd:latest
```

### Build the image yourself

```bash
docker build -t sefaultd .
docker run --rm -p 8080:8080 sefaultd
```

## Build from source and run

Needs a C compiler (`gcc` or `clang`), [CMake](https://cmake.org/) 3.16 or newer, a
build tool for it (`make` or `ninja`), and Linux. No external libraries; the only
vendored code is the FNV hash in `vendor/`.

Configure once, then build. Everything generated lands in `build/`, which is ignored
by git; deleting that directory is the full clean.

```bash
cmake -S . -B build
cmake --build build
```

That produces `build/sefaultd`. `site/` is resolved against the working directory,
so start it from the project root, not from inside `build/`. With no arguments it
listens on port 8080:

```bash
./build/sefaultd
```

Options:

| Flag             | Meaning                                        | Default |
| ---------------- | ----------------------------------------------- | ------- |
| `-p`, `--port`   | port to listen on                                | `8080`  |
| `-l`, `--log`    | log level: `DEBUG`, `INFO`, `WARN`, or `ERROR`   | `INFO`  |
| `-h`, `--help`   | print the option list and exit                   |         |

```bash
./build/sefaultd --port 9000 --log DEBUG
```

There is also a `run` target that builds first and sets the working directory for
you, on the default port:

```bash
cmake --build build --target run
```

`CMAKE_BUILD_TYPE` defaults to `Debug`. For an optimised binary, configure a second
directory rather than overwriting the first:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release
```

## Check that it works

```bash
curl -i http://localhost:8080/
```

On start-up the server prints its name as ASCII art to stderr, then one line per
request at the current log level:

```
███████╗███████╗███████╗ █████╗ ██╗   ██╗██╗  ████████╗██████╗
██╔════╝██╔════╝██╔════╝██╔══██╗██║   ██║██║  ╚══██╔══╝██╔══██╗
███████╗█████╗  █████╗  ███████║██║   ██║██║     ██║   ██║  ██║
╚════██║██╔══╝  ██╔══╝  ██╔══██║██║   ██║██║     ██║   ██║  ██║
███████║███████╗██║     ██║  ██║╚██████╔╝███████╗██║   ██████╔╝
╚══════╝╚══════╝╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝   ╚═════╝  v1.1.0

2026-09-17 19:34:26 [INFO] server.c:108: Server is listening on port 8080
2026-09-17 19:34:27 [INFO] src/request.c:13: GET / HTTP/1.
```

On a real terminal the level tag, timestamp and port number are coloured; that is
stripped above. Every line is written to stderr and, when `logs/` exists relative to
the working directory, appended to `logs/server.log` as well — a fresh clone already
has that directory (kept in git with a `.gitkeep`).

`INFO` is the default level, which is why the two lines above are all that show.
`--log DEBUG` (or `-l DEBUG`) adds a line for the file each request resolves to and
for each page as it enters the cache; `WARN` and `ERROR` show less than `INFO` does.

Routing rules:

| Request path  | Served file                                     | Status |
| ------------- | ----------------------------------------------- | ------ |
| `/`           | `site/index.html`                               | 200    |
| `/<name>`     | `site/<name>` if it is an existing regular file | 200    |
| anything else | `site/not_found.html`                           | 404    |

`Content-Type` is guessed from the file's suffix (`.css` → `text/css`, `.png` →
`image/png`, and so on), falling back to `application/octet-stream` for anything
unrecognised; the 404 page and any other response are otherwise sent with a
`Content-Length` and `Connection: close`. Only the request line is parsed; request
headers are read off the socket but ignored.

Add pages by dropping files into `site/`. They are picked up on the next request for
that path, and the body is cached in memory from the first hit onward, so restart the
server after editing a file you have already requested.

## Run the tests

The suite uses [Criterion](https://github.com/Snaipe/Criterion). It is a test-only
dependency — it never gets linked into the server, and a tree without it builds and
runs exactly as before, just with the tests skipped.

```bash
sudo apt-get install libcriterion-dev
```

Tests are picked up by the ordinary configure step and built alongside the server,
then run through CTest:

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

Each of the three suites is also a binary you can run on its own, which is the
quicker loop while working on one of them:

```bash
./build/test/test_serve --verbose
```

Criterion takes `--list` to show the cases and `--filter` to pick them. The filter
separates suite from test with a slash, even though the output prints them with
`::`:

```bash
./build/test/test_serve --filter 'serve/a_query*'
```

The suite is green in a normal build. Criterion runs every test in its own process,
so a test that crashes is reported as a single `CRASH` and the rest of the suite
still runs rather than taking the run down with it.

Configuring with `-DSANITIZE=address` builds the tests sanitized too, which is how
the leaks in that list show up; see [DEVELOPMENT.md](DEVELOPMENT.md). To leave the
tests out of the build entirely, configure with `-DBUILD_TESTING=OFF`.
