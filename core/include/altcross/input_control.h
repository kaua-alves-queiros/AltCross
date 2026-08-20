#ifndef ALTCROSS_INPUT_CONTROL_H
#define ALTCROSS_INPUT_CONTROL_H

#include "altcross/export.h"
#include "altcross/hotzone.h"

#define ALTCROSS_LOCAL_DEVICE_ID (-1)

/* Tela de um dispositivo remoto conhecido, usada para mapear a coordenada de
 * entrada/saída do cursor entre telas de tamanhos diferentes. */
typedef struct {
    int device_id;
    altcross_screen_config_t screen;
} altcross_device_screen_t;

/* Máquina de estados de "quem é dono do input local" (mouse + teclado, já que
 * o teclado segue o mouse por padrão). Não faz captura nem injeção de eventos
 * de verdade — isso é responsabilidade da camada de plataforma
 * (CGEventTap/SendInput/uinput) e da rede, que chamam esta API. */
typedef struct {
    altcross_screen_config_t local_screen;
    const altcross_hotzone_t *zones;
    int zone_count;
    const altcross_device_screen_t *devices;
    int device_count;

    int active_device_id; /* ALTCROSS_LOCAL_DEVICE_ID quando o controle é local */
    altcross_edge_t entry_edge; /* borda local por onde o controle saiu */
    int virtual_x;
    int virtual_y;
} altcross_input_control_t;

ALTCROSS_API void altcross_input_control_init(
    altcross_input_control_t *ic, const altcross_screen_config_t *local_screen,
    const altcross_hotzone_t *zones, int zone_count,
    const altcross_device_screen_t *devices, int device_count);

/* Chamado com a posição absoluta do cursor local (só faz sentido em modo
 * LOCAL; se já estiver em modo remoto, não faz nada e retorna 0 — nesse modo
 * use altcross_input_control_on_remote_delta). Se a posição cair numa hotzone
 * com dispositivo resolvido, transiciona para REMOTO: grava o novo
 * active_device_id, calcula a coordenada de entrada espelhada na tela do
 * dispositivo remoto em out_remote_x/out_remote_y e retorna 1. Caso
 * contrário retorna 0 sem escrever nos parâmetros de saída. */
ALTCROSS_API int altcross_input_control_on_local_move(
    altcross_input_control_t *ic, int x, int y, int *out_remote_x,
    int *out_remote_y);

/* Chamado com o delta de movimento do mouse (só faz sentido em modo REMOTO;
 * em modo LOCAL não faz nada e retorna 0). Atualiza a posição virtual do
 * cursor no espaço de coordenadas do dispositivo remoto. Se essa posição
 * cruzar de volta a borda por onde o controle saiu, transiciona para LOCAL:
 * calcula a coordenada de retorno espelhada na tela local em
 * out_local_x/out_local_y e retorna 1. Caso contrário, só atualiza a
 * posição virtual (clampada aos limites da tela remota) e retorna 0. */
ALTCROSS_API int altcross_input_control_on_remote_delta(
    altcross_input_control_t *ic, int dx, int dy, int *out_local_x,
    int *out_local_y);

ALTCROSS_API int altcross_input_control_is_remote(
    const altcross_input_control_t *ic);

/* Tecla de pânico: força retorno imediato ao controle local, não importa a
 * posição do cursor virtual. Deve estar sempre disponível como saída de
 * emergência (ver "Sensibilidade e Trava Anti-Escape" no AGENTS.md). */
ALTCROSS_API void altcross_input_control_release(altcross_input_control_t *ic);

#endif
