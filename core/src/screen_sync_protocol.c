#include "altcross/screen_sync_protocol.h"

#include <stdio.h>
#include <string.h>

#include "altcross/net_socket.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

static const char SYNC_MAGIC[4] = {'A', 'L', 'T', 'S'};
#define SYNC_HEADER_SIZE 6 /* magic(4) + versão(1) + tipo(1) */
#define SYNC_VERSION 1

typedef enum {
    SYNC_MSG_SCREENS_QUERY = 1,
    SYNC_MSG_SCREENS_REPLY = 2,
    SYNC_MSG_ZONE_PUSH = 3,
} sync_msg_type_t;

/* count(1) + ALTCROSS_MAX_DISPLAYS * (x,y,w,h em int32 + is_primary em 1
 * byte). Coordenadas de tela são sempre valores inteiros de pixel na
 * prática (ver displays.h) — truncar double->int32 no fio não perde nada
 * de verdade. */
#define SYNC_DISPLAY_ENCODED_SIZE (4 * 4 + 1)
#define SYNC_MAX_SIZE                                                        \
    (SYNC_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + ALTCROSS_DEVICE_NAME_SIZE + \
     4 + 1 + ALTCROSS_MAX_DISPLAYS * SYNC_DISPLAY_ENCODED_SIZE)

typedef struct {
    sync_msg_type_t type;
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    char name[ALTCROSS_DEVICE_NAME_SIZE];
    int edge;
    int target_screen_index;
    altcross_display_t displays[ALTCROSS_MAX_DISPLAYS];
    int display_count;
} decoded_sync_msg_t;

static void put_id_field(uint8_t *buf, const char *device_id) {
    memset(buf, 0, ALTCROSS_DEVICE_ID_SIZE);
    snprintf((char *)buf, ALTCROSS_DEVICE_ID_SIZE, "%s", device_id);
}

static void put_i32(uint8_t *p, int32_t v) {
    uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)((u >> 24) & 0xFF);
    p[1] = (uint8_t)((u >> 16) & 0xFF);
    p[2] = (uint8_t)((u >> 8) & 0xFF);
    p[3] = (uint8_t)(u & 0xFF);
}

static int32_t get_i32(const uint8_t *p) {
    uint32_t u = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                 ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    return (int32_t)u;
}

static int encode_screens_query(const char *device_id, uint8_t *buf,
                                 size_t buf_size) {
    size_t total = SYNC_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE;
    if (buf_size < total) {
        return -1;
    }
    memcpy(buf, SYNC_MAGIC, 4);
    buf[4] = SYNC_VERSION;
    buf[5] = (uint8_t)SYNC_MSG_SCREENS_QUERY;
    put_id_field(buf + SYNC_HEADER_SIZE, device_id);
    return (int)total;
}

static int encode_screens_reply(const char *device_id,
                                 const altcross_display_t *displays,
                                 int count, uint8_t *buf, size_t buf_size) {
    if (count < 0) {
        count = 0;
    }
    if (count > ALTCROSS_MAX_DISPLAYS) {
        count = ALTCROSS_MAX_DISPLAYS;
    }
    size_t total = SYNC_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 1 +
                   (size_t)count * SYNC_DISPLAY_ENCODED_SIZE;
    if (buf_size < total) {
        return -1;
    }
    memcpy(buf, SYNC_MAGIC, 4);
    buf[4] = SYNC_VERSION;
    buf[5] = (uint8_t)SYNC_MSG_SCREENS_REPLY;
    uint8_t *p = buf + SYNC_HEADER_SIZE;
    put_id_field(p, device_id);
    p += ALTCROSS_DEVICE_ID_SIZE;
    p[0] = (uint8_t)count;
    p += 1;
    for (int i = 0; i < count; i++) {
        put_i32(p, (int32_t)displays[i].x);
        put_i32(p + 4, (int32_t)displays[i].y);
        put_i32(p + 8, (int32_t)displays[i].width);
        put_i32(p + 12, (int32_t)displays[i].height);
        p[16] = (uint8_t)(displays[i].is_primary ? 1 : 0);
        p += SYNC_DISPLAY_ENCODED_SIZE;
    }
    return (int)total;
}

static int encode_zone_push(const char *device_id, const char *name,
                             int edge, int target_screen_index, uint8_t *buf,
                             size_t buf_size) {
    size_t name_len = strlen(name);
    if (name_len > ALTCROSS_DEVICE_NAME_SIZE - 1) {
        return -1;
    }
    size_t total =
        SYNC_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 1 + name_len + 4 + 4;
    if (buf_size < total) {
        return -1;
    }
    memcpy(buf, SYNC_MAGIC, 4);
    buf[4] = SYNC_VERSION;
    buf[5] = (uint8_t)SYNC_MSG_ZONE_PUSH;
    uint8_t *p = buf + SYNC_HEADER_SIZE;
    put_id_field(p, device_id);
    p += ALTCROSS_DEVICE_ID_SIZE;
    p[0] = (uint8_t)name_len;
    p += 1;
    memcpy(p, name, name_len);
    p += name_len;
    put_i32(p, edge);
    p += 4;
    put_i32(p, target_screen_index);
    return (int)total;
}

