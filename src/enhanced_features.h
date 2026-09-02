#ifndef ODDSOCKETS_ENHANCED_FEATURES_H
#define ODDSOCKETS_ENHANCED_FEATURES_H

#include "oddsockets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Enhanced Features for OddSockets C SDK
 * Provides 67 new Slack-like events with callback-based async patterns
 */

// Callback types
typedef void (*oddsockets_success_callback)(const char* data, void* user_data);
typedef void (*oddsockets_error_callback)(const char* error, void* user_data);

// Thread Events (7 methods)
int oddsockets_thread_reply(oddsockets_client_t* client,
                            const char* channel,
                            const char* parent_message_id,
                            const char* message,
                            const char* user_id,
                            const char* user_name,
                            oddsockets_success_callback on_success,
                            oddsockets_error_callback on_error,
                            void* user_data);

int oddsockets_get_thread(oddsockets_client_t* client,
                         const char* thread_id,
                         oddsockets_success_callback on_success,
                         oddsockets_error_callback on_error,
                         void* user_data);

int oddsockets_subscribe_thread(oddsockets_client_t* client,
                                const char* thread_id,
                                const char* user_id,
                                oddsockets_success_callback on_success,
                                oddsockets_error_callback on_error,
                                void* user_data);

int oddsockets_mark_thread_read(oddsockets_client_t* client,
                                const char* thread_id,
                                const char* user_id);

int oddsockets_follow_thread(oddsockets_client_t* client,
                             const char* thread_id,
                             const char* user_id);

int oddsockets_unfollow_thread(oddsockets_client_t* client,
                               const char* thread_id,
                               const char* user_id);

// Reaction Events (6 methods)
int oddsockets_add_reaction(oddsockets_client_t* client,
                            const char* message_id,
                            const char* channel,
                            const char* emoji,
                            const char* user_id,
                            const char* user_name);

int oddsockets_remove_reaction(oddsockets_client_t* client,
                               const char* message_id,
                               const char* channel,
                               const char* emoji,
                               const char* user_id);

int oddsockets_get_reactions(oddsockets_client_t* client,
                             const char* message_id,
                             oddsockets_success_callback on_success,
                             oddsockets_error_callback on_error,
                             void* user_data);

// Read Receipt Events (6 methods)
int oddsockets_mark_read(oddsockets_client_t* client,
                        const char* message_id,
                        const char* channel,
                        const char* user_id,
                        const char* user_name);

int oddsockets_get_unread_counts(oddsockets_client_t* client,
                                 const char* user_id,
                                 const char** channels,
                                 int channel_count,
                                 oddsockets_success_callback on_success,
                                 oddsockets_error_callback on_error,
                                 void* user_data);

int oddsockets_mark_all_read(oddsockets_client_t* client,
                             const char* channel,
                             const char* user_id);

// Channel Events (11 methods)
int oddsockets_create_channel(oddsockets_client_t* client,
                              const char* name,
                              const char* type,
                              const char* description,
                              const char* topic,
                              const char* created_by,
                              const char* created_by_name,
                              oddsockets_success_callback on_success,
                              oddsockets_error_callback on_error,
                              void* user_data);

int oddsockets_update_channel(oddsockets_client_t* client,
                              const char* channel_id,
                              const char* updates_json,
                              const char* user_id);

int oddsockets_archive_channel(oddsockets_client_t* client,
                               const char* channel_id,
                               const char* user_id);

int oddsockets_invite_to_channel(oddsockets_client_t* client,
                                 const char* channel_id,
                                 const char* invited_user_id,
                                 const char* invited_user_name,
                                 const char* invited_by);

int oddsockets_remove_from_channel(oddsockets_client_t* client,
                                   const char* channel_id,
                                   const char* removed_user_id,
                                   const char* removed_by);

int oddsockets_join_channel(oddsockets_client_t* client,
                            const char* channel_id,
                            const char* user_id,
                            const char* user_name);

int oddsockets_leave_channel(oddsockets_client_t* client,
                             const char* channel_id,
                             const char* user_id);

int oddsockets_get_channel_members(oddsockets_client_t* client,
                                   const char* channel_id,
                                   oddsockets_success_callback on_success,
                                   oddsockets_error_callback on_error,
                                   void* user_data);

