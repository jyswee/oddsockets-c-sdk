# OddSockets C SDK

Official C SDK for OddSockets real-time messaging platform. Optimized for embedded systems and IoT devices. C99, libcurl, libwebsockets.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Quick Start

```c
#include "oddsockets.h"

oddsockets_config_t config;
oddsockets_config_init(&config, "YOUR_API_KEY");
oddsockets_client_t* client = oddsockets_create(&config);

oddsockets_channel_t* ch = oddsockets_channel_create(client, "my-channel");
oddsockets_channel_subscribe(ch, on_message, NULL, NULL);
oddsockets_channel_publish(ch, "{\"text\":\"Hello from C\"}", NULL);
```

## Enhanced Features

Enhanced (Slack-like) events layer on top of the core pub/sub. The **send** side is a
set of free functions taking the client handle; each returns `ODDSOCKETS_SUCCESS` and
emits a real worker event over the live socket. The matching **broadcast** arrives on
`oddsockets_on(client, "<event>", ...)` as a JSON payload string, so any subscriber in
the channel room can react. As with all C SDK I/O, pump `oddsockets_process_events()`
to service the event loop.

```c
#include "oddsockets.h"
#include "enhanced_features.h"

/* Raw event listener: enhanced broadcasts arrive here as JSON strings. */
static void on_typing(const char* event, const char* payload, void* user_data) {
    printf("typing: %s\n", payload);
}
static void on_reaction(const char* event, const char* payload, void* user_data) {
    printf("reaction: %s\n", payload);
}
static void on_message(const char* channel, const char* message, void* user_data) {}

oddsockets_config_t config;
oddsockets_config_init(&config, "YOUR_API_KEY");
strncpy(config.user_id, "alice", sizeof(config.user_id) - 1);
oddsockets_client_t* client = oddsockets_create(&config);

/* Join the scoped room so enhanced broadcasts are delivered. */
oddsockets_channel_t* ch = oddsockets_channel_create(client, "room-42");
oddsockets_channel_subscribe(ch, on_message, NULL, NULL);

/* Receive-path */
oddsockets_on(client, "user_typing",    on_typing,   NULL);
oddsockets_on(client, "reaction_added", on_reaction, NULL);

/* Send-path */
oddsockets_start_typing(client, "alice", "room-42");
oddsockets_add_reaction(client, "msg-1", "room-42", ":thumbsup:", "alice", "Alice");

/* Drive the event loop to flush sends and dispatch broadcasts. */
for (int i = 0; i < 200; i++) { oddsockets_process_events(client); usleep(5000); }
```

### Event surface

| Area | Send | Broadcast (`oddsockets_on`) |
|---|---|---|
| **Typing** | `oddsockets_start_typing(client, user_id, channel)` · `oddsockets_stop_typing(client, user_id, channel)` | `user_typing` · `user_stopped_typing` |
| **Reactions** | `oddsockets_add_reaction(client, message_id, channel, emoji, user_id, user_name)` · `oddsockets_remove_reaction(client, message_id, channel, emoji, user_id)` | `reaction_added` · `reaction_removed` |

The C enhanced surface is deliberately focused on typing and reactions. Any other worker
event your channel emits is still available directly on `oddsockets_on(client, "<event>", ...)`.

## Get a Free API Key

```bash
curl -X POST https://oddsockets.com/api/agent-signup \
  -H "Content-Type: application/json" \
  -d '{"email": "you@example.com", "agentName": "my-agent", "platform": "c"}'
curl -X POST https://oddsockets.com/api/agent-signup/verify \
  -H "Content-Type: application/json" \
  -d '{"email": "you@example.com", "code": "123456", "agentName": "my-agent"}'
```

## Plans

| | Free | Starter | Pro |
|---|---|---|---|
| **Price** | $0/mo | $49.99/mo | $299/mo |
| **MAU** | 100 | 1,000 | 50,000 |
| **Concurrent connections** | 50 | 1,000 | Unlimited |
| **Messages/day** | 10,000 | 4,320,000 | Unlimited |
| **Channels** | 10 | Unlimited | Unlimited |
| **Storage** | 100MB (24h) | 50GB (6 months) | Unlimited |

## Support

- [Documentation](https://docs.oddsockets.com/sdks/c)
- [Issue Tracker](https://github.com/jyswee/oddsockets-c-sdk/issues)
- [Email Support](mailto:support@oddsockets.com)

## License

MIT License - Copyright (c) 2026 Joe Wee, Tyga.Cloud Ltd. See [LICENSE](LICENSE) for details.
