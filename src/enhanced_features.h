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

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_ENHANCED_FEATURES_H */
