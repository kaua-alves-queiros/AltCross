/* Daemon real do AltCross: liga a lógica pura (hotzone + input_control) à
 * captura/injeção de verdade do SO (platform_input) e ao transporte de rede
 * (net_socket + protocol).
 *
 * *** ATENÇÃO ***
 * Rodar este binário instala um hook global de mouse/teclado de verdade
 * nesta máquina (ver core/src/platform/<so>/platform_input.c). No macOS ele
 * vai pedir permissão de Acessibilidade na primeira vez. NUNCA rode isso
 * automaticamente em CI/teste — só manualmente, sabendo o que está fazendo.
 *
 * Uso (MVP manual, sem descoberta/pareamento ainda):
 *   ./altcrossd --peer <ip> --peer-port <porta> --listen-port <porta> \
 *               --edge right --peer-device-id 1
 *
 * O mesmo binário funciona nas 2 pontas: ele escuta em --listen-port pra
 * receber pacotes (quando é o destino do controle) e manda pra --peer:port
 * quando o cursor local cruza a hotzone configurada (quando é a origem).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "altcross/hotzone.h"
#include "altcross/input_control.h"
#include "altcross/net_socket.h"
#include "altcross/platform_input.h"
#include "altcross/protocol.h"

typedef struct {
    const char *peer_host;
    int peer_port;
    int listen_port;
    altcross_edge_t edge;
    int peer_device_id;
} config_t;

static struct {
    altcross_input_control_t ic;
    altcross_socket_t *socket;
    const config_t *cfg;
} g_state;

static altcross_edge_t parse_edge(const char *s) {
    if (strcmp(s, "top") == 0) return ALTCROSS_EDGE_TOP;
    if (strcmp(s, "bottom") == 0) return ALTCROSS_EDGE_BOTTOM;
    if (strcmp(s, "left") == 0) return ALTCROSS_EDGE_LEFT;
    if (strcmp(s, "right") == 0) return ALTCROSS_EDGE_RIGHT;
    return ALTCROSS_EDGE_NONE;
}

static void send_packet(const altcross_packet_t *pkt) {
    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_protocol_encode(pkt, buf, sizeof(buf));
    if (n < 0) {
        return;
    }
    altcross_socket_send_to(g_state.socket, g_state.cfg->peer_host,
                             g_state.cfg->peer_port, buf, (size_t)n);
}

static void on_local_mouse_move(int x, int y, void *user_data) {
    (void)user_data;
    int rx, ry;
    if (!altcross_input_control_on_local_move(&g_state.ic, x, y, &rx, &ry)) {
        return;
    }

    altcross_platform_input_set_captured(1);

    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_ENTER;
    pkt.data.enter.x = rx;
    pkt.data.enter.y = ry;
    send_packet(&pkt);

    fprintf(stderr, "[altcrossd] controle -> remoto (entrada em %d,%d)\n", rx,
            ry);
}

static void on_captured_mouse_delta(int dx, int dy, void *user_data) {
    (void)user_data;
    int lx, ly;
    if (altcross_input_control_on_remote_delta(&g_state.ic, dx, dy, &lx,
                                                &ly)) {
        altcross_platform_input_set_captured(0);
        altcross_packet_t pkt;
        pkt.type = ALTCROSS_PKT_LEAVE;
        send_packet(&pkt);
        fprintf(stderr, "[altcrossd] controle <- local (retorno em %d,%d)\n",
                lx, ly);
        return;
    }

    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_MOUSE_DELTA;
    pkt.data.mouse_delta.dx = dx;
    pkt.data.mouse_delta.dy = dy;
    send_packet(&pkt);
}

static void on_captured_mouse_button(altcross_mouse_button_t button, int down,
                                      void *user_data) {
    (void)user_data;
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_MOUSE_BUTTON;
    pkt.data.mouse_button.button = (int32_t)button;
    pkt.data.mouse_button.down = down;
    send_packet(&pkt);
}

static void on_captured_key(altcross_keycode_t key, int down,
                             void *user_data) {
    (void)user_data;
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_KEY;
    pkt.data.key.keycode = (int32_t)key;
    pkt.data.key.down = down;
    send_packet(&pkt);
}

static void on_panic_key(void *user_data) {
    (void)user_data;
    altcross_input_control_release(&g_state.ic);
    altcross_platform_input_set_captured(0);
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_LEAVE;
    send_packet(&pkt);
    fprintf(stderr, "[altcrossd] pânico: controle forçado de volta ao local\n");
}

/* Processa pacotes recebidos do peer (papel de "destino": injeta localmente o
 * que o outro lado está mandando). Não bloqueante — chamar periodicamente. */
