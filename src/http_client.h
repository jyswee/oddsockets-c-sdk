/**
 * HTTP Client for OddSockets C SDK
 * Lightweight wrapper around libcurl for HTTP GET/POST operations.
 */

#ifndef ODDSOCKETS_HTTP_CLIENT_H
#define ODDSOCKETS_HTTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * Perform an HTTP GET request
 * @param url The URL to request
 * @param response Output buffer for response body
 * @param response_size Size of response buffer
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int http_get(const char* url, char* response, size_t response_size, int timeout_ms);

/**
 * Perform an HTTP POST request with JSON body
 * @param url The URL to request
 * @param json_body JSON body string
 * @param response Output buffer for response body
 * @param response_size Size of response buffer
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int http_post_json(const char* url, const char* json_body, char* response, size_t response_size, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_HTTP_CLIENT_H */
