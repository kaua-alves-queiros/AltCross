#include "altcross/discovery.h"

#include <stdio.h>
#include <string.h>

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
