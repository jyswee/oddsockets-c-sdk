/**
 * Manager Discovery for OddSockets C SDK
 *
 * Resolves the manager endpoint the client asks for a worker assignment.
 *
 * The resolved URL is used verbatim. There is deliberately no fallback to the
 * public endpoint when a configured manager is unreachable: silently
 * redirecting a self-hosted or staging deployment at production would make a
 * misconfigured client look healthy, and would invalidate any test aimed at a
 * non-production manager.
 */

#ifndef ODDSOCKETS_MANAGER_DISCOVERY_H
#define ODDSOCKETS_MANAGER_DISCOVERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

/* Configuration Constants */
#define ODDSOCKETS_MAX_URL_LENGTH 512
#define ODDSOCKETS_MAX_MANAGER_URLS 8
#define ODDSOCKETS_DISCOVERY_TIMEOUT_MS 5000

/* Recommended size for the error_message buffers below. Large enough to hold
   "Invalid managerUrl: " followed by a maximum-length URL. */
#define ODDSOCKETS_DISCOVERY_MAX_ERROR_LENGTH 576

/* Environment variable consulted when no manager URL has been configured. */
#define ODDSOCKETS_MANAGER_URL_ENV "ODDSOCKETS_MANAGER_URL"

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
 * Resolve the manager URL the client will contact
 *
 * Precedence is configured_url, then the ODDSOCKETS_MANAGER_URL environment
 * variable, then the built-in default. The built-in default applies only when
 * nothing at all has been configured; it is never used to recover from a
 * configured manager that turns out to be unreachable.
 *
 * The accepted value must be an absolute http:// or https:// URL. Trailing
 * slashes are stripped. A value that does not fit in manager_url is rejected
 * rather than truncated.
 *
 * @param configured_url The manager URL from the client configuration. May be
 *                       NULL or empty, which means "not configured"
 * @param manager_url Output buffer for the resolved manager URL
 * @param url_buffer_size Size of the manager_url buffer
 * @param error_message Optional buffer receiving a human-readable reason on
 *                      failure, e.g. "Invalid managerUrl: <value>". May be NULL
 * @param error_message_size Size of the error_message buffer
 * @return ODDSOCKETS_DISCOVERY_SUCCESS on success,
 *         ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER on failure
 */
int oddsockets_discover_manager_url(const char* configured_url,
                                   char* manager_url,
                                   size_t url_buffer_size,
                                   char* error_message,
                                   size_t error_message_size);

/**
 * Report the managers this client would use
 *
 * Resolves configured_url exactly as oddsockets_discover_manager_url() does and
 * reports the single resulting endpoint. It performs no probing, so
 * is_available reflects "configured", not "reachable".
 *
 * @param configured_url The manager URL from the client configuration. May be
 *                       NULL or empty, which means "not configured"
 * @param managers Output array for manager information
 * @param max_managers Maximum number of managers to report
 * @param discovered_count Output for the number of managers reported
 * @return ODDSOCKETS_DISCOVERY_SUCCESS on success, error code on failure
 */
int oddsockets_discover_all_managers(const char* configured_url,
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
 * Get the manager URL used when the caller configures none
 *
 * Returns the ODDSOCKETS_MANAGER_URL environment value when it is set,
 * otherwise the built-in default.
 *
 * @param manager_url Output buffer for the URL
 * @param url_buffer_size Size of the manager_url buffer
 * @return ODDSOCKETS_DISCOVERY_SUCCESS on success, error code on failure
 */
int oddsockets_get_default_manager_url(char* manager_url, size_t url_buffer_size);

/**
 * Get the default manager URLs
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
 * Get error message for discovery error code
 *
 * @param error Error code
 * @return Error message string
 */
const char* oddsockets_discovery_error_string(oddsockets_discovery_error_t error);

/* Internal alias used by oddsockets.c */
int manager_discovery_get_url(const char* configured_url,
                              char* manager_url,
                              size_t url_buffer_size,
                              char* error_message,
                              size_t error_message_size);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_MANAGER_DISCOVERY_H */
