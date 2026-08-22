#include "altcross/connection_monitor.h"

#include <stdio.h>
#include <string.h>

#include "altcross/net_socket.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

static const char MAGIC[4] = {'A', 'L', 'T', 'H'};
#define HB_HEADER_SIZE 6

#define MAX_MONITORED_PEERS 32

static altcross_peer_status_t g_peers[MAX_MONITORED_PEERS];
static int g_peer_count = 0;
static altcross_connection_status_cb g_callback = NULL;
static void *g_user_data = NULL;
static altcross_socket_t *g_sock = NULL;
static volatile int g_running = 0;
static char g_local_id[ALTCROSS_DEVICE_ID_SIZE];

#if defined(_WIN32)
static HANDLE g_thread;
#else
static pthread_t g_thread;
#endif

static uint64_t now_ms(void) {
#if defined(_WIN32)
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

static void put_id_field(uint8_t *buf, const char *device_id) {
    memset(buf, 0, ALTCROSS_DEVICE_ID_SIZE);
    snprintf((char *)buf, ALTCROSS_DEVICE_ID_SIZE, "%s", device_id);
}

int altcross_heartbeat_encode(altcross_heartbeat_type_t type,
                               const char *device_id, uint8_t *buf,
                               size_t buf_size) {
    size_t total = HB_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE;
    if (buf_size < total) {
        return -1;
    }
    memcpy(buf, MAGIC, 4);
    buf[4] = 1;
    buf[5] = (uint8_t)type;
    put_id_field(buf + HB_HEADER_SIZE, device_id);
    return (int)total;
}

int altcross_heartbeat_decode(const uint8_t *buf, size_t len,
                               altcross_heartbeat_type_t *out_type,
                               char *out_device_id, size_t device_id_size) {
    if (len < HB_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE ||
        memcmp(buf, MAGIC, 4) != 0 || buf[4] != 1) {
        return 0;
    }
    *out_type = (altcross_heartbeat_type_t)buf[5];
    if (*out_type != ALTCROSS_HEARTBEAT_PING &&
        *out_type != ALTCROSS_HEARTBEAT_PONG) {
        return 0;
    }
    size_t id_len = len - HB_HEADER_SIZE;
    if (id_len > device_id_size - 1) {
        id_len = device_id_size - 1;
    }
    memcpy(out_device_id, buf + HB_HEADER_SIZE, id_len);
    out_device_id[id_len] = '\0';
    return 1;
}

static void send_pings(void) {
    uint8_t buf[ALTCROSS_HEARTBEAT_MAX_SIZE];
    for (int i = 0; i < g_peer_count; i++) {
        if (g_peers[i].host[0] == '\0') {
            continue;
        }
        int len = altcross_heartbeat_encode(ALTCROSS_HEARTBEAT_PING, g_local_id,
                                             buf, sizeof(buf));
        if (len > 0) {
            altcross_socket_send_to(g_sock, g_peers[i].host, g_peers[i].port,
                                     buf, (size_t)len);
        }
    }
}

static void check_timeouts(void) {
    uint64_t now = now_ms();
    for (int i = 0; i < g_peer_count; i++) {
        if (g_peers[i].online && g_peers[i].last_pong_ms > 0 &&
            (now - g_peers[i].last_pong_ms) > ALTCROSS_HEARTBEAT_TIMEOUT_MS) {
            g_peers[i].online = 0;
            if (g_callback) {
                g_callback(g_peers[i].device_id, 0, g_user_data);
            }
        }
    }
}

static void handle_pong(const char *from_device_id) {
    uint64_t now = now_ms();
    for (int i = 0; i < g_peer_count; i++) {
        if (strcmp(g_peers[i].device_id, from_device_id) == 0) {
            int was_online = g_peers[i].online;
            g_peers[i].online = 1;
            g_peers[i].last_pong_ms = now;
            if (!was_online && g_callback) {
                g_callback(g_peers[i].device_id, 1, g_user_data);
            }
            break;
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI monitor_thread_proc(LPVOID arg) {
    (void)arg;
#else
static void *monitor_thread_proc(void *arg) {
    (void)arg;
#endif

    uint64_t last_ping_ms = 0;

    while (g_running) {
        uint64_t now = now_ms();

        if ((now - last_ping_ms) >= ALTCROSS_HEARTBEAT_INTERVAL_MS) {
            send_pings();
            last_ping_ms = now;
        }

        check_timeouts();

        uint8_t buf[ALTCROSS_HEARTBEAT_MAX_SIZE];
        char from_host[ALTCROSS_HOST_SIZE];
        int from_port;
        int n = altcross_socket_receive_from(g_sock, buf, sizeof(buf), 500,
                                              from_host, sizeof(from_host),
                                              &from_port);
        if (n == ALTCROSS_SOCKET_TIMEOUT || n < 0) {
            continue;
        }

        altcross_heartbeat_type_t type;
        char from_device_id[ALTCROSS_DEVICE_ID_SIZE];
        if (!altcross_heartbeat_decode(buf, (size_t)n, &type, from_device_id,
                                        sizeof(from_device_id))) {
            continue;
        }

        if (strcmp(from_device_id, g_local_id) == 0) {
            continue;
        }

        if (type == ALTCROSS_HEARTBEAT_PING) {
            uint8_t reply_buf[ALTCROSS_HEARTBEAT_MAX_SIZE];
            int reply_len = altcross_heartbeat_encode(
                ALTCROSS_HEARTBEAT_PONG, g_local_id, reply_buf,
                sizeof(reply_buf));
            if (reply_len > 0) {
                altcross_socket_send_to(g_sock, from_host, from_port,
                                         reply_buf, (size_t)reply_len);
            }
        } else if (type == ALTCROSS_HEARTBEAT_PONG) {
            handle_pong(from_device_id);
        }
    }

#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

static void populate_peers_from_store(const altcross_pairing_store_t *store) {
    g_peer_count = 0;
    for (int i = 0; i < store->count && g_peer_count < MAX_MONITORED_PEERS;
         i++) {
        const altcross_trusted_device_t *dev = &store->devices[i];
        if (dev->device_id[0] == '\0') {
            continue;
        }
        /* snprintf trunca e termina em '\0' sem o warning de depreca��o
         * do strncpy no MSVC (C4996). */
        snprintf(g_peers[g_peer_count].device_id, ALTCROSS_DEVICE_ID_SIZE,
                 "%s", dev->device_id);
        snprintf(g_peers[g_peer_count].host, ALTCROSS_HOST_SIZE, "%s",
                 dev->last_host);
        g_peers[g_peer_count].port = dev->last_port;
        g_peers[g_peer_count].online = 0;
        g_peers[g_peer_count].last_pong_ms = 0;
        g_peer_count++;
    }
}

int altcross_connection_monitor_start(const char *store_path,
                                       const char *my_device_id,
                                       altcross_connection_status_cb callback,
                                       void *user_data) {
    if (g_running) {
        return 1;
    }
    if (!my_device_id || my_device_id[0] == '\0') {
        return 1;
    }
    snprintf(g_local_id, sizeof(g_local_id), "%s", my_device_id);

    g_sock = altcross_socket_open_udp(ALTCROSS_HEARTBEAT_PORT);
    if (!g_sock) {
        return 1;
    }

    g_callback = callback;
    g_user_data = user_data;

    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    if (store_path && store_path[0] != '\0') {
        altcross_pairing_store_load(&store, store_path);
    }
    populate_peers_from_store(&store);

    g_running = 1;

#if defined(_WIN32)
    g_thread = CreateThread(NULL, 0, monitor_thread_proc, NULL, 0, NULL);
    if (!g_thread) {
        g_running = 0;
        altcross_socket_close(g_sock);
        g_sock = NULL;
        return 1;
    }
#else
    if (pthread_create(&g_thread, NULL, monitor_thread_proc, NULL) != 0) {
        g_running = 0;
        altcross_socket_close(g_sock);
        g_sock = NULL;
        return 1;
    }
#endif

    return 0;
}

void altcross_connection_monitor_stop(void) {
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

    altcross_socket_close(g_sock);
    g_sock = NULL;
    g_callback = NULL;
    g_user_data = NULL;
}

int altcross_connection_monitor_get_peers(altcross_peer_status_t *out,
                                           int max_count) {
    if (!g_running) {
        return -1;
    }
    int count = g_peer_count < max_count ? g_peer_count : max_count;
    for (int i = 0; i < count; i++) {
        out[i] = g_peers[i];
    }
    return count;
}

int altcross_connection_monitor_is_peer_online(const char *device_id) {
    if (!g_running) {
        return -1;
    }
    for (int i = 0; i < g_peer_count; i++) {
        if (strcmp(g_peers[i].device_id, device_id) == 0) {
            return g_peers[i].online;
        }
    }
    return -1;
}
