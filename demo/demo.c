/*
 * OddSockets C SDK - two-client honest regression demo
 *
 * Proves real real-time behaviour using TWO independent clients (alice and
 * bob) on SEPARATE connections. Everything below crosses the wire through the
 * assigned OddSockets worker - no mocks, no local echo.
 *
 * Scenario 1 - core pub/sub:
 *   alice subscribes -> bob publishes a nonce-tagged body -> alice receives it.
 *
 * Scenario 2 - enhanced (Slack-like) events:
 *   both subscribe -> alice registers on("user_typing")/on("reaction_added")
 *   -> bob fires start_typing + add_reaction -> alice receives both broadcasts
 *   on her public on() surface with the real server payloads.
 *
 * Because the subscriber and publisher are separate connections, an event that
 * reaches alice can only have travelled through the worker. The SDK speaks
 * genuine Socket.IO (Engine.IO v4) over a WebSocket.
 *
 * Reads the API key from ODDSOCKETS_API_KEY.
 *   ODDSOCKETS_API_KEY=ak_... ./oddsockets-demo
 *
 * Exit codes: 0 all scenarios verified, 1 setup error, 2 a scenario timed out.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "oddsockets.h"
#include "enhanced_features.h"

typedef struct {
    const char* nonce;
    volatile int received;
} inbox_t;

typedef struct {
    volatile int typing;
    volatile int reaction;
} enh_inbox_t;

static volatile int g_presence = -1;

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* Alice's subscription callback - a message only lands here if it came back
   through the worker from bob's separate connection. */
static void on_message(const char* channel, const char* message, void* user_data) {
    (void)channel;
    inbox_t* inbox = (inbox_t*)user_data;
    if (message && inbox->nonce && strstr(message, inbox->nonce)) {
        inbox->received = 1;
        printf("[alice] received bob's message (nonce matched) - real round-trip.\n");
    }
}

static void on_presence(const char* channel, const char* action, const char* user_id, void* user_data) {
    (void)channel; (void)action; (void)user_data;
    if (user_id) g_presence = atoi(user_id);
}

/* No-op subscription callback for the enhanced channel (both clients must be
   subscribed to join the scoped room where enhanced broadcasts are delivered). */
static void on_enh_message(const char* channel, const char* message, void* user_data) {
    (void)channel; (void)message; (void)user_data;
}

/* Raw enhanced-event listeners on alice's public on() surface. These fire only
   when bob's enhanced actions are broadcast back through the worker. */
static void on_user_typing(const char* event, const char* payload, void* user_data) {
    (void)event;
    enh_inbox_t* e = (enh_inbox_t*)user_data;
    if (payload) {
        e->typing = 1;
        printf("[alice on user_typing] %s\n", payload);
    }
}

static void on_reaction_added(const char* event, const char* payload, void* user_data) {
    (void)event;
    enh_inbox_t* e = (enh_inbox_t*)user_data;
    if (payload) {
        e->reaction = 1;
        printf("[alice on reaction_added] %s\n", payload);
    }
}

/* Service both clients' event loops once. */
static void pump(oddsockets_client_t* a, oddsockets_client_t* b) {
    oddsockets_process_events(a);
    oddsockets_process_events(b);
}

