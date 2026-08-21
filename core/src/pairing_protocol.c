#include "altcross/pairing_protocol.h"

#include <stdio.h>
#include <string.h>

#include "altcross/net_socket.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

static const char PAIR_MAGIC[4] = {'A', 'L', 'T', 'P'};
#define PAIR_HEADER_SIZE 6 /* magic(4) + versão(1) + tipo(1) */
#define PAIR_VERSION 1
#define PAIR_HOST_SIZE 46
#define PAIR_MAX_SIZE                                                        \
    (PAIR_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 1 +                        \
     ALTCROSS_DEVICE_NAME_SIZE + 1 + (ALTCROSS_SECRET_SIZE - 1))

typedef enum {
    PAIR_MSG_REQUEST = 1,
    PAIR_MSG_CONFIRM = 2,
    PAIR_MSG_ACCEPT = 3,
    PAIR_MSG_REJECT = 4,
} pair_msg_type_t;

typedef struct {
    pair_msg_type_t type;
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    char name[ALTCROSS_DEVICE_NAME_SIZE];
    int code;
    char secret[ALTCROSS_SECRET_SIZE];
} decoded_pair_msg_t;

static void put_id_field(uint8_t *buf, const char *device_id) {
    memset(buf, 0, ALTCROSS_DEVICE_ID_SIZE);
    snprintf((char *)buf, ALTCROSS_DEVICE_ID_SIZE, "%s", device_id);
}

static int encode_request(const char *device_id, const char *name,
                           uint8_t *buf, size_t buf_size) {
    size_t name_len = strlen(name);
    if (name_len > ALTCROSS_DEVICE_NAME_SIZE - 1) {
        return -1;
    }
    size_t total = PAIR_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 1 + name_len;
    if (buf_size < total) {
        return -1;
    }

    memcpy(buf, PAIR_MAGIC, 4);
    buf[4] = PAIR_VERSION;
    buf[5] = (uint8_t)PAIR_MSG_REQUEST;
    uint8_t *p = buf + PAIR_HEADER_SIZE;
    put_id_field(p, device_id);
    p += ALTCROSS_DEVICE_ID_SIZE;
    p[0] = (uint8_t)name_len;
    p += 1;
    memcpy(p, name, name_len);

    return (int)total;
}

static int encode_confirm(const char *device_id, int code, uint8_t *buf,
                           size_t buf_size) {
    size_t total = PAIR_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 4;
    if (buf_size < total) {
        return -1;
    }

    memcpy(buf, PAIR_MAGIC, 4);
    buf[4] = PAIR_VERSION;
    buf[5] = (uint8_t)PAIR_MSG_CONFIRM;
    uint8_t *p = buf + PAIR_HEADER_SIZE;
    put_id_field(p, device_id);
    p += ALTCROSS_DEVICE_ID_SIZE;

    uint32_t c = (uint32_t)code;
    p[0] = (uint8_t)((c >> 24) & 0xFF);
    p[1] = (uint8_t)((c >> 16) & 0xFF);
    p[2] = (uint8_t)((c >> 8) & 0xFF);
    p[3] = (uint8_t)(c & 0xFF);

    return (int)total;
}

static int encode_accept(const char *device_id, const char *name,
                          const char *secret, uint8_t *buf, size_t buf_size) {
    size_t name_len = strlen(name);
    size_t secret_len = strlen(secret);
    if (name_len > ALTCROSS_DEVICE_NAME_SIZE - 1 ||
        secret_len > ALTCROSS_SECRET_SIZE - 1) {
        return -1;
    }
    size_t total = PAIR_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 1 + name_len +
                   1 + secret_len;
    if (buf_size < total) {
        return -1;
    }

    memcpy(buf, PAIR_MAGIC, 4);
    buf[4] = PAIR_VERSION;
    buf[5] = (uint8_t)PAIR_MSG_ACCEPT;
    uint8_t *p = buf + PAIR_HEADER_SIZE;
    put_id_field(p, device_id);
    p += ALTCROSS_DEVICE_ID_SIZE;
    p[0] = (uint8_t)name_len;
    p += 1;
    memcpy(p, name, name_len);
    p += name_len;
    p[0] = (uint8_t)secret_len;
    p += 1;
    memcpy(p, secret, secret_len);

    return (int)total;
}

