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
