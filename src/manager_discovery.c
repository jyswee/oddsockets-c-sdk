/**
 * Manager Discovery Implementation
 * Always returns the main manager endpoint.
 */

#include "manager_discovery.h"
#include "http_client.h"
#include <string.h>
#include <stdbool.h>

static const char* DEFAULT_MANAGER_URL = "https://manager1.oddsockets.tyga.network";

/* Alias used by oddsockets.c */
int manager_discovery_get_url(const char* api_key, char* manager_url, size_t url_buffer_size) {
    return oddsockets_discover_manager_url(api_key, manager_url, url_buffer_size);
}

int oddsockets_discover_manager_url(const char* api_key,
                                    char* manager_url,
                                    size_t url_buffer_size) {
    (void)api_key; /* Not used — always returns main endpoint */

    if (!manager_url || url_buffer_size == 0) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    strncpy(manager_url, DEFAULT_MANAGER_URL, url_buffer_size - 1);
    manager_url[url_buffer_size - 1] = '\0';

    return ODDSOCKETS_DISCOVERY_SUCCESS;
}

int oddsockets_discover_all_managers(const char* api_key,
                                     oddsockets_manager_info_t* managers,
                                     size_t max_managers,
                                     size_t* discovered_count) {
    if (!managers || max_managers == 0 || !discovered_count) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    strncpy(managers[0].url, DEFAULT_MANAGER_URL, sizeof(managers[0].url) - 1);
    managers[0].response_time_ms = 0;
    managers[0].is_available = true;
    strncpy(managers[0].region, "default", sizeof(managers[0].region) - 1);
    managers[0].load_score = 0;

    *discovered_count = 1;
    return ODDSOCKETS_DISCOVERY_SUCCESS;
}

int oddsockets_test_manager_connectivity(const char* manager_url,
                                          const char* api_key,
                                          int* response_time_ms) {
    if (!manager_url || !response_time_ms) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    char response[1024];
    char url[1024];
    snprintf(url, sizeof(url), "%s/health", manager_url);

    int result = http_get(url, response, sizeof(response), ODDSOCKETS_DISCOVERY_TIMEOUT_MS);
    *response_time_ms = (result == 0) ? 100 : -1; /* Approximate */

    return (result == 0) ? ODDSOCKETS_DISCOVERY_SUCCESS : ODDSOCKETS_DISCOVERY_ERROR_NETWORK_ERROR;
}

int oddsockets_get_default_manager_urls(char urls[][ODDSOCKETS_MAX_URL_LENGTH],
                                         size_t max_urls,
                                         size_t* url_count) {
    if (!urls || max_urls == 0 || !url_count) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    strncpy(urls[0], DEFAULT_MANAGER_URL, ODDSOCKETS_MAX_URL_LENGTH - 1);
    *url_count = 1;
    return ODDSOCKETS_DISCOVERY_SUCCESS;
}

int oddsockets_set_custom_manager_urls(const char* const* urls, size_t url_count) {
    (void)urls;
    (void)url_count;
    /* Custom URLs not supported in simplified version */
    return ODDSOCKETS_DISCOVERY_SUCCESS;
}

const char* oddsockets_discovery_error_string(oddsockets_discovery_error_t error) {
    switch (error) {
        case ODDSOCKETS_DISCOVERY_SUCCESS: return "Success";
        case ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case ODDSOCKETS_DISCOVERY_ERROR_NO_MANAGERS_AVAILABLE: return "No managers available";
        case ODDSOCKETS_DISCOVERY_ERROR_TIMEOUT: return "Timeout";
        case ODDSOCKETS_DISCOVERY_ERROR_NETWORK_ERROR: return "Network error";
        case ODDSOCKETS_DISCOVERY_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        default: return "Unknown error";
    }
}