static int encode_reject(uint8_t *buf, size_t buf_size) {
    if (buf_size < PAIR_HEADER_SIZE) {
        return -1;
    }
    memcpy(buf, PAIR_MAGIC, 4);
    buf[4] = PAIR_VERSION;
    buf[5] = (uint8_t)PAIR_MSG_REJECT;
    return PAIR_HEADER_SIZE;
}

static int decode(const uint8_t *buf, size_t len, decoded_pair_msg_t *out) {
    if (len < PAIR_HEADER_SIZE || memcmp(buf, PAIR_MAGIC, 4) != 0 ||
        buf[4] != PAIR_VERSION) {
        return 0;
    }
    out->type = (pair_msg_type_t)buf[5];
    const uint8_t *p = buf + PAIR_HEADER_SIZE;
    size_t remaining = len - PAIR_HEADER_SIZE;

    switch (out->type) {
    case PAIR_MSG_REQUEST: {
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
        if (remaining < name_len || name_len > ALTCROSS_DEVICE_NAME_SIZE - 1) {
            return 0;
        }
        memcpy(out->name, p, name_len);
        out->name[name_len] = '\0';
        return 1;
    }
    case PAIR_MSG_CONFIRM: {
        if (remaining < ALTCROSS_DEVICE_ID_SIZE + 4) {
            return 0;
        }
        memcpy(out->device_id, p, ALTCROSS_DEVICE_ID_SIZE);
        out->device_id[ALTCROSS_DEVICE_ID_SIZE - 1] = '\0';
        p += ALTCROSS_DEVICE_ID_SIZE;
        uint32_t c = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8) | (uint32_t)p[3];
        out->code = (int)c;
        return 1;
    }
    case PAIR_MSG_ACCEPT: {
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
        if (remaining < (size_t)name_len + 1) {
            return 0;
        }
        memcpy(out->name, p, name_len);
        out->name[name_len] = '\0';
        p += name_len;
        remaining -= name_len;

        uint8_t secret_len = p[0];
        p += 1;
        remaining -= 1;
        if (remaining < secret_len || secret_len > ALTCROSS_SECRET_SIZE - 1) {
            return 0;
        }
        memcpy(out->secret, p, secret_len);
        out->secret[secret_len] = '\0';
        return 1;
    }
    case PAIR_MSG_REJECT:
        return 1;
    default:
        return 0;
    }
}

static altcross_socket_t *g_pair_socket = NULL;
static volatile int g_pair_running = 0;
static char g_pair_device_id[ALTCROSS_DEVICE_ID_SIZE];
static char g_pair_name[ALTCROSS_DEVICE_NAME_SIZE];
static char g_pair_store_path[512];

/* Notificação pra UI (consumida por poll_incoming_request). */
static altcross_pairing_incoming_request_t g_pending;

/* Notificação pra UI do lado de quem recebeu o pedido, de que o CONFIRM
 * chegou certo e o pareamento terminou (consumida por poll_completed). */
static altcross_pairing_completed_t g_completed;

/* Estado real de "esperando confirmação", independente de a UI já ter lido
 * a notificação acima ou não — sem isso, a UI consumindo a notificação
 * (poll) apagaria também a validação do código quando o CONFIRM chegasse. */
static int g_awaiting_confirm = 0;
static char g_awaiting_device_id[ALTCROSS_DEVICE_ID_SIZE];
static char g_awaiting_name[ALTCROSS_DEVICE_NAME_SIZE];
static int g_awaiting_code;

#if defined(_WIN32)
static HANDLE g_pair_thread;
#else
static pthread_t g_pair_thread;
#endif

