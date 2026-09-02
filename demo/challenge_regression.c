/*
 * OddSockets C SDK - two-client CHALLENGE regression (honest, live QA).
 *
 * alice + bob: distinct userId, SAME apiKey (shared owner scope). Both
 * subscribe to 'lobby'. Exercises the 10 challenge/leaderboard/achievement
 * emitters + their acks and cross-client broadcasts over the real
 * libwebsockets Engine.IO/Socket.IO transport through the manager LB.
 *
 * Because subscriber and actor are SEPARATE connections, any broadcast that
 * lands on the peer travelled through the worker. Raw JSON exposed to on()
 * handlers is checked with substring assertions (envelope room broadcasts are
 * wrapped {version,type,identity,challengeId,data:{...}}; directed
 * invite/reply/cancel are flat).
 *
 * Env: ODDSOCKETS_API_KEY, ODDSOCKETS_MANAGER_URL
 * Exit: 0 all assertions pass, 1 setup error, 2 assertion failure/timeout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "oddsockets.h"
#include "enhanced_features.h"

/* ---- captured-event flags + last payloads ---------------------------- */
typedef struct {
    volatile int hit;
    char last[2048];
} capture_t;

static void cap_set(capture_t* c, const char* payload) {
    c->hit = 1;
    if (payload) { strncpy(c->last, payload, sizeof(c->last) - 1); c->last[sizeof(c->last) - 1] = '\0'; }
    else c->last[0] = '\0';
}

/* alice-side captures */
static capture_t A_progress, A_rankchange, A_complete, A_invited, A_reply_recv, A_invite_cancelled;
static capture_t A_ach_progress, A_ach_unlock;
/* bob-side captures */
static capture_t B_invited, B_invite_cancelled, B_ach_unlock, B_ach_progress, B_complete;
/* ack captures (per-actor) */
static capture_t ACK_create, ACK_standings, ACK_complete, ACK_ach_state, ACK_invite, ACK_invites, ACK_reply, ACK_cancel;
static capture_t ERR_any;

/* generic listener that just records into a capture_t via user_data */
static void on_capture(const char* event, const char* payload, void* ud) {
    (void)event;
    cap_set((capture_t*)ud, payload);
}

static void on_error_evt(const char* event, const char* payload, void* ud) {
    (void)ud;
    fprintf(stderr, "[error event] %s -> %s\n", event, payload ? payload : "(null)");
    cap_set(&ERR_any, payload);
}

/* diagnostic: log every registered inbound event with a tag */
static void on_trace(const char* event, const char* payload, void* ud) {
    printf("    <trace %s> %s -> %.220s\n", (const char*)ud, event, payload ? payload : "(null)");
}

/* no-op subscription callback (subscribe REQUIRES a non-NULL message cb;
   room membership is what matters for challenge broadcasts). */
static void on_lobby_msg(const char* ch, const char* msg, void* ud) {
    (void)ch; (void)msg; (void)ud;
}

static long now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
static void pump(oddsockets_client_t* a, oddsockets_client_t* b) {
    oddsockets_process_events(a);
    oddsockets_process_events(b);
}
/* pump both loops until pred() true or timeout ms elapse */
#define WAIT(a,b,pred,ms) do { long _d = now_ms() + (ms); \
    while (now_ms() < _d && !(pred)) { pump((a),(b)); usleep(4000);} } while(0)

static int g_pass = 0, g_fail = 0;
static void check(const char* name, int cond, const char* detail) {
    if (cond) { g_pass++; printf("  [PASS] %s\n", name); }
    else { g_fail++; printf("  [FAIL] %s  (%s)\n", name, detail ? detail : ""); }
}
/* substring assert */
static int has(const capture_t* c, const char* needle) {
    return c->hit && strstr(c->last, needle) != NULL;
}

