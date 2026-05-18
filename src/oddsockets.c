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
#include "message_validator.h"
#include "json_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* WebSocket Event Handlers */
static void on_websocket_connected(void* user_data) {
    oddsockets_client_t* client = (oddsockets_client_t*)user_data;
    set_state(client, ODDSOCKETS_STATE_CONNECTED);
    client->reconnect_attempts = 0;
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
    
    /* Parse message */
    json_object_t* json = json_parse(message);
    if (!json) {
        log_message(client, ODDSOCKETS_LOG_WARN, "Failed to parse WebSocket message: %s", message);
        return;
    }
    
    const char* event_type = json_get_string(json, "type");
    const char* channel_name = json_get_string(json, "channel");
    
    if (!event_type) {
        json_free(json);
        return;
    }
    
    /* Find channel */
    oddsockets_channel_t* channel = NULL;
    if (channel_name) {
        pthread_mutex_lock(&client->mutex);
        for (int i = 0; i < client->channel_count; i++) {
            if (strcmp(client->channels[i]->name, channel_name) == 0) {
                channel = client->channels[i];
                break;
            }
        }
        pthread_mutex_unlock(&client->mutex);
    }
    
    /* Handle different event types */
    if (strcmp(event_type, "message") == 0 && channel) {
        const char* msg_content = json_get_string(json, "message");
        if (msg_content && channel->message_callback) {
            channel->message_callback(channel_name, msg_content, channel->message_user_data);
        }
    }
    else if (strcmp(event_type, "subscribed") == 0 && channel) {
        channel->subscribed = true;
        channel->subscribing = false;
        log_message(client, ODDSOCKETS_LOG_INFO, "Subscribed to channel: %s", channel_name);
    }
    else if (strcmp(event_type, "unsubscribed") == 0 && channel) {
        channel->subscribed = false;
        log_message(client, ODDSOCKETS_LOG_INFO, "Unsubscribed from channel: %s", channel_name);
    }
    
    json_free(json);
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
    
    /* Create WebSocket client */
    websocket_config_t ws_config = {
        .url = client->worker_url,
        .enable_ssl = client->config.enable_ssl,
        .ssl_verify_peer = client->config.ssl_verify_peer,
        .ca_cert_path = client->config.ca_cert_path,
        .connection_timeout_ms = client->config.connection_timeout_ms,
        .on_connected = on_websocket_connected,
        .on_disconnected = on_websocket_disconnected,
        .on_message = on_websocket_message,
        .on_error = on_websocket_error,
        .user_data = client
    };
    
    /* Add authentication headers */
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
    
    /* Destroy all channels */
    pthread_mutex_lock(&client->mutex);
    for (int i = 0; i < client->channel_count; i++) {
        oddsockets_channel_destroy(client->channels[i]);
    }
    pthread_mutex_unlock(&client->mutex);
    
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
    
    /* Send subscribe message */
    char subscribe_msg[512];
    snprintf(subscribe_msg, sizeof(subscribe_msg),
             "{\"type\":\"subscribe\",\"channel\": \"%s\",\"options\":{\"maxHistory\":%d,\"retainHistory\":%s,\"enablePresence\":%s}}",
             channel->name,
             channel->subscribe_options.max_history,
             channel->subscribe_options.retain_history ? "true" : "false",
             channel->subscribe_options.enable_presence ? "true" : "false");
    
    channel->subscribing = true;
    
    int result = websocket_client_send(channel->client->websocket, subscribe_msg);
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
    
    /* Send unsubscribe message */
    char unsubscribe_msg[256];
    snprintf(unsubscribe_msg, sizeof(unsubscribe_msg),
             "{\"type\":\"unsubscribe\",\"channel\":\"%s\"}", channel->name);
    
    int result = websocket_client_send(channel->client->websocket, unsubscribe_msg);
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
    
    /* Build publish message */
    char* publish_msg = custom_malloc(strlen(message) + 512);
    if (!publish_msg) {
        return ODDSOCKETS_ERROR_MEMORY_ALLOCATION;
    }
    
    if (options && options->metadata) {
        snprintf(publish_msg, strlen(message) + 512,
                 "{\"type\":\"publish\",\"channel\":\"%s\",\"message\":\"%s\",\"options\":{\"ttl\":%d,\"metadata\":\"%s\",\"storeInHistory\":%s}}",
                 channel->name, message,
                 options->ttl_seconds,
                 options->metadata,
                 options->store_in_history ? "true" : "false");
    } else {
        snprintf(publish_msg, strlen(message) + 512,
                 "{\"type\":\"publish\",\"channel\":\"%s\",\"message\":\"%s\"}",
                 channel->name, message);
    }
    
    int result = websocket_client_send(channel->client->websocket, publish_msg);
    custom_free(publish_msg);
    
    if (result != ODDSOCKETS_SUCCESS) {
        handle_error(channel->client, result, "Failed to send publish message");
        return result;
    }
    
    log_message(channel->client, ODDSOCKETS_LOG_DEBUG, "Published to channel: %s", channel->name);
    return ODDSOCKETS_SUCCESS;
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
