#include "altcross/hotzone.h"
#include "test_framework.h"

static altcross_screen_config_t make_cfg(void) {
    altcross_screen_config_t cfg;
    cfg.screen_width = 1920;
    cfg.screen_height = 1080;
    cfg.corner_size = 20;
    return cfg;
}

static void test_detect_returns_none_in_the_middle(void) {
    altcross_screen_config_t cfg = make_cfg();
    ASSERT_EQ(ALTCROSS_EDGE_NONE, altcross_hotzone_detect(960, 540, &cfg));
}

static void test_detect_plain_edges(void) {
    altcross_screen_config_t cfg = make_cfg();
    ASSERT_EQ(ALTCROSS_EDGE_TOP, altcross_hotzone_detect(960, 0, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_BOTTOM, altcross_hotzone_detect(960, 1079, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_LEFT, altcross_hotzone_detect(0, 540, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_RIGHT, altcross_hotzone_detect(1919, 540, &cfg));
}

static void test_detect_corners_take_priority_over_edges(void) {
    altcross_screen_config_t cfg = make_cfg();
    ASSERT_EQ(ALTCROSS_EDGE_TOP_LEFT, altcross_hotzone_detect(0, 0, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_TOP_RIGHT, altcross_hotzone_detect(1919, 0, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_BOTTOM_LEFT,
              altcross_hotzone_detect(0, 1079, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_BOTTOM_RIGHT,
              altcross_hotzone_detect(1919, 1079, &cfg));
    /* dentro do quadrado do canto (corner_size = 20), ainda conta como
     * canto, não como borda simples */
    ASSERT_EQ(ALTCROSS_EDGE_TOP_LEFT, altcross_hotzone_detect(10, 0, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_TOP_LEFT, altcross_hotzone_detect(0, 10, &cfg));
}

static void test_detect_clamps_out_of_bounds_coordinates(void) {
    altcross_screen_config_t cfg = make_cfg();
    ASSERT_EQ(ALTCROSS_EDGE_TOP_LEFT, altcross_hotzone_detect(-5, -5, &cfg));
    ASSERT_EQ(ALTCROSS_EDGE_BOTTOM_RIGHT,
              altcross_hotzone_detect(5000, 5000, &cfg));
}

static void test_resolve_finds_matching_enabled_zone(void) {
    altcross_hotzone_t zones[] = {
        {ALTCROSS_EDGE_TOP, 1, 1},
        {ALTCROSS_EDGE_RIGHT, 2, 1},
    };
    int device_id = -1;
    int found = altcross_hotzone_resolve(zones, 2, ALTCROSS_EDGE_RIGHT,
                                          &device_id);
    ASSERT_EQ(1, found);
    ASSERT_EQ(2, device_id);
}

static void test_resolve_ignores_disabled_zones(void) {
    altcross_hotzone_t zones[] = {
        {ALTCROSS_EDGE_RIGHT, 2, 0},
    };
    int device_id = -1;
    int found = altcross_hotzone_resolve(zones, 1, ALTCROSS_EDGE_RIGHT,
                                          &device_id);
    ASSERT_EQ(0, found);
}

static void test_resolve_returns_zero_when_no_match(void) {
    altcross_hotzone_t zones[] = {
        {ALTCROSS_EDGE_TOP, 1, 1},
    };
    int device_id = -1;
    int found = altcross_hotzone_resolve(zones, 1, ALTCROSS_EDGE_LEFT,
                                          &device_id);
    ASSERT_EQ(0, found);
}

static void test_resolve_returns_zero_for_edge_none(void) {
    altcross_hotzone_t zones[] = {
        {ALTCROSS_EDGE_NONE, 1, 1},
    };
    int device_id = -1;
    int found = altcross_hotzone_resolve(zones, 1, ALTCROSS_EDGE_NONE,
                                          &device_id);
    ASSERT_EQ(0, found);
}

void run_hotzone_tests(void) {
    RUN_TEST(test_detect_returns_none_in_the_middle);
    RUN_TEST(test_detect_plain_edges);
    RUN_TEST(test_detect_corners_take_priority_over_edges);
    RUN_TEST(test_detect_clamps_out_of_bounds_coordinates);
    RUN_TEST(test_resolve_finds_matching_enabled_zone);
    RUN_TEST(test_resolve_ignores_disabled_zones);
    RUN_TEST(test_resolve_returns_zero_when_no_match);
    RUN_TEST(test_resolve_returns_zero_for_edge_none);
}
