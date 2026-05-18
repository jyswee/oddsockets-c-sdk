/**
 * HTTP Client Implementation — uses libcurl
 */

#include "http_client.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} response_buffer_t;

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    response_buffer_t* buf = (response_buffer_t*)userp;

    if (buf->size + total >= buf->capacity) {
        return 0; /* Buffer full */
    }

    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

int http_get(const char* url, char* response, size_t response_size, int timeout_ms) {
    if (!url || !response || response_size == 0) return -1;

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    response_buffer_t buf = { .data = response, .size = 0, .capacity = response_size };
    response[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OddSockets-C-SDK/1.0.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}

int http_post_json(const char* url, const char* json_body, char* response, size_t response_size, int timeout_ms) {
    if (!url || !json_body || !response || response_size == 0) return -1;

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    response_buffer_t buf = { .data = response, .size = 0, .capacity = response_size };
    response[0] = '\0';

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OddSockets-C-SDK/1.0.0");

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}
