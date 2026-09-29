FROM debian:bookworm AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake
WORKDIR /src
COPY server.c server.c
COPY CMakeLists.txt CMakeLists.txt
COPY include include
COPY src src
COPY vendor vendor

RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static -s" -DBUILD_TESTING=OFF
RUN cmake --build build

FROM scratch
ENV LOG_LEVEL=INFO
WORKDIR /
COPY --from=build /src/build/sefaultd /usr/local/bin/sefaultd
COPY site /site
COPY logs /logs
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/sefaultd", "-p", "8080"]