// Direct Message Events (6 methods)
int oddsockets_create_dm(oddsockets_client_t* client,
                        const char** user_ids,
                        int user_count,
                        const char* type,
                        oddsockets_success_callback on_success,
                        oddsockets_error_callback on_error,
                        void* user_data);

int oddsockets_send_dm(oddsockets_client_t* client,
                      const char* conversation_id,
                      const char* message,
                      const char* user_id,
                      const char* user_name);

int oddsockets_get_dm_conversations(oddsockets_client_t* client,
                                    const char* user_id,
                                    int include_archived,
                                    oddsockets_success_callback on_success,
                                    oddsockets_error_callback on_error,
                                    void* user_data);

// Notification Events (6 methods)
int oddsockets_subscribe_notifications(oddsockets_client_t* client,
                                       const char* user_id);

int oddsockets_mark_notification_read(oddsockets_client_t* client,
                                      const char* notification_id,
                                      const char* user_id);

int oddsockets_mark_all_notifications_read(oddsockets_client_t* client,
                                           const char* user_id);

int oddsockets_clear_notifications(oddsockets_client_t* client,
                                   const char* user_id);

int oddsockets_get_notifications(oddsockets_client_t* client,
                                 const char* user_id,
                                 int limit,
                                 const char* status,
                                 oddsockets_success_callback on_success,
                                 oddsockets_error_callback on_error,
                                 void* user_data);

// Presence Events (8 methods)
int oddsockets_set_status(oddsockets_client_t* client,
                          const char* user_id,
                          const char* status);

int oddsockets_set_custom_status(oddsockets_client_t* client,
                                 const char* user_id,
                                 const char* emoji,
                                 const char* text,
                                 const char* expires_at);

int oddsockets_clear_custom_status(oddsockets_client_t* client,
                                   const char* user_id);

int oddsockets_set_dnd(oddsockets_client_t* client,
                      const char* user_id,
                      const char* until);

int oddsockets_clear_dnd(oddsockets_client_t* client,
                        const char* user_id);

int oddsockets_start_typing(oddsockets_client_t* client,
                            const char* user_id,
                            const char* channel);

int oddsockets_stop_typing(oddsockets_client_t* client,
                           const char* user_id,
                           const char* channel);

int oddsockets_get_user_presence(oddsockets_client_t* client,
                                 const char** user_ids,
                                 int user_count,
                                 oddsockets_success_callback on_success,
                                 oddsockets_error_callback on_error,
                                 void* user_data);

// Message Editing Events (5 methods)
int oddsockets_edit_message(oddsockets_client_t* client,
                            const char* message_id,
                            const char* channel,
                            const char* new_content,
                            const char* user_id);

int oddsockets_delete_message(oddsockets_client_t* client,
                              const char* message_id,
                              const char* channel,
                              const char* user_id);

int oddsockets_pin_message(oddsockets_client_t* client,
                           const char* message_id,
                           const char* channel,
                           const char* user_id);

int oddsockets_unpin_message(oddsockets_client_t* client,
                             const char* message_id,
                             const char* channel,
                             const char* user_id);

int oddsockets_get_pinned_messages(oddsockets_client_t* client,
                                   const char* channel,
                                   oddsockets_success_callback on_success,
                                   oddsockets_error_callback on_error,
                                   void* user_data);

// Search Events (4 methods)
int oddsockets_search_messages(oddsockets_client_t* client,
                               const char* query,
                               const char* user_id,
                               int limit,
                               oddsockets_success_callback on_success,
                               oddsockets_error_callback on_error,
                               void* user_data);

int oddsockets_filter_messages(oddsockets_client_t* client,
                               const char* filters_json,
                               oddsockets_success_callback on_success,
                               oddsockets_error_callback on_error,
                               void* user_data);

int oddsockets_search_in_channel(oddsockets_client_t* client,
                                 const char* channel,
                                 const char* query,
                                 int limit,
                                 oddsockets_success_callback on_success,
                                 oddsockets_error_callback on_error,
                                 void* user_data);

int oddsockets_search_by_user(oddsockets_client_t* client,
                              const char* user_id,
                              const char* query,
                              int limit,
                              oddsockets_success_callback on_success,
                              oddsockets_error_callback on_error,
                              void* user_data);

