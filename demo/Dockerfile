# OddSockets C SDK - two-client round-trip demo
#
# Build from the repo ROOT so the SDK source is in context:
#
#   docker build -f demo/Dockerfile -t oddsockets-c-demo .
#   docker run --rm -e ODDSOCKETS_API_KEY="ak_your_key_here" oddsockets-c-demo
#
# A successful run prints "OK - cross-client round-trip verified" and exits 0.
# The SDK is compiled from source at image-build time, so a broken SDK fails
# the build.

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc \
      libc6-dev \
      libwebsockets-dev \
      libcurl4-openssl-dev \
      libssl-dev \
      pkg-config \
      ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /sdk
COPY . /sdk

# Compile the SDK sources together with the demo. libwebsockets carries the
# Engine.IO/Socket.IO frames; libcurl performs manager discovery.
RUN gcc -O2 -Wall -I src \
      demo/demo.c \
      src/oddsockets.c \
      src/websocket_client.c \
      src/http_client.c \
      src/json_parser.c \
      src/manager_discovery.c \
      -o /usr/local/bin/oddsockets-demo \
      $(pkg-config --cflags --libs libwebsockets) \
      -lcurl -lssl -lcrypto -lpthread

CMD ["oddsockets-demo"]