static int decode(const uint8_t *buf, size_t len, decoded_sync_msg_t *out) {
    if (len < SYNC_HEADER_SIZE || memcmp(buf, SYNC_MAGIC, 4) != 0 ||
        buf[4] != SYNC_VERSION) {
        return 0;
    }
    out->type = (sync_msg_type_t)buf[5];
    const uint8_t *p = buf + SYNC_HEADER_SIZE;
    size_t remaining = len - SYNC_HEADER_SIZE;

    switch (out->type) {
    case SYNC_MSG_SCREENS_QUERY: {
        if (remaining < ALTCROSS_DEVICE_ID_SIZE) {
            return 0;
        }
        memcpy(out->device_id, p, ALTCROSS_DEVICE_ID_SIZE);
        out->device_id[ALTCROSS_DEVICE_ID_SIZE - 1] = '\0';
        return 1;
    }
    case SYNC_MSG_SCREENS_REPLY: {
        if (remaining < ALTCROSS_DEVICE_ID_SIZE + 1) {
            return 0;
        }
        memcpy(out->device_id, p, ALTCROSS_DEVICE_ID_SIZE);
        out->device_id[ALTCROSS_DEVICE_ID_SIZE - 1] = '\0';
        p += ALTCROSS_DEVICE_ID_SIZE;
        remaining -= ALTCROSS_DEVICE_ID_SIZE;

        uint8_t count = p[0];
        p += 1;
        remaining -= 1;
        if (count > ALTCROSS_MAX_DISPLAYS ||
            remaining < (size_t)count * SYNC_DISPLAY_ENCODED_SIZE) {
            return 0;
        }
        out->display_count = count;
        for (int i = 0; i < count; i++) {
            out->displays[i].x = get_i32(p);
            out->displays[i].y = get_i32(p + 4);
            out->displays[i].width = get_i32(p + 8);
            out->displays[i].height = get_i32(p + 12);
            out->displays[i].is_primary = p[16] != 0;
            p += SYNC_DISPLAY_ENCODED_SIZE;
        }
        return 1;
    }
    case SYNC_MSG_ZONE_PUSH: {
        if (remaining < ALTCROSS_DEVICE_ID_SIZE + 1) {
            return 0;
        }
        memcpy(out->device_id, p, ALTCROSS_DEVICE_ID_SIZE);
        out->device_id[ALTCROSS_DEVICE_ID_SIZE - 1] = '\0';
        p += ALTCROSS_DEVICE_ID_SIZE;
        remaining -= ALTCROSS_DEVICE_ID_SIZE;

        uint8_t name_len = p[0];
        p += 1;
        remaining -= 1;
        if (remaining < (size_t)name_len + 8 ||
            name_len > ALTCROSS_DEVICE_NAME_SIZE - 1) {
            return 0;
        }
        memcpy(out->name, p, name_len);
        out->name[name_len] = '\0';
        p += name_len;
        out->edge = get_i32(p);
        p += 4;
        out->target_screen_index = get_i32(p);
        return 1;
    }
    default:
        return 0;
    }
}

static altcross_socket_t *g_sync_socket = NULL;
static volatile int g_sync_running = 0;
static char g_sync_device_id[ALTCROSS_DEVICE_ID_SIZE];
static char g_sync_name[ALTCROSS_DEVICE_NAME_SIZE];

static altcross_screen_sync_incoming_zone_t g_pending_zone;

#if defined(_WIN32)
static HANDLE g_sync_thread;
#else
static pthread_t g_sync_thread;
#endif

static void handle_screens_query(const char *from_host, int from_port) {
    altcross_display_t displays[ALTCROSS_MAX_DISPLAYS];
    int total = altcross_displays_enumerate(displays, ALTCROSS_MAX_DISPLAYS);
    int count = total < ALTCROSS_MAX_DISPLAYS ? total : ALTCROSS_MAX_DISPLAYS;
    if (count < 0) {
        count = 0;
    }

    uint8_t buf[SYNC_MAX_SIZE];
    int len =
        encode_screens_reply(g_sync_device_id, displays, count, buf, sizeof(buf));
    if (len > 0) {
        altcross_socket_send_to(g_sync_socket, from_host, from_port, buf,
                                 (size_t)len);
    }
}

static void handle_zone_push(const decoded_sync_msg_t *msg) {
    g_pending_zone.pending = 1;
    snprintf(g_pending_zone.sender_device_id,
             sizeof(g_pending_zone.sender_device_id), "%s", msg->device_id);
    snprintf(g_pending_zone.sender_name, sizeof(g_pending_zone.sender_name),
             "%s", msg->name);
    g_pending_zone.sender_edge = msg->edge;
    g_pending_zone.sender_target_screen_index = msg->target_screen_index;
}

