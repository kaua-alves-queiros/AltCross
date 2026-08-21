#include "altcross/discovery.h"

#include <stdio.h>
#include <string.h>

#include "altcross/net_socket.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

static const char MAGIC[4] = {'A', 'L', 'T', 'X'};
#define HEADER_SIZE 6 /* magic(4) + versão(1) + tipo(1) */

static void put_id_field(uint8_t *buf, const char *device_id) {
    memset(buf, 0, ALTCROSS_DEVICE_ID_SIZE);
    snprintf((char *)buf, ALTCROSS_DEVICE_ID_SIZE, "%s", device_id);
}

int altcross_discovery_encode_query(const char *from_device_id, uint8_t *buf,
                                     size_t buf_size) {
    size_t total = HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE;
    if (buf_size < total) {
        return -1;
    }

    memcpy(buf, MAGIC, 4);
    buf[4] = ALTCROSS_DISCOVERY_VERSION;
    buf[5] = (uint8_t)ALTCROSS_DISCOVERY_MSG_QUERY;
    put_id_field(buf + HEADER_SIZE, from_device_id);

    return (int)total;
}

int altcross_discovery_encode_reply(const char *device_id, const char *name,
                                     int port, uint8_t *buf,
                                     size_t buf_size) {
    size_t name_len = strlen(name);
    if (name_len > ALTCROSS_DEVICE_NAME_SIZE - 1 || port < 0 ||
        port > 0xFFFF) {
        return -1;
    }

    size_t total = HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 2 + 1 + name_len;
    if (buf_size < total) {
        return -1;
    }

    memcpy(buf, MAGIC, 4);
    buf[4] = ALTCROSS_DISCOVERY_VERSION;
    buf[5] = (uint8_t)ALTCROSS_DISCOVERY_MSG_REPLY;

    uint8_t *p = buf + HEADER_SIZE;
    put_id_field(p, device_id);
    p += ALTCROSS_DEVICE_ID_SIZE;

    p[0] = (uint8_t)((port >> 8) & 0xFF);
    p[1] = (uint8_t)(port & 0xFF);
    p += 2;

    p[0] = (uint8_t)name_len;
    p += 1;

    memcpy(p, name, name_len);

    return (int)total;
}

int altcross_discovery_decode(const uint8_t *buf, size_t len,
                               altcross_discovery_msg_type_t *out_type,
                               char *out_query_from_device_id,
                               altcross_discovery_reply_t *out_reply) {
    if (len < HEADER_SIZE || memcmp(buf, MAGIC, 4) != 0 ||
        buf[4] != ALTCROSS_DISCOVERY_VERSION) {
        return 0;
    }

    altcross_discovery_msg_type_t type =
        (altcross_discovery_msg_type_t)buf[5];

    if (type == ALTCROSS_DISCOVERY_MSG_QUERY) {
        if (len < HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE) {
            return 0;
        }
        memcpy(out_query_from_device_id, buf + HEADER_SIZE,
               ALTCROSS_DEVICE_ID_SIZE);
        out_query_from_device_id[ALTCROSS_DEVICE_ID_SIZE - 1] = '\0';
        *out_type = type;
        return 1;
    }

    if (type == ALTCROSS_DISCOVERY_MSG_REPLY) {
        if (len < HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 2 + 1) {
            return 0;
        }
        const uint8_t *p = buf + HEADER_SIZE;
        memcpy(out_reply->device_id, p, ALTCROSS_DEVICE_ID_SIZE);
        out_reply->device_id[ALTCROSS_DEVICE_ID_SIZE - 1] = '\0';
        p += ALTCROSS_DEVICE_ID_SIZE;

        out_reply->port = (p[0] << 8) | p[1];
        p += 2;

        uint8_t name_len = p[0];
        p += 1;

        if (len < (size_t)(p - buf) + name_len ||
            name_len > ALTCROSS_DEVICE_NAME_SIZE - 1) {
            return 0;
        }
        memcpy(out_reply->name, p, name_len);
        out_reply->name[name_len] = '\0';

        *out_type = type;
        return 1;
    }

    return 0;
}

