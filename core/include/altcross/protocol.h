#ifndef ALTCROSS_PROTOCOL_H
#define ALTCROSS_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "altcross/export.h"

/* Protocolo versionado de handoff de mouse/teclado entre 2 máquinas (ver
 * "Boas Práticas" no AGENTS.md: todo pacote de rede carrega versão + tipo). */
#define ALTCROSS_PROTOCOL_VERSION 1

/* Tamanho máximo de um pacote codificado — grande o suficiente pro maior
 * pacote hoje (ENTER), com folga pra campos futuros. */
#define ALTCROSS_PACKET_MAX_SIZE 32

typedef enum {
    /* Cursor entrou vindo da máquina remota: (x, y) é a posição de entrada
     * já calculada no espaço de coordenadas de quem recebe. */
    ALTCROSS_PKT_ENTER = 1,
    /* Delta de movimento do mouse enquanto o controle está com o remetente. */
    ALTCROSS_PKT_MOUSE_DELTA = 2,
    /* Botão do mouse pressionado/solto. */
    ALTCROSS_PKT_MOUSE_BUTTON = 3,
    /* Tecla pressionada/solta (keycode neutro, ver altcross/keycode.h). */
    ALTCROSS_PKT_KEY = 4,
    /* Controle está voltando para quem enviou (tecla de pânico ou cursor
     * cruzou de volta a borda de saída). */
    ALTCROSS_PKT_LEAVE = 5,
} altcross_packet_type_t;

typedef struct {
    altcross_packet_type_t type;
    union {
        struct {
            int32_t x;
            int32_t y;
        } enter;
        struct {
            int32_t dx;
            int32_t dy;
        } mouse_delta;
        struct {
            int32_t button;
            int32_t down;
        } mouse_button;
        struct {
            int32_t keycode;
            int32_t down;
        } key;
    } data;
} altcross_packet_t;

/* Serializa pkt em buf (que deve ter pelo menos ALTCROSS_PACKET_MAX_SIZE
 * bytes). Retorna o número de bytes escritos, ou -1 se o tipo for
 * desconhecido ou buf_size for pequeno demais. */
ALTCROSS_API int altcross_protocol_encode(const altcross_packet_t *pkt,
                                           uint8_t *buf, size_t buf_size);

/* Decodifica buf (tamanho len) em *out_pkt. Retorna 1 em sucesso; retorna 0
 * (sem tocar em *out_pkt) se os dados forem curtos demais, a versão do
 * protocolo não bater, ou o tipo for desconhecido. */
ALTCROSS_API int altcross_protocol_decode(const uint8_t *buf, size_t len,
                                           altcross_packet_t *out_pkt);

#endif
