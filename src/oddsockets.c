/**
 * OddSockets C SDK - Main Implementation
 * 
 * A lightweight C SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#include "oddsockets.h"
#include "manager_discovery.h"
#include "websocket_client.h"
#include "http_client.h"
#include "message_validator.h"
#include "json_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

/* Internal Structures */
struct oddsockets_client {
    oddsockets_config_t config;
    oddsockets_state_t state;
    
    /* Connection Management */
    websocket_client_t* websocket;
    char worker_url[ODDSOCKETS_MAX_URL_LENGTH];
    char worker_id[64];
    char session_id[128];
    char client_identifier[256];
    
    /* Reconnection */
    int reconnect_attempts;
    time_t last_reconnect_time;
    
    /* Channels */
    oddsockets_channel_t* channels[ODDSOCKETS_MAX_CHANNELS];
    int channel_count;
    
    /* Threading */
    pthread_mutex_t mutex;
    pthread_t event_thread;
    bool event_thread_running;
    
    /* Memory Management */
    size_t allocated_bytes;
    size_t peak_bytes;
};

struct oddsockets_channel {
    char name[ODDSOCKETS_MAX_CHANNEL_NAME_LENGTH];
    oddsockets_client_t* client;
    bool subscribed;
    bool subscribing;
    
    /* Callbacks */
    oddsockets_message_callback_t message_callback;
    void* message_user_data;
    oddsockets_presence_callback_t presence_callback;
    void* presence_user_data;

    /* Options */
    oddsockets_subscribe_options_t subscribe_options;
    
    /* Message History */
    char** message_history;
    int history_count;
    int max_history;
};

/* Memory Management */
static void* (*custom_malloc)(size_t) = malloc;
static void (*custom_free)(void*) = free;
static void* (*custom_realloc)(void*, size_t) = realloc;

/* Logging */
static void log_message(oddsockets_client_t* client, oddsockets_log_level_t level, const char* format, ...) {
    if (!client || !client->config.log_callback || level > client->config.log_level) {
        return;
    }
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    client->config.log_callback(level, buffer, client->config.log_user_data);
}

/* Error Handling */
static void handle_error(oddsockets_client_t* client, oddsockets_error_t error, const char* message) {
    if (client && client->config.error_callback) {
        client->config.error_callback(error, message, client->config.error_user_data);
    }
    
    log_message(client, ODDSOCKETS_LOG_ERROR, "Error %d: %s", error, message);
}

/* State Management */
static void set_state(oddsockets_client_t* client, oddsockets_state_t new_state) {
    if (!client) return;
    
    pthread_mutex_lock(&client->mutex);
    
    if (client->state != new_state) {
        client->state = new_state;
        
        if (client->config.connection_callback) {
            client->config.connection_callback(new_state, client->config.connection_user_data);
        }
        
        log_message(client, ODDSOCKETS_LOG_INFO, "State changed to: %s", oddsockets_state_string(new_state));
    }
    
    pthread_mutex_unlock(&client->mutex);
}

/* Client Identifier Generation */
static void generate_client_identifier(oddsockets_client_t* client) {
    /* Create a hash of the API key for session stickiness */
    uint32_t hash = 0;
    const char* api_key = client->config.api_key;
    
    for (int i = 0; api_key[i]; i++) {
        hash = ((hash << 5) - hash) + api_key[i];
    }
    
    const char* user_id = client->config.user_id[0] ? client->config.user_id : "default";
    snprintf(client->client_identifier, sizeof(client->client_identifier), 
             "%x_%s", hash, user_id);
}

