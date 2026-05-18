/**
 * OddSockets C SDK - Basic Usage Example
 * 
 * This example demonstrates the basic usage of the OddSockets C SDK
 * for embedded systems and IoT devices.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "oddsockets.h"

/* Global variables for signal handling */
static volatile int running = 1;
static oddsockets_client_t* client = NULL;

/* Signal handler for graceful shutdown */
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}

/* Message callback function */
void on_message(const char* channel_name, const char* message, void* user_data) {
    printf("Received message on channel '%s': %s\n", channel_name, message);
}

/* Connection state callback */
void on_connection_state(oddsockets_state_t state, void* user_data) {
    printf("Connection state changed: %s\n", oddsockets_state_string(state));
}

/* Error callback */
void on_error(oddsockets_error_t error, const char* message, void* user_data) {
    printf("Error %d: %s\n", error, message);
}

/* Logging callback */
void on_log(oddsockets_log_level_t level, const char* message, void* user_data) {
    const char* level_str;
    switch (level) {
        case ODDSOCKETS_LOG_ERROR: level_str = "ERROR"; break;
        case ODDSOCKETS_LOG_WARN:  level_str = "WARN";  break;
        case ODDSOCKETS_LOG_INFO:  level_str = "INFO";  break;
        case ODDSOCKETS_LOG_DEBUG: level_str = "DEBUG"; break;
        default: level_str = "UNKNOWN"; break;
    }
    printf("[%s] %s\n", level_str, message);
}

int main(int argc, char* argv[]) {
    int result;
    oddsockets_channel_t* channel;
    
    /* Check command line arguments */
    if (argc != 2) {
        printf("Usage: %s <api_key>\n", argv[0]);
        printf("Example: %s your-api-key-here\n", argv[0]);
        return 1;
    }
    
    const char* api_key = argv[1];
    
    printf("OddSockets C SDK Basic Usage Example\n");
    printf("SDK Version: %s\n", oddsockets_get_version());
    printf("API Key: %s\n", api_key);
    printf("\n");
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize configuration */
    oddsockets_config_t config;
    oddsockets_config_init(&config, api_key);
    
    /* Set optional configuration */
    strncpy(config.user_id, "c-sdk-example", sizeof(config.user_id) - 1);
    config.auto_connect = false; /* We'll connect manually */
    config.log_level = ODDSOCKETS_LOG_INFO;
    config.log_callback = on_log;
    config.connection_callback = on_connection_state;
    config.error_callback = on_error;
    
    /* Create client */
    printf("Creating OddSockets client...\n");
    client = oddsockets_create(&config);
    if (!client) {
        printf("Failed to create OddSockets client\n");
        return 1;
    }
    
    /* Connect to the platform */
    printf("Connecting to OddSockets platform...\n");
    result = oddsockets_connect(client);
    if (result != ODDSOCKETS_SUCCESS) {
        printf("Failed to connect: %s\n", oddsockets_error_string(result));
        oddsockets_destroy(client);
        return 1;
    }
    
    /* Wait for connection */
    printf("Waiting for connection...\n");
    int timeout = 30; /* 30 seconds timeout */
    while (timeout > 0 && oddsockets_get_state(client) == ODDSOCKETS_STATE_CONNECTING) {
        oddsockets_process_events(client);
        usleep(100000); /* 100ms */
        timeout--;
    }
    
    if (oddsockets_get_state(client) != ODDSOCKETS_STATE_CONNECTED) {
        printf("Connection timeout or failed\n");
        oddsockets_destroy(client);
        return 1;
    }
    
    printf("Connected successfully!\n");
    
    /* Get worker information */
    oddsockets_worker_info_t worker_info;
    result = oddsockets_get_worker_info(client, &worker_info);
    if (result == ODDSOCKETS_SUCCESS) {
        printf("Assigned to worker: %s (%s)\n", worker_info.worker_id, worker_info.worker_url);
        if (worker_info.session_id[0]) {
            printf("Session ID: %s\n", worker_info.session_id);
        }
    }
    
    /* Create a channel */
    printf("\nCreating channel 'test-channel'...\n");
    channel = oddsockets_channel_create(client, "test-channel");
    if (!channel) {
        printf("Failed to create channel\n");
        oddsockets_destroy(client);
        return 1;
    }
    
    /* Subscribe to the channel */
    printf("Subscribing to channel...\n");
    oddsockets_subscribe_options_t subscribe_options = {
        .max_history = 50,
        .retain_history = true,
        .enable_presence = false
    };
    
    result = oddsockets_channel_subscribe(channel, on_message, NULL, &subscribe_options);
    if (result != ODDSOCKETS_SUCCESS) {
        printf("Failed to subscribe: %s\n", oddsockets_error_string(result));
        oddsockets_channel_destroy(channel);
        oddsockets_destroy(client);
        return 1;
    }
    
    /* Wait for subscription confirmation */
    timeout = 10; /* 10 seconds timeout */
    while (timeout > 0 && !oddsockets_channel_is_subscribed(channel)) {
        oddsockets_process_events(client);
        usleep(100000); /* 100ms */
        timeout--;
    }
    
    if (!oddsockets_channel_is_subscribed(channel)) {
        printf("Subscription timeout\n");
        oddsockets_channel_destroy(channel);
        oddsockets_destroy(client);
        return 1;
    }
    
    printf("Subscribed successfully!\n");
    
    /* Publish some test messages */
    printf("\nPublishing test messages...\n");
    
    oddsockets_publish_options_t publish_options = {
        .ttl_seconds = 3600, /* 1 hour TTL */
        .metadata = "example-metadata",
        .store_in_history = true
    };
    
    for (int i = 1; i <= 5; i++) {
        char message[256];
        snprintf(message, sizeof(message), "Hello from C SDK! Message #%d", i);
        
        result = oddsockets_channel_publish(channel, message, &publish_options);
        if (result != ODDSOCKETS_SUCCESS) {
            printf("Failed to publish message %d: %s\n", i, oddsockets_error_string(result));
        } else {
            printf("Published: %s\n", message);
        }
        
        /* Process events to handle any incoming messages */
        oddsockets_process_events(client);
        usleep(500000); /* 500ms delay between messages */
    }
    
    printf("\nListening for messages... (Press Ctrl+C to exit)\n");
    
    /* Main event loop */
    while (running && oddsockets_get_state(client) == ODDSOCKETS_STATE_CONNECTED) {
        /* Process WebSocket events */
        result = oddsockets_process_events(client);
        if (result != ODDSOCKETS_SUCCESS) {
            printf("Error processing events: %s\n", oddsockets_error_string(result));
            break;
        }
        
        /* Small delay to prevent busy waiting */
        usleep(10000); /* 10ms */
    }
    
    /* Cleanup */
    printf("\nCleaning up...\n");
    
    if (channel) {
        if (oddsockets_channel_is_subscribed(channel)) {
            printf("Unsubscribing from channel...\n");
            oddsockets_channel_unsubscribe(channel);
        }
        oddsockets_channel_destroy(channel);
    }
    
    if (client) {
        printf("Disconnecting...\n");
        oddsockets_disconnect(client);
        oddsockets_destroy(client);
    }
    
    printf("Example completed successfully!\n");
    return 0;
}
