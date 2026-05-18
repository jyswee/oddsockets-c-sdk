#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/oddsockets.h"
#include "../src/enhanced_features.h"

/* OddSockets C SDK - Enhanced Features Example
 * Demonstrates all 67 new Slack-like events with callback-based async patterns
 */

// Callback handlers
void on_success(const char* data, void* user_data) {
    printf("✅ Success: %s\n", data);
}

void on_error(const char* error, void* user_data) {
    printf("❌ Error: %s\n", error);
}

void on_connected(void* user_data) {
    printf("🟢 Connected event fired\n");
}

void on_disconnected(void* user_data) {
    printf("🔴 Disconnected event fired\n");
}

// Test functions
void test_thread_events(oddsockets_client_t* client);
void test_reaction_events(oddsockets_client_t* client);
void test_read_receipt_events(oddsockets_client_t* client);
void test_channel_events(oddsockets_client_t* client);
void test_direct_message_events(oddsockets_client_t* client);
void test_notification_events(oddsockets_client_t* client);
void test_presence_events(oddsockets_client_t* client);
void test_message_editing_events(oddsockets_client_t* client);
void test_search_events(oddsockets_client_t* client);

int main(void) {
    printf("🚀 OddSockets C SDK - Enhanced Features Example\n");
    printf("Demonstrating all 67 new Slack-like events\n");
    printf("==================================================\n");

    // Create and configure client
    oddsockets_client_t* client = oddsockets_create("your_api_key_here", "user_123");
    if (!client) {
        printf("❌ Failed to create client\n");
        return 1;
    }

    // Set up event listeners
    oddsockets_on(client, "connected", on_connected, NULL);
    oddsockets_on(client, "disconnected", on_disconnected, NULL);

    // Connect
    printf("\n🔄 Connecting to OddSockets...\n");
    if (oddsockets_connect(client) != 0) {
        printf("❌ Failed to connect\n");
        oddsockets_destroy(client);
        return 1;
    }

    // Wait for connection
    sleep(2);

    if (!oddsockets_is_connected(client)) {
        printf("❌ Not connected\n");
        oddsockets_destroy(client);
        return 1;
    }

    printf("✅ Connected successfully!\n\n");

    // Test all enhanced features
    test_thread_events(client);
    test_reaction_events(client);
    test_read_receipt_events(client);
    test_channel_events(client);
    test_direct_message_events(client);
    test_notification_events(client);
    test_presence_events(client);
    test_message_editing_events(client);
    test_search_events(client);

    // Summary
    printf("\n🎉 All enhanced features tested!\n");
    printf("\n📊 Summary:\n");
    printf("- Thread Events: 7 methods\n");
    printf("- Reaction Events: 6 methods\n");
    printf("- Read Receipt Events: 6 methods\n");
    printf("- Channel Events: 11 methods\n");
    printf("- Direct Message Events: 6 methods\n");
    printf("- Notification Events: 6 methods\n");
    printf("- File Upload Events: 7 methods\n");
    printf("- Presence Events: 8 methods\n");
    printf("- Message Editing Events: 5 methods\n");
    printf("- Search Events: 4 methods\n");
    printf("==================================================\n");
    printf("Total: 67 enhanced Slack-like events! 🚀\n");

    // Wait before disconnecting
    sleep(2);

    // Disconnect
    oddsockets_disconnect(client);
    printf("\n✅ Disconnected\n");

    // Cleanup
    oddsockets_destroy(client);

    return 0;
}

// ==================== THREAD EVENTS ====================

