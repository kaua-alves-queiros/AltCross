#include "altcross/handoff_protocol.h"

#include <stdio.h>
#include <string.h>

#include "altcross/hmac_sha256.h"

static const char HANDOFF_MAGIC[4] = {'A', 'L', 'T', 'H'};
#define HANDOFF_VERSION 1
#define HANDOFF_HEADER_SIZE 6 /* magic(4) + versão(1) + reservado(1) */
#define HANDOFF_MAX_SIZE                                                     \
    (HANDOFF_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 4 +                     \
     ALTCROSS_SHA256_DIGEST_SIZE + ALTCROSS_PACKET_MAX_SIZE)

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t get_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

int altcross_handoff_send(altcross_socket_t *sock, const char *peer_host,
                           const char *my_device_id,
                           const uint8_t secret[ALTCROSS_HANDOFF_SECRET_SIZE],
                           uint32_t seq, const altcross_packet_t *pkt) {
    uint8_t payload[ALTCROSS_PACKET_MAX_SIZE];
    int payload_len = altcross_protocol_encode(pkt, payload, sizeof(payload));
    if (payload_len < 0) {
        return 1;
    }

    /* Tudo que a HMAC assina: device_id + seq + payload (não o header
     * magic/versão — esses não precisam de autenticação, só o conteúdo). */
    uint8_t signed_part[ALTCROSS_DEVICE_ID_SIZE + 4 + ALTCROSS_PACKET_MAX_SIZE];
    memset(signed_part, 0, ALTCROSS_DEVICE_ID_SIZE);
    snprintf((char *)signed_part, ALTCROSS_DEVICE_ID_SIZE, "%s", my_device_id);
    put_u32(signed_part + ALTCROSS_DEVICE_ID_SIZE, seq);
    memcpy(signed_part + ALTCROSS_DEVICE_ID_SIZE + 4, payload,
           (size_t)payload_len);
    size_t signed_len = ALTCROSS_DEVICE_ID_SIZE + 4 + (size_t)payload_len;

    uint8_t hmac[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_hmac_sha256(secret, ALTCROSS_HANDOFF_SECRET_SIZE, signed_part,
                          signed_len, hmac);

    uint8_t buf[HANDOFF_MAX_SIZE];
    memcpy(buf, HANDOFF_MAGIC, 4);
    buf[4] = HANDOFF_VERSION;
    buf[5] = 0;
    uint8_t *p = buf + HANDOFF_HEADER_SIZE;
    memcpy(p, signed_part, signed_len);
    p += signed_len;
    memcpy(p, hmac, ALTCROSS_SHA256_DIGEST_SIZE);
    p += ALTCROSS_SHA256_DIGEST_SIZE;

    size_t total = (size_t)(p - buf);
    int sent = altcross_socket_send_to(sock, peer_host, ALTCROSS_HANDOFF_PORT,
                                        buf, total);
    return sent > 0 ? 0 : 1;
}

int altcross_handoff_receive(altcross_socket_t *sock, int timeout_ms,
                              altcross_handoff_secret_lookup_t lookup_secret,
                              void *user_data,
                              altcross_handoff_received_t *out) {
    uint8_t buf[HANDOFF_MAX_SIZE];
    int n = altcross_socket_receive(sock, buf, sizeof(buf), timeout_ms);
    if (n == ALTCROSS_SOCKET_TIMEOUT || n < 0) {
        return 0;
    }
    size_t len = (size_t)n;

    if (len < HANDOFF_HEADER_SIZE + ALTCROSS_DEVICE_ID_SIZE + 4 +
                  ALTCROSS_SHA256_DIGEST_SIZE) {
        return 0;
    }
    if (memcmp(buf, HANDOFF_MAGIC, 4) != 0 || buf[4] != HANDOFF_VERSION) {
        return 0;
    }

    const uint8_t *signed_part = buf + HANDOFF_HEADER_SIZE;
    size_t signed_len =
        len - HANDOFF_HEADER_SIZE - ALTCROSS_SHA256_DIGEST_SIZE;
    const uint8_t *received_hmac = buf + HANDOFF_HEADER_SIZE + signed_len;

    char sender_device_id[ALTCROSS_DEVICE_ID_SIZE];
    memcpy(sender_device_id, signed_part, ALTCROSS_DEVICE_ID_SIZE);
    sender_device_id[ALTCROSS_DEVICE_ID_SIZE - 1] = '\0';

    uint8_t secret[ALTCROSS_HANDOFF_SECRET_SIZE];
    if (!lookup_secret(sender_device_id, secret, user_data)) {
        return 0;
    }

    uint8_t expected_hmac[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_hmac_sha256(secret, ALTCROSS_HANDOFF_SECRET_SIZE, signed_part,
                          signed_len, expected_hmac);
    if (!altcross_hmac_constant_time_equal(expected_hmac, received_hmac,
                                            ALTCROSS_SHA256_DIGEST_SIZE)) {
        return 0;
    }

    size_t payload_len = signed_len - ALTCROSS_DEVICE_ID_SIZE - 4;
    altcross_packet_t pkt;
    if (!altcross_protocol_decode(signed_part + ALTCROSS_DEVICE_ID_SIZE + 4,
                                   payload_len, &pkt)) {
        return 0;
    }

    snprintf(out->sender_device_id, sizeof(out->sender_device_id), "%s",
             sender_device_id);
    out->seq = get_u32(signed_part + ALTCROSS_DEVICE_ID_SIZE);
    out->packet = pkt;
    return 1;
}

int altcross_handoff_replay_check(uint32_t *last_seq, uint32_t seq) {
    if (seq <= *last_seq && *last_seq != 0) {
        return 0;
    }
    if (*last_seq == 0 && seq == 0) {
        /* 0 nunca é uma sequência válida de verdade (quem manda começa em
         * 1) — só bate aqui se alguém mandar seq=0 de propósito, rejeita. */
        return 0;
    }
    *last_seq = seq;
    return 1;
}
