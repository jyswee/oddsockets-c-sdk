/**
 * Manager Discovery for OddSockets C SDK
 * 
 * Handles automatic discovery of the optimal manager URL for load balancing
 * and high availability. Follows the JavaScript SDK pattern for consistency.
 */

#ifndef ODDSOCKETS_MANAGER_DISCOVERY_H
#define ODDSOCKETS_MANAGER_DISCOVERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Configuration Constants */
#define ODDSOCKETS_MAX_URL_LENGTH 512
#define ODDSOCKETS_MAX_MANAGER_URLS 8
#define ODDSOCKETS_DISCOVERY_TIMEOUT_MS 5000

/* Error Codes */
typedef enum {
    ODDSOCKETS_DISCOVERY_SUCCESS = 0,
    ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER = -1,
    ODDSOCKETS_DISCOVERY_ERROR_NO_MANAGERS_AVAILABLE = -2,
    ODDSOCKETS_DISCOVERY_ERROR_TIMEOUT = -3,
    ODDSOCKETS_DISCOVERY_ERROR_NETWORK_ERROR = -4,
    ODDSOCKETS_DISCOVERY_ERROR_MEMORY_ALLOCATION = -5
} oddsockets_discovery_error_t;

/* Manager Information */
typedef struct {
    char url[ODDSOCKETS_MAX_URL_LENGTH];
    int response_time_ms;
    bool is_available;
    char region[64];
    int load_score;
} oddsockets_manager_info_t;

/**
 * Discover the optimal manager URL for the given API key
 * 
 * This function automatically discovers and selects the best manager URL
 * based on response time, availability, and load balancing.
 * 
 * @param api_key The API key to use for discovery
 * @param manager_url Output buffer for the discovered manager URL
 * @param url_buffer_size Size of the manager_url buffer
 * @return ODDSOCKETS_DISCOVERY_SUCCESS on success, error code on failure
 */
int oddsockets_discover_manager_url(const char* api_key, 
                                   char* manager_url, 
                                   size_t url_buffer_size);

/**
 * Discover all available managers and their information
 * 
 * @param api_key The API key to use for discovery
 * @param managers Output array for manager information
 * @param max_managers Maximum number of managers to discover
 * @param discovered_count Output for the number of managers discovered
 * @return ODDSOCKETS_DISCOVERY_SUCCESS on success, error code on failure
 */
int oddsockets_discover_all_managers(const char* api_key,
                                    oddsockets_manager_info_t* managers,
                                    size_t max_managers,
                                    size_t* discovered_count);

/**
 * Test connectivity to a specific manager URL
 * 
 * @param manager_url The manager URL to test
 * @param api_key The API key to use for testing
 * @param response_time_ms Output for response time in milliseconds
 * @return ODDSOCKETS_DISCOVERY_SUCCESS if available, error code if not
 */
int oddsockets_test_manager_connectivity(const char* manager_url,
                                        const char* api_key,
                                        int* response_time_ms);

/**
 * Get the default manager URLs for fallback
 * 
 * @param urls Output array for manager URLs
 * @param max_urls Maximum number of URLs to return
 * @param url_count Output for the number of URLs returned
 * @return ODDSOCKETS_DISCOVERY_SUCCESS on success
 */
int oddsockets_get_default_manager_urls(char urls[][ODDSOCKETS_MAX_URL_LENGTH],
                                       size_t max_urls,
                                       size_t* url_count);

/**
 * Set custom manager URLs for discovery
 * 
 * @param urls Array of custom manager URLs
 * @param url_count Number of URLs in the array
 * @return ODDSOCKETS_DISCOVERY_SUCCESS on success
 */
int oddsockets_set_custom_manager_urls(const char* const* urls, size_t url_count);

/**
 * Get error message for discovery error code
 * 
 * @param error Error code
 * @return Error message string
 */
const char* oddsockets_discovery_error_string(oddsockets_discovery_error_t error);

/* Internal alias used by oddsockets.c */
int manager_discovery_get_url(const char* api_key, char* manager_url, size_t url_buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_MANAGER_DISCOVERY_H */
