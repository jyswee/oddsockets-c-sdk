/**
 * WebSocket Client Implementation — uses libwebsockets
 */

#include "websocket_client.h"
#include <libwebsockets.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_PAYLOAD 65536

struct websocket_client {
    websocket_config_t config;
    struct lws_context* context;
    struct lws* wsi;
    bool connected;
    bool should_close;

    /* Send queue */
    char* pending_send;
    size_t pending_send_len;

    pthread_mutex_t mutex;
};

/* libwebsockets callback */
static int lws_callback(struct lws* wsi, enum lws_callback_reasons reason,
                         void* user, void* in, size_t len) {
    websocket_client_t* client = (websocket_client_t*)lws_context_user(lws_get_context(wsi));
    if (!client) return 0;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            client->connected = true;
            if (client->config.on_connected) {
                client->config.on_connected(client->config.user_data);
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (in && len > 0 && client->config.on_message) {
                char* msg = malloc(len + 1);
                if (msg) {
                    memcpy(msg, in, len);
                    msg[len] = '\0';
                    client->config.on_message(msg, client->config.user_data);
                    free(msg);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            pthread_mutex_lock(&client->mutex);
            if (client->pending_send && client->pending_send_len > 0) {
                unsigned char* buf = malloc(LWS_PRE + client->pending_send_len);
                if (buf) {
                    memcpy(buf + LWS_PRE, client->pending_send, client->pending_send_len);
                    lws_write(wsi, buf + LWS_PRE, client->pending_send_len, LWS_WRITE_TEXT);
                    free(buf);
                }
                free(client->pending_send);
                client->pending_send = NULL;
                client->pending_send_len = 0;
            }
            pthread_mutex_unlock(&client->mutex);
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            client->connected = false;
            if (client->config.on_error) {
                const char* err = in ? (const char*)in : "Connection error";
                client->config.on_error(err, client->config.user_data);
            }
            break;

        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CLOSED:
            client->connected = false;
            client->wsi = NULL;
            if (client->config.on_disconnected) {
                client->config.on_disconnected(client->config.user_data);
            }
            break;

        default:
            break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "oddsockets", lws_callback, 0, MAX_PAYLOAD },
    { NULL, NULL, 0, 0 }
};

websocket_client_t* websocket_client_create(const websocket_config_t* config) {
    if (!config || !config->url[0]) return NULL;

    websocket_client_t* client = calloc(1, sizeof(websocket_client_t));
    if (!client) return NULL;

    memcpy(&client->config, config, sizeof(websocket_config_t));
    pthread_mutex_init(&client->mutex, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.user = client;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    client->context = lws_create_context(&info);
    if (!client->context) {
        free(client);
        return NULL;
    }

    return client;
}

int websocket_client_connect(websocket_client_t* client) {
    if (!client || !client->context) return -1;

    /* Parse URL */
    const char* url = client->config.url;
    bool use_ssl = (strncmp(url, "https://", 8) == 0 || strncmp(url, "wss://", 6) == 0);
    const char* host_start = strstr(url, "://");
    if (!host_start) return -1;
    host_start += 3;

    char host[256] = {0};
    char path[256] = "/socket.io/?EIO=4&transport=websocket";
    int port = use_ssl ? 443 : 80;

    const char* port_start = strchr(host_start, ':');
    const char* path_start = strchr(host_start, '/');

    if (port_start && (!path_start || port_start < path_start)) {
        size_t host_len = (size_t)(port_start - host_start);
        if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
        memcpy(host, host_start, host_len);
        port = atoi(port_start + 1);
    } else if (path_start) {
        size_t host_len = (size_t)(path_start - host_start);
        if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
        memcpy(host, host_start, host_len);
    } else {
        strncpy(host, host_start, sizeof(host) - 1);
    }

    struct lws_client_connect_info conn_info;
    memset(&conn_info, 0, sizeof(conn_info));
    conn_info.context = client->context;
    conn_info.address = host;
    conn_info.port = port;
    conn_info.path = path;
    conn_info.host = host;
    conn_info.origin = host;
    conn_info.protocol = "oddsockets";
    conn_info.ssl_connection = use_ssl ? LCCSCF_USE_SSL : 0;

    client->wsi = lws_client_connect_via_info(&conn_info);
    if (!client->wsi) return -1;

    return 0;
}

int websocket_client_send(websocket_client_t* client, const char* message) {
    if (!client || !message || !client->connected) return -1;

    pthread_mutex_lock(&client->mutex);
    free(client->pending_send);
    client->pending_send_len = strlen(message);
    client->pending_send = malloc(client->pending_send_len + 1);
    if (client->pending_send) {
        memcpy(client->pending_send, message, client->pending_send_len + 1);
    }
    pthread_mutex_unlock(&client->mutex);

    if (client->wsi) {
        lws_callback_on_writable(client->wsi);
    }

    return 0;
}

int websocket_client_process_events(websocket_client_t* client) {
    if (!client || !client->context) return -1;
    return lws_service(client->context, 0);
}

void websocket_client_disconnect(websocket_client_t* client) {
    if (!client) return;
    client->should_close = true;
    client->connected = false;
}

void websocket_client_destroy(websocket_client_t* client) {
    if (!client) return;

    websocket_client_disconnect(client);

    if (client->context) {
        lws_context_destroy(client->context);
        client->context = NULL;
    }

    pthread_mutex_lock(&client->mutex);
    free(client->pending_send);
    client->pending_send = NULL;
    pthread_mutex_unlock(&client->mutex);

    pthread_mutex_destroy(&client->mutex);
    free(client);
}

bool websocket_client_is_connected(websocket_client_t* client) {
    return client ? client->connected : false;
}
