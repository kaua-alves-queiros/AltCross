#ifndef ALTCROSS_HOTZONE_H
#define ALTCROSS_HOTZONE_H

#include "altcross/export.h"

typedef enum {
    ALTCROSS_EDGE_NONE = 0,
    ALTCROSS_EDGE_TOP,
    ALTCROSS_EDGE_BOTTOM,
    ALTCROSS_EDGE_LEFT,
    ALTCROSS_EDGE_RIGHT,
    ALTCROSS_EDGE_TOP_LEFT,
    ALTCROSS_EDGE_TOP_RIGHT,
    ALTCROSS_EDGE_BOTTOM_LEFT,
    ALTCROSS_EDGE_BOTTOM_RIGHT,
} altcross_edge_t;

typedef struct {
    int screen_width;
    int screen_height;
    /* Tamanho (em px) do quadrado no canto que conta como "corner" em vez
     * de uma borda simples. */
    int corner_size;
} altcross_screen_config_t;

typedef struct {
    altcross_edge_t edge;
    int target_device_id;
    int enabled;
} altcross_hotzone_t;

/* Detecta em qual borda/canto o cursor (x, y) está tocando, considerando as
 * dimensões da tela em cfg. Retorna ALTCROSS_EDGE_NONE se o cursor não está
 * tocando nenhuma borda. Coordenadas fora dos limites da tela são tratadas
 * como grudadas na borda mais próxima (clamped). */
ALTCROSS_API altcross_edge_t altcross_hotzone_detect(
    int x, int y, const altcross_screen_config_t *cfg);

/* Procura, em zones[0..zone_count), a primeira hotzone habilitada cujo edge
 * bate com o parâmetro edge. Se encontrar, grava o device_id de destino em
 * *out_device_id e retorna 1. Caso contrário (nenhuma zona habilitada
 * configurada para essa borda, ou edge == ALTCROSS_EDGE_NONE), retorna 0 e
 * não escreve em *out_device_id. */
ALTCROSS_API int altcross_hotzone_resolve(const altcross_hotzone_t *zones,
                                           int zone_count,
                                           altcross_edge_t edge,
                                           int *out_device_id);

#endif