static void handle_request(const decoded_pair_msg_t *msg) {
    g_awaiting_confirm = 1;
    snprintf(g_awaiting_device_id, sizeof(g_awaiting_device_id), "%s",
             msg->device_id);
    snprintf(g_awaiting_name, sizeof(g_awaiting_name), "%s", msg->name);
    g_awaiting_code = altcross_pairing_generate_code();

    g_pending.pending = 1;
    snprintf(g_pending.requester_device_id,
             sizeof(g_pending.requester_device_id), "%s", msg->device_id);
    snprintf(g_pending.requester_name, sizeof(g_pending.requester_name), "%s",
             msg->name);
    g_pending.code = g_awaiting_code;
}

static void handle_confirm(const decoded_pair_msg_t *msg,
                            const char *from_host, int from_port) {
    uint8_t reply_buf[PAIR_MAX_SIZE];
    int reply_len;

    if (g_awaiting_confirm &&
        strcmp(msg->device_id, g_awaiting_device_id) == 0 &&
        msg->code == g_awaiting_code) {
        char secret[ALTCROSS_SECRET_SIZE];
        altcross_pairing_generate_secret(secret);

        altcross_pairing_store_t store;
        if (altcross_pairing_store_load(&store, g_pair_store_path) != 0) {
            altcross_pairing_store_init(&store);
        }
        altcross_pairing_store_upsert(&store, msg->device_id,
                                       g_awaiting_name, secret, from_host, 0);
        altcross_pairing_store_save(&store, g_pair_store_path);

        g_completed.pending = 1;
        snprintf(g_completed.peer_device_id, sizeof(g_completed.peer_device_id),
                  "%s", msg->device_id);
        snprintf(g_completed.peer_name, sizeof(g_completed.peer_name), "%s",
                  g_awaiting_name);

        reply_len = encode_accept(g_pair_device_id, g_pair_name, secret,
                                   reply_buf, sizeof(reply_buf));
    } else {
        reply_len = encode_reject(reply_buf, sizeof(reply_buf));
    }

    g_awaiting_confirm = 0;

    if (reply_len > 0) {
        altcross_socket_send_to(g_pair_socket, from_host, from_port,
                                 reply_buf, (size_t)reply_len);
    }
}

