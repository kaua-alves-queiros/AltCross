#include <string.h>

#include "altcross/handoff_protocol.h"
#include "test_framework.h"

static const uint8_t SECRET_A[ALTCROSS_HANDOFF_SECRET_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
};
static const uint8_t SECRET_B[ALTCROSS_HANDOFF_SECRET_SIZE] = {
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
};

/* Simula o cadastro de confiança de um dispositivo só: sabe o segredo de
 * "device-A", rejeita qualquer outro remetente. */
static int lookup_only_device_a(const char *sender_device_id,
                                 uint8_t out_secret[ALTCROSS_HANDOFF_SECRET_SIZE],
                                 void *user_data) {
    (void)user_data;
    if (strcmp(sender_device_id, "device-A") != 0) {
        return 0;
    }
    memcpy(out_secret, SECRET_A, ALTCROSS_HANDOFF_SECRET_SIZE);
    return 1;
}

static altcross_packet_t mouse_delta_packet(int32_t dx, int32_t dy) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_MOUSE_DELTA;
    pkt.data.mouse_delta.dx = dx;
    pkt.data.mouse_delta.dy = dy;
    return pkt;
}

static void test_send_receive_round_trip_with_correct_secret(void) {
    altcross_socket_t *receiver = altcross_socket_open_udp(ALTCROSS_HANDOFF_PORT);
    ASSERT_TRUE(receiver != NULL);
    altcross_socket_t *sender = altcross_socket_open_udp(0);
    ASSERT_TRUE(sender != NULL);

    altcross_packet_t pkt = mouse_delta_packet(12, -7);
    int rc = altcross_handoff_send(sender, "127.0.0.1", "device-A", SECRET_A,
                                    1, &pkt);
    ASSERT_EQ(0, rc);

    altcross_handoff_received_t received;
    int got = altcross_handoff_receive(receiver, 2000, lookup_only_device_a,
                                        NULL, &received);
    ASSERT_EQ(1, got);
    ASSERT_EQ(0, strcmp("device-A", received.sender_device_id));
    ASSERT_EQ(1u, received.seq);
    ASSERT_EQ(ALTCROSS_PKT_MOUSE_DELTA, received.packet.type);
    ASSERT_EQ(12, received.packet.data.mouse_delta.dx);
    ASSERT_EQ(-7, received.packet.data.mouse_delta.dy);

    altcross_socket_close(sender);
    altcross_socket_close(receiver);
}

static void test_receive_rejects_wrong_secret(void) {
    altcross_socket_t *receiver = altcross_socket_open_udp(ALTCROSS_HANDOFF_PORT);
    altcross_socket_t *sender = altcross_socket_open_udp(0);

    /* manda como se fosse device-A, mas assinado com o segredo ERRADO
     * (SECRET_B) — como um atacante que não sabe o segredo de verdade. */
    altcross_packet_t pkt = mouse_delta_packet(5, 5);
    altcross_handoff_send(sender, "127.0.0.1", "device-A", SECRET_B, 1, &pkt);

    altcross_handoff_received_t received;
    int got = altcross_handoff_receive(receiver, 500, lookup_only_device_a,
                                        NULL, &received);
    ASSERT_EQ(0, got);

    altcross_socket_close(sender);
    altcross_socket_close(receiver);
}

static void test_receive_rejects_unknown_sender(void) {
    altcross_socket_t *receiver = altcross_socket_open_udp(ALTCROSS_HANDOFF_PORT);
    altcross_socket_t *sender = altcross_socket_open_udp(0);

    altcross_packet_t pkt = mouse_delta_packet(1, 1);
    /* device-Z nunca foi cadastrado em lookup_only_device_a */
    altcross_handoff_send(sender, "127.0.0.1", "device-Z", SECRET_A, 1, &pkt);

    altcross_handoff_received_t received;
    int got = altcross_handoff_receive(receiver, 500, lookup_only_device_a,
                                        NULL, &received);
    ASSERT_EQ(0, got);

    altcross_socket_close(sender);
    altcross_socket_close(receiver);
}

static void test_receive_rejects_tampered_payload(void) {
    /* altcross_handoff_send sempre manda pra ALTCROSS_HANDOFF_PORT — então
     * pra interceptar de verdade, quem faz esse papel aqui é `relay`,
     * ligado nessa porta fixa; ele adultera 1 byte da parte assinada e
     * reenvia pra `receiver` (numa porta qualquer, o real destinatário do
     * teste), simulando um atacante on-path alterando o pacote em
     * trânsito. */
    altcross_socket_t *relay = altcross_socket_open_udp(ALTCROSS_HANDOFF_PORT);
    altcross_socket_t *receiver = altcross_socket_open_udp(0);
    altcross_socket_t *sender = altcross_socket_open_udp(0);

    altcross_packet_t pkt = mouse_delta_packet(3, 4);
    altcross_handoff_send(sender, "127.0.0.1", "device-A", SECRET_A, 1, &pkt);

    uint8_t buf[256];
    char from_host[64];
    int from_port;
    int n = altcross_socket_receive_from(relay, buf, sizeof(buf), 2000,
                                          from_host, sizeof(from_host),
                                          &from_port);
    ASSERT_TRUE(n > 0);

    /* adultera 1 byte bem no meio do envelope (dentro da parte assinada) */
    buf[n / 2] ^= 0xFF;
    int receiver_port = altcross_socket_local_port(receiver);
    altcross_socket_send_to(relay, "127.0.0.1", receiver_port, buf, (size_t)n);

    altcross_handoff_received_t received;
    int got = altcross_handoff_receive(receiver, 2000, lookup_only_device_a,
                                        NULL, &received);
    ASSERT_EQ(0, got);

    altcross_socket_close(sender);
    altcross_socket_close(relay);
    altcross_socket_close(receiver);
}

static void test_replay_check_accepts_strictly_increasing(void) {
    uint32_t last_seq = 0;
    ASSERT_TRUE(altcross_handoff_replay_check(&last_seq, 1));
    ASSERT_EQ(1u, last_seq);
    ASSERT_TRUE(altcross_handoff_replay_check(&last_seq, 2));
    ASSERT_TRUE(altcross_handoff_replay_check(&last_seq, 100));
}

static void test_replay_check_rejects_repeat_and_out_of_order(void) {
    uint32_t last_seq = 0;
    ASSERT_TRUE(altcross_handoff_replay_check(&last_seq, 10));
    ASSERT_TRUE(!altcross_handoff_replay_check(&last_seq, 10)); /* repetido */
    ASSERT_TRUE(!altcross_handoff_replay_check(&last_seq, 5));  /* fora de ordem */
    ASSERT_EQ(10u, last_seq); /* estado não avançou nas rejeições */
}

static void test_replay_check_rejects_zero(void) {
    uint32_t last_seq = 0;
    ASSERT_TRUE(!altcross_handoff_replay_check(&last_seq, 0));
}

void run_handoff_protocol_tests(void) {
    RUN_TEST(test_send_receive_round_trip_with_correct_secret);
    RUN_TEST(test_receive_rejects_wrong_secret);
    RUN_TEST(test_receive_rejects_unknown_sender);
    RUN_TEST(test_receive_rejects_tampered_payload);
    RUN_TEST(test_replay_check_accepts_strictly_increasing);
    RUN_TEST(test_replay_check_rejects_repeat_and_out_of_order);
    RUN_TEST(test_replay_check_rejects_zero);
}