int main(void) {
    const char* api_key = getenv("ODDSOCKETS_API_KEY");
    if (!api_key || !api_key[0]) {
        fprintf(stderr, "Missing ODDSOCKETS_API_KEY. Get a free key (see README), then:\n");
        fprintf(stderr, "  export ODDSOCKETS_API_KEY=\"ak_...\"\n");
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    srand((unsigned)time(NULL));

    char nonce[64];
    snprintf(nonce, sizeof(nonce), "%ld-%d", (long)time(NULL), rand());
    char core_channel[80];
    snprintf(core_channel, sizeof(core_channel), "demo-core-%s", nonce);
    char enh_channel[80];
    snprintf(enh_channel, sizeof(enh_channel), "demo-enh-%s", nonce);

    oddsockets_config_t ca, cb;
    oddsockets_config_init(&ca, api_key);
    strncpy(ca.user_id, "alice", sizeof(ca.user_id) - 1);
    ca.log_level = ODDSOCKETS_LOG_ERROR;
    oddsockets_config_init(&cb, api_key);
    strncpy(cb.user_id, "bob", sizeof(cb.user_id) - 1);
    cb.log_level = ODDSOCKETS_LOG_ERROR;

    printf("[connect] connecting both clients...\n");

    /* Two independent connections - this is what makes the test honest. */
    oddsockets_client_t* alice = oddsockets_create(&ca);
    oddsockets_client_t* bob = oddsockets_create(&cb);
    if (!alice || !bob) {
        fprintf(stderr, "\nFAIL - could not create clients (worker assignment failed?)\n");
        return 1;
    }

    /* Drive the handshakes to completion (Engine.IO OPEN -> CONNECT -> ack). */
    long deadline = now_ms() + 10000;
    while (now_ms() < deadline &&
           !(oddsockets_get_state(alice) == ODDSOCKETS_STATE_CONNECTED &&
             oddsockets_get_state(bob) == ODDSOCKETS_STATE_CONNECTED)) {
        pump(alice, bob);
        usleep(5000);
    }
    if (oddsockets_get_state(alice) != ODDSOCKETS_STATE_CONNECTED ||
        oddsockets_get_state(bob) != ODDSOCKETS_STATE_CONNECTED) {
        fprintf(stderr, "\nFAIL - clients did not connect within 10s\n");
        return 2;
    }

    oddsockets_worker_info_t wa, wb;
    if (oddsockets_get_worker_info(alice, &wa) == ODDSOCKETS_SUCCESS)
        printf("[alice] worker %s\n", wa.worker_id);
    if (oddsockets_get_worker_info(bob, &wb) == ODDSOCKETS_SUCCESS)
        printf("[bob]   worker %s\n", wb.worker_id);
    printf("[connect] alice = connected, bob = connected\n");

    /* ============================ Scenario 1 ============================ */
    printf("\n=== Scenario 1: core pub/sub on %s ===\n", core_channel);

    inbox_t inbox = { .nonce = nonce, .received = 0 };
    oddsockets_channel_t* ac = oddsockets_channel_create(alice, core_channel);
    oddsockets_subscribe_options_t opts = { .max_history = 0, .retain_history = false, .enable_presence = true };
    if (oddsockets_channel_subscribe(ac, on_message, &inbox, &opts) != ODDSOCKETS_SUCCESS) {
        fprintf(stderr, "\nFAIL - subscribe failed\n");
        return 1;
    }
    long sub_deadline = now_ms() + 3000;
    while (now_ms() < sub_deadline && !oddsockets_channel_is_subscribed(ac)) {
        pump(alice, bob);
        usleep(5000);
    }
    printf("[alice] subscribed to %s (presence on)\n", core_channel);

    /* Publisher (bob) - a DIFFERENT connection. The marker lives only inside
       the payload, so a match proves the real body crossed the wire. */
    char body[160];
    snprintf(body, sizeof(body), "{\"text\":\"hello from bob marker=%s\",\"username\":\"bob\"}", nonce);
    oddsockets_channel_t* bc = oddsockets_channel_create(bob, core_channel);
    if (oddsockets_channel_publish(bc, body, NULL) != ODDSOCKETS_SUCCESS) {
        fprintf(stderr, "\nFAIL - publish failed\n");
        return 1;
    }
    printf("[bob] published to %s\n", core_channel);

    long recv_deadline = now_ms() + 12000;
    while (now_ms() < recv_deadline && !inbox.received) {
        pump(alice, bob);
        usleep(5000);
    }
    if (!inbox.received) {
        fprintf(stderr, "\nTIMEOUT - no cross-client delivery within 12s\n");
        return 2;
    }
    printf("[PASS] alice received bob's message across separate connections\n");

    /* Presence (best effort). */
    oddsockets_channel_get_presence(ac, on_presence, NULL);
    long pres_deadline = now_ms() + 3000;
    while (now_ms() < pres_deadline && g_presence < 0) {
        pump(alice, bob);
        usleep(5000);
    }
    if (g_presence >= 0) printf("[alice] presence: %d user(s).\n", g_presence);

    /* ============================ Scenario 2 ============================ */
    printf("\n=== Scenario 2: enhanced events on %s ===\n", enh_channel);

    /* Both clients subscribe so both join the scoped room where enhanced
       broadcasts are delivered. */
    enh_inbox_t enh = { .typing = 0, .reaction = 0 };
    oddsockets_channel_t* ae = oddsockets_channel_create(alice, enh_channel);
    oddsockets_channel_t* be = oddsockets_channel_create(bob, enh_channel);
    oddsockets_channel_subscribe(ae, on_enh_message, NULL, NULL);
    oddsockets_channel_subscribe(be, on_enh_message, NULL, NULL);
    long enh_sub_deadline = now_ms() + 3000;
    while (now_ms() < enh_sub_deadline &&
           !(oddsockets_channel_is_subscribed(ae) && oddsockets_channel_is_subscribed(be))) {
        pump(alice, bob);
        usleep(5000);
    }
    printf("[alice/bob] subscribed to enhanced channel\n");

    /* Alice listens for the enhanced broadcasts on her public on() surface. */
    oddsockets_on(alice, "user_typing", on_user_typing, &enh);
    oddsockets_on(alice, "reaction_added", on_reaction_added, &enh);

    /* Bob fires the enhanced actions from his separate connection. */
    char message_id[96];
    snprintf(message_id, sizeof(message_id), "msg-%s", nonce);
    if (oddsockets_start_typing(bob, "bob", enh_channel) != ODDSOCKETS_SUCCESS) {
        fprintf(stderr, "\nFAIL - start_typing emit failed\n");
        return 1;
    }
    printf("[bob] enhanced.start_typing fired\n");
    if (oddsockets_add_reaction(bob, message_id, enh_channel, ":thumbsup:", "bob", "Bob") != ODDSOCKETS_SUCCESS) {
        fprintf(stderr, "\nFAIL - add_reaction emit failed\n");
        return 1;
    }
    printf("[bob] enhanced.add_reaction fired\n");

    long enh_deadline = now_ms() + 12000;
    while (now_ms() < enh_deadline && !(enh.typing && enh.reaction)) {
        pump(alice, bob);
        usleep(5000);
    }
    if (!(enh.typing && enh.reaction)) {
        fprintf(stderr, "\nTIMEOUT - enhanced broadcast never arrived (typing=%d reaction=%d)\n",
                enh.typing, enh.reaction);
        return 2;
    }
    printf("[PASS] alice received user_typing AND reaction_added from bob\n");

    /* Tidy up. */
    oddsockets_channel_unsubscribe(ac);
    oddsockets_channel_unsubscribe(ae);
    oddsockets_channel_unsubscribe(be);
    long unsub_deadline = now_ms() + 2000;
    while (now_ms() < unsub_deadline) {
        pump(alice, bob);
        usleep(5000);
    }
    printf("[alice] unsubscribed.\n");

    oddsockets_disconnect(alice);
    oddsockets_disconnect(bob);
    oddsockets_destroy(alice);
    oddsockets_destroy(bob);

    printf("\nOK - all scenarios verified live through the OddSockets platform\n");
    return 0;
}