/* Worker Assignment */
static int get_worker_assignment(oddsockets_client_t* client) {
    char manager_url[ODDSOCKETS_MAX_URL_LENGTH];
    
    /* Discover manager URL */
    int result = manager_discovery_get_url(client->config.api_key, manager_url, sizeof(manager_url));
    if (result != ODDSOCKETS_SUCCESS) {
        handle_error(client, result, "Failed to discover manager URL");
        return result;
    }
    
    /* Request worker assignment */
    char request_url[ODDSOCKETS_MAX_URL_LENGTH * 2];
    snprintf(request_url, sizeof(request_url), 
             "%s/api/cluster/select-worker?apiKey=%s&userId=%s&clientIdentifier=%s",
             manager_url, client->config.api_key, 
             client->config.user_id[0] ? client->config.user_id : client->client_identifier,
             client->client_identifier);
    
    /* Make HTTP request */
    char response[4096];
    result = http_get(request_url, response, sizeof(response), client->config.connection_timeout_ms);
    if (result != ODDSOCKETS_SUCCESS) {
        handle_error(client, ODDSOCKETS_ERROR_MANAGER_OFFLINE, "Manager is offline");
        return ODDSOCKETS_ERROR_MANAGER_OFFLINE;
    }
    
    /* Parse JSON response */
    json_object_t* json = json_parse(response);
    if (!json) {
        handle_error(client, ODDSOCKETS_ERROR_JSON_PARSE_ERROR, "Failed to parse worker assignment response");
        return ODDSOCKETS_ERROR_JSON_PARSE_ERROR;
    }
    
    /* Extract worker information */
    const char* worker_url = json_get_string(json, "url");
    const char* worker_id = json_get_string(json, "workerId");
    const char* session_id = json_get_string(json, "session.id");
    
    if (!worker_url || !worker_id) {
        json_free(json);
        handle_error(client, ODDSOCKETS_ERROR_WORKER_ASSIGNMENT_FAILED, "Invalid worker assignment response");
        return ODDSOCKETS_ERROR_WORKER_ASSIGNMENT_FAILED;
    }
    
    /* Store worker information */
    strncpy(client->worker_url, worker_url, sizeof(client->worker_url) - 1);
    strncpy(client->worker_id, worker_id, sizeof(client->worker_id) - 1);
    if (session_id) {
        strncpy(client->session_id, session_id, sizeof(client->session_id) - 1);
    }
    
    json_free(json);
    
    log_message(client, ODDSOCKETS_LOG_INFO, "Assigned to worker: %s (%s)", worker_id, worker_url);
    return ODDSOCKETS_SUCCESS;
}

/* ---- Socket.IO / Engine.IO framing helpers ----
 *
 * The worker speaks genuine Socket.IO (Engine.IO v4). The libwebsockets layer
 * only carries raw text frames, so the Engine.IO / Socket.IO packet framing is
 * implemented here, on top of it:
 *
 *   server OPEN  "0{...}"          -> we reply CONNECT "40{apiKey,userId}"
 *   server PING  "2"               -> we reply PONG "3"
 *   server MSG   "40{sid}"         -> handshake complete (state = CONNECTED)
 *   server EVENT "42[\"ev\",{..}]" -> routed to the owning channel
 *   we emit      "42[\"ev\",{..}]" for subscribe / publish / get_presence ...
 */

static const char* os_skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* Extract a JSON string starting at the opening quote; returns the pointer
   past the closing quote, or NULL. */
