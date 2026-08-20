#include "altcross/hotzone.h"

static int clamp(int value, int min, int max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

altcross_edge_t altcross_hotzone_detect(int x, int y,
                                         const altcross_screen_config_t *cfg) {
    int max_x = cfg->screen_width - 1;
    int max_y = cfg->screen_height - 1;

    x = clamp(x, 0, max_x);
    y = clamp(y, 0, max_y);

    int at_left = x <= cfg->corner_size;
    int at_right = x >= max_x - cfg->corner_size;
    int at_top = y <= cfg->corner_size;
    int at_bottom = y >= max_y - cfg->corner_size;

    if (x == 0 && y == 0) {
        return ALTCROSS_EDGE_TOP_LEFT;
    }
    if (at_top && at_left && (x == 0 || y == 0)) {
        return ALTCROSS_EDGE_TOP_LEFT;
    }
    if (at_top && at_right && (x == max_x || y == 0)) {
        return ALTCROSS_EDGE_TOP_RIGHT;
    }
    if (at_bottom && at_left && (x == 0 || y == max_y)) {
        return ALTCROSS_EDGE_BOTTOM_LEFT;
    }
    if (at_bottom && at_right && (x == max_x || y == max_y)) {
        return ALTCROSS_EDGE_BOTTOM_RIGHT;
    }

    if (y == 0) {
        return ALTCROSS_EDGE_TOP;
    }
    if (y == max_y) {
        return ALTCROSS_EDGE_BOTTOM;
    }
    if (x == 0) {
        return ALTCROSS_EDGE_LEFT;
    }
    if (x == max_x) {
        return ALTCROSS_EDGE_RIGHT;
    }

    return ALTCROSS_EDGE_NONE;
}

int altcross_hotzone_resolve(const altcross_hotzone_t *zones, int zone_count,
                              altcross_edge_t edge, int *out_device_id) {
    if (edge == ALTCROSS_EDGE_NONE) {
        return 0;
    }

    for (int i = 0; i < zone_count; i++) {
        if (zones[i].enabled && zones[i].edge == edge) {
            *out_device_id = zones[i].target_device_id;
            return 1;
        }
    }

    return 0;
}
