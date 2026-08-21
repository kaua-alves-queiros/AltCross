#ifndef ALTCROSS_CONNECTION_MONITOR_H
#define ALTCROSS_CONNECTION_MONITOR_H

#include <stddef.h>
#include <stdint.h>

#include "altcross/export.h"
#include "altcross/pairing.h"

#define ALTCROSS_HEARTBEAT_PORT 45205
#define ALTCROSS_HEARTBEAT_INTERVAL_MS 5000
#define ALTCROSS_HEARTBEAT_TIMEOUT_MS 15000
#define ALTCROSS_HEARTBEAT_MAX_SIZE 64

typedef enum {
    ALTCROSS_HEARTBEAT_PING = 1,
    ALTCROSS_HEARTBEAT_PONG = 2,
} altcross_heartbeat_type_t;

/* Callback chamado quando o status de um peer muda.
 * device_id: identificador do dispositivo.
 * online: 1 se ficou online, 0 se ficou offline. */
typedef void (*altcross_connection_status_cb)(const char *device_id,
                                              int online, void *user_data);

typedef struct {
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    char host[ALTCROSS_HOST_SIZE];
    int port;
    int online;
    uint64_t last_pong_ms;
} altcross_peer_status_t;

ALTCROSS_API int altcross_heartbeat_encode(altcross_heartbeat_type_t type,
                                            const char *device_id,
                                            uint8_t *buf, size_t buf_size);

ALTCROSS_API int altcross_heartbeat_decode(
    const uint8_t *buf, size_t len, altcross_heartbeat_type_t *out_type,
    char *out_device_id, size_t device_id_size);

/* Inicia o monitor em background thread. Carrega o store de store_path,
 * envia pings periódicos e chama callback quando um peer fica
 * online/offline. Retorna 0 em sucesso. */
ALTCROSS_API int
altcross_connection_monitor_start(const char *store_path,
                                   altcross_connection_status_cb callback,
                                   void *user_data);

ALTCROSS_API void altcross_connection_monitor_stop(void);

ALTCROSS_API int
altcross_connection_monitor_get_peers(altcross_peer_status_t *out,
                                       int max_count);

ALTCROSS_API int
altcross_connection_monitor_is_peer_online(const char *device_id);

#endif
