/**
 * OddSockets C SDK
 * 
 * A lightweight C SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#ifndef ODDSOCKETS_H
#define ODDSOCKETS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Version Information */
#define ODDSOCKETS_VERSION_MAJOR 1
#define ODDSOCKETS_VERSION_MINOR 0
#define ODDSOCKETS_VERSION_PATCH 0
#define ODDSOCKETS_VERSION_STRING "1.0.0"

/* Configuration Constants */
#define ODDSOCKETS_MAX_API_KEY_LENGTH 256
#define ODDSOCKETS_MAX_USER_ID_LENGTH 128
#define ODDSOCKETS_MAX_CHANNEL_NAME_LENGTH 128
#define ODDSOCKETS_MAX_URL_LENGTH 512
#define ODDSOCKETS_MAX_MESSAGE_SIZE 32768  /* 32KB - industry standard */
#define ODDSOCKETS_MAX_CHANNELS 32
#define ODDSOCKETS_MAX_CALLBACKS 16

/* Default Configuration Values */
#define ODDSOCKETS_DEFAULT_RECONNECT_ATTEMPTS 5
#define ODDSOCKETS_DEFAULT_RECONNECT_DELAY_MS 1000
#define ODDSOCKETS_DEFAULT_CONNECTION_TIMEOUT_MS 10000
#define ODDSOCKETS_DEFAULT_MESSAGE_TIMEOUT_MS 5000
#define ODDSOCKETS_DEFAULT_MANAGER_URL "https://manager1.oddsockets.tyga.network"

/* Error Codes */
typedef enum {
    ODDSOCKETS_SUCCESS = 0,
    ODDSOCKETS_ERROR_INVALID_PARAMETER = -1,
    ODDSOCKETS_ERROR_INVALID_API_KEY = -2,
    ODDSOCKETS_ERROR_CONNECTION_FAILED = -3,
    ODDSOCKETS_ERROR_TIMEOUT = -4,
    ODDSOCKETS_ERROR_MEMORY_ALLOCATION = -5,
    ODDSOCKETS_ERROR_MESSAGE_TOO_LARGE = -6,
    ODDSOCKETS_ERROR_CHANNEL_NOT_FOUND = -7,
    ODDSOCKETS_ERROR_NOT_CONNECTED = -8,
    ODDSOCKETS_ERROR_ALREADY_CONNECTED = -9,
    ODDSOCKETS_ERROR_ALREADY_SUBSCRIBED = -10,
    ODDSOCKETS_ERROR_NOT_SUBSCRIBED = -11,
    ODDSOCKETS_ERROR_WEBSOCKET_ERROR = -12,
    ODDSOCKETS_ERROR_HTTP_ERROR = -13,
    ODDSOCKETS_ERROR_JSON_PARSE_ERROR = -14,
    ODDSOCKETS_ERROR_SSL_ERROR = -15,
    ODDSOCKETS_ERROR_MANAGER_OFFLINE = -16,
    ODDSOCKETS_ERROR_WORKER_ASSIGNMENT_FAILED = -17,
    ODDSOCKETS_ERROR_UNKNOWN = -99
} oddsockets_error_t;

/* Connection States */
typedef enum {
    ODDSOCKETS_STATE_DISCONNECTED = 0,
    ODDSOCKETS_STATE_CONNECTING = 1,
    ODDSOCKETS_STATE_CONNECTED = 2,
    ODDSOCKETS_STATE_RECONNECTING = 3,
    ODDSOCKETS_STATE_ERROR = 4
} oddsockets_state_t;

/* Log Levels */
typedef enum {
    ODDSOCKETS_LOG_NONE = 0,
    ODDSOCKETS_LOG_ERROR = 1,
    ODDSOCKETS_LOG_WARN = 2,
    ODDSOCKETS_LOG_INFO = 3,
    ODDSOCKETS_LOG_DEBUG = 4
} oddsockets_log_level_t;

