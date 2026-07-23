/**
 * OddSockets C SDK - Enhanced Features Implementation
 *
 * Slack-like enhanced events that emit real Socket.IO events over the live
 * connection. The worker persists them and broadcasts to the scoped channel
 * room, so subscribers observe the resulting events (e.g. "user_typing",
 * "reaction_added") through oddsockets_on().
 *
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#include "enhanced_features.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Escape a string for embedding inside a JSON string literal. */
static void json_escape(const char* in, char* out, size_t out_size) {
    size_t o = 0;
    if (!in) { if (out_size) out[0] = '\0'; return; }
    for (size_t i = 0; in[i] && o + 2 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        switch (c) {
            case '"':  if (o + 2 < out_size) { out[o++] = '\\'; out[o++] = '"'; } break;
            case '\\': if (o + 2 < out_size) { out[o++] = '\\'; out[o++] = '\\'; } break;
            case '\n': if (o + 2 < out_size) { out[o++] = '\\'; out[o++] = 'n'; } break;
            case '\r': if (o + 2 < out_size) { out[o++] = '\\'; out[o++] = 'r'; } break;
            case '\t': if (o + 2 < out_size) { out[o++] = '\\'; out[o++] = 't'; } break;
            default:   out[o++] = (char)c; break;
        }
    }
    out[o] = '\0';
}

/* Presence Events - Typing */

int oddsockets_start_typing(oddsockets_client_t* client,
                            const char* user_id,
                            const char* channel) {
    if (!client || !user_id || !channel) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    char eu[128], ec[256], payload[512];
    json_escape(user_id, eu, sizeof(eu));
    json_escape(channel, ec, sizeof(ec));
    snprintf(payload, sizeof(payload),
             "{\"userId\":\"%s\",\"channel\":\"%s\"}", eu, ec);
    return oddsockets_emit(client, "start_typing", payload);
}

int oddsockets_stop_typing(oddsockets_client_t* client,
                           const char* user_id,
                           const char* channel) {
    if (!client || !user_id || !channel) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    char eu[128], ec[256], payload[512];
    json_escape(user_id, eu, sizeof(eu));
    json_escape(channel, ec, sizeof(ec));
    snprintf(payload, sizeof(payload),
             "{\"userId\":\"%s\",\"channel\":\"%s\"}", eu, ec);
    return oddsockets_emit(client, "stop_typing", payload);
}

/* Reaction Events */

int oddsockets_add_reaction(oddsockets_client_t* client,
                            const char* message_id,
                            const char* channel,
                            const char* emoji,
                            const char* user_id,
                            const char* user_name) {
    if (!client || !message_id || !channel || !emoji || !user_id) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    char em[128], ec[256], ee[128], eu[128], en[128], payload[1024];
    json_escape(message_id, em, sizeof(em));
    json_escape(channel, ec, sizeof(ec));
    json_escape(emoji, ee, sizeof(ee));
    json_escape(user_id, eu, sizeof(eu));
    json_escape(user_name ? user_name : user_id, en, sizeof(en));
    snprintf(payload, sizeof(payload),
             "{\"messageId\":\"%s\",\"channel\":\"%s\",\"emoji\":\"%s\","
             "\"userId\":\"%s\",\"userName\":\"%s\"}",
             em, ec, ee, eu, en);
    return oddsockets_emit(client, "add_reaction", payload);
}

int oddsockets_remove_reaction(oddsockets_client_t* client,
                               const char* message_id,
                               const char* channel,
                               const char* emoji,
                               const char* user_id) {
    if (!client || !message_id || !channel || !emoji || !user_id) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    char em[128], ec[256], ee[128], eu[128], payload[768];
    json_escape(message_id, em, sizeof(em));
    json_escape(channel, ec, sizeof(ec));
    json_escape(emoji, ee, sizeof(ee));
    json_escape(user_id, eu, sizeof(eu));
    snprintf(payload, sizeof(payload),
             "{\"messageId\":\"%s\",\"channel\":\"%s\",\"emoji\":\"%s\","
             "\"userId\":\"%s\"}",
             em, ec, ee, eu);
    return oddsockets_emit(client, "remove_reaction", payload);
}