int altcross_discovery_run_query(const char *from_device_id, int timeout_ms,
                                  char *out_device_ids, char *out_names,
                                  int *out_ports, char *out_hosts,
                                  int max_count) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    if (!sock) {
        return -1;
    }
    if (altcross_socket_enable_broadcast(sock) != 0) {
        altcross_socket_close(sock);
        return -1;
    }

    uint8_t query_buf[ALTCROSS_DISCOVERY_MAX_SIZE];
    int query_len =
        altcross_discovery_encode_query(from_device_id, query_buf,
                                         sizeof(query_buf));
    if (query_len < 0) {
        altcross_socket_close(sock);
        return -1;
    }
    altcross_socket_send_to(sock, ALTCROSS_DISCOVERY_BROADCAST_ADDRESS,
                             ALTCROSS_DISCOVERY_PORT, query_buf,
                             (size_t)query_len);

    int found = 0;
    int elapsed_ms = 0;
    const int step_ms = 200;
    while (elapsed_ms < timeout_ms) {
        uint8_t buf[ALTCROSS_DISCOVERY_MAX_SIZE];
        char from_host[ALTCROSS_DISCOVERY_HOST_SIZE];
        int from_port;
        int n = altcross_socket_receive_from(sock, buf, sizeof(buf), step_ms,
                                              from_host, sizeof(from_host),
                                              &from_port);
        elapsed_ms += step_ms;
        if (n == ALTCROSS_SOCKET_TIMEOUT) {
            continue;
        }
        if (n < 0) {
            break;
        }

        altcross_discovery_msg_type_t type;
        char query_from[ALTCROSS_DEVICE_ID_SIZE];
        altcross_discovery_reply_t reply;
        if (!altcross_discovery_decode(buf, (size_t)n, &type, query_from,
                                        &reply)) {
            continue;
        }
        if (type != ALTCROSS_DISCOVERY_MSG_REPLY) {
            continue; /* ignora nosso próprio QUERY ecoado pelo broadcast */
        }
        if (strcmp(reply.device_id, from_device_id) == 0) {
            continue;
        }

        if (found < max_count) {
            memcpy(out_device_ids + (size_t)found * ALTCROSS_DEVICE_ID_SIZE,
                   reply.device_id, ALTCROSS_DEVICE_ID_SIZE);
            memcpy(out_names + (size_t)found * ALTCROSS_DEVICE_NAME_SIZE,
                   reply.name, ALTCROSS_DEVICE_NAME_SIZE);
            out_ports[found] = reply.port;
            snprintf(out_hosts + (size_t)found * ALTCROSS_DISCOVERY_HOST_SIZE,
                     ALTCROSS_DISCOVERY_HOST_SIZE, "%s", from_host);
        }
        found++;
    }

    altcross_socket_close(sock);
    return found;
}

static altcross_socket_t *g_responder_socket = NULL;
static volatile int g_responder_running = 0;
static char g_responder_device_id[ALTCROSS_DEVICE_ID_SIZE];
static char g_responder_name[ALTCROSS_DEVICE_NAME_SIZE];
static int g_responder_port;

#if defined(_WIN32)
static HANDLE g_responder_thread;
#else
static pthread_t g_responder_thread;
#endif

static void responder_loop(void) {
    while (g_responder_running) {
        uint8_t buf[ALTCROSS_DISCOVERY_MAX_SIZE];
        char from_host[ALTCROSS_DISCOVERY_HOST_SIZE];
        int from_port;
        /* timeout curto só pra poder checar g_responder_running com
         * frequência e sair rápido quando stop_responder for chamado */
        int n = altcross_socket_receive_from(g_responder_socket, buf,
                                              sizeof(buf), 300, from_host,
                                              sizeof(from_host), &from_port);
        if (n == ALTCROSS_SOCKET_TIMEOUT || n < 0) {
            continue;
        }

        altcross_discovery_msg_type_t type;
        char query_from[ALTCROSS_DEVICE_ID_SIZE];
        altcross_discovery_reply_t unused_reply;
        if (!altcross_discovery_decode(buf, (size_t)n, &type, query_from,
                                        &unused_reply)) {
            continue;
        }
        if (type != ALTCROSS_DISCOVERY_MSG_QUERY) {
            continue;
        }
        if (strcmp(query_from, g_responder_device_id) == 0) {
            continue; /* nunca deveria acontecer, mas por garantia */
        }

        uint8_t reply_buf[ALTCROSS_DISCOVERY_MAX_SIZE];
        int reply_len = altcross_discovery_encode_reply(
            g_responder_device_id, g_responder_name, g_responder_port,
            reply_buf, sizeof(reply_buf));
        if (reply_len > 0) {
            altcross_socket_send_to(g_responder_socket, from_host, from_port,
                                     reply_buf, (size_t)reply_len);
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI responder_thread_proc(LPVOID arg) {
    (void)arg;
    responder_loop();
    return 0;
}
#else
static void *responder_thread_proc(void *arg) {
    (void)arg;
    responder_loop();
    return NULL;
}
#endif

int altcross_discovery_start_responder(const char *device_id,
                                        const char *name, int port) {
    if (g_responder_running) {
        return 1;
    }

    g_responder_socket = altcross_socket_open_udp(ALTCROSS_DISCOVERY_PORT);
    if (!g_responder_socket) {
        return 1;
    }

    snprintf(g_responder_device_id, sizeof(g_responder_device_id), "%s",
             device_id);
    snprintf(g_responder_name, sizeof(g_responder_name), "%s", name);
    g_responder_port = port;
    g_responder_running = 1;

#if defined(_WIN32)
    g_responder_thread =
        CreateThread(NULL, 0, responder_thread_proc, NULL, 0, NULL);
    if (!g_responder_thread) {
        g_responder_running = 0;
        altcross_socket_close(g_responder_socket);
        g_responder_socket = NULL;
        return 1;
    }
#else
    if (pthread_create(&g_responder_thread, NULL, responder_thread_proc,
                        NULL) != 0) {
        g_responder_running = 0;
        altcross_socket_close(g_responder_socket);
        g_responder_socket = NULL;
        return 1;
    }
#endif

    return 0;
}

void altcross_discovery_stop_responder(void) {
    if (!g_responder_running) {
        return;
    }
    g_responder_running = 0;

#if defined(_WIN32)
    WaitForSingleObject(g_responder_thread, INFINITE);
    CloseHandle(g_responder_thread);
#else
    pthread_join(g_responder_thread, NULL);
#endif

    altcross_socket_close(g_responder_socket);
    g_responder_socket = NULL;
}