static void poll_incoming_packets(void) {
    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_socket_receive(g_state.socket, buf, sizeof(buf), 0);
    if (n < 0) {
        return;
    }

    altcross_packet_t pkt;
    if (!altcross_protocol_decode(buf, (size_t)n, &pkt)) {
        return;
    }

    switch (pkt.type) {
    case ALTCROSS_PKT_MOUSE_DELTA:
        altcross_platform_input_inject_mouse_delta(pkt.data.mouse_delta.dx,
                                                     pkt.data.mouse_delta.dy);
        break;
    case ALTCROSS_PKT_MOUSE_BUTTON:
        altcross_platform_input_inject_mouse_button(
            (altcross_mouse_button_t)pkt.data.mouse_button.button,
            pkt.data.mouse_button.down);
        break;
    case ALTCROSS_PKT_KEY:
        altcross_platform_input_inject_key((altcross_keycode_t)pkt.data.key.keycode,
                                            pkt.data.key.down);
        break;
    case ALTCROSS_PKT_ENTER:
    case ALTCROSS_PKT_LEAVE:
        /* Só relevantes pro lado que decide a hotzone; o lado "destino" só
         * injeta o que chega. */
        break;
    }
}

int main(int argc, char **argv) {
    config_t cfg;
    cfg.peer_host = "127.0.0.1";
    cfg.peer_port = 45100;
    cfg.listen_port = 45100;
    cfg.edge = ALTCROSS_EDGE_RIGHT;
    cfg.peer_device_id = 1;

    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--peer") == 0) {
            cfg.peer_host = argv[++i];
        } else if (strcmp(argv[i], "--peer-port") == 0) {
            cfg.peer_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--listen-port") == 0) {
            cfg.listen_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--edge") == 0) {
            cfg.edge = parse_edge(argv[++i]);
        } else if (strcmp(argv[i], "--peer-device-id") == 0) {
            cfg.peer_device_id = atoi(argv[++i]);
        }
    }

    fprintf(stderr,
            "[altcrossd] AVISO: isto vai capturar seu mouse/teclado reais "
            "quando o cursor cruzar a borda configurada. Ctrl+Esc solta o "
            "controle a qualquer momento.\n");

    altcross_screen_config_t local_screen = {1920, 1080, 20};
    altcross_hotzone_t zones[] = {{cfg.edge, cfg.peer_device_id, 1}};
    altcross_device_screen_t devices[] = {{cfg.peer_device_id, local_screen}};
    altcross_input_control_init(&g_state.ic, &local_screen, zones, 1, devices,
                                 1);

    g_state.socket = altcross_socket_open_udp(cfg.listen_port);
    if (!g_state.socket) {
        fprintf(stderr, "[altcrossd] erro: não consegui abrir a porta %d\n",
                cfg.listen_port);
        return 1;
    }
    g_state.cfg = &cfg;

    altcross_platform_input_callbacks_t callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.on_local_mouse_move = on_local_mouse_move;
    callbacks.on_captured_mouse_delta = on_captured_mouse_delta;
    callbacks.on_captured_mouse_button = on_captured_mouse_button;
    callbacks.on_captured_key = on_captured_key;
    callbacks.on_panic_key = on_panic_key;

    if (altcross_platform_input_start(&callbacks) != 0) {
        fprintf(stderr,
                "[altcrossd] erro ao instalar o hook de input — no macOS, "
                "confira Ajustes > Privacidade e Segurança > Acessibilidade\n");
        altcross_socket_close(g_state.socket);
        return 1;
    }

    fprintf(stderr, "[altcrossd] rodando. peer=%s:%d listen=%d edge=%d\n",
            cfg.peer_host, cfg.peer_port, cfg.listen_port, cfg.edge);

    for (;;) {
        poll_incoming_packets();
        altcross_platform_input_pump(50);
    }
}
