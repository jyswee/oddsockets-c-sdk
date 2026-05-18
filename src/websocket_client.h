/**
 * WebSocket Client for OddSockets C SDK
 * Wrapper around libwebsockets for WebSocket connections.
 */

#ifndef ODDSOCKETS_WEBSOCKET_CLIENT_H
#define ODDSOCKETS_WEBSOCKET_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* Forward declaration */
typedef struct websocket_client websocket_client_t;

/* Callback types */
typedef void (*ws_on_connected_t)(void* user_data);
typedef void (*ws_on_disconnected_t)(void* user_data);
typedef void (*ws_on_message_t)(const char* message, void* user_data);
typedef void (*ws_on_error_t)(const char* error_message, void* user_data);

/* WebSocket configuration */
typedef struct {
    char url[512];
    bool enable_ssl;
    bool ssl_verify_peer;
    char ca_cert_path[512];
    int connection_timeout_ms;
    char auth_header[1024];

    ws_on_connected_t on_connected;
    ws_on_disconnected_t on_disconnected;
    ws_on_message_t on_message;
    ws_on_error_t on_error;
    void* user_data;
} websocket_config_t;

/**
 * Create a WebSocket client
 * @param config Configuration
 * @return Client instance or NULL
 */
websocket_client_t* websocket_client_create(const websocket_config_t* config);

/**
 * Connect to the WebSocket server
 * @param client Client instance
 * @return 0 on success
 */
int websocket_client_connect(websocket_client_t* client);

/**
 * Send a text message
 * @param client Client instance
 * @param message Message string
 * @return 0 on success
 */
int websocket_client_send(websocket_client_t* client, const char* message);

/**
 * Process pending events (non-blocking)
 * @param client Client instance
 * @return 0 on success
 */
int websocket_client_process_events(websocket_client_t* client);

/**
 * Disconnect from the server
 * @param client Client instance
 */
void websocket_client_disconnect(websocket_client_t* client);

/**
 * Destroy client and free resources
 * @param client Client instance
 */
void websocket_client_destroy(websocket_client_t* client);

/**
 * Check if connected
 * @param client Client instance
 * @return true if connected
 */
bool websocket_client_is_connected(websocket_client_t* client);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_WEBSOCKET_CLIENT_H */
