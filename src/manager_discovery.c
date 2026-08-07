/**
 * Manager Discovery Implementation
 *
 * Resolves the configured manager endpoint and hands it back verbatim. The
 * built-in default is a starting point for an unconfigured client, never a
 * recovery path for a configured manager that is unreachable.
 */

#include "manager_discovery.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static const char DEFAULT_MANAGER_URL[] = "https://connect.oddsockets.tyga.network";

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

/* Case-insensitive prefix match, used for the URL scheme. */
static bool has_prefix_ci(const char* value, size_t value_length, const char* lowercase_prefix) {
    size_t i;
    for (i = 0; lowercase_prefix[i] != '\0'; i++) {
        char c;
        if (i >= value_length) {
            return false;
        }
        c = value[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != lowercase_prefix[i]) {
            return false;
        }
    }
    return true;
}

/* An acceptable manager URL is absolute, http(s), and names a host. */
static bool is_absolute_http_url(const char* url, size_t length) {
    size_t scheme_length;

    if (has_prefix_ci(url, length, "https://")) {
        scheme_length = 8;
    } else if (has_prefix_ci(url, length, "http://")) {
        scheme_length = 7;
    } else {
        return false;
    }

    if (length <= scheme_length) {
        return false;
    }

    /* Reject "http:///path" and friends: there must be a host. */
    return url[scheme_length] != '/'
        && url[scheme_length] != '?'
        && url[scheme_length] != '#';
}

static void set_error_message(char* error_message,
                              size_t error_message_size,
                              const char* prefix,
                              const char* value) {
    if (!error_message || error_message_size == 0) {
        return;
    }
    snprintf(error_message, error_message_size, "%s%s", prefix, value ? value : "");
}

/**
 * Trim surrounding whitespace and trailing slashes from source into
 * destination. Refuses to write a truncated URL.
 */
static int copy_normalized_url(const char* source, char* destination, size_t destination_size) {
    size_t start = 0;
    size_t end;

    if (!source || !destination || destination_size == 0) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    end = strlen(source);
    while (start < end && is_space(source[start])) {
        start++;
    }
    while (end > start && is_space(source[end - 1])) {
        end--;
    }
    while (end > start && source[end - 1] == '/') {
        end--;
    }

    if (end == start || (end - start) >= destination_size) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    memcpy(destination, source + start, end - start);
    destination[end - start] = '\0';
    return ODDSOCKETS_DISCOVERY_SUCCESS;
}

/* Alias used by oddsockets.c */
int manager_discovery_get_url(const char* configured_url,
                              char* manager_url,
                              size_t url_buffer_size,
                              char* error_message,
                              size_t error_message_size) {
    return oddsockets_discover_manager_url(configured_url,
                                           manager_url,
                                           url_buffer_size,
                                           error_message,
                                           error_message_size);
}

int oddsockets_discover_manager_url(const char* configured_url,
                                    char* manager_url,
                                    size_t url_buffer_size,
                                    char* error_message,
                                    size_t error_message_size) {
    const char* candidate;
    char normalized[ODDSOCKETS_MAX_URL_LENGTH];
    size_t normalized_length;

    if (error_message && error_message_size > 0) {
        error_message[0] = '\0';
    }

    if (!manager_url || url_buffer_size == 0) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }
    manager_url[0] = '\0';

    if (configured_url && configured_url[0] != '\0') {
        candidate = configured_url;
    } else {
        const char* from_environment = getenv(ODDSOCKETS_MANAGER_URL_ENV);
        candidate = (from_environment && from_environment[0] != '\0')
                  ? from_environment
                  : DEFAULT_MANAGER_URL;
    }

    /* Normalise into a bounded scratch buffer so the caller's buffer never
       receives a partially written endpoint. */
    if (copy_normalized_url(candidate, normalized, sizeof(normalized)) != ODDSOCKETS_DISCOVERY_SUCCESS) {
        set_error_message(error_message, error_message_size, "Invalid managerUrl: ", candidate);
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    normalized_length = strlen(normalized);
    if (!is_absolute_http_url(normalized, normalized_length)) {
        set_error_message(error_message, error_message_size, "Invalid managerUrl: ", candidate);
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    if (normalized_length >= url_buffer_size) {
        set_error_message(error_message, error_message_size,
                          "Manager URL does not fit in the supplied buffer: ", normalized);
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    memcpy(manager_url, normalized, normalized_length + 1);
    return ODDSOCKETS_DISCOVERY_SUCCESS;
}

int oddsockets_discover_all_managers(const char* configured_url,
                                     oddsockets_manager_info_t* managers,
                                     size_t max_managers,
                                     size_t* discovered_count) {
    int result;

    if (!managers || max_managers == 0 || !discovered_count) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    *discovered_count = 0;

    result = oddsockets_discover_manager_url(configured_url,
                                             managers[0].url,
                                             sizeof(managers[0].url),
                                             NULL,
                                             0);
    if (result != ODDSOCKETS_DISCOVERY_SUCCESS) {
        return result;
    }

    managers[0].response_time_ms = 0;
    managers[0].is_available = true;
    snprintf(managers[0].region, sizeof(managers[0].region), "%s", "default");
    managers[0].load_score = 0;

    *discovered_count = 1;
    return ODDSOCKETS_DISCOVERY_SUCCESS;
}

int oddsockets_test_manager_connectivity(const char* manager_url,
                                          const char* api_key,
                                          int* response_time_ms) {
    char response[1024];
    char url[ODDSOCKETS_MAX_URL_LENGTH + 16];
    int written;
    int result;

    (void)api_key;

    if (!manager_url || !manager_url[0] || !response_time_ms) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    written = snprintf(url, sizeof(url), "%s/health", manager_url);
    if (written < 0 || (size_t)written >= sizeof(url)) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    result = http_get(url, response, sizeof(response), ODDSOCKETS_DISCOVERY_TIMEOUT_MS);
    *response_time_ms = (result == 0) ? 100 : -1; /* Approximate */

    return (result == 0) ? ODDSOCKETS_DISCOVERY_SUCCESS : ODDSOCKETS_DISCOVERY_ERROR_NETWORK_ERROR;
}

int oddsockets_get_default_manager_url(char* manager_url, size_t url_buffer_size) {
    return oddsockets_discover_manager_url(NULL, manager_url, url_buffer_size, NULL, 0);
}

int oddsockets_get_default_manager_urls(char urls[][ODDSOCKETS_MAX_URL_LENGTH],
                                         size_t max_urls,
                                         size_t* url_count) {
    int result;

    if (!urls || max_urls == 0 || !url_count) {
        return ODDSOCKETS_DISCOVERY_ERROR_INVALID_PARAMETER;
    }

    *url_count = 0;

    result = oddsockets_get_default_manager_url(urls[0], ODDSOCKETS_MAX_URL_LENGTH);
    if (result != ODDSOCKETS_DISCOVERY_SUCCESS) {
        return result;
    }

    *url_count = 1;
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