/* Forward Declarations */
typedef struct oddsockets_client oddsockets_client_t;
typedef struct oddsockets_channel oddsockets_channel_t;

/* Callback Function Types */
typedef void (*oddsockets_message_callback_t)(const char* channel_name, 
                                              const char* message, 
                                              void* user_data);

typedef void (*oddsockets_connection_callback_t)(oddsockets_state_t state, 
                                                void* user_data);

typedef void (*oddsockets_error_callback_t)(oddsockets_error_t error, 
                                           const char* message, 
                                           void* user_data);

typedef void (*oddsockets_history_callback_t)(const char* channel_name,
                                             const char** messages,
                                             size_t message_count,
                                             void* user_data);

typedef void (*oddsockets_presence_callback_t)(const char* channel_name,
                                              const char* action,
                                              const char* user_id,
                                              void* user_data);

typedef void (*oddsockets_log_callback_t)(oddsockets_log_level_t level,
                                         const char* message,
                                         void* user_data);

/* Configuration Structure */
typedef struct {
    /* Required */
    char api_key[ODDSOCKETS_MAX_API_KEY_LENGTH];
    
    /* Optional */
    char user_id[ODDSOCKETS_MAX_USER_ID_LENGTH];
    char manager_url[ODDSOCKETS_MAX_URL_LENGTH];
    
    /* Connection Options */
    bool auto_connect;
    int reconnect_attempts;
    int reconnect_delay_ms;
    int connection_timeout_ms;
    int message_timeout_ms;
    
    /* SSL/TLS Options */
    bool enable_ssl;
    bool ssl_verify_peer;
    char ca_cert_path[ODDSOCKETS_MAX_URL_LENGTH];
    
    /* Logging */
    oddsockets_log_level_t log_level;
    oddsockets_log_callback_t log_callback;
    void* log_user_data;
    
    /* Callbacks */
    oddsockets_connection_callback_t connection_callback;
    void* connection_user_data;
    
    oddsockets_error_callback_t error_callback;
    void* error_user_data;
} oddsockets_config_t;

/* Publish Options */
typedef struct {
    int ttl_seconds;
    char* metadata;
    bool store_in_history;
} oddsockets_publish_options_t;

/* History Options */
typedef struct {
    int count;
    char* start_time;  /* ISO 8601 format */
    char* end_time;    /* ISO 8601 format */
} oddsockets_history_options_t;

/* Subscribe Options */
typedef struct {
    int max_history;
    bool retain_history;
    bool enable_presence;
} oddsockets_subscribe_options_t;

/* Worker Information */
typedef struct {
    char worker_id[64];
    char worker_url[ODDSOCKETS_MAX_URL_LENGTH];
    char session_id[128];
} oddsockets_worker_info_t;

/* Client Management Functions */

/**
 * Create a new OddSockets client
 * @param config Configuration structure
 * @return Client instance or NULL on error
 */
oddsockets_client_t* oddsockets_create(const oddsockets_config_t* config);

/**
 * Connect to the OddSockets platform
 * @param client Client instance
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_connect(oddsockets_client_t* client);

/**
 * Disconnect from the platform
 * @param client Client instance
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_disconnect(oddsockets_client_t* client);

/**
 * Get current connection state
 * @param client Client instance
 * @return Current connection state
 */
oddsockets_state_t oddsockets_get_state(oddsockets_client_t* client);

/**
 * Get assigned worker information
 * @param client Client instance
 * @param worker_info Output structure for worker information
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_get_worker_info(oddsockets_client_t* client, 
                              oddsockets_worker_info_t* worker_info);

/**
 * Process pending events (call regularly in main loop)
 * @param client Client instance
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_process_events(oddsockets_client_t* client);

/**
 * Destroy client and free resources
 * @param client Client instance
 */
void oddsockets_destroy(oddsockets_client_t* client);

/* Channel Management Functions */

