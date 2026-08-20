#include "altcross/protocol.h"

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#define HEADER_SIZE 2 /* 1 byte versão + 1 byte tipo */

static void put_u32(uint8_t *buf, int32_t value) {
    uint32_t net = htonl((uint32_t)value);
    buf[0] = (uint8_t)(net >> 24);
    buf[1] = (uint8_t)(net >> 16);
    buf[2] = (uint8_t)(net >> 8);
    buf[3] = (uint8_t)(net);
}

static int32_t get_u32(const uint8_t *buf) {
    uint32_t net = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                   ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
    return (int32_t)ntohl(net);
}

static int payload_size(altcross_packet_type_t type) {
    switch (type) {
    case ALTCROSS_PKT_ENTER:
    case ALTCROSS_PKT_MOUSE_DELTA:
    case ALTCROSS_PKT_MOUSE_BUTTON:
    case ALTCROSS_PKT_KEY:
        return 8; /* 2 campos int32 */
    case ALTCROSS_PKT_LEAVE:
        return 0;
    default:
        return -1;
    }
}

int altcross_protocol_encode(const altcross_packet_t *pkt, uint8_t *buf,
                              size_t buf_size) {
    int payload = payload_size(pkt->type);
    if (payload < 0) {
        return -1;
    }

    size_t total = (size_t)(HEADER_SIZE + payload);
    if (buf_size < total) {
        return -1;
    }

    buf[0] = ALTCROSS_PROTOCOL_VERSION;
    buf[1] = (uint8_t)pkt->type;

    switch (pkt->type) {
    case ALTCROSS_PKT_ENTER:
        put_u32(buf + 2, pkt->data.enter.x);
        put_u32(buf + 6, pkt->data.enter.y);
        break;
    case ALTCROSS_PKT_MOUSE_DELTA:
        put_u32(buf + 2, pkt->data.mouse_delta.dx);
        put_u32(buf + 6, pkt->data.mouse_delta.dy);
        break;
    case ALTCROSS_PKT_MOUSE_BUTTON:
        put_u32(buf + 2, pkt->data.mouse_button.button);
        put_u32(buf + 6, pkt->data.mouse_button.down);
        break;
    case ALTCROSS_PKT_KEY:
        put_u32(buf + 2, pkt->data.key.keycode);
        put_u32(buf + 6, pkt->data.key.down);
        break;
    case ALTCROSS_PKT_LEAVE:
        break;
    }

    return (int)total;
}

int altcross_protocol_decode(const uint8_t *buf, size_t len,
                              altcross_packet_t *out_pkt) {
    if (len < HEADER_SIZE) {
        return 0;
    }
    if (buf[0] != ALTCROSS_PROTOCOL_VERSION) {
        return 0;
    }

    altcross_packet_type_t type = (altcross_packet_type_t)buf[1];
    int payload = payload_size(type);
    if (payload < 0) {
        return 0;
    }
    if (len < (size_t)(HEADER_SIZE + payload)) {
        return 0;
    }

    out_pkt->type = type;
    switch (type) {
    case ALTCROSS_PKT_ENTER:
        out_pkt->data.enter.x = get_u32(buf + 2);
        out_pkt->data.enter.y = get_u32(buf + 6);
        break;
    case ALTCROSS_PKT_MOUSE_DELTA:
        out_pkt->data.mouse_delta.dx = get_u32(buf + 2);
        out_pkt->data.mouse_delta.dy = get_u32(buf + 6);
        break;
    case ALTCROSS_PKT_MOUSE_BUTTON:
        out_pkt->data.mouse_button.button = get_u32(buf + 2);
        out_pkt->data.mouse_button.down = get_u32(buf + 6);
        break;
    case ALTCROSS_PKT_KEY:
        out_pkt->data.key.keycode = get_u32(buf + 2);
        out_pkt->data.key.down = get_u32(buf + 6);
        break;
    case ALTCROSS_PKT_LEAVE:
        break;
    }

    return 1;
}