static void screen_sync_responder_loop(void) {
    while (g_sync_running) {
        uint8_t buf[SYNC_MAX_SIZE];
        char from_host[64];
        int from_port;
        int n = altcross_socket_receive_from(g_sync_socket, buf, sizeof(buf),
                                              300, from_host,
                                              sizeof(from_host), &from_port);
        if (n == ALTCROSS_SOCKET_TIMEOUT || n < 0) {
            continue;
        }

        decoded_sync_msg_t msg;
        if (!decode(buf, (size_t)n, &msg)) {
            continue;
        }

        if (msg.type == SYNC_MSG_SCREENS_QUERY) {
            handle_screens_query(from_host, from_port);
        } else if (msg.type == SYNC_MSG_ZONE_PUSH) {
            handle_zone_push(&msg);
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI screen_sync_thread_proc(LPVOID arg) {
    (void)arg;
    screen_sync_responder_loop();
    return 0;
}
#else
static void *screen_sync_thread_proc(void *arg) {
    (void)arg;
    screen_sync_responder_loop();
    return NULL;
}
#endif

int altcross_screen_sync_start_responder(const char *my_device_id,
                                          const char *my_name) {
    if (g_sync_running) {
        return 1;
    }

    g_sync_socket = altcross_socket_open_udp(ALTCROSS_SCREEN_SYNC_PORT);
    if (!g_sync_socket) {
        return 1;
    }

    snprintf(g_sync_device_id, sizeof(g_sync_device_id), "%s", my_device_id);
    snprintf(g_sync_name, sizeof(g_sync_name), "%s", my_name);
    g_pending_zone.pending = 0;
    g_sync_running = 1;

#if defined(_WIN32)
    g_sync_thread =
        CreateThread(NULL, 0, screen_sync_thread_proc, NULL, 0, NULL);
    if (!g_sync_thread) {
        g_sync_running = 0;
        altcross_socket_close(g_sync_socket);
        g_sync_socket = NULL;
        return 1;
    }
#else
    if (pthread_create(&g_sync_thread, NULL, screen_sync_thread_proc, NULL) !=
        0) {
        g_sync_running = 0;
        altcross_socket_close(g_sync_socket);
        g_sync_socket = NULL;
        return 1;
    }
#endif

    return 0;
}

void altcross_screen_sync_stop_responder(void) {
    if (!g_sync_running) {
        return;
    }
    g_sync_running = 0;

#if defined(_WIN32)
    WaitForSingleObject(g_sync_thread, INFINITE);
    CloseHandle(g_sync_thread);
#else
    pthread_join(g_sync_thread, NULL);
#endif

    altcross_socket_close(g_sync_socket);
    g_sync_socket = NULL;
}

int altcross_screen_sync_poll_incoming_zone(
    altcross_screen_sync_incoming_zone_t *out) {
    if (!g_pending_zone.pending) {
        return 0;
    }
    *out = g_pending_zone;
    g_pending_zone.pending = 0;
    return 1;
}

int altcross_screen_sync_query_screens(const char *peer_host, int timeout_ms,
                                        altcross_display_t *out,
                                        int max_count) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    if (!sock) {
        return -1;
    }

    uint8_t buf[SYNC_MAX_SIZE];
    /* device_id de quem pergunta não importa pro lado que responde (ele só
     * devolve as próprias telas) — manda vazio. */
    int len = encode_screens_query("", buf, sizeof(buf));
    if (len < 0) {
        altcross_socket_close(sock);
        return -1;
    }
    altcross_socket_send_to(sock, peer_host, ALTCROSS_SCREEN_SYNC_PORT, buf,
                             (size_t)len);

    uint8_t reply_buf[SYNC_MAX_SIZE];
    char from_host[64];
    int from_port;
    int n = altcross_socket_receive_from(sock, reply_buf, sizeof(reply_buf),
                                          timeout_ms, from_host,
                                          sizeof(from_host), &from_port);
    altcross_socket_close(sock);

    if (n == ALTCROSS_SOCKET_TIMEOUT || n < 0) {
        return -1;
    }

    decoded_sync_msg_t msg;
    if (!decode(reply_buf, (size_t)n, &msg) ||
        msg.type != SYNC_MSG_SCREENS_REPLY) {
        return -1;
    }

    int count = msg.display_count < max_count ? msg.display_count : max_count;
    for (int i = 0; i < count; i++) {
        out[i] = msg.displays[i];
    }
    return msg.display_count;
}

int altcross_screen_sync_push_zone(const char *peer_host,
                                    const char *my_device_id,
                                    const char *my_name, int my_edge,
                                    int target_screen_index) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    if (!sock) {
        return 1;
    }

    uint8_t buf[SYNC_MAX_SIZE];
    int len = encode_zone_push(my_device_id, my_name, my_edge,
                                target_screen_index, buf, sizeof(buf));
    if (len < 0) {
        altcross_socket_close(sock);
        return 1;
    }

    int sent = altcross_socket_send_to(sock, peer_host,
                                        ALTCROSS_SCREEN_SYNC_PORT, buf,
                                        (size_t)len);
    altcross_socket_close(sock);
    return sent > 0 ? 0 : 1;
}