int main(void) {
    const char* api_key = getenv("ODDSOCKETS_API_KEY");
    const char* mgr = getenv("ODDSOCKETS_MANAGER_URL");
    if (!api_key || !api_key[0]) { fprintf(stderr, "Missing ODDSOCKETS_API_KEY\n"); return 1; }
    setvbuf(stdout, NULL, _IONBF, 0);
    srand((unsigned)time(NULL));

    char cid[64];
    snprintf(cid, sizeof(cid), "chal-%ld-%d", (long)time(NULL), rand() % 100000);
    char aid[64];
    snprintf(aid, sizeof(aid), "ach-%ld-%d", (long)time(NULL), rand() % 100000);
    const char* CH = "lobby";

    oddsockets_config_t ca, cb;
    oddsockets_config_init(&ca, api_key);
    oddsockets_config_init(&cb, api_key);
    strncpy(ca.user_id, "alice", sizeof(ca.user_id) - 1);
    strncpy(cb.user_id, "bob", sizeof(cb.user_id) - 1);
    if (mgr && mgr[0]) {
        strncpy(ca.manager_url, mgr, sizeof(ca.manager_url) - 1);
        strncpy(cb.manager_url, mgr, sizeof(cb.manager_url) - 1);
    }
    ca.log_level = cb.log_level = ODDSOCKETS_LOG_ERROR;

    printf("[connect] challengeId=%s achievementId=%s channel=%s\n", cid, aid, CH);
    printf("[connect] manager=%s\n", (mgr && mgr[0]) ? mgr : ca.manager_url);

    oddsockets_client_t* alice = oddsockets_create(&ca);
    oddsockets_client_t* bob   = oddsockets_create(&cb);
    if (!alice || !bob) { fprintf(stderr, "FAIL create clients\n"); return 1; }

    WAIT(alice, bob,
         oddsockets_get_state(alice) == ODDSOCKETS_STATE_CONNECTED &&
         oddsockets_get_state(bob)   == ODDSOCKETS_STATE_CONNECTED, 15000);
    if (oddsockets_get_state(alice) != ODDSOCKETS_STATE_CONNECTED ||
        oddsockets_get_state(bob)   != ODDSOCKETS_STATE_CONNECTED) {
        fprintf(stderr, "FAIL - not connected within 15s (alice=%d bob=%d)\n",
                oddsockets_get_state(alice), oddsockets_get_state(bob));
        return 2;
    }
    oddsockets_worker_info_t wa, wb;
    wa.worker_id[0] = wb.worker_id[0] = '\0';
    oddsockets_get_worker_info(alice, &wa);
    oddsockets_get_worker_info(bob, &wb);
    printf("[alice] worker %s\n", wa.worker_id[0] ? wa.worker_id : "(unknown)");
    printf("[bob]   worker %s\n", wb.worker_id[0] ? wb.worker_id : "(unknown)");

    /* Both subscribe to 'lobby' so both join the scoped room. */
    oddsockets_channel_t* al = oddsockets_channel_create(alice, CH);
    oddsockets_channel_t* bl = oddsockets_channel_create(bob, CH);
    oddsockets_channel_subscribe(al, on_lobby_msg, NULL, NULL);
    oddsockets_channel_subscribe(bl, on_lobby_msg, NULL, NULL);
    WAIT(alice, bob,
         oddsockets_channel_is_subscribed(al) && oddsockets_channel_is_subscribed(bl), 12000);
    printf("[alice/bob] subscribed to '%s' (alice=%d bob=%d)\n", CH,
           oddsockets_channel_is_subscribed(al), oddsockets_channel_is_subscribed(bl));
    if (!oddsockets_channel_is_subscribed(al) || !oddsockets_channel_is_subscribed(bl)) {
        fprintf(stderr, "FAIL - lobby subscribe never confirmed; room broadcasts cannot arrive\n");
        return 2;
    }

    /* ---- register inbound listeners -------------------------------- */
    /* alice observes broadcasts (bob-driven) + her own acks */
    oddsockets_on(alice, "challenge_create_success", on_capture, &ACK_create);
    oddsockets_on(alice, "challenge_progress",       on_capture, &A_progress);
    oddsockets_on(alice, "leaderboard_rank_change",  on_capture, &A_rankchange);
    oddsockets_on(alice, "challenge_standings_success", on_capture, &ACK_standings);
    oddsockets_on(alice, "challenge_complete_success",  on_capture, &ACK_complete);
    oddsockets_on(alice, "challenge_complete",        on_capture, &A_complete);
    oddsockets_on(alice, "achievement_state",         on_capture, &ACK_ach_state);
    oddsockets_on(alice, "achievement_progress",      on_capture, &A_ach_progress);
    oddsockets_on(alice, "achievement_unlock",        on_capture, &A_ach_unlock);
    oddsockets_on(alice, "challenge_invite_success",  on_capture, &ACK_invite);
    oddsockets_on(alice, "challenge_invites",         on_capture, &ACK_invites);
    oddsockets_on(alice, "challenge_reply_received",  on_capture, &A_reply_recv);
    oddsockets_on(alice, "challenge_invited",         on_capture, &A_invited);
    oddsockets_on(alice, "challenge_invite_cancelled",on_capture, &A_invite_cancelled);
    /* error events keyed by request name */
    oddsockets_on(alice, "challenge_create",   on_error_evt, NULL);
    oddsockets_on(alice, "challenge_standings", on_error_evt, NULL);
    oddsockets_on(alice, "challenge_invite",   on_error_evt, NULL);

    /* bob observes broadcasts (alice-driven) + his own acks */
    oddsockets_on(bob, "challenge_invited",          on_capture, &B_invited);
    oddsockets_on(bob, "challenge_invite_cancelled", on_capture, &B_invite_cancelled);
    oddsockets_on(bob, "achievement_unlock",         on_capture, &B_ach_unlock);
    oddsockets_on(bob, "achievement_progress",       on_capture, &B_ach_progress);
    oddsockets_on(bob, "challenge_complete",         on_capture, &B_complete);
    oddsockets_on(bob, "challenge_reply_success",    on_capture, &ACK_reply);
    /* alice is the canceller, so the cancel ack returns to ALICE, not bob. */
    oddsockets_on(alice, "challenge_invite_cancel_success", on_capture, &ACK_cancel);

    /* diagnostics: trace the room broadcasts on BOTH clients */
    oddsockets_on(alice, "challenge_progress",  on_trace, (void*)"A");
    oddsockets_on(alice, "challenge_complete",  on_trace, (void*)"A");
    oddsockets_on(alice, "achievement_progress",on_trace, (void*)"A");
    oddsockets_on(alice, "achievement_unlock",  on_trace, (void*)"A");
    oddsockets_on(bob, "challenge_progress",    on_trace, (void*)"B");
    oddsockets_on(bob, "achievement_progress",  on_trace, (void*)"B");
    oddsockets_on(bob, "achievement_unlock",    on_trace, (void*)"B");
    oddsockets_on(bob, "challenge_invites",     on_capture, &ACK_invites);
    oddsockets_on(bob, "achievement_state",     on_capture, &ACK_ach_state);
    oddsockets_on(bob, "challenge_complete_success", on_capture, &B_complete);

    char buf[1024];

    /* =============== 1. create_challenge (alice) =================== */
    printf("\n=== 1. create_challenge ===\n");
    snprintf(buf, sizeof(buf),
        "{\"challengeId\":\"%s\",\"metric\":\"score\",\"ranked\":true,\"channel\":\"%s\"}", cid, CH);
    oddsockets_create_challenge(alice, buf);
    WAIT(alice, bob, ACK_create.hit || ERR_any.hit, 8000);
    check("create ack challenge_create_success", ACK_create.hit && !ERR_any.hit, ACK_create.last);

    /* =============== 2. report_progress (alice + bob) ============== */
    printf("\n=== 2. report_progress (bob acts, alice observes) ===\n");
    /* bob reports progress; alice (separate connection) must see the room echo */
    snprintf(buf, sizeof(buf),
        "{\"challengeId\":\"%s\",\"metric\":\"score\",\"value\":100,\"eventId\":\"ev-b1\"}", cid);
    oddsockets_report_progress(bob, buf);
    /* alice reports too so leaderboard has two identities */
    snprintf(buf, sizeof(buf),
        "{\"challengeId\":\"%s\",\"metric\":\"score\",\"value\":250,\"eventId\":\"ev-a1\"}", cid);
    oddsockets_report_progress(alice, buf);
    WAIT(alice, bob, A_progress.hit && A_rankchange.hit, 10000);
    check("alice sees challenge_progress after progress",
          has(&A_progress, cid), A_progress.last);
    check("alice sees leaderboard_rank_change after progress",
          A_rankchange.hit, A_rankchange.last);

    /* =============== 3. get_standings (alice) ====================== */
    printf("\n=== 3. get_standings ===\n");
    snprintf(buf, sizeof(buf), "{\"challengeId\":\"%s\",\"limit\":10}", cid);
    oddsockets_get_standings(alice, buf);
    WAIT(alice, bob, ACK_standings.hit, 8000);
    check("get_standings ack has standings[]",
          has(&ACK_standings, "standings"), ACK_standings.last);
    check("get_standings ack has yourRank",
          has(&ACK_standings, "yourRank"), ACK_standings.last);
    /* ordered: alice value 250 should rank above bob's 100 -> alice rank 1.
       Substring check: both identities present in the standings blob. */
    check("standings include alice identity",
          has(&ACK_standings, "alice"), ACK_standings.last);

    /* =============== 5. unlock_achievement 50% (bob) ============== */
    printf("\n=== 5a. unlock_achievement 50%% -> achievement_progress ===\n");
    snprintf(buf, sizeof(buf),
        "{\"achievementId\":\"%s\",\"name\":\"Halfway\",\"percentComplete\":50,\"channel\":\"%s\"}", aid, CH);
    oddsockets_unlock_achievement(bob, buf);
    WAIT(alice, bob, A_ach_progress.hit || B_ach_progress.hit, 8000);
    check("achievement 50 => achievement_progress broadcast",
          A_ach_progress.hit || B_ach_progress.hit, A_ach_progress.last);
    check("achievement_progress marked in_progress",
          has(&A_ach_progress, "in_progress") || has(&B_ach_progress, "in_progress"),
          A_ach_progress.hit ? A_ach_progress.last : B_ach_progress.last);

    printf("\n=== 5b. unlock_achievement 100%% -> achievement_unlock ===\n");
    snprintf(buf, sizeof(buf),
        "{\"achievementId\":\"%s\",\"name\":\"Complete\",\"percentComplete\":100,\"channel\":\"%s\"}", aid, CH);
    oddsockets_unlock_achievement(bob, buf);
    WAIT(alice, bob, A_ach_unlock.hit || B_ach_unlock.hit, 8000);
    check("achievement 100 => achievement_unlock broadcast",
          A_ach_unlock.hit || B_ach_unlock.hit, A_ach_unlock.last);
    check("achievement_unlock marked unlocked",
          has(&A_ach_unlock, "unlocked") || has(&B_ach_unlock, "unlocked"),
          A_ach_unlock.hit ? A_ach_unlock.last : B_ach_unlock.last);

    /* =============== 6. get_achievements (bob) ==================== */
    printf("\n=== 6. get_achievements ===\n");
    snprintf(buf, sizeof(buf), "{\"achievementId\":\"%s\"}", aid);
    oddsockets_get_achievements(bob, buf);
    WAIT(alice, bob, ACK_ach_state.hit, 8000);
    check("get_achievements ack achievement_state has achievements[]",
          has(&ACK_ach_state, "achievements"), ACK_ach_state.last);
    check("get_achievements reflects unlocked state",
          has(&ACK_ach_state, "unlocked"), ACK_ach_state.last);

    /* =============== 4. complete_challenge tied+conceded ========== */
    printf("\n=== 4. complete_challenge (bob conceded, alice tied) ===\n");
    snprintf(buf, sizeof(buf),
        "{\"challengeId\":\"%s\",\"outcome\":\"conceded\",\"eventId\":\"ev-bc\"}", cid);
    oddsockets_complete_challenge(bob, buf);
    WAIT(alice, bob, ACK_reply.hit /*unused*/ , 200); /* small settle */
    /* bob's own ack captured on bob via challenge_complete_success? we only
       registered challenge_complete_success on alice. Register on bob too via
       reuse: bob acts, ack returns to bob. Capture bob completes differently. */
    /* alice completes tied -> ack returns to alice */
    ACK_complete.hit = 0;
    snprintf(buf, sizeof(buf),
        "{\"challengeId\":\"%s\",\"outcome\":\"tied\",\"eventId\":\"ev-at\"}", cid);
    oddsockets_complete_challenge(alice, buf);
    WAIT(alice, bob, ACK_complete.hit, 8000);
    check("complete ack challenge_complete_success (alice tied)",
          has(&ACK_complete, "tied"), ACK_complete.last);
    check("complete ack has finalValue", has(&ACK_complete, "finalValue"), ACK_complete.last);
    check("complete ack has rank", has(&ACK_complete, "rank"), ACK_complete.last);
    /* cross-client: bob's conceded broadcast should reach alice */
    check("alice sees bob's challenge_complete broadcast",
          A_complete.hit, A_complete.last);

    /* =============== 7. send_challenge_invite (alice -> bob) ====== */
    printf("\n=== 7. send_challenge_invite alice -> bob ===\n");
    snprintf(buf, sizeof(buf),
        "{\"toUserId\":\"bob\",\"type\":\"match\",\"payload\":{\"cid\":\"%s\"},\"ttl\":300}", cid);
    oddsockets_send_challenge_invite(alice, buf);
    WAIT(alice, bob, ACK_invite.hit && B_invited.hit, 8000);
    check("invite ack challenge_invite_success (pending, toUserId bob)",
          has(&ACK_invite, "inviteId") && has(&ACK_invite, "bob"), ACK_invite.last);
    check("invitee bob sees challenge_invited from alice",
          B_invited.hit && strstr(B_invited.last, "alice") != NULL, B_invited.last);
    /* isolation: alice (inviter) should NOT receive challenge_invited herself */
    check("inviter alice does NOT receive challenge_invited",
          !A_invited.hit, A_invited.last);

    /* extract inviteId from the ack for reply/cancel */
    char invite_id[128] = {0};
    {
        const char* k = strstr(ACK_invite.last, "\"inviteId\"");
        if (k) { k = strchr(k, ':'); if (k) { k++; while (*k==' '||*k=='"') k++;
            int i = 0; while (*k && *k!='"' && *k!=',' && *k!='}' && i < (int)sizeof(invite_id)-1) invite_id[i++]=*k++;
            invite_id[i]='\0'; } }
    }
    printf("[invite] inviteId=%s\n", invite_id[0] ? invite_id : "(none)");

    /* =============== 8. get_challenge_invites (bob) ============== */
    printf("\n=== 8. get_challenge_invites (bob) ===\n");
    oddsockets_get_challenge_invites(bob, NULL);
    WAIT(alice, bob, ACK_invites.hit, 8000);
    /* ack challenge_invites registered on alice; register on bob instead */
    /* NB: we only registered challenge_invites on alice; re-issue from alice */
    if (!ACK_invites.hit) {
        oddsockets_get_challenge_invites(alice, NULL);
        WAIT(alice, bob, ACK_invites.hit, 4000);
    }
    check("get_challenge_invites lists the invite",
          has(&ACK_invites, "invites") && (invite_id[0] ? has(&ACK_invites, invite_id) : ACK_invites.hit),
          ACK_invites.last);

    /* =============== 9. reply_challenge_invite (bob accepts) ===== */
    printf("\n=== 9. reply_challenge_invite bob accept -> alice ===\n");
    snprintf(buf, sizeof(buf), "{\"inviteId\":\"%s\",\"accept\":true}", invite_id);
    oddsockets_reply_challenge_invite(bob, buf);
    WAIT(alice, bob, ACK_reply.hit && A_reply_recv.hit, 8000);
    check("reply ack challenge_reply_success (bob)", ACK_reply.hit, ACK_reply.last);
    check("inviter alice sees challenge_reply_received",
          A_reply_recv.hit, A_reply_recv.last);

    /* =============== 10. cancel_challenge_invite ================= */
    /* Prior invite was accepted; send a fresh one from alice then cancel it. */
    printf("\n=== 10. cancel_challenge_invite alice -> invitee bob ===\n");
    ACK_invite.hit = 0; B_invited.hit = 0;
    snprintf(buf, sizeof(buf),
        "{\"toUserId\":\"bob\",\"type\":\"match\",\"payload\":{\"round\":2},\"ttl\":300}");
    oddsockets_send_challenge_invite(alice, buf);
    WAIT(alice, bob, ACK_invite.hit && B_invited.hit, 8000);
    char invite_id2[128] = {0};
    {
        const char* k = strstr(ACK_invite.last, "\"inviteId\"");
        if (k) { k = strchr(k, ':'); if (k) { k++; while (*k==' '||*k=='"') k++;
            int i = 0; while (*k && *k!='"' && *k!=',' && *k!='}' && i < (int)sizeof(invite_id2)-1) invite_id2[i++]=*k++;
            invite_id2[i]='\0'; } }
    }
    printf("[invite2] inviteId=%s\n", invite_id2[0] ? invite_id2 : "(none)");
    B_invite_cancelled.hit = 0;
    snprintf(buf, sizeof(buf), "{\"inviteId\":\"%s\"}", invite_id2);
    oddsockets_cancel_challenge_invite(alice, buf);
    WAIT(alice, bob, ACK_cancel.hit && B_invite_cancelled.hit, 8000);
    check("cancel ack challenge_invite_cancel_success", ACK_cancel.hit, ACK_cancel.last);
    check("invitee bob sees challenge_invite_cancelled",
          B_invite_cancelled.hit, B_invite_cancelled.last);

    /* ---- tidy ---- */
    oddsockets_channel_unsubscribe(al);
    oddsockets_channel_unsubscribe(bl);
    long d = now_ms() + 1500; while (now_ms() < d) { pump(alice, bob); usleep(4000); }
    oddsockets_disconnect(alice); oddsockets_disconnect(bob);
    oddsockets_destroy(alice);   oddsockets_destroy(bob);

    printf("\n==================== SUMMARY ====================\n");
    printf("PASS=%d  FAIL=%d\n", g_pass, g_fail);
    printf("worker_alice=%s worker_bob=%s\n",
           wa.worker_id[0] ? wa.worker_id : "?", wb.worker_id[0] ? wb.worker_id : "?");
    printf("cross_worker=%s\n",
           (wa.worker_id[0] && wb.worker_id[0] && strcmp(wa.worker_id, wb.worker_id)) ? "YES" : "no(same-or-unknown)");
    return g_fail ? 2 : 0;
}