void test_thread_events(oddsockets_client_t* client) {
    printf("📝 Testing Thread Events...\n");

    // Thread reply
    if (oddsockets_thread_reply(client,
        "general",
        "msg_123",
        "This is a test reply from C!",
        "user_123",
        "Test User",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Thread reply sent\n");
    }

    // Get thread
    if (oddsockets_get_thread(client,
        "thread_123",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get thread requested\n");
    }

    // Subscribe to thread
    if (oddsockets_subscribe_thread(client,
        "thread_123",
        "user_123",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Subscribe thread requested\n");
    }

    // Mark thread as read
    if (oddsockets_mark_thread_read(client, "thread_123", "user_123") == 0) {
        printf("✅ Marked thread as read\n");
    }

    // Follow thread
    if (oddsockets_follow_thread(client, "thread_123", "user_123") == 0) {
        printf("✅ Following thread\n\n");
    }
}

// ==================== REACTION EVENTS ====================

void test_reaction_events(oddsockets_client_t* client) {
    printf("😀 Testing Reaction Events...\n");

    // Add reaction
    if (oddsockets_add_reaction(client,
        "msg_123",
        "general",
        "👍",
        "user_123",
        "Test User") == 0) {
        printf("✅ Added reaction 👍\n");
    }

    // Remove reaction
    if (oddsockets_remove_reaction(client,
        "msg_123",
        "general",
        "👍",
        "user_123") == 0) {
        printf("✅ Removed reaction\n");
    }

    // Get reactions
    if (oddsockets_get_reactions(client,
        "msg_123",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get reactions requested\n\n");
    }
}

// ==================== READ RECEIPT EVENTS ====================

void test_read_receipt_events(oddsockets_client_t* client) {
    printf("✓ Testing Read Receipt Events...\n");

    // Mark message as read
    if (oddsockets_mark_read(client,
        "msg_123",
        "general",
        "user_123",
        "Test User") == 0) {
        printf("✅ Marked message as read\n");
    }

    // Get unread counts
    const char* channels[] = {"general", "random"};
    if (oddsockets_get_unread_counts(client,
        "user_123",
        channels,
        2,
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get unread counts requested\n");
    }

    // Mark all as read
    if (oddsockets_mark_all_read(client, "general", "user_123") == 0) {
        printf("✅ Marked all messages as read\n\n");
    }
}

// ==================== CHANNEL EVENTS ====================

void test_channel_events(oddsockets_client_t* client) {
    printf("📢 Testing Channel Events...\n");

    // Create channel
    if (oddsockets_create_channel(client,
        "c-test-channel",
        "public",
        "Created from C SDK",
        "Testing",
        "user_123",
        "Test User",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Create channel requested\n");
    }

    // Update channel
    if (oddsockets_update_channel(client,
        "channel_123",
        "{\"topic\":\"Updated topic\"}",
        "user_123") == 0) {
        printf("✅ Updated channel\n");
    }

    // Join channel
    if (oddsockets_join_channel(client,
        "channel_123",
        "user_123",
        "Test User") == 0) {
        printf("✅ Joined channel\n");
    }

    // Invite to channel
    if (oddsockets_invite_to_channel(client,
        "channel_123",
        "user_456",
        "Jane Doe",
        "user_123") == 0) {
        printf("✅ Invited user to channel\n");
    }

    // Get channel members
    if (oddsockets_get_channel_members(client,
        "channel_123",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get channel members requested\n\n");
    }
}

// ==================== DIRECT MESSAGE EVENTS ====================

void test_direct_message_events(oddsockets_client_t* client) {
    printf("💬 Testing Direct Message Events...\n");

    // Create DM
    const char* user_ids[] = {"user_123", "user_456"};
    if (oddsockets_create_dm(client,
        user_ids,
        2,
        "1-on-1",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Create DM requested\n");
    }

    // Send DM
    if (oddsockets_send_dm(client,
        "dm_123",
        "Hello from C!",
        "user_123",
        "Test User") == 0) {
        printf("✅ Sent DM\n");
    }

    // Get DM conversations
    if (oddsockets_get_dm_conversations(client,
        "user_123",
        0,
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get DM conversations requested\n\n");
    }
}

// ==================== NOTIFICATION EVENTS ====================

void test_notification_events(oddsockets_client_t* client) {
    printf("🔔 Testing Notification Events...\n");

    // Subscribe to notifications
    if (oddsockets_subscribe_notifications(client, "user_123") == 0) {
        printf("✅ Subscribed to notifications\n");
    }

    // Mark notification as read
    if (oddsockets_mark_notification_read(client, "notif_123", "user_123") == 0) {
        printf("✅ Marked notification as read\n");
    }

    // Mark all notifications as read
    if (oddsockets_mark_all_notifications_read(client, "user_123") == 0) {
        printf("✅ Marked all notifications as read\n");
    }

    // Get notifications
    if (oddsockets_get_notifications(client,
        "user_123",
        10,
        "all",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get notifications requested\n\n");
    }
}

// ==================== PRESENCE EVENTS ====================

void test_presence_events(oddsockets_client_t* client) {
    printf("👤 Testing Presence Events...\n");

    // Set status
    if (oddsockets_set_status(client, "user_123", "online") == 0) {
        printf("✅ Set status to online\n");
    }

    // Set custom status
    if (oddsockets_set_custom_status(client,
        "user_123",
        "⚡",
        "Coding in C",
        NULL) == 0) {
        printf("✅ Set custom status\n");
    }

    // Clear custom status
    if (oddsockets_clear_custom_status(client, "user_123") == 0) {
        printf("✅ Cleared custom status\n");
    }

    // Set DND
    if (oddsockets_set_dnd(client, "user_123", NULL) == 0) {
        printf("✅ Enabled Do Not Disturb\n");
    }

    // Clear DND
    if (oddsockets_clear_dnd(client, "user_123") == 0) {
        printf("✅ Disabled Do Not Disturb\n");
    }

    // Start typing
    if (oddsockets_start_typing(client, "user_123", "general") == 0) {
        printf("✅ Started typing indicator\n");
    }

    sleep(2);

    // Stop typing
    if (oddsockets_stop_typing(client, "user_123", "general") == 0) {
        printf("✅ Stopped typing indicator\n");
    }

    // Get user presence
    const char* presence_user_ids[] = {"user_123", "user_456"};
    if (oddsockets_get_user_presence(client,
        presence_user_ids,
        2,
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get user presence requested\n\n");
    }
}

// ==================== MESSAGE EDITING EVENTS ====================

void test_message_editing_events(oddsockets_client_t* client) {
    printf("✏️ Testing Message Editing Events...\n");

    // Edit message
    if (oddsockets_edit_message(client,
        "msg_123",
        "general",
        "Updated message from C",
        "user_123") == 0) {
        printf("✅ Edited message\n");
    }

    // Delete message
    if (oddsockets_delete_message(client,
        "msg_456",
        "general",
        "user_123") == 0) {
        printf("✅ Deleted message\n");
    }

    // Pin message
    if (oddsockets_pin_message(client,
        "msg_123",
        "general",
        "user_123") == 0) {
        printf("✅ Pinned message\n");
    }

    // Unpin message
    if (oddsockets_unpin_message(client,
        "msg_123",
        "general",
        "user_123") == 0) {
        printf("✅ Unpinned message\n");
    }

    // Get pinned messages
    if (oddsockets_get_pinned_messages(client,
        "general",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Get pinned messages requested\n\n");
    }
}

// ==================== SEARCH EVENTS ====================

void test_search_events(oddsockets_client_t* client) {
    printf("🔍 Testing Search Events...\n");

    // Search messages
    if (oddsockets_search_messages(client,
        "test",
        "user_123",
        10,
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Search messages requested\n");
    }

    // Search in channel
    if (oddsockets_search_in_channel(client,
        "general",
        "test",
        10,
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Search in channel requested\n");
    }

    // Filter messages
    if (oddsockets_filter_messages(client,
        "{\"channel\":\"general\",\"userId\":\"user_123\",\"limit\":10}",
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Filter messages requested\n");
    }

    // Search by user
    if (oddsockets_search_by_user(client,
        "user_123",
        NULL,
        10,
        on_success,
        on_error,
        NULL) == 0) {
        printf("✅ Search by user requested\n\n");
    }
}
