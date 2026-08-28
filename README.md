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

## Token auth for game clients (token provider)

Ship game clients **without embedding an API key**. Give the config a token provider
callback instead: your backend verifies the player (its own session/JWT), calls the
OddSockets `POST /v1/token` mint endpoint with **its** API key server-side, and returns
the short-lived token. The SDK invokes your callback to fetch a fresh token before every
connect, and silently re-mints it before expiry while the client stays connected.

```c
#include "oddsockets.h"

/* Called by the SDK whenever it needs a fresh token (connect + pre-expiry refresh).
 * Fill token_out with the minted token and return ODDSOCKETS_SUCCESS.
 * Set *expires_at_ms_out to the expiry in epoch ms, or 0 to let the SDK
 * read the exp claim straight from the JWT. */
static int token_provider(char* token_out, size_t token_size,
                          int64_t* expires_at_ms_out, void* user_data) {
    /* Ask YOUR backend for an OddSockets token, e.g.
     * POST https://your-game-backend.example.com/oddsockets/token
     * (authenticated with the player's own session). */
    if (fetch_token_from_my_backend(token_out, token_size) != 0) return -1;
    *expires_at_ms_out = 0; /* SDK reads the JWT exp claim */
    return ODDSOCKETS_SUCCESS;
}

static void on_refreshed(const char* event, const char* payload, void* u) {
    printf("token refreshed: %s\n", payload); /* {"expiresAt":<epoch ms>} */
}

oddsockets_config_t config;
oddsockets_config_init_token(&config, token_provider, NULL); /* no API key */
strncpy(config.user_id, "player-1", sizeof(config.user_id) - 1);

oddsockets_client_t* client = oddsockets_create(&config);
oddsockets_on(client, "token_refreshed", on_refreshed, NULL);
oddsockets_connect(client);

/* Pump as usual — refresh checks run inside oddsockets_process_events(). */
while (running) { oddsockets_process_events(client); usleep(5000); }
```

Notes:

- Either an API key **or** a token provider is required — `oddsockets_create` returns
  `NULL` if the config has neither.
- The provider is called for a **fresh** token on every connect and reconnect, and again
  `config.token_refresh_lead_ms` (default 120000) before the current token expires. On a
  failed refresh the SDK emits `token_refresh_failed` and keeps the current connection.
- Expiry resolution: your `*expires_at_ms_out` if non-zero, otherwise the JWT `exp`
  claim decoded from the token itself.
- Refresh runs on the normal single-threaded pump — no extra threads; just keep calling
  `oddsockets_process_events()`.

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

## Get Accredited

<a href="https://tyga.games/accreditation"><img src="https://prodmedia.tyga.host/public/tyga.cloud/landing/tyga.games/tygagames-black-words.svg" alt="tyga.games accreditation" height="44"></a>

Prove you can build and operate real-time features on OddSockets — channels, presence, pub/sub, delivery guarantees and production liveops — on the stack itself. Three tiers (**TCU / TCA / TCP**), certified through **tyga.games** and delivered on ClassaaS.

[**Get accredited on tyga.games →**](https://tyga.games/accreditation)

## Support

- [Documentation](https://docs.oddsockets.com/sdks/c)
- [Issue Tracker](https://github.com/jyswee/oddsockets-c-sdk/issues)
- [Email Support](mailto:support@oddsockets.com)

## License

MIT License - Copyright (c) 2026 Joe Wee, Tyga.Cloud Ltd. See [LICENSE](LICENSE) for details.
