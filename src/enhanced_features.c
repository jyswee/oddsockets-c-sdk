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

/* Challenge / Leaderboard / Achievement Events
 *
 * Like the reaction/typing emitters above, each of these forwards a caller-
 * supplied JSON payload verbatim to the worker over the live connection. The
 * worker persists the request and broadcasts the resulting events to the scoped
 * room; where a response is defined it replies with a "<event>_success" event
 * (or "achievement_state" / "challenge_invites" for the query methods) on
 * success and an error event keyed by the request name on failure. Observe the
 * acks, error events and broadcasts through oddsockets_on(). The payload_json
 * must be a complete JSON object literal; the query-only method accepts NULL or
 * "{}" for an empty payload. */

/* create{challengeId,metric,ranked?,channel?,resultWebhookUrl?,standingsUrl?} */
int oddsockets_create_challenge(oddsockets_client_t* client,
                                const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_create", payload_json);
}

/* progress{challengeId,value,metric?,eventId?,cohort?,platform?,channel?}
 * Fire-and-forget: the worker echoes "challenge_progress" (and, for ranked
 * challenges, "leaderboard_rank_change") to the room. */
int oddsockets_report_progress(oddsockets_client_t* client,
                               const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_progress", payload_json);
}

/* complete{challengeId,outcome,eventId?,reward?}
 * outcome one of: completed | failed | expired | conceded | tied */
int oddsockets_complete_challenge(oddsockets_client_t* client,
                                  const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_complete", payload_json);
}

/* unlock_achievement{achievementId,name?,tier?,percentComplete?,challengeId?,channel?}
 * Fire-and-forget: percentComplete < 100 broadcasts "achievement_progress",
 * >= 100 (or omitted) broadcasts "achievement_unlock". */
int oddsockets_unlock_achievement(oddsockets_client_t* client,
                                  const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "achievement_unlock", payload_json);
}

/* get_standings{challengeId,limit?=20,offset?=0}
 * ack "challenge_standings_success"; error event "challenge_standings". */
int oddsockets_get_standings(oddsockets_client_t* client,
                             const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_standings", payload_json);
}

/* get_achievements{achievementId?}
 * ack "achievement_state"; error event "achievement_query". */
int oddsockets_get_achievements(oddsockets_client_t* client,
                                const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "achievement_query", payload_json);
}

/* send_challenge_invite{toUserId,type?='match',payload?<=8KB,ttl?=300,channel?,inviteId?}
 * ack "challenge_invite_success"; error event "challenge_invite". */
int oddsockets_send_challenge_invite(oddsockets_client_t* client,
                                     const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_invite", payload_json);
}

/* reply_challenge_invite{inviteId,accept,reason?}
 * ack "challenge_reply_success"; error event "challenge_reply". */
int oddsockets_reply_challenge_invite(oddsockets_client_t* client,
                                      const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_reply", payload_json);
}

/* cancel_challenge_invite{inviteId}
 * ack "challenge_invite_cancel_success"; error event "challenge_invite_cancel". */
int oddsockets_cancel_challenge_invite(oddsockets_client_t* client,
                                       const char* payload_json) {
    if (!client || !payload_json || !payload_json[0]) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_invite_cancel", payload_json);
}

/* get_challenge_invites{}  (empty payload; NULL or "{}" accepted)
 * ack "challenge_invites"; error event "challenge_invites_query". */
int oddsockets_get_challenge_invites(oddsockets_client_t* client,
                                     const char* payload_json) {
    if (!client) {
        return ODDSOCKETS_ERROR_INVALID_PARAMETER;
    }
    return oddsockets_emit(client, "challenge_invites_query",
                           (payload_json && payload_json[0]) ? payload_json : "{}");
}
