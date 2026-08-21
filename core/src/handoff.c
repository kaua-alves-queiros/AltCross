#include "altcross/handoff.h"

#include <stdio.h>
#include <string.h>

#include "altcross/handoff_protocol.h"
#include "altcross/input_control.h"
#include "altcross/net_socket.h"
#include "altcross/platform_input.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

static altcross_input_control_t g_ic;
static altcross_socket_t *g_socket = NULL;
static volatile int g_running = 0;
static char g_my_device_id[ALTCROSS_DEVICE_ID_SIZE];
static altcross_pairing_store_t g_store; /* carregado 1x em start() */

static char g_device_ids[ALTCROSS_HANDOFF_MAX_ZONES][ALTCROSS_DEVICE_ID_SIZE];
static int g_device_count;
static altcross_hotzone_t g_hz_zones[ALTCROSS_HANDOFF_MAX_ZONES];
static altcross_device_screen_t g_hz_devices[ALTCROSS_HANDOFF_MAX_ZONES];

static uint32_t g_send_seq = 0;
static char g_recv_sender_ids[ALTCROSS_MAX_TRUSTED_DEVICES][ALTCROSS_DEVICE_ID_SIZE];
static uint32_t g_recv_last_seq[ALTCROSS_MAX_TRUSTED_DEVICES];
static int g_recv_sender_count = 0;

#if defined(_WIN32)
static HANDLE g_thread;
#else
static pthread_t g_thread;
#endif

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_to_secret(const char *hex,
                          uint8_t out[ALTCROSS_HANDOFF_SECRET_SIZE]) {
    if (strlen(hex) < ALTCROSS_HANDOFF_SECRET_SIZE * 2) {
        return 0;
    }
    for (int i = 0; i < ALTCROSS_HANDOFF_SECRET_SIZE; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

static int device_index_for(const char *device_id) {
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_device_ids[i], device_id) == 0) {
            return i;
        }
    }
    return -1;
}

/* Resolve host+segredo de um device_id de VERDADE (não o índice interno)
 * consultando o cadastro de confiança carregado em start(). Retorna 1 em
 * sucesso. */
static int resolve_peer(const char *device_id, char *out_host,
                         size_t out_host_size,
                         uint8_t out_secret[ALTCROSS_HANDOFF_SECRET_SIZE]) {
    const altcross_trusted_device_t *trusted =
        altcross_pairing_store_find(&g_store, device_id);
    if (!trusted) {
        return 0;
    }
    snprintf(out_host, out_host_size, "%s", trusted->last_host);
    return hex_to_secret(trusted->secret, out_secret);
}

/* Callback de altcross_handoff_receive: qualquer dispositivo já cadastrado
 * como confiável (pareado antes) é aceito — quem decide se DEVE aceitar
 * input dele é o app na hora de configurar handoff_start (só chamado com
 * hotzones de verdade configuradas); aqui só valida quem ele diz que é. */
static int lookup_secret_from_store(
    const char *sender_device_id, uint8_t out_secret[ALTCROSS_HANDOFF_SECRET_SIZE],
    void *user_data) {
    (void)user_data;
    const altcross_trusted_device_t *trusted =
        altcross_pairing_store_find(&g_store, sender_device_id);
    if (!trusted) {
        return 0;
    }
    return hex_to_secret(trusted->secret, out_secret);
}

static uint32_t *replay_slot_for(const char *sender_device_id) {
    for (int i = 0; i < g_recv_sender_count; i++) {
        if (strcmp(g_recv_sender_ids[i], sender_device_id) == 0) {
            return &g_recv_last_seq[i];
        }
    }
    if (g_recv_sender_count < ALTCROSS_MAX_TRUSTED_DEVICES) {
        int i = g_recv_sender_count++;
        snprintf(g_recv_sender_ids[i], sizeof(g_recv_sender_ids[i]), "%s",
                  sender_device_id);
        g_recv_last_seq[i] = 0;
        return &g_recv_last_seq[i];
    }
    return NULL;
}