static const char* os_extract_string(const char* p, char* out, size_t out_size) {
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

/* Copy the raw JSON value for `key` out of an object string. Handles balanced
   objects/arrays, strings, and primitives - needed because the worker's message
   envelope carries the published body as a nested object under "message". */
static bool os_extract_raw_value(const char* obj, const char* key, char* out, size_t out_size) {
    char needle[160];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(obj, needle);
    if (!p) return false;
    p += strlen(needle);
    p = os_skip_ws(p);
    if (*p != ':') return false;
    p++;
    p = os_skip_ws(p);
    const char* start = p;
    if (*p == '{' || *p == '[') {
        char open = *p, close = (open == '{') ? '}' : ']';
        int depth = 0;
        while (*p) {
            if (*p == open) depth++;
            else if (*p == close) { depth--; if (depth == 0) { p++; break; } }
            p++;
        }
    } else if (*p == '"') {
        p++;
        while (*p && *p != '"') { if (*p == '\\' && *(p + 1)) p++; p++; }
        if (*p == '"') p++;
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    }
    size_t len = (size_t)(p - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

/* Decode a Socket.IO EVENT frame ("42[\"event\",payload]"). Writes the event
   name and points payload_out at the argument object (or NULL if none). */
static bool parse_socketio_event_frame(const char* frame, char* event_out,
                                       size_t event_size, const char** payload_out) {
    *payload_out = NULL;
    const char* p = strchr(frame, '[');
    if (!p) return false;
    p++;
    p = os_skip_ws(p);
    if (*p != '"') return false;
    p = os_extract_string(p, event_out, event_size);
    if (!p) return false;
    p = os_skip_ws(p);
    if (*p == ',') { p++; p = os_skip_ws(p); }
    if (*p && *p != ']') *payload_out = p;
    return true;
}

/* Send a Socket.IO CONNECT packet carrying auth (lands in handshake.auth,
   where the worker's io.use() middleware reads socket.handshake.auth.apiKey). */
static void send_socketio_connect(oddsockets_client_t* client) {
    char frame[ODDSOCKETS_MAX_API_KEY_LENGTH + ODDSOCKETS_MAX_USER_ID_LENGTH + 64];
    snprintf(frame, sizeof(frame),
             "40{\"apiKey\":\"%s\",\"userId\":\"%s\"}",
             client->config.api_key,
             client->config.user_id[0] ? client->config.user_id : client->client_identifier);
    websocket_client_send(client->websocket, frame);
}

/* Emit a Socket.IO EVENT packet: 42["event",<payload_json>]. */
static int send_socketio_event(oddsockets_client_t* client, const char* event,
                               const char* payload_json) {
    if (!client || !client->websocket) return ODDSOCKETS_ERROR_NOT_CONNECTED;
    size_t len = strlen(event) + (payload_json ? strlen(payload_json) : 0) + 16;
    char* frame = custom_malloc(len);
    if (!frame) return ODDSOCKETS_ERROR_MEMORY_ALLOCATION;
    if (payload_json && payload_json[0]) {
        snprintf(frame, len, "42[\"%s\",%s]", event, payload_json);
    } else {
        snprintf(frame, len, "42[\"%s\"]", event);
    }
    int result = websocket_client_send(client->websocket, frame);
    custom_free(frame);
    return result == 0 ? ODDSOCKETS_SUCCESS : ODDSOCKETS_ERROR_WEBSOCKET_ERROR;
}

/* Look up a channel by name. */
static oddsockets_channel_t* find_channel(oddsockets_client_t* client, const char* name) {
    oddsockets_channel_t* channel = NULL;
    pthread_mutex_lock(&client->mutex);
    for (int i = 0; i < client->channel_count; i++) {
        if (strcmp(client->channels[i]->name, name) == 0) { channel = client->channels[i]; break; }
    }
    pthread_mutex_unlock(&client->mutex);
    return channel;
}

/* Route a decoded Socket.IO event to the owning channel. */
static void dispatch_socketio_event(oddsockets_client_t* client, const char* event,
                                    const char* payload) {
    if (!payload) return;

    json_object_t* pj = json_parse(payload);
    const char* channel_name = pj ? json_get_string(pj, "channel") : NULL;
    oddsockets_channel_t* channel = channel_name ? find_channel(client, channel_name) : NULL;

    if (strcmp(event, "message") == 0 && channel && channel->message_callback) {
        /* The envelope carries the published body as a nested object under
           "message" - hand the raw JSON to the callback, falling back to the
           whole envelope if the field cannot be isolated. */
        char* body = custom_malloc(ODDSOCKETS_MAX_MESSAGE_SIZE);
        if (body) {
            if (!os_extract_raw_value(payload, "message", body, ODDSOCKETS_MAX_MESSAGE_SIZE)) {
                strncpy(body, payload, ODDSOCKETS_MAX_MESSAGE_SIZE - 1);
                body[ODDSOCKETS_MAX_MESSAGE_SIZE - 1] = '\0';
            }
            channel->message_callback(channel->name, body, channel->message_user_data);
            custom_free(body);
        }
    } else if (strcmp(event, "subscribed") == 0 && channel) {
        channel->subscribed = true;
        channel->subscribing = false;
        log_message(client, ODDSOCKETS_LOG_INFO, "Subscribed to channel: %s", channel->name);
    } else if (strcmp(event, "unsubscribed") == 0 && channel) {
        channel->subscribed = false;
        log_message(client, ODDSOCKETS_LOG_INFO, "Unsubscribed from channel: %s", channel->name);
    } else if (strcmp(event, "published") == 0) {
        /* Publish acknowledgement (messageId available in payload). */
    } else if (strcmp(event, "presence") == 0 && channel && channel->presence_callback) {
        char occ[32] = "0";
        os_extract_raw_value(payload, "occupancy", occ, sizeof(occ));
        channel->presence_callback(channel->name, "occupancy", occ, channel->presence_user_data);
    } else if (strcmp(event, "error") == 0) {
        char msg[256] = "worker error";
        os_extract_raw_value(payload, "message", msg, sizeof(msg));
        handle_error(client, ODDSOCKETS_ERROR_WEBSOCKET_ERROR, msg);
    }

    if (pj) json_free(pj);
}

/* WebSocket Event Handlers */
static void on_websocket_connected(void* user_data) {
    oddsockets_client_t* client = (oddsockets_client_t*)user_data;
    /* The raw WebSocket is up, but we are not a live Socket.IO client until the
       Engine.IO OPEN -> CONNECT -> CONNECT-ack handshake completes. */
    log_message(client, ODDSOCKETS_LOG_DEBUG, "WebSocket established, awaiting Socket.IO handshake");
}

static void on_websocket_disconnected(void* user_data) {
    oddsockets_client_t* client = (oddsockets_client_t*)user_data;
    set_state(client, ODDSOCKETS_STATE_DISCONNECTED);

    /* Schedule reconnection if not manually disconnected */
    if (client->reconnect_attempts < client->config.reconnect_attempts) {
        set_state(client, ODDSOCKETS_STATE_RECONNECTING);
        client->reconnect_attempts++;

        /* Exponential backoff */
        int delay = client->config.reconnect_delay_ms * (1 << (client->reconnect_attempts - 1));
        if (delay > 30000) delay = 30000; /* Max 30 seconds */

        log_message(client, ODDSOCKETS_LOG_INFO, "Reconnecting in %dms (attempt %d/%d)",
                   delay, client->reconnect_attempts, client->config.reconnect_attempts);

        usleep(delay * 1000);
        oddsockets_connect(client);
    }
}

static void on_websocket_message(const char* message, void* user_data) {
    oddsockets_client_t* client = (oddsockets_client_t*)user_data;
    if (!message || !message[0]) return;

    /* The Engine.IO packet type is the leading digit of the frame. */
    switch (message[0]) {
        case '0': /* OPEN - reply with a Socket.IO CONNECT carrying auth */
            send_socketio_connect(client);
            break;
        case '2': /* PING - reply PONG */
            websocket_client_send(client->websocket, "3");
            break;
        case '3': /* PONG */
            break;
        case '4': /* MESSAGE - a Socket.IO packet follows */
            switch (message[1]) {
                case '0': /* CONNECT ack - the handshake is complete */
                    set_state(client, ODDSOCKETS_STATE_CONNECTED);
                    client->reconnect_attempts = 0;
                    break;
                case '1': /* DISCONNECT */
                    set_state(client, ODDSOCKETS_STATE_DISCONNECTED);
                    break;
                case '2': { /* EVENT */
                    char event[64];
                    const char* payload = NULL;
                    if (parse_socketio_event_frame(message, event, sizeof(event), &payload)) {
                        dispatch_socketio_event(client, event, payload);
                    }
                    break;
                }
                case '4': /* CONNECT_ERROR */
                    handle_error(client, ODDSOCKETS_ERROR_CONNECTION_FAILED, "Socket.IO connect error");
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void on_websocket_error(const char* error_message, void* user_data) {
    oddsockets_client_t* client = (oddsockets_client_t*)user_data;
    handle_error(client, ODDSOCKETS_ERROR_WEBSOCKET_ERROR, error_message);
}

/* Public API Implementation */

void oddsockets_config_init(oddsockets_config_t* config, const char* api_key) {
    if (!config || !api_key) return;
    
    memset(config, 0, sizeof(oddsockets_config_t));
    
    strncpy(config->api_key, api_key, sizeof(config->api_key) - 1);
    strncpy(config->manager_url, ODDSOCKETS_DEFAULT_MANAGER_URL, sizeof(config->manager_url) - 1);
    
    config->auto_connect = true;
    config->reconnect_attempts = ODDSOCKETS_DEFAULT_RECONNECT_ATTEMPTS;
    config->reconnect_delay_ms = ODDSOCKETS_DEFAULT_RECONNECT_DELAY_MS;
    config->connection_timeout_ms = ODDSOCKETS_DEFAULT_CONNECTION_TIMEOUT_MS;
    config->message_timeout_ms = ODDSOCKETS_DEFAULT_MESSAGE_TIMEOUT_MS;
    config->enable_ssl = true;
    config->ssl_verify_peer = true;
    config->log_level = ODDSOCKETS_LOG_INFO;
}

oddsockets_client_t* oddsockets_create(const oddsockets_config_t* config) {
    if (!config || !config->api_key[0]) {
        return NULL;
    }
    
    oddsockets_client_t* client = custom_malloc(sizeof(oddsockets_client_t));
    if (!client) {
        return NULL;
    }
    
    memset(client, 0, sizeof(oddsockets_client_t));
    memcpy(&client->config, config, sizeof(oddsockets_config_t));
    
    client->state = ODDSOCKETS_STATE_DISCONNECTED;
    client->allocated_bytes = sizeof(oddsockets_client_t);
    client->peak_bytes = client->allocated_bytes;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&client->mutex, NULL) != 0) {
        custom_free(client);
        return NULL;
    }
    
    /* Generate client identifier */
    generate_client_identifier(client);
    
    /* Auto-connect if requested */
    if (config->auto_connect) {
        if (oddsockets_connect(client) != ODDSOCKETS_SUCCESS) {
            oddsockets_destroy(client);
            return NULL;
        }
    }
    
    log_message(client, ODDSOCKETS_LOG_INFO, "OddSockets client created (version %s)", ODDSOCKETS_VERSION_STRING);
    return client;
}

int oddsockets_connect(oddsockets_client_t* client) {
    if (!client) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    if (client->state == ODDSOCKETS_STATE_CONNECTED || client->state == ODDSOCKETS_STATE_CONNECTING) {
        return ODDSOCKETS_ERROR_ALREADY_CONNECTED;
    }
    
    set_state(client, ODDSOCKETS_STATE_CONNECTING);
    
    /* Get worker assignment */
    int result = get_worker_assignment(client);
    if (result != ODDSOCKETS_SUCCESS) {
        set_state(client, ODDSOCKETS_STATE_ERROR);
        return result;
    }
    
    /* Create WebSocket client (array members are copied, not aliased). */
    websocket_config_t ws_config;
    memset(&ws_config, 0, sizeof(ws_config));
    strncpy(ws_config.url, client->worker_url, sizeof(ws_config.url) - 1);
    strncpy(ws_config.ca_cert_path, client->config.ca_cert_path, sizeof(ws_config.ca_cert_path) - 1);
    ws_config.enable_ssl = client->config.enable_ssl;
    ws_config.ssl_verify_peer = client->config.ssl_verify_peer;
    ws_config.connection_timeout_ms = client->config.connection_timeout_ms;
    ws_config.on_connected = on_websocket_connected;
    ws_config.on_disconnected = on_websocket_disconnected;
    ws_config.on_message = on_websocket_message;
    ws_config.on_error = on_websocket_error;
    ws_config.user_data = client;

    /* Auth travels in the Socket.IO handshake (send_socketio_connect), not an
       HTTP header, but keep the header populated for transports that use it. */
    snprintf(ws_config.auth_header, sizeof(ws_config.auth_header),
             "Authorization: Bearer %s", client->config.api_key);
    
    client->websocket = websocket_client_create(&ws_config);
    if (!client->websocket) {
        set_state(client, ODDSOCKETS_STATE_ERROR);
        handle_error(client, ODDSOCKETS_ERROR_CONNECTION_FAILED, "Failed to create WebSocket client");
        return ODDSOCKETS_ERROR_CONNECTION_FAILED;
    }
    
    result = websocket_client_connect(client->websocket);
    if (result != ODDSOCKETS_SUCCESS) {
        websocket_client_destroy(client->websocket);
        client->websocket = NULL;
        set_state(client, ODDSOCKETS_STATE_ERROR);
        handle_error(client, ODDSOCKETS_ERROR_CONNECTION_FAILED, "Failed to connect WebSocket");
        return ODDSOCKETS_ERROR_CONNECTION_FAILED;
    }
    
    return ODDSOCKETS_SUCCESS;
}

int oddsockets_disconnect(oddsockets_client_t* client) {
    if (!client) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    if (client->websocket) {
        websocket_client_disconnect(client->websocket);
        websocket_client_destroy(client->websocket);
        client->websocket = NULL;
    }
    
    set_state(client, ODDSOCKETS_STATE_DISCONNECTED);
    return ODDSOCKETS_SUCCESS;
}

oddsockets_state_t oddsockets_get_state(oddsockets_client_t* client) {
    if (!client) {
        return ODDSOCKETS_STATE_ERROR;
    }
    
    return client->state;
}

int oddsockets_get_worker_info(oddsockets_client_t* client, oddsockets_worker_info_t* worker_info) {
    if (!client || !worker_info) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    if (!client->worker_id[0]) {
        return ODDSOCKETS_ERROR_NOT_CONNECTED;
    }
    
    strncpy(worker_info->worker_id, client->worker_id, sizeof(worker_info->worker_id) - 1);
    strncpy(worker_info->worker_url, client->worker_url, sizeof(worker_info->worker_url) - 1);
    strncpy(worker_info->session_id, client->session_id, sizeof(worker_info->session_id) - 1);
    
    return ODDSOCKETS_SUCCESS;
}

int oddsockets_process_events(oddsockets_client_t* client) {
    if (!client) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    if (client->websocket) {
        return websocket_client_process_events(client->websocket);
    }
    
    return ODDSOCKETS_SUCCESS;
}

void oddsockets_destroy(oddsockets_client_t* client) {
    if (!client) return;
    
    /* Disconnect if connected */
    oddsockets_disconnect(client);

    /* Destroy all channels. oddsockets_channel_destroy() takes client->mutex and
       removes the channel from the list itself, so it must NOT be called while
       the mutex is held (the mutex is not recursive) - draining from the head
       lets each destroy remove its own entry. */
    while (client->channel_count > 0) {
        oddsockets_channel_destroy(client->channels[0]);
    }

    /* Cleanup */
    pthread_mutex_destroy(&client->mutex);
    
    log_message(client, ODDSOCKETS_LOG_INFO, "OddSockets client destroyed");
    custom_free(client);
}

/* Channel Implementation */

oddsockets_channel_t* oddsockets_channel_create(oddsockets_client_t* client, const char* channel_name) {
    if (!client || !channel_name || strlen(channel_name) >= ODDSOCKETS_MAX_CHANNEL_NAME_LENGTH) {
        return NULL;
    }
    
    pthread_mutex_lock(&client->mutex);
    
    /* Check if channel already exists */
    for (int i = 0; i < client->channel_count; i++) {
        if (strcmp(client->channels[i]->name, channel_name) == 0) {
            pthread_mutex_unlock(&client->mutex);
            return client->channels[i];
        }
    }
    
    /* Check channel limit */
    if (client->channel_count >= ODDSOCKETS_MAX_CHANNELS) {
        pthread_mutex_unlock(&client->mutex);
        handle_error(client, ODDSOCKETS_ERROR_MEMORY_ALLOCATION, "Maximum number of channels reached");
        return NULL;
    }
    
    /* Create new channel */
    oddsockets_channel_t* channel = custom_malloc(sizeof(oddsockets_channel_t));
    if (!channel) {
        pthread_mutex_unlock(&client->mutex);
        handle_error(client, ODDSOCKETS_ERROR_MEMORY_ALLOCATION, "Failed to allocate channel");
        return NULL;
    }
    
    memset(channel, 0, sizeof(oddsockets_channel_t));
    strncpy(channel->name, channel_name, sizeof(channel->name) - 1);
    channel->client = client;
    channel->max_history = 100; /* Default */
    
    /* Add to client's channel list */
    client->channels[client->channel_count++] = channel;
    client->allocated_bytes += sizeof(oddsockets_channel_t);
    if (client->allocated_bytes > client->peak_bytes) {
        client->peak_bytes = client->allocated_bytes;
    }
    
    pthread_mutex_unlock(&client->mutex);
    
    log_message(client, ODDSOCKETS_LOG_INFO, "Channel created: %s", channel_name);
    return channel;
}

int oddsockets_channel_subscribe(oddsockets_channel_t* channel,
                                oddsockets_message_callback_t callback,
                                void* user_data,
                                const oddsockets_subscribe_options_t* options) {
    if (!channel || !callback) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    if (channel->subscribed || channel->subscribing) {
        return ODDSOCKETS_ERROR_ALREADY_SUBSCRIBED;
    }
    
    if (channel->client->state != ODDSOCKETS_STATE_CONNECTED) {
        return ODDSOCKETS_ERROR_NOT_CONNECTED;
    }
    
    /* Set callback and options */
    channel->message_callback = callback;
    channel->message_user_data = user_data;
    
    if (options) {
        channel->subscribe_options = *options;
        channel->max_history = options->max_history > 0 ? options->max_history : 100;
    } else {
        /* Default options */
        channel->subscribe_options.max_history = 100;
        channel->subscribe_options.retain_history = true;
        channel->subscribe_options.enable_presence = false;
    }
    
    /* Emit a Socket.IO "subscribe" event (options in camelCase, as the worker
       reads them). */
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"channel\":\"%s\",\"options\":{\"maxHistory\":%d,\"retainHistory\":%s,\"enablePresence\":%s}}",
             channel->name,
             channel->subscribe_options.max_history,
             channel->subscribe_options.retain_history ? "true" : "false",
             channel->subscribe_options.enable_presence ? "true" : "false");

    channel->subscribing = true;

    int result = send_socketio_event(channel->client, "subscribe", payload);
    if (result != ODDSOCKETS_SUCCESS) {
        channel->subscribing = false;
        handle_error(channel->client, result, "Failed to send subscribe message");
        return result;
    }
    
    log_message(channel->client, ODDSOCKETS_LOG_INFO, "Subscribing to channel: %s", channel->name);
    return ODDSOCKETS_SUCCESS;
}

int oddsockets_channel_unsubscribe(oddsockets_channel_t* channel) {
    if (!channel) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    if (!channel->subscribed) {
        return ODDSOCKETS_ERROR_NOT_SUBSCRIBED;
    }
    
    if (channel->client->state != ODDSOCKETS_STATE_CONNECTED) {
        return ODDSOCKETS_ERROR_NOT_CONNECTED;
    }
    
    /* Emit a Socket.IO "unsubscribe" event. */
    char payload[192];
    snprintf(payload, sizeof(payload), "{\"channel\":\"%s\"}", channel->name);

    int result = send_socketio_event(channel->client, "unsubscribe", payload);
    if (result != ODDSOCKETS_SUCCESS) {
        handle_error(channel->client, result, "Failed to send unsubscribe message");
        return result;
    }
    
    log_message(channel->client, ODDSOCKETS_LOG_INFO, "Unsubscribing from channel: %s", channel->name);
    return ODDSOCKETS_SUCCESS;
}

int oddsockets_channel_publish(oddsockets_channel_t* channel,
                              const char* message,
                              const oddsockets_publish_options_t* options) {
    if (!channel || !message) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    if (channel->client->state != ODDSOCKETS_STATE_CONNECTED) {
        return ODDSOCKETS_ERROR_NOT_CONNECTED;
    }
    
    /* Validate message size */
    int validation_result = oddsockets_validate_message_size(message);
    if (validation_result != ODDSOCKETS_SUCCESS) {
        return validation_result;
    }
    
    /* Embed the message raw when it is already JSON (an object or array),
       otherwise as a JSON string. This lets callers publish structured bodies
       - e.g. {"text":"..","nonce":".."} - which the worker delivers verbatim. */
    const char* m = message;
    while (*m == ' ' || *m == '\t') m++;
    bool raw = (*m == '{' || *m == '[');

    size_t cap = strlen(message) + 768;
    char* payload = custom_malloc(cap);
    if (!payload) {
        return ODDSOCKETS_ERROR_MEMORY_ALLOCATION;
    }

    int n = snprintf(payload, cap, "{\"channel\":\"%s\",\"message\":", channel->name);
    if (raw) {
        n += snprintf(payload + n, cap - n, "%s", message);
    } else {
        n += snprintf(payload + n, cap - n, "\"%s\"", message);
    }

    /* Only attach options when provided - the worker's handler defaults them on
       `undefined`, and a JSON null would crash it. */
    if (options) {
        if (options->metadata) {
            n += snprintf(payload + n, cap - n,
                          ",\"options\":{\"ttl\":%d,\"storeInHistory\":%s,\"metadata\":\"%s\"}",
                          options->ttl_seconds, options->store_in_history ? "true" : "false",
                          options->metadata);
        } else {
            n += snprintf(payload + n, cap - n,
                          ",\"options\":{\"ttl\":%d,\"storeInHistory\":%s}",
                          options->ttl_seconds, options->store_in_history ? "true" : "false");
        }
    }
    snprintf(payload + n, cap - n, "}");

    int result = send_socketio_event(channel->client, "publish", payload);
    custom_free(payload);

    if (result != ODDSOCKETS_SUCCESS) {
        handle_error(channel->client, result, "Failed to send publish message");
        return result;
    }
    
    log_message(channel->client, ODDSOCKETS_LOG_DEBUG, "Published to channel: %s", channel->name);
    return ODDSOCKETS_SUCCESS;
}

int oddsockets_channel_get_presence(oddsockets_channel_t* channel,
                                   oddsockets_presence_callback_t callback,
                                   void* user_data) {
    if (!channel || !callback) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }

    if (channel->client->state != ODDSOCKETS_STATE_CONNECTED) {
        return ODDSOCKETS_ERROR_NOT_CONNECTED;
    }

    channel->presence_callback = callback;
    channel->presence_user_data = user_data;

    /* Emit a Socket.IO "get_presence" event; the worker replies with a
       "presence" event carrying occupancy, routed back to the callback. */
    char payload[192];
    snprintf(payload, sizeof(payload), "{\"channel\":\"%s\"}", channel->name);
    return send_socketio_event(channel->client, "get_presence", payload);
}

bool oddsockets_channel_is_subscribed(oddsockets_channel_t* channel) {
    return channel ? channel->subscribed : false;
}

const char* oddsockets_channel_get_name(oddsockets_channel_t* channel) {
    return channel ? channel->name : NULL;
}

void oddsockets_channel_destroy(oddsockets_channel_t* channel) {
    if (!channel) return;
    
    /* Unsubscribe if subscribed */
    if (channel->subscribed) {
        oddsockets_channel_unsubscribe(channel);
    }
    
    /* Free message history */
    if (channel->message_history) {
        for (int i = 0; i < channel->history_count; i++) {
            custom_free(channel->message_history[i]);
        }
        custom_free(channel->message_history);
    }
    
    /* Remove from client's channel list */
    if (channel->client) {
        pthread_mutex_lock(&channel->client->mutex);
        for (int i = 0; i < channel->client->channel_count; i++) {
            if (channel->client->channels[i] == channel) {
                /* Shift remaining channels */
                for (int j = i; j < channel->client->channel_count - 1; j++) {
                    channel->client->channels[j] = channel->client->channels[j + 1];
                }
                channel->client->channel_count--;
                channel->client->allocated_bytes -= sizeof(oddsockets_channel_t);
                break;
            }
        }
        pthread_mutex_unlock(&channel->client->mutex);
        
        log_message(channel->client, ODDSOCKETS_LOG_INFO, "Channel destroyed: %s", channel->name);
    }
    
    custom_free(channel);
}

/* Utility Functions */

const char* oddsockets_error_string(oddsockets_error_t error) {
    switch (error) {
        case ODDSOCKETS_SUCCESS: return "Success";
        case ODDSOCKETS_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case ODDSOCKETS_ERROR_INVALID_API_KEY: return "Invalid API key";
        case ODDSOCKETS_ERROR_CONNECTION_FAILED: return "Connection failed";
        case ODDSOCKETS_ERROR_TIMEOUT: return "Timeout";
        case ODDSOCKETS_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case ODDSOCKETS_ERROR_MESSAGE_TOO_LARGE: return "Message too large";
        case ODDSOCKETS_ERROR_CHANNEL_NOT_FOUND: return "Channel not found";
        case ODDSOCKETS_ERROR_NOT_CONNECTED: return "Not connected";
        case ODDSOCKETS_ERROR_ALREADY_CONNECTED: return "Already connected";
        case ODDSOCKETS_ERROR_ALREADY_SUBSCRIBED: return "Already subscribed";
        case ODDSOCKETS_ERROR_NOT_SUBSCRIBED: return "Not subscribed";
        case ODDSOCKETS_ERROR_WEBSOCKET_ERROR: return "WebSocket error";
        case ODDSOCKETS_ERROR_HTTP_ERROR: return "HTTP error";
        case ODDSOCKETS_ERROR_JSON_PARSE_ERROR: return "JSON parse error";
        case ODDSOCKETS_ERROR_SSL_ERROR: return "SSL error";
        case ODDSOCKETS_ERROR_MANAGER_OFFLINE: return "Manager offline";
        case ODDSOCKETS_ERROR_WORKER_ASSIGNMENT_FAILED: return "Worker assignment failed";
        default: return "Unknown error";
    }
}

const char* oddsockets_state_string(oddsockets_state_t state) {
    switch (state) {
        case ODDSOCKETS_STATE_DISCONNECTED: return "Disconnected";
        case ODDSOCKETS_STATE_CONNECTING: return "Connecting";
        case ODDSOCKETS_STATE_CONNECTED: return "Connected";
        case ODDSOCKETS_STATE_RECONNECTING: return "Reconnecting";
        case ODDSOCKETS_STATE_ERROR: return "Error";
        default: return "Unknown";
    }
}

int oddsockets_validate_message_size(const char* message) {
    if (!message) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    
    size_t message_size = strlen(message);
    if (message_size > ODDSOCKETS_MAX_MESSAGE_SIZE) {
        return ODDSOCKETS_ERROR_MESSAGE_TOO_LARGE;
    }
    
    return ODDSOCKETS_SUCCESS;
}

const char* oddsockets_get_version(void) {
    return ODDSOCKETS_VERSION_STRING;
}

void oddsockets_set_memory_functions(void* (*malloc_func)(size_t),
                                    void (*free_func)(void*),
                                    void* (*realloc_func)(void*, size_t)) {
    if (malloc_func && free_func && realloc_func) {
        custom_malloc = malloc_func;
        custom_free = free_func;
        custom_realloc = realloc_func;
    }
}

int oddsockets_get_memory_stats(size_t* allocated_bytes, size_t* peak_bytes) {
    /* This would require a more sophisticated memory tracking system */
    /* For now, return basic stats if available */
    if (allocated_bytes) *allocated_bytes = 0;
    if (peak_bytes) *peak_bytes = 0;
    return ODDSOCKETS_SUCCESS;
}