static void pairing_responder_loop(void) {
    while (g_pair_running) {
        uint8_t buf[PAIR_MAX_SIZE];
        char from_host[PAIR_HOST_SIZE];
        int from_port;
        int n = altcross_socket_receive_from(g_pair_socket, buf, sizeof(buf),
                                              300, from_host,
                                              sizeof(from_host), &from_port);
        if (n == ALTCROSS_SOCKET_TIMEOUT || n < 0) {
            continue;
        }

        decoded_pair_msg_t msg;
        if (!decode(buf, (size_t)n, &msg)) {
            continue;
        }

        if (msg.type == PAIR_MSG_REQUEST) {
            handle_request(&msg);
        } else if (msg.type == PAIR_MSG_CONFIRM) {
            handle_confirm(&msg, from_host, from_port);
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI pairing_thread_proc(LPVOID arg) {
    (void)arg;
    pairing_responder_loop();
    return 0;
}
#else
static void *pairing_thread_proc(void *arg) {
    (void)arg;
    pairing_responder_loop();
    return NULL;
}
#endif

int altcross_pairing_start_responder(const char *my_device_id,
                                      const char *my_name,
                                      const char *store_path) {
    if (g_pair_running) {
        return 1;
    }

    g_pair_socket = altcross_socket_open_udp(ALTCROSS_PAIRING_PORT);
    if (!g_pair_socket) {
        return 1;
    }

    snprintf(g_pair_device_id, sizeof(g_pair_device_id), "%s", my_device_id);
    snprintf(g_pair_name, sizeof(g_pair_name), "%s", my_name);
    snprintf(g_pair_store_path, sizeof(g_pair_store_path), "%s", store_path);
    g_pending.pending = 0;
    g_completed.pending = 0;
    g_awaiting_confirm = 0;
    g_pair_running = 1;

#if defined(_WIN32)
    g_pair_thread =
        CreateThread(NULL, 0, pairing_thread_proc, NULL, 0, NULL);
    if (!g_pair_thread) {
        g_pair_running = 0;
        altcross_socket_close(g_pair_socket);
        g_pair_socket = NULL;
        return 1;
    }
#else
    if (pthread_create(&g_pair_thread, NULL, pairing_thread_proc, NULL) !=
        0) {
        g_pair_running = 0;
        altcross_socket_close(g_pair_socket);
        g_pair_socket = NULL;
        return 1;
    }
#endif

    return 0;
}

void altcross_pairing_stop_responder(void) {
    if (!g_pair_running) {
        return;
    }
    g_pair_running = 0;

#if defined(_WIN32)
    WaitForSingleObject(g_pair_thread, INFINITE);
    CloseHandle(g_pair_thread);
#else
    pthread_join(g_pair_thread, NULL);
#endif

    altcross_socket_close(g_pair_socket);
    g_pair_socket = NULL;
}

int altcross_pairing_poll_incoming_request(
    altcross_pairing_incoming_request_t *out) {
    if (!g_pending.pending) {
        return 0;
    }
    *out = g_pending;
    g_pending.pending = 0;
    return 1;
}

int altcross_pairing_poll_completed(altcross_pairing_completed_t *out) {
    if (!g_completed.pending) {
        return 0;
    }
    *out = g_completed;
    g_completed.pending = 0;
    return 1;
}

int altcross_pairing_send_request(const char *my_device_id,
                                   const char *my_name,
                                   const char *peer_host) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    if (!sock) {
        return 1;
    }

    uint8_t buf[PAIR_MAX_SIZE];
    int len = encode_request(my_device_id, my_name, buf, sizeof(buf));
    if (len < 0) {
        altcross_socket_close(sock);
        return 1;
    }

    int sent = altcross_socket_send_to(sock, peer_host, ALTCROSS_PAIRING_PORT,
                                        buf, (size_t)len);
    altcross_socket_close(sock);
    return sent > 0 ? 0 : 1;
}

int altcross_pairing_confirm(const char *my_device_id, const char *peer_host,
                              int code, int timeout_ms,
                              const char *store_path, char *out_device_id,
                              char *out_name) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    if (!sock) {
        return -1;
    }

    uint8_t buf[PAIR_MAX_SIZE];
    int len = encode_confirm(my_device_id, code, buf, sizeof(buf));
    if (len < 0) {
        altcross_socket_close(sock);
        return -1;
    }
    altcross_socket_send_to(sock, peer_host, ALTCROSS_PAIRING_PORT, buf,
                             (size_t)len);

    uint8_t reply_buf[PAIR_MAX_SIZE];
    char from_host[PAIR_HOST_SIZE];
    int from_port;
    int n = altcross_socket_receive_from(sock, reply_buf, sizeof(reply_buf),
                                          timeout_ms, from_host,
                                          sizeof(from_host), &from_port);
    altcross_socket_close(sock);

    if (n == ALTCROSS_SOCKET_TIMEOUT || n < 0) {
        return -1;
    }

    decoded_pair_msg_t msg;
    if (!decode(reply_buf, (size_t)n, &msg)) {
        return -1;
    }

    if (msg.type == PAIR_MSG_REJECT) {
        return 0;
    }
    if (msg.type != PAIR_MSG_ACCEPT) {
        return -1;
    }

    altcross_pairing_store_t store;
    if (altcross_pairing_store_load(&store, store_path) != 0) {
        altcross_pairing_store_init(&store);
    }
    altcross_pairing_store_upsert(&store, msg.device_id, msg.name,
                                   msg.secret, peer_host, 0);
    altcross_pairing_store_save(&store, store_path);

    if (out_device_id) {
        snprintf(out_device_id, ALTCROSS_DEVICE_ID_SIZE, "%s", msg.device_id);
    }
    if (out_name) {
        snprintf(out_name, ALTCROSS_DEVICE_NAME_SIZE, "%s", msg.name);
    }

    return 1;
}