/**
 * Create a channel instance
 * @param client Client instance
 * @param channel_name Channel name
 * @return Channel instance or NULL on error
 */
oddsockets_channel_t* oddsockets_channel_create(oddsockets_client_t* client, 
                                               const char* channel_name);

/**
 * Subscribe to a channel
 * @param channel Channel instance
 * @param callback Message callback function
 * @param user_data User data passed to callback
 * @param options Subscribe options (can be NULL for defaults)
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_channel_subscribe(oddsockets_channel_t* channel,
                                oddsockets_message_callback_t callback,
                                void* user_data,
                                const oddsockets_subscribe_options_t* options);

/**
 * Unsubscribe from a channel
 * @param channel Channel instance
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_channel_unsubscribe(oddsockets_channel_t* channel);

/**
 * Publish a message to a channel
 * @param channel Channel instance
 * @param message Message to publish
 * @param options Publish options (can be NULL for defaults)
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_channel_publish(oddsockets_channel_t* channel,
                              const char* message,
                              const oddsockets_publish_options_t* options);

/**
 * Get message history for a channel
 * @param channel Channel instance
 * @param callback History callback function
 * @param user_data User data passed to callback
 * @param options History options (can be NULL for defaults)
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_channel_get_history(oddsockets_channel_t* channel,
                                  oddsockets_history_callback_t callback,
                                  void* user_data,
                                  const oddsockets_history_options_t* options);

/**
 * Get presence information for a channel
 * @param channel Channel instance
 * @param callback Presence callback function
 * @param user_data User data passed to callback
 * @return ODDSOCKETS_SUCCESS on success, error code on failure
 */
int oddsockets_channel_get_presence(oddsockets_channel_t* channel,
                                   oddsockets_presence_callback_t callback,
                                   void* user_data);

/**
 * Check if channel is subscribed
 * @param channel Channel instance
 * @return true if subscribed, false otherwise
 */
bool oddsockets_channel_is_subscribed(oddsockets_channel_t* channel);

/**
 * Get channel name
 * @param channel Channel instance
 * @return Channel name or NULL on error
 */
const char* oddsockets_channel_get_name(oddsockets_channel_t* channel);

/**
 * Destroy channel and free resources
 * @param channel Channel instance
 */
void oddsockets_channel_destroy(oddsockets_channel_t* channel);

/* Utility Functions */

/**
 * Get error message for error code
 * @param error Error code
 * @return Error message string
 */
const char* oddsockets_error_string(oddsockets_error_t error);

/**
 * Get state name for state code
 * @param state State code
 * @return State name string
 */
const char* oddsockets_state_string(oddsockets_state_t state);

/**
 * Validate message size
 * @param message Message to validate
 * @return ODDSOCKETS_SUCCESS if valid, ODDSOCKETS_ERROR_MESSAGE_TOO_LARGE if too large
 */
int oddsockets_validate_message_size(const char* message);

/**
 * Get SDK version string
 * @return Version string
 */
const char* oddsockets_get_version(void);

/**
 * Initialize default configuration
 * @param config Configuration structure to initialize
 * @param api_key API key to use
 */
void oddsockets_config_init(oddsockets_config_t* config, const char* api_key);

/* Memory Management (for embedded systems) */

/**
 * Set custom memory allocation functions
 * @param malloc_func Custom malloc function
 * @param free_func Custom free function
 * @param realloc_func Custom realloc function
 */
void oddsockets_set_memory_functions(void* (*malloc_func)(size_t),
                                    void (*free_func)(void*),
                                    void* (*realloc_func)(void*, size_t));

/**
 * Get memory usage statistics
 * @param allocated_bytes Output for currently allocated bytes
 * @param peak_bytes Output for peak allocated bytes
 * @return ODDSOCKETS_SUCCESS on success
 */
int oddsockets_get_memory_stats(size_t* allocated_bytes, size_t* peak_bytes);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_H */
