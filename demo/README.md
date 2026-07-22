# OddSockets C SDK - Demo

A tiny, runnable program that proves a real real-time round-trip against OddSockets
using **two independent clients**: **connect -> subscribe -> publish -> receive**.

Because the subscriber (`alice`) and the publisher (`bob`) are separate connections,
a message that reaches the subscriber can only have travelled through the OddSockets
worker - so this doubles as an honest end-to-end regression test (no mocks, no local
echo). The SDK speaks genuine Socket.IO (Engine.IO v4) over a WebSocket to the
assigned worker; libwebsockets carries the frames and libcurl performs manager
discovery.

## Proof it's real

`demo/PROOF.txt` is a captured transcript of this demo running in Docker against the
live platform. Reproduce it yourself in one command (see below) - here is a real run:

```
[connect] connecting both clients...
[alice] worker w002-oddsockets-1
[bob]   worker w002-oddsockets-1
[connect] alice = connected, bob = connected
[alice] subscribed to demo-161746 (presence on)
[bob] published to demo-161746
[alice] received bob's message (nonce matched) - real round-trip.
[alice] presence: 1 user(s).
[alice] unsubscribed.

OK - cross-client round-trip verified
```

## 1. Get a free API key

Two-step email verification (no card required):

```bash
# Step 1 - request a code
curl -X POST https://oddsockets.com/api/agent-signup \
  -H "Content-Type: application/json" \
  -d '{"email":"you@example.com","agentName":"demo","platform":"c"}'

# Step 2 - verify and receive your apiKey
curl -X POST https://oddsockets.com/api/agent-signup/verify \
  -H "Content-Type: application/json" \
  -d '{"email":"you@example.com","code":"123456","agentName":"demo"}'
```

The verify response contains your `apiKey` (starts with `ak_`).

## 2. Run it in Docker (recommended)

No local C toolchain needed. Build from the repo root so the SDK source is in context
(the demo compiles the SDK straight from the parent, without installing anything):

```bash
docker build -f demo/Dockerfile -t oddsockets-c-demo .
docker run --rm -e ODDSOCKETS_API_KEY="ak_your_key_here" oddsockets-c-demo
```

The SDK is compiled from source at image-build time, so a broken SDK fails the build.
A successful run prints `OK - cross-client round-trip verified` and exits `0`.

## 2b. Build it locally

Requires a C toolchain plus libwebsockets, libcurl and OpenSSL development headers.
On Debian/Ubuntu:

```bash
sudo apt-get install -y gcc libc6-dev libwebsockets-dev libcurl4-openssl-dev \
    libssl-dev pkg-config

gcc -O2 -Wall -I src \
    demo/demo.c \
    src/oddsockets.c src/websocket_client.c src/http_client.c \
    src/json_parser.c src/manager_discovery.c \
    -o oddsockets-demo \
    $(pkg-config --cflags --libs libwebsockets) \
    -lcurl -lssl -lcrypto -lpthread

export ODDSOCKETS_API_KEY="ak_your_key_here"
./oddsockets-demo
```

The key is read from `ODDSOCKETS_API_KEY` and never hardcoded; if it is missing the
program prints the signup instructions above and exits non-zero.

## The code, step by step

Create two clients - a subscriber and a publisher - each on its own connection. The
event loops are non-blocking; call `oddsockets_process_events()` to service them:

```c
oddsockets_config_t ca, cb;
oddsockets_config_init(&ca, api_key);
strncpy(ca.user_id, "alice", sizeof(ca.user_id) - 1);
oddsockets_config_init(&cb, api_key);
strncpy(cb.user_id, "bob", sizeof(cb.user_id) - 1);

oddsockets_client_t* alice = oddsockets_create(&ca);
oddsockets_client_t* bob   = oddsockets_create(&cb);

/* Drive the Engine.IO OPEN -> CONNECT -> ack handshake to completion. */
while (oddsockets_get_state(alice) != ODDSOCKETS_STATE_CONNECTED ||
       oddsockets_get_state(bob)   != ODDSOCKETS_STATE_CONNECTED) {
    oddsockets_process_events(alice);
    oddsockets_process_events(bob);
    usleep(5000);
}
```

Subscribe on the subscriber (presence enabled). A message only lands in the callback
if it came back through the worker:

```c
static void on_message(const char* channel, const char* message, void* user_data) {
    printf("received: %s\n", message);
}

oddsockets_channel_t* ac = oddsockets_channel_create(alice, "my-channel");
oddsockets_subscribe_options_t opts = { .enable_presence = true };
oddsockets_channel_subscribe(ac, on_message, NULL, &opts);
```

Publish from the *other* client - this is what makes the test honest:

```c
oddsockets_channel_t* bc = oddsockets_channel_create(bob, "my-channel");
oddsockets_channel_publish(bc, "{\"text\":\"hello from bob\"}", NULL);
```

Inspect presence, then tear down cleanly:

```c
static void on_presence(const char* channel, const char* action,
                        const char* user_id, void* user_data) {
    printf("occupancy: %s\n", user_id);
}

oddsockets_channel_get_presence(ac, on_presence, NULL);
oddsockets_channel_unsubscribe(ac);

oddsockets_disconnect(alice);
oddsockets_disconnect(bob);
oddsockets_destroy(alice);
oddsockets_destroy(bob);
```

## What it demonstrates

- Manager discovery + automatic worker assignment (fully transparent)
- `oddsockets_channel_create()` -> `oddsockets_channel_subscribe()` ->
  `oddsockets_channel_publish()`
- **Cross-client delivery**: a message published by `bob` is delivered to `alice`'s
  subscription in real time - provably through the worker, not a local echo
- Presence tracking, unsubscribe, and graceful disconnect
- Timeouts so a stalled handshake or round-trip is reported as a failure (non-zero exit)

## Files

- `Dockerfile` - compiles the SDK from source and runs the two-client demo on
  `debian:bookworm-slim`.
- `PROOF.txt` - captured transcript of a real containerised run against the platform.
- `demo.c` - the two-client round-trip program.
