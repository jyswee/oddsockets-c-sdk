/*
 * OddSockets C SDK - two-client round-trip demo
 *
 * Proves a real real-time round-trip using TWO independent clients:
 *   connect -> subscribe (alice) -> publish (bob) -> receive (alice)
 *
 * Because the subscriber (alice) and the publisher (bob) are separate
 * connections, a message that reaches the subscriber can only have travelled
 * through the OddSockets worker - so this doubles as an honest end-to-end
 * regression test (no mocks, no local echo). The SDK speaks genuine Socket.IO
 * (Engine.IO v4) over a WebSocket to the assigned worker.
 *
 * Reads the API key from ODDSOCKETS_API_KEY.
 *   ODDSOCKETS_API_KEY=ak_... ./oddsockets-demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "oddsockets.h"

typedef struct {
    const char* nonce;
    volatile int received;
} inbox_t;

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

    char channel[64];
    snprintf(channel, sizeof(channel), "demo-%d", rand() % 1000000);
    char nonce[64];
    snprintf(nonce, sizeof(nonce), "%ld-%d", (long)time(NULL), rand());

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

    /* Subscriber (alice) - presence enabled. */
    inbox_t inbox = { .nonce = nonce, .received = 0 };
    oddsockets_channel_t* ac = oddsockets_channel_create(alice, channel);
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
    printf("[alice] subscribed to %s (presence on)\n", channel);

    /* Publisher (bob) - a DIFFERENT connection. */
    char body[128];
    snprintf(body, sizeof(body), "{\"text\":\"hello from bob\",\"nonce\":\"%s\"}", nonce);
    oddsockets_channel_t* bc = oddsockets_channel_create(bob, channel);
    if (oddsockets_channel_publish(bc, body, NULL) != ODDSOCKETS_SUCCESS) {
        fprintf(stderr, "\nFAIL - publish failed\n");
        return 1;
    }
    printf("[bob] published to %s\n", channel);

    /* Wait for cross-client delivery. */
    long recv_deadline = now_ms() + 12000;
    while (now_ms() < recv_deadline && !inbox.received) {
        pump(alice, bob);
        usleep(5000);
    }
    if (!inbox.received) {
        fprintf(stderr, "\nTIMEOUT - no cross-client delivery within 12s\n");
        return 2;
    }

    /* Presence (best effort). */
    oddsockets_channel_get_presence(ac, on_presence, NULL);
    long pres_deadline = now_ms() + 3000;
    while (now_ms() < pres_deadline && g_presence < 0) {
        pump(alice, bob);
        usleep(5000);
    }
    if (g_presence >= 0) printf("[alice] presence: %d user(s).\n", g_presence);

    /* Tidy up. */
    oddsockets_channel_unsubscribe(ac);
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

    printf("\nOK - cross-client round-trip verified\n");
    return 0;
}