/* Challenge / Leaderboard / Achievement Events (10 methods)
 *
 * These mirror the fire-and-forget enhanced emitters above: each takes a raw
 * JSON payload string (as documented per-function) that is emitted verbatim to
 * the worker over the live connection. The worker persists the request and
 * broadcasts the resulting events to the scoped room. Where a request has a
 * response, the worker replies with a "<event>_success" event on success and an
 * error event keyed by the request event name on failure; observe both (and the
 * unsolicited broadcasts listed at the top of oddsockets.h) via oddsockets_on().
 *
 * The payload_json argument must be a complete JSON object literal, e.g.
 *   "{\"challengeId\":\"c1\",\"metric\":\"score\",\"ranked\":true}".
 * Pass NULL or "{}" where the operation takes no fields.
 */

/* Create a challenge / leaderboard.
 * payload: {challengeId, metric, ranked?, channel?, resultWebhookUrl?, standingsUrl?}
 * emits "challenge_create"; ack "challenge_create_success"; error event "challenge_create". */
int oddsockets_create_challenge(oddsockets_client_t* client,
                                const char* payload_json);

/* Report progress toward a challenge metric (fire-and-forget).
 * payload: {challengeId, value, metric?, eventId?, cohort?, platform?, channel?}
 * emits "challenge_progress"; no acknowledgement. */
int oddsockets_report_progress(oddsockets_client_t* client,
                               const char* payload_json);

/* Complete (or otherwise finalise) a challenge for the caller.
 * payload: {challengeId, outcome, eventId?, reward?}
 *   outcome one of: completed | failed | expired | conceded | tied
 * emits "challenge_complete"; ack "challenge_complete_success"; error event "challenge_complete". */
int oddsockets_complete_challenge(oddsockets_client_t* client,
                                  const char* payload_json);

/* Unlock (or advance) an achievement (fire-and-forget).
 * payload: {achievementId, name?, tier?, percentComplete?, challengeId?, channel?}
 *   percentComplete < 100 broadcasts "achievement_progress";
 *   percentComplete >= 100 (or omitted) broadcasts "achievement_unlock".
 * emits "achievement_unlock"; no acknowledgement. */
int oddsockets_unlock_achievement(oddsockets_client_t* client,
                                  const char* payload_json);

/* Query the current standings/leaderboard for a challenge.
 * payload: {challengeId, limit?=20, offset?=0}
 * emits "challenge_standings"; ack "challenge_standings_success"; error event "challenge_standings". */
int oddsockets_get_standings(oddsockets_client_t* client,
                             const char* payload_json);

/* Query the caller's achievement state.
 * payload: {achievementId?}  (omit achievementId to return all)
 * emits "achievement_query"; ack "achievement_state"; error event "achievement_query". */
int oddsockets_get_achievements(oddsockets_client_t* client,
                                const char* payload_json);

/* Send a directed challenge invite to another user.
 * payload: {toUserId, type?='match', payload?<=8KB, ttl?=300, channel?, inviteId?}
 * emits "challenge_invite"; ack "challenge_invite_success"; error event "challenge_invite". */
int oddsockets_send_challenge_invite(oddsockets_client_t* client,
                                     const char* payload_json);

/* Reply to a received challenge invite.
 * payload: {inviteId, accept, reason?}
 * emits "challenge_reply"; ack "challenge_reply_success"; error event "challenge_reply". */
int oddsockets_reply_challenge_invite(oddsockets_client_t* client,
                                      const char* payload_json);

/* Cancel a challenge invite the caller previously sent.
 * payload: {inviteId}
 * emits "challenge_invite_cancel"; ack "challenge_invite_cancel_success"; error event "challenge_invite_cancel". */
int oddsockets_cancel_challenge_invite(oddsockets_client_t* client,
                                       const char* payload_json);

/* List the caller's pending challenge invites (takes no fields).
 * payload: {} (pass NULL or "{}")
 * emits "challenge_invites_query"; ack "challenge_invites"; error event "challenge_invites_query". */
int oddsockets_get_challenge_invites(oddsockets_client_t* client,
                                     const char* payload_json);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_ENHANCED_FEATURES_H */
