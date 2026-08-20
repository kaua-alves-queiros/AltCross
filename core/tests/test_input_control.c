#include "altcross/input_control.h"
#include "test_framework.h"

#define LOCAL_W 1920
#define LOCAL_H 1080

static altcross_screen_config_t make_screen(int w, int h) {
    altcross_screen_config_t s;
    s.screen_width = w;
    s.screen_height = h;
    s.corner_size = 20;
    return s;
}

static void test_starts_in_local_mode(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, NULL, 0, NULL, 0);
    ASSERT_EQ(0, altcross_input_control_is_remote(&ic));
}

static void test_local_move_on_configured_edge_switches_to_remote(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_RIGHT, 2, 1}};
    altcross_device_screen_t devices[] = {{2, make_screen(LOCAL_W, LOCAL_H)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx = -1, ry = -1;
    int transitioned =
        altcross_input_control_on_local_move(&ic, LOCAL_W - 1, 540, &rx, &ry);

    ASSERT_EQ(1, transitioned);
    ASSERT_EQ(1, altcross_input_control_is_remote(&ic));
    ASSERT_EQ(0, rx); /* entra pela borda oposta (LEFT) do remoto */
    ASSERT_EQ(540, ry);
}

static void test_local_move_off_edge_does_not_transition(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_RIGHT, 2, 1}};
    altcross_device_screen_t devices[] = {{2, make_screen(LOCAL_W, LOCAL_H)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx, ry;
    int transitioned =
        altcross_input_control_on_local_move(&ic, 960, 540, &rx, &ry);

    ASSERT_EQ(0, transitioned);
    ASSERT_EQ(0, altcross_input_control_is_remote(&ic));
}

static void test_local_move_on_unconfigured_edge_does_not_transition(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_RIGHT, 2, 1}};
    altcross_device_screen_t devices[] = {{2, make_screen(LOCAL_W, LOCAL_H)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx, ry;
    /* borda esquerda não tem zona configurada */
    int transitioned = altcross_input_control_on_local_move(&ic, 0, 540, &rx, &ry);

    ASSERT_EQ(0, transitioned);
    ASSERT_EQ(0, altcross_input_control_is_remote(&ic));
}

static void test_remote_delta_without_crossing_back_updates_virtual_cursor(
    void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_RIGHT, 2, 1}};
    altcross_device_screen_t devices[] = {{2, make_screen(LOCAL_W, LOCAL_H)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx, ry;
    altcross_input_control_on_local_move(&ic, LOCAL_W - 1, 540, &rx, &ry);

    int lx = -1, ly = -1;
    int transitioned =
        altcross_input_control_on_remote_delta(&ic, 50, 10, &lx, &ly);

    ASSERT_EQ(0, transitioned);
    ASSERT_EQ(1, altcross_input_control_is_remote(&ic));
}

static void test_remote_delta_crossing_back_returns_to_local(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_RIGHT, 2, 1}};
    altcross_device_screen_t devices[] = {{2, make_screen(LOCAL_W, LOCAL_H)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx, ry;
    altcross_input_control_on_local_move(&ic, LOCAL_W - 1, 540, &rx, &ry);
    ASSERT_EQ(0, rx);
    ASSERT_EQ(540, ry);

    int lx = -1, ly = -1;
    int transitioned = altcross_input_control_on_remote_delta(&ic, -1, 0, &lx, &ly);

    ASSERT_EQ(1, transitioned);
    ASSERT_EQ(0, altcross_input_control_is_remote(&ic));
    ASSERT_EQ(LOCAL_W - 1, lx);
    ASSERT_EQ(540, ly);
}

static void test_release_forces_local_regardless_of_position(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_RIGHT, 2, 1}};
    altcross_device_screen_t devices[] = {{2, make_screen(LOCAL_W, LOCAL_H)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx, ry;
    altcross_input_control_on_local_move(&ic, LOCAL_W - 1, 540, &rx, &ry);
    ASSERT_EQ(1, altcross_input_control_is_remote(&ic));

    altcross_input_control_release(&ic);

    ASSERT_EQ(0, altcross_input_control_is_remote(&ic));
}

static void test_different_screen_sizes_scale_along_the_edge(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_BOTTOM, 3, 1}};
    altcross_device_screen_t devices[] = {{3, make_screen(1280, 800)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx = -1, ry = -1;
    int transitioned =
        altcross_input_control_on_local_move(&ic, 960, LOCAL_H - 1, &rx, &ry);

    ASSERT_EQ(1, transitioned);
    ASSERT_EQ(0, ry); /* entra pela borda oposta (TOP) do remoto */
    ASSERT_EQ(639, rx); /* 960 * 1279 / 1919, truncado */
}

static void test_corner_switches_to_fixed_point_on_opposite_corner(void) {
    altcross_screen_config_t local = make_screen(LOCAL_W, LOCAL_H);
    altcross_hotzone_t zones[] = {{ALTCROSS_EDGE_TOP_LEFT, 4, 1}};
    altcross_device_screen_t devices[] = {{4, make_screen(LOCAL_W, LOCAL_H)}};
    altcross_input_control_t ic;
    altcross_input_control_init(&ic, &local, zones, 1, devices, 1);

    int rx = -1, ry = -1;
    int transitioned = altcross_input_control_on_local_move(&ic, 0, 0, &rx, &ry);

    ASSERT_EQ(1, transitioned);
    ASSERT_EQ(LOCAL_W - 1, rx); /* entra pelo canto oposto (BOTTOM_RIGHT) */
    ASSERT_EQ(LOCAL_H - 1, ry);
}

void run_input_control_tests(void) {
    RUN_TEST(test_starts_in_local_mode);
    RUN_TEST(test_local_move_on_configured_edge_switches_to_remote);
    RUN_TEST(test_local_move_off_edge_does_not_transition);
    RUN_TEST(test_local_move_on_unconfigured_edge_does_not_transition);
    RUN_TEST(test_remote_delta_without_crossing_back_updates_virtual_cursor);
    RUN_TEST(test_remote_delta_crossing_back_returns_to_local);
    RUN_TEST(test_release_forces_local_regardless_of_position);
    RUN_TEST(test_different_screen_sizes_scale_along_the_edge);
    RUN_TEST(test_corner_switches_to_fixed_point_on_opposite_corner);
}