static void send_to_active(const altcross_packet_t *pkt) {
    if (g_ic.active_device_id == ALTCROSS_LOCAL_DEVICE_ID ||
        g_ic.active_device_id < 0 || g_ic.active_device_id >= g_device_count) {
        return;
    }
    const char *device_id = g_device_ids[g_ic.active_device_id];
    char host[ALTCROSS_HOST_SIZE];
    uint8_t secret[ALTCROSS_HANDOFF_SECRET_SIZE];
    if (!resolve_peer(device_id, host, sizeof(host), secret)) {
        return;
    }
    g_send_seq++;
    altcross_handoff_send(g_socket, host, g_my_device_id, secret, g_send_seq,
                           pkt);
}

static void on_local_mouse_move(int x, int y, void *user_data) {
    (void)user_data;
    int rx, ry;
    if (!altcross_input_control_on_local_move(&g_ic, x, y, &rx, &ry)) {
        return;
    }
    altcross_platform_input_set_captured(1);

    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_ENTER;
    pkt.data.enter.x = rx;
    pkt.data.enter.y = ry;
    send_to_active(&pkt);
}

static void on_captured_mouse_delta(int dx, int dy, void *user_data) {
    (void)user_data;
    int lx, ly;
    if (altcross_input_control_on_remote_delta(&g_ic, dx, dy, &lx, &ly)) {
        altcross_packet_t leave;
        leave.type = ALTCROSS_PKT_LEAVE;
        send_to_active(&leave);
        altcross_platform_input_set_captured(0);
        return;
    }

    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_MOUSE_DELTA;
    pkt.data.mouse_delta.dx = dx;
    pkt.data.mouse_delta.dy = dy;
    send_to_active(&pkt);
}

static void on_captured_mouse_button(altcross_mouse_button_t button, int down,
                                      void *user_data) {
    (void)user_data;
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_MOUSE_BUTTON;
    pkt.data.mouse_button.button = (int32_t)button;
    pkt.data.mouse_button.down = down;
    send_to_active(&pkt);
}

static void on_captured_key(altcross_keycode_t key, int down, void *user_data) {
    (void)user_data;
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_KEY;
    pkt.data.key.keycode = (int32_t)key;
    pkt.data.key.down = down;
    send_to_active(&pkt);
}

static void on_panic_key(void *user_data) {
    (void)user_data;
    if (altcross_input_control_is_remote(&g_ic)) {
        altcross_packet_t leave;
        leave.type = ALTCROSS_PKT_LEAVE;
        send_to_active(&leave);
    }
    altcross_input_control_release(&g_ic);
    altcross_platform_input_set_captured(0);
}

/* Processa 1 pacote recebido do peer (papel de DESTINO: injeta localmente o
 * que o outro lado está mandando) — não bloqueante. */
static void poll_incoming_once(void) {
    altcross_handoff_received_t received;
    if (!altcross_handoff_receive(g_socket, 0, lookup_secret_from_store, NULL,
                                    &received)) {
        return;
    }

    uint32_t *slot = replay_slot_for(received.sender_device_id);
    if (!slot) {
        return;
    }
    if (received.packet.type == ALTCROSS_PKT_ENTER) {
        /* ENTER marca o início de uma nova sessão de controle vinda desse
         * remetente — reseta a guarda anti-replay pra essa sessão, senão o
         * contador do remetente (que ele também reinicia a cada ENTER, ver
         * handoff_flutter wiring) nunca bateria de novo. */
        *slot = 0;
    }
    if (!altcross_handoff_replay_check(slot, received.seq)) {
        return;
    }

    switch (received.packet.type) {
    case ALTCROSS_PKT_MOUSE_DELTA:
        altcross_platform_input_inject_mouse_delta(
            received.packet.data.mouse_delta.dx,
            received.packet.data.mouse_delta.dy);
        break;
    case ALTCROSS_PKT_MOUSE_BUTTON:
        altcross_platform_input_inject_mouse_button(
            (altcross_mouse_button_t)received.packet.data.mouse_button.button,
            received.packet.data.mouse_button.down);
        break;
    case ALTCROSS_PKT_KEY:
        altcross_platform_input_inject_key(
            (altcross_keycode_t)received.packet.data.key.keycode,
            received.packet.data.key.down);
        break;
    case ALTCROSS_PKT_ENTER:
    case ALTCROSS_PKT_LEAVE:
        /* só relevantes pro lado que decide a hotzone (quem manda); o lado
         * destino só injeta o que chega. */
        break;
    }
}

