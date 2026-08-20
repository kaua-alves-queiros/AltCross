#include "altcross/input_control.h"

#include <stddef.h>

typedef enum {
    AXIS_VERTICAL,   /* TOP / BOTTOM: o eixo de cruzamento é Y */
    AXIS_HORIZONTAL, /* LEFT / RIGHT: o eixo de cruzamento é X */
    AXIS_NONE        /* cantos: ponto fixo, sem coordenada "ao longo" da borda */
} axis_t;

static int clamp(int value, int min, int max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

/* Escala um valor de um eixo [0, from_max] para [0, to_max]. */
static int scale(int value, int from_max, int to_max) {
    if (from_max <= 0) {
        return 0;
    }
    long scaled = (long)value * (long)to_max / (long)from_max;
    return (int)scaled;
}

static axis_t crossing_axis(altcross_edge_t edge) {
    switch (edge) {
    case ALTCROSS_EDGE_TOP:
    case ALTCROSS_EDGE_BOTTOM:
        return AXIS_VERTICAL;
    case ALTCROSS_EDGE_LEFT:
    case ALTCROSS_EDGE_RIGHT:
        return AXIS_HORIZONTAL;
    default:
        return AXIS_NONE;
    }
}

static altcross_edge_t opposite_edge(altcross_edge_t edge) {
    switch (edge) {
    case ALTCROSS_EDGE_TOP:
        return ALTCROSS_EDGE_BOTTOM;
    case ALTCROSS_EDGE_BOTTOM:
        return ALTCROSS_EDGE_TOP;
    case ALTCROSS_EDGE_LEFT:
        return ALTCROSS_EDGE_RIGHT;
    case ALTCROSS_EDGE_RIGHT:
        return ALTCROSS_EDGE_LEFT;
    case ALTCROSS_EDGE_TOP_LEFT:
        return ALTCROSS_EDGE_BOTTOM_RIGHT;
    case ALTCROSS_EDGE_TOP_RIGHT:
        return ALTCROSS_EDGE_BOTTOM_LEFT;
    case ALTCROSS_EDGE_BOTTOM_LEFT:
        return ALTCROSS_EDGE_TOP_RIGHT;
    case ALTCROSS_EDGE_BOTTOM_RIGHT:
        return ALTCROSS_EDGE_TOP_LEFT;
    default:
        return ALTCROSS_EDGE_NONE;
    }
}

/* Ponto de entrada na tela `screen` para quem cruza por `edge`. `along` é a
 * coordenada ao longo da borda (ignorada para cantos, que são ponto fixo). */
static void entry_point_for_edge(altcross_edge_t edge,
                                  const altcross_screen_config_t *screen,
                                  int along, int *out_x, int *out_y) {
    int max_x = screen->screen_width - 1;
    int max_y = screen->screen_height - 1;
    switch (edge) {
    case ALTCROSS_EDGE_TOP:
        *out_x = along;
        *out_y = 0;
        break;
    case ALTCROSS_EDGE_BOTTOM:
        *out_x = along;
        *out_y = max_y;
        break;
    case ALTCROSS_EDGE_LEFT:
        *out_x = 0;
        *out_y = along;
        break;
    case ALTCROSS_EDGE_RIGHT:
        *out_x = max_x;
        *out_y = along;
        break;
    case ALTCROSS_EDGE_TOP_LEFT:
        *out_x = 0;
        *out_y = 0;
        break;
    case ALTCROSS_EDGE_TOP_RIGHT:
        *out_x = max_x;
        *out_y = 0;
        break;
    case ALTCROSS_EDGE_BOTTOM_LEFT:
        *out_x = 0;
        *out_y = max_y;
        break;
    case ALTCROSS_EDGE_BOTTOM_RIGHT:
        *out_x = max_x;
        *out_y = max_y;
        break;
    default:
        *out_x = 0;
        *out_y = 0;
        break;
    }
}

static const altcross_device_screen_t *
find_device(const altcross_input_control_t *ic, int device_id) {
    for (int i = 0; i < ic->device_count; i++) {
        if (ic->devices[i].device_id == device_id) {
            return &ic->devices[i];
        }
    }
    return NULL;
}

void altcross_input_control_init(altcross_input_control_t *ic,
                                  const altcross_screen_config_t *local_screen,
                                  const altcross_hotzone_t *zones,
                                  int zone_count,
                                  const altcross_device_screen_t *devices,
                                  int device_count) {
    ic->local_screen = *local_screen;
    ic->zones = zones;
    ic->zone_count = zone_count;
    ic->devices = devices;
    ic->device_count = device_count;
    ic->active_device_id = ALTCROSS_LOCAL_DEVICE_ID;
    ic->entry_edge = ALTCROSS_EDGE_NONE;
    ic->virtual_x = 0;
    ic->virtual_y = 0;
}

int altcross_input_control_on_local_move(altcross_input_control_t *ic, int x,
                                          int y, int *out_remote_x,
                                          int *out_remote_y) {
    if (ic->active_device_id != ALTCROSS_LOCAL_DEVICE_ID) {
        return 0;
    }

    altcross_edge_t edge = altcross_hotzone_detect(x, y, &ic->local_screen);
    int device_id;
    if (!altcross_hotzone_resolve(ic->zones, ic->zone_count, edge,
                                   &device_id)) {
        return 0;
    }

    const altcross_device_screen_t *remote = find_device(ic, device_id);
    if (!remote) {
        return 0;
    }

    axis_t axis = crossing_axis(edge);
    int along = 0;
    if (axis == AXIS_VERTICAL) {
        along = scale(x, ic->local_screen.screen_width - 1,
                      remote->screen.screen_width - 1);
    } else if (axis == AXIS_HORIZONTAL) {
        along = scale(y, ic->local_screen.screen_height - 1,
                      remote->screen.screen_height - 1);
    }

    altcross_edge_t remote_entry_edge = opposite_edge(edge);
    int rx, ry;
    entry_point_for_edge(remote_entry_edge, &remote->screen, along, &rx, &ry);

    ic->active_device_id = device_id;
    ic->entry_edge = edge;
    ic->virtual_x = rx;
    ic->virtual_y = ry;

    *out_remote_x = rx;
    *out_remote_y = ry;
    return 1;
}

int altcross_input_control_on_remote_delta(altcross_input_control_t *ic,
                                            int dx, int dy, int *out_local_x,
                                            int *out_local_y) {
    if (ic->active_device_id == ALTCROSS_LOCAL_DEVICE_ID) {
        return 0;
    }

    const altcross_device_screen_t *remote =
        find_device(ic, ic->active_device_id);
    int max_x = remote->screen.screen_width - 1;
    int max_y = remote->screen.screen_height - 1;
    int new_x = ic->virtual_x + dx;
    int new_y = ic->virtual_y + dy;

    axis_t axis = crossing_axis(ic->entry_edge);
    int crossed_back = 0;
    if (axis == AXIS_VERTICAL) {
        if (ic->entry_edge == ALTCROSS_EDGE_TOP && new_y > max_y) {
            crossed_back = 1;
        }
        if (ic->entry_edge == ALTCROSS_EDGE_BOTTOM && new_y < 0) {
            crossed_back = 1;
        }
    } else if (axis == AXIS_HORIZONTAL) {
        if (ic->entry_edge == ALTCROSS_EDGE_LEFT && new_x > max_x) {
            crossed_back = 1;
        }
        if (ic->entry_edge == ALTCROSS_EDGE_RIGHT && new_x < 0) {
            crossed_back = 1;
        }
    } else {
        /* Cantos: aproximação — só volta quando os dois eixos saem da tela
         * remota ao mesmo tempo, na direção do canto original. */
        if ((new_x < 0 || new_x > max_x) && (new_y < 0 || new_y > max_y)) {
            crossed_back = 1;
        }
    }

    if (!crossed_back) {
        ic->virtual_x = clamp(new_x, 0, max_x);
        ic->virtual_y = clamp(new_y, 0, max_y);
        return 0;
    }

    int along = 0;
    if (axis == AXIS_VERTICAL) {
        along = scale(clamp(new_x, 0, max_x), max_x,
                      ic->local_screen.screen_width - 1);
    } else if (axis == AXIS_HORIZONTAL) {
        along = scale(clamp(new_y, 0, max_y), max_y,
                      ic->local_screen.screen_height - 1);
    }

    int lx, ly;
    entry_point_for_edge(ic->entry_edge, &ic->local_screen, along, &lx, &ly);

    ic->active_device_id = ALTCROSS_LOCAL_DEVICE_ID;
    *out_local_x = lx;
    *out_local_y = ly;
    return 1;
}

int altcross_input_control_is_remote(const altcross_input_control_t *ic) {
    return ic->active_device_id != ALTCROSS_LOCAL_DEVICE_ID;
}

void altcross_input_control_release(altcross_input_control_t *ic) {
    ic->active_device_id = ALTCROSS_LOCAL_DEVICE_ID;
}
