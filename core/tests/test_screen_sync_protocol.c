#include <string.h>

#include "altcross/screen_sync_protocol.h"
#include "test_framework.h"

#if defined(_WIN32)
#include <windows.h>
#define TEST_SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define TEST_SLEEP_MS(ms) usleep((unsigned int)(ms) * 1000)
#endif

static int wait_for_zone(altcross_screen_sync_incoming_zone_t *out) {
    for (int i = 0; i < 20; i++) {
        if (altcross_screen_sync_poll_incoming_zone(out)) {
            return 1;
        }
        TEST_SLEEP_MS(50);
    }
    return 0;
}

static void test_poll_returns_zero_when_nothing_pending(void) {
    altcross_screen_sync_incoming_zone_t zone;
    ASSERT_EQ(0, altcross_screen_sync_poll_incoming_zone(&zone));
}

/* Consulta as telas reais desta própria máquina (via 127.0.0.1) — prova que
 * SCREENS_QUERY/SCREENS_REPLY de verdade batem com o que
 * altcross_displays_enumerate devolve localmente, não um valor chutado. */
static void test_query_screens_matches_local_enumeration(void) {
    ASSERT_EQ(0, altcross_screen_sync_start_responder("device-S1", "PC S1"));

    altcross_display_t local[ALTCROSS_MAX_DISPLAYS];
    int local_count =
        altcross_displays_enumerate(local, ALTCROSS_MAX_DISPLAYS);

    altcross_display_t queried[ALTCROSS_MAX_DISPLAYS];
    int queried_count = altcross_screen_sync_query_screens(
        "127.0.0.1", 2000, queried, ALTCROSS_MAX_DISPLAYS);

    altcross_screen_sync_stop_responder();

    ASSERT_EQ(local_count, queried_count);
    ASSERT_TRUE(queried_count >= 1);
    for (int i = 0; i < queried_count && i < local_count; i++) {
        ASSERT_EQ((int)local[i].x, (int)queried[i].x);
        ASSERT_EQ((int)local[i].y, (int)queried[i].y);
        ASSERT_EQ((int)local[i].width, (int)queried[i].width);
        ASSERT_EQ((int)local[i].height, (int)queried[i].height);
        ASSERT_EQ(local[i].is_primary != 0, queried[i].is_primary != 0);
    }
}

static void test_query_screens_times_out_without_responder(void) {
    altcross_display_t out[ALTCROSS_MAX_DISPLAYS];
    int rc = altcross_screen_sync_query_screens("127.0.0.1", 300, out,
                                                 ALTCROSS_MAX_DISPLAYS);
    ASSERT_EQ(-1, rc);
}

static void test_zone_push_delivers_sender_info(void) {
    ASSERT_EQ(0, altcross_screen_sync_start_responder("device-S2", "PC S2"));

    int rc = altcross_screen_sync_push_zone("127.0.0.1", "device-Z9", "PC Z9",
                                             4 /* right, ver HotZoneEdge */,
                                             1);
    ASSERT_EQ(0, rc);

    altcross_screen_sync_incoming_zone_t zone;
    ASSERT_TRUE(wait_for_zone(&zone));
    ASSERT_EQ(0, strcmp("device-Z9", zone.sender_device_id));
    ASSERT_EQ(0, strcmp("PC Z9", zone.sender_name));
    ASSERT_EQ(4, zone.sender_edge);
    ASSERT_EQ(1, zone.sender_target_screen_index);

    /* consumida — não aparece de novo */
    ASSERT_EQ(0, altcross_screen_sync_poll_incoming_zone(&zone));

    altcross_screen_sync_stop_responder();
}

void run_screen_sync_protocol_tests(void) {
    RUN_TEST(test_poll_returns_zero_when_nothing_pending);
    RUN_TEST(test_query_screens_matches_local_enumeration);
    RUN_TEST(test_query_screens_times_out_without_responder);
    RUN_TEST(test_zone_push_delivers_sender_info);
}