static void handoff_loop(void) {
    while (g_running) {
        poll_incoming_once();
        altcross_platform_input_pump(20);
    }
}

#if defined(_WIN32)
static DWORD WINAPI handoff_thread_proc(LPVOID arg) {
    (void)arg;
    handoff_loop();
    return 0;
}
#else
static void *handoff_thread_proc(void *arg) {
    (void)arg;
    handoff_loop();
    return NULL;
}
#endif

int altcross_handoff_start(const altcross_screen_config_t *local_screen,
                            const altcross_handoff_zone_t *zones,
                            int zone_count, const char *my_device_id,
                            const char *pairing_store_path) {
    if (g_running) {
        return 1;
    }
    if (zone_count > ALTCROSS_HANDOFF_MAX_ZONES) {
        zone_count = ALTCROSS_HANDOFF_MAX_ZONES;
    }

    if (altcross_pairing_store_load(&g_store, pairing_store_path) != 0) {
        altcross_pairing_store_init(&g_store);
    }
    snprintf(g_my_device_id, sizeof(g_my_device_id), "%s", my_device_id);

    g_device_count = 0;
    for (int i = 0; i < zone_count; i++) {
        int idx = device_index_for(zones[i].target_device_id);
        if (idx < 0) {
            if (g_device_count >= ALTCROSS_HANDOFF_MAX_ZONES) {
                continue;
            }
            idx = g_device_count++;
            snprintf(g_device_ids[idx], sizeof(g_device_ids[idx]), "%s",
                      zones[i].target_device_id);
            g_hz_devices[idx].device_id = idx;
            g_hz_devices[idx].screen = zones[i].target_screen;
        }
        g_hz_zones[i].edge = zones[i].edge;
        g_hz_zones[i].target_device_id = idx;
        g_hz_zones[i].enabled = 1;
    }

    altcross_input_control_init(&g_ic, local_screen, g_hz_zones, zone_count,
                                 g_hz_devices, g_device_count);

    g_socket = altcross_socket_open_udp(ALTCROSS_HANDOFF_PORT);
    if (!g_socket) {
        return 1;
    }

    g_send_seq = 0;
    g_recv_sender_count = 0;

    altcross_platform_input_callbacks_t callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.on_local_mouse_move = on_local_mouse_move;
    callbacks.on_captured_mouse_delta = on_captured_mouse_delta;
    callbacks.on_captured_mouse_button = on_captured_mouse_button;
    callbacks.on_captured_key = on_captured_key;
    callbacks.on_panic_key = on_panic_key;

    if (altcross_platform_input_start(&callbacks) != 0) {
        altcross_socket_close(g_socket);
        g_socket = NULL;
        return 1;
    }

    g_running = 1;
#if defined(_WIN32)
    g_thread = CreateThread(NULL, 0, handoff_thread_proc, NULL, 0, NULL);
    if (!g_thread) {
        g_running = 0;
        altcross_platform_input_stop();
        altcross_socket_close(g_socket);
        g_socket = NULL;
        return 1;
    }
#else
    if (pthread_create(&g_thread, NULL, handoff_thread_proc, NULL) != 0) {
        g_running = 0;
        altcross_platform_input_stop();
        altcross_socket_close(g_socket);
        g_socket = NULL;
        return 1;
    }
#endif

    return 0;
}

void altcross_handoff_stop(void) {
    if (!g_running) {
        return;
    }
    g_running = 0;

#if defined(_WIN32)
    WaitForSingleObject(g_thread, INFINITE);
    CloseHandle(g_thread);
#else
    pthread_join(g_thread, NULL);
#endif

    altcross_platform_input_stop();
    altcross_socket_close(g_socket);
    g_socket = NULL;
}

int altcross_handoff_is_active(void) { return g_running; }

int altcross_handoff_is_remote(void) {
    if (!g_running) {
        return 0;
    }
    return altcross_input_control_is_remote(&g_ic);
}
